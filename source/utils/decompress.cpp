/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * decompress.cpp
 *
 * File decompression routines
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <zlib.h>
#include <sys/stat.h>
#include <memory>

#include "snes9xgx.h"
#include "fileop.h"
#include "filebrowser.h"
#include "../menu/menu.h"
#include "../libgui/Gui.h"
#include "decompress.h"

extern "C" {
#include "sz/7zCrc.h"
#include "sz/7zIn.h"
#include "sz/7zExtract.h"
}

#define ZIPCHUNK 2048

#if defined(HW_RVL) || defined(HW_DOL)
#define ZIP_READ_CHUNK ZIPCHUNK
#else
#define ZIP_READ_CHUNK FILE_READ_CHUNK
#endif

/*
 * Zip files are stored little endian - read fields byte by byte so the
 * parsing does not depend on the host's endianness or struct packing
 */
static uint16_t
LE16 (const unsigned char * p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t
LE32 (const unsigned char * p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define ZIP_LOCAL_HEADER_SIZE 30
#define ZIP_FLAG_ENCRYPTED    0x0001
#define ZIP_FLAG_DATA_DESC    0x0008 // sizes/CRC follow the data; the local header holds zeros
#define ZIP_METHOD_DEFLATE    8

/****************************************************************************
 * IsZipFile
 *
 * Returns TRUE when 0x504b0304 is first four characters of buffer
 ***************************************************************************/
int
IsZipFile (char *buffer)
{
	unsigned int *check = (unsigned int *) buffer;

	if (check[0] == 0x504b0304)
		return 1;

	return 0;
}

/*****************************************************************************
* ZipCentralDirUncompressedSize
*
* Returns the uncompressed size of the first entry according to the central
* directory at the end of the file, or 0 if it can't be determined.
* Archives written by streaming zippers set the "data descriptor" flag and
* leave the sizes in the local header as zero; the central directory always
* has the real values. Moves the file position - the caller must seek again.
******************************************************************************/

static uint32_t
ZipCentralDirUncompressedSize (FILE * fp)
{
	const size_t TAIL_SIZE = 4096; // enough for the end record plus a typical archive comment
	uint32_t result = 0;

	if(fseeko(fp, 0, SEEK_END) != 0)
		return 0;

	off_t fileSize = ftello(fp);

	if(fileSize < 22)
		return 0;

	size_t tailSize = ((uint64_t)fileSize < TAIL_SIZE) ? (size_t)fileSize : TAIL_SIZE;
	unsigned char * tail = (unsigned char *)malloc(tailSize);

	if(!tail)
		return 0;

	if(fseeko(fp, fileSize - tailSize, SEEK_SET) == 0 && fread(tail, 1, tailSize, fp) == tailSize)
	{
		// find the end of central directory record (PK\5\6), searching backwards
		for(size_t i = tailSize - 22 + 1; i-- > 0; )
		{
			if(LE32(&tail[i]) != 0x06054b50)
				continue;

			uint16_t entries = LE16(&tail[i + 10]);
			uint32_t cdOffset = LE32(&tail[i + 16]);
			unsigned char cd[46];

			if(entries > 0 && (off_t)cdOffset < fileSize
				&& fseeko(fp, cdOffset, SEEK_SET) == 0
				&& fread(cd, 1, sizeof(cd), fp) == sizeof(cd)
				&& LE32(cd) == 0x02014b50)
			{
				result = LE32(&cd[24]); // uncompressed size
				if(result == 0xFFFFFFFF) // ZIP64 marker - the real size lives in the extra field
					result = 0;
			}
			break;
		}
	}

	free(tail);
	return result;
}

/*****************************************************************************
* UnZipBuffer
*
* Inflates the first entry of the ZIP read from fp into outbuffer.
* fp is owned by the caller (opened before, closed after); it is read from
* the start of the file.
*
* Returns the number of bytes written to outbuffer, or 0 on failure. On
* failure *errorMsg (if given) points to a message suitable for ErrorPrompt()
* (a string literal - nothing to free). Nothing is displayed here.
******************************************************************************/

size_t
UnZipBuffer (FILE * fp, unsigned char *outbuffer, size_t buffersize, const char ** errorMsg)
{
	size_t zipoffset = 0;
	size_t zipchunk = 0;
	char out[ZIPCHUNK];
	z_stream zs;
	int res = Z_ERRNO;
	size_t bufferoffset = 0;
	size_t have = 0;
	size_t sizeread = 0;
	uint64_t inputRead = 0;
	off_t fileSize = 0;
	uint16_t flags = 0;
	uint16_t method = 0;
	uint32_t knownSize = 0;
	bool inflateStarted = false;
	const char * error = nullptr;

	// input buffer is on the heap - it can be far larger than the worker thread's stack allows
	std::unique_ptr<char, decltype(&free)> readbufferOwner((char *)memalign(FILE_BUFFER_ALIGN, ZIP_READ_CHUNK), free);
	unsigned char * readbuffer = (unsigned char *)readbufferOwner.get();

	if(errorMsg)
		*errorMsg = nullptr;

	if(!readbuffer)
	{
		error = "Out of memory!";
		goto done;
	}

	if(!fp)
	{
		error = "Error reading file!";
		goto done;
	}

	// the file size lets us report progress on the compressed input, which is always known
	if(fseeko(fp, 0, SEEK_END) == 0)
		fileSize = ftello(fp);

	// Read Zip Header
	fseeko(fp, 0, SEEK_SET);
	sizeread = fread (readbuffer, 1, ZIP_READ_CHUNK, fp);
	inputRead = sizeread;

	if(sizeread < ZIP_LOCAL_HEADER_SIZE || LE32(readbuffer) != 0x04034b50)
	{
		error = ferror(fp) ? "Error reading file!" : "Error - Invalid ZIP file!";
		goto done;
	}

	flags = LE16(&readbuffer[6]);
	method = LE16(&readbuffer[8]);
	knownSize = LE32(&readbuffer[22]);
	zipoffset = ZIP_LOCAL_HEADER_SIZE + LE16(&readbuffer[26]) + LE16(&readbuffer[28]);

	if((flags & ZIP_FLAG_ENCRYPTED) || method != ZIP_METHOD_DEFLATE)
	{
		error = "File uses unsupported compression settings.";
		goto done;
	}

	// Streamed archives have zeros (or the ZIP64 marker) here - ask the central directory. This is
	// only used for an early "too large" check; the output is bounds-checked below whether or not
	// it is known.
	if((flags & ZIP_FLAG_DATA_DESC) || knownSize == 0 || knownSize == 0xFFFFFFFF)
	{
		knownSize = ZipCentralDirUncompressedSize(fp);
		fseeko(fp, sizeread, SEEK_SET); // resume where the header read left off
	}

	if(knownSize > buffersize)
	{
		error = "Compressed ROM file is too large to decompress!";
		goto done;
	}

	/*** Prepare the zip stream ***/
	memset (&zs, 0, sizeof (z_stream));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = 0;
	zs.next_in = Z_NULL;
	res = inflateInit2 (&zs, -MAX_WBITS);

	if (res != Z_OK)
	{
		error = "Out of memory!";
		goto done;
	}
	inflateStarted = true;

	/*** Skip the filename + extra field - they can be longer than the first read ***/
	if(zipoffset >= sizeread)
	{
		if(fseeko(fp, zipoffset, SEEK_SET) != 0)
		{
			error = "Error - Invalid ZIP file!";
			goto done;
		}

		sizeread = fread (readbuffer, 1, ZIP_READ_CHUNK, fp);
		inputRead = zipoffset + sizeread;
		zipoffset = 0;

		if(sizeread == 0)
		{
			error = ferror(fp) ? "Error reading file!" : "Error - Invalid ZIP file!";
			goto done;
		}
	}

	zipchunk = sizeread - zipoffset;

	/*** Now do it! ***/
	for(;;)
	{
		zs.avail_in = zipchunk;
		zs.next_in = (Bytef *) & readbuffer[zipoffset];

		/*** Now inflate until input buffer is exhausted ***/
		do
		{
			zs.avail_out = ZIPCHUNK;
			zs.next_out = (Bytef *) & out;

			res = inflate (&zs, Z_NO_FLUSH);

			// Z_BUF_ERROR just means "needs more input" here
			if (res != Z_OK && res != Z_STREAM_END && res != Z_BUF_ERROR)
			{
				error = (res == Z_MEM_ERROR) ? "Out of memory!" : "Error extracting ZIP file!";
				goto done;
			}

			have = ZIPCHUNK - zs.avail_out;
			if (have)
			{
				// the size is never taken from the file's own headers, so never trust it to fit
				if (have > buffersize - bufferoffset)
				{
					error = "Compressed ROM file is too large to decompress!";
					goto done;
				}

				/*** Copy to normal block buffer ***/
				memcpy (&outbuffer[bufferoffset], &out, have);
				bufferoffset += have;
			}
		}
		while (res != Z_STREAM_END && zs.avail_out == 0);

		if (res == Z_STREAM_END)
			break;

		// Readup the next block
		zipoffset = 0;

		sizeread = fread (readbuffer, 1, ZIP_READ_CHUNK, fp);
		if(sizeread == 0)
		{
			// the data ended before the deflate stream did
			error = ferror(fp) ? "Error reading file!" : "Error extracting ZIP file!";
			goto done;
		}
		zipchunk = sizeread;
		inputRead += sizeread;

		if(fileSize > 0)
			ShowProgress ("Loading...", (int)inputRead, (int)fileSize);
	}

done:
	if(inflateStarted)
		inflateEnd (&zs);
	CancelAction();

	if(!error && bufferoffset == 0)
		error = "Empty or invalid ZIP file!";

	if(error)
	{
		if(errorMsg)
			*errorMsg = error;
		return 0;
	}

	return bufferoffset;
}

/****************************************************************************
* GetFirstZipFilename
*
* Returns the filename of the first file in the zipped archive
* The idea here is to do the least amount of work required
***************************************************************************/

char *
GetFirstZipFilename ()
{
	char * firstFilename = nullptr;
	char tempbuffer[ZIPCHUNK];
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_ROM))
		return nullptr;

	// read start of ZIP
	size_t startSize = LoadFile (tempbuffer, filepath, ZIPCHUNK, ZIPCHUNK, NOTSILENT);

	if(startSize == 0)
		return nullptr;

	if(startSize < 35) // too short to hold a ZIP entry
	{
		ErrorPrompt("Empty or invalid ZIP file!");
		return nullptr;
	}

	tempbuffer[28] = 0; // truncate - filename length is 2 bytes long (bytes 26-27)
	int namelength = tempbuffer[26]; // filename length starts 26 bytes in

	if(namelength < 0 || namelength > 200) // filename is not a reasonable length
	{
		ErrorPrompt("Error - Invalid ZIP file!");
		return nullptr;
	}
	
	firstFilename = &tempbuffer[30]; // first filename of a ZIP starts 31 bytes in
	firstFilename[namelength] = 0; // truncate at filename length
	firstFilename = strdup(firstFilename);

	if(!firstFilename)
		ErrorPrompt("Out of memory!");

	return firstFilename;
}

/****************************************************************************
* 7z functions
***************************************************************************/

typedef struct _SzFileInStream
{
   ISzInStream InStream;
   FILE * fp; // open archive - owned by the caller of SzParse()/SzExtractFile(), only valid during those calls
   uint64_t offset; // offset of the file
   unsigned int len; // length of the file
   uint64_t pos;  // current position of the file pointer
} SzFileInStream;

// 7zip error list
static const char * const szerrormsg[] = {
   "File is corrupt.", // 7z: Data error
   "Archive contains too many files.", // 7z: Out of memory
   "File is corrupt (CRC mismatch).", // 7z: CRC Error
   "File uses unsupported compression settings.", // 7z: Not implemented
   "File is corrupt.", // 7z: Fail
   "Failed to read file data.", // 7z: Data read failure
   "File is corrupt.", // 7z: Archive error
   "File uses too high of compression settings (dictionary size is too large).", // 7z: Dictionary too large
};

static SZ_RESULT SzRes;
static SzFileInStream SzArchiveStream;
static CArchiveDatabaseEx SzDb;
static ISzAlloc SzAllocImp;
static ISzAlloc SzAllocTempImp;
static UInt32 SzBlockIndex = 0xFFFFFFFF;
static size_t SzBufferSize;
static size_t SzOffset;
static size_t SzOutSizeProcessed;
static CFileItem *SzF;

static char sz_buffer[ZIP_READ_CHUNK] __attribute__((aligned(FILE_BUFFER_ALIGN)));
static int szMethod = 0;

/****************************************************************************
* Is7ZipFile
*
* Returns 1 when 7z signature is found
****************************************************************************/
int
Is7ZipFile (char *buffer)
{
	// 7z signature
	static Byte Signature[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

	int i;
	for(i = 0; i < 6; i++)
		if(buffer[i] != Signature[i])
			return 0;

	return 1; // 7z archive found
}

// display an error message
static void SzDisplayError(SZ_RESULT res)
{
	ErrorPrompt(TrMessage<>("7z decompression failed: %s", Tr(szerrormsg[(res - 1)])));
}

// function used by the 7zip SDK to read data from SD/USB/DVD/SMB
static SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxRequiredSize, size_t *processedSize)
{
	size_t sizeread = 0;

	if(maxRequiredSize == 0)
		return SZ_OK;

	// the void* object is a SzFileInStream
	SzFileInStream *s = (SzFileInStream *) object;

	if (maxRequiredSize > sizeof(sz_buffer))
		maxRequiredSize = sizeof(sz_buffer);

	// read data
	if(!s->fp)
		return SZE_FAILREAD;

	sizeread = fread(sz_buffer, 1, maxRequiredSize, s->fp);

	if(sizeread <= 0)
		return SZE_FAILREAD;

	*buffer = sz_buffer;
	*processedSize = sizeread;
	s->pos += sizeread;

	if(sizeread > 1024) // only show progress for large reads
		// this isn't quite right, but oh well
		ShowProgress ("Loading...", s->pos, browserList[browser.selIndex].length);

	return SZ_OK;
}

// function used by the 7zip SDK to change the filepointer
static SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
	// the void* object is a SzFileInStream
	SzFileInStream *s = (SzFileInStream *) object;

	// check if the 7z SDK wants to move the pointer to somewhere after the EOF
	if (pos >= s->len)
		return SZE_FAIL;

	// save new position and return
	if(!s->fp || fseek(s->fp, (long)pos, SEEK_SET) != 0)
		return SZE_FAIL;

	s->pos = pos;
	return SZ_OK;
}

/****************************************************************************
* SzClose
*
* Closes a 7z file
***************************************************************************/

void SzClose()
{
	if(SzDb.Database.NumFiles > 0)
		SzArDbExFree(&SzDb, SzAllocImp.Free);
}

/****************************************************************************
* SzBaseName
*
* Returns the last path component of a 7z entry name, treating both '/' and
* '\\' (archives made on Windows) as separators.
***************************************************************************/
static const char * SzBaseName(const char * name)
{
	if(!name)
		return nullptr;

	const char * base = name;
	for(const char * c = name; *c; c++)
	{
		if(*c == '/' || *c == '\\')
			base = c + 1;
	}

	if(base[0] == 0 || strcmp(base, ".") == 0 || strcmp(base, "..") == 0)
		return nullptr;

	return base;
}

/****************************************************************************
* SzParse
*
* Opens a 7z file, and parses it
* It parses the entire 7z for full browsing capability
***************************************************************************/

int SzParse(char * filepath)
{
	if(!filepath)
		return 0;
	
	int device;
	
	struct stat filestat;
	if(stat(filepath, &filestat) < 0)
	{
		ErrorPrompt("Error opening archive!");
		return 0;
	}
	unsigned int filelen = filestat.st_size;
	
	if(!FindDevice(filepath, &device) || !filelen)
	{
		ErrorPrompt("Error opening archive!");
		return 0;
	}

	int nbfiles = 0;
	// setup archive stream
	SzArchiveStream.offset = 0;
	SzArchiveStream.len = filelen;
	SzArchiveStream.pos = 0;

	// open file
	FILE * fp = fopen (filepath, "rb");
	if(!fp)
	{
		ErrorPrompt("Error opening archive!");
		return 0;
	}
	SzArchiveStream.fp = fp;

	// set szMethod to current chosen load device
	szMethod = device;

	// set handler functions for reading data from SD/USB/SMB/DVD
	SzArchiveStream.InStream.Read = SzFileReadImp;
	SzArchiveStream.InStream.Seek = SzFileSeekImp;

	// set default 7Zip SDK handlers for allocation and freeing memory
	SzAllocImp.Alloc = SzAlloc;
	SzAllocImp.Free = SzFree;
	SzAllocTempImp.Alloc = SzAllocTemp;
	SzAllocTempImp.Free = SzFreeTemp;

	// prepare CRC and 7Zip database structures
	InitCrcTable();
	SzArDbExInit(&SzDb);

	// open the archive
	SzRes = SzArchiveOpen(&SzArchiveStream.InStream, &SzDb, &SzAllocImp,
			&SzAllocTempImp);

	if (SzRes != SZ_OK)
	{
		SzDisplayError(SzRes);
		// free memory used by the 7z SDK
		SzClose();
	}
	else // archive opened successfully
	{
		if(SzDb.Database.NumFiles > 0)
		{
			// Parses the 7z into a full file listing

			HaltParseThread(); // halt parsing
			ResetBrowser(); // reset browser

			// add '..' folder in case the user wants exit the 7z
			AddBrowserEntry();
			sprintf(browserList[0].filename, "..");
			sprintf(browserList[0].displayname, "Up One Level");
			browserList[0].isdir = 1;
			browserList[0].icon = ICON_FOLDER;

			// get contents and parse them into file list structure
			unsigned int SzI, SzJ;
			SzJ = 1;
			for (SzI = 0; SzI < SzDb.Database.NumFiles; SzI++)
			{
				SzF = SzDb.Database.Files + SzI;

				// skip directories
				if (SzF->IsDirectory)
					continue;

				// only the base name is used - an entry name must not carry directory parts
				const char * baseName = SzBaseName(SzF->Name);
				if(!baseName)
					continue;

				if(!AddBrowserEntry())
				{
					ResetBrowser();
					ErrorPrompt("Out of memory: too many files!");
					SzClose();
					SzJ = 0;
					break;
				}

				// parse information about this file to the file list structure
				snprintf(browserList[SzJ].filename, MAXJOLIET, "%s", baseName);
				StripExt(browserList[SzJ].displayname, browserList[SzJ].filename);
				char* strPos = strstr(browserList[SzJ].displayname, szname);
				// (not when the file is named exactly like the archive: nothing would be left to show)
				if(strPos && strPos[strlen(szname)] != 0)
				{
					snprintf(browserList[SzJ].displayname, MAXJOLIET, "%s", strPos + strlen(szname));
				}
				
				browserList[SzJ].length = SzF->Size; // filesize
				browserList[SzJ].isdir = 0; // only files will be displayed (-> no flags)
				browserList[SzJ].filenum = SzI; // the extraction function identifies the file with this number
				SzJ++;
			}
			nbfiles = SzJ;
		}
		else
		{
			SzClose();
			ErrorPrompt("Error opening archive!"); // no files in it
		}
	}

	CancelAction();

	// close file
	SzArchiveStream.fp = nullptr;
	fclose(fp);
	return nbfiles;
}

/****************************************************************************
* SzExtractFile
*
* Extracts the given file # into the buffer specified
* Must parse the 7z BEFORE running this function
* fp is the opened 7z archive, owned by the caller (closed by the caller)
***************************************************************************/

size_t SzExtractFile(FILE * fp, int i, unsigned char *buffer)
{
	if(!fp)
		return 0;

	// the SDK reads through SzArchiveStream - point it at the caller's handle
	SzArchiveStream.fp = fp;

	// prepare some variables
	SzBlockIndex = 0xFFFFFFFF;
	SzOffset = 0;

	// Unzip the file

	SzRes = SzExtract2(
		&SzArchiveStream.InStream,
		&SzDb,
		i,                      // index of file
		&SzBlockIndex,          // index of solid block
		&buffer,
		&SzBufferSize,
		&SzOffset,              // offset of stream for required file in *outBuffer
		&SzOutSizeProcessed,    // size of file in *outBuffer
		&SzAllocImp,
		&SzAllocTempImp);

	// the caller closes fp - don't keep a pointer to it
	SzArchiveStream.fp = nullptr;

	// close 7Zip archive and free memory
	SzClose();

	CancelAction();

	// check for errors
	if(SzRes != SZ_OK)
	{
		// display error message
		SzDisplayError(SzRes);
		return 0;
	}
	else if(SzOutSizeProcessed == 0) // nothing came out - not a usable file
	{
		SzDisplayError(SZE_DATA_ERROR);
		return 0;
	}
	else
	{
		return SzOutSizeProcessed;
	}
}
