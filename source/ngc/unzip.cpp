/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * Michniewski 2008
 * Tantric September 2008
 *
 * unzip.cpp
 *
 * File unzip routines
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

extern "C" {
#include "../sz/7zCrc.h"
#include "../sz/7zIn.h"
#include "../sz/7zExtract.h"
}

#include "snes9xGX.h"
#include "dvd.h"
#include "smbop.h"
#include "fileop.h"
#include "video.h"
#include "menudraw.h"
#include "unzip.h"

/*
  * PKWare Zip Header - adopted into zip standard
  */
#define PKZIPID 0x504b0304
#define MAXROM 0x500000
#define ZIPCHUNK 2048

/*
 * Zip files are stored little endian
 * Support functions for short and int types
 */
u32
FLIP32 (u32 b)
{
	unsigned int c;

	c = (b & 0xff000000) >> 24;
	c |= (b & 0xff0000) >> 8;
	c |= (b & 0xff00) << 8;
	c |= (b & 0xff) << 24;

	return c;
}

u16
FLIP16 (u16 b)
{
	u16 c;

	c = (b & 0xff00) >> 8;
	c |= (b & 0xff) << 8;

	return c;
}

/****************************************************************************
 * IsZipFile
 *
 * Returns TRUE when PKZIPID is first four characters of buffer
 ****************************************************************************/
int
IsZipFile (char *buffer)
{
	unsigned int *check;

	check = (unsigned int *) buffer;

	if (check[0] == PKZIPID)
		return 1;

	return 0;
}

/*****************************************************************************
* UnZipBuffer
*
* It should be noted that there is a limit of 5MB total size for any ROM
******************************************************************************/

int
UnZipBuffer (unsigned char *outbuffer, int method)
{
	PKZIPHEADER pkzip;
	int zipoffset = 0;
	int zipchunk = 0;
	char out[ZIPCHUNK];
	z_stream zs;
	int res;
	int bufferoffset = 0;
	int readoffset = 0;
	int have = 0;
	char readbuffer[ZIPCHUNK];
	char msg[128];
	u64 discoffset = 0;

	// Read Zip Header
	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
			fseek(fatfile, 0, SEEK_SET);
			fread (readbuffer, 1, ZIPCHUNK, fatfile);
			break;

		case METHOD_DVD:
			discoffset = dvddir;
			dvd_read (readbuffer, ZIPCHUNK, discoffset);
			break;

		case METHOD_SMB:
			SMB_ReadFile(readbuffer, ZIPCHUNK, 0, smbfile);
			break;
	}

	/*** Copy PKZip header to local, used as info ***/
	memcpy (&pkzip, readbuffer, sizeof (PKZIPHEADER));

	pkzip.uncompressedSize = FLIP32 (pkzip.uncompressedSize);

	sprintf (msg, "Unzipping %d bytes ... Wait",
	pkzip.uncompressedSize);
	ShowAction (msg);

	/*** Prepare the zip stream ***/
	memset (&zs, 0, sizeof (z_stream));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = 0;
	zs.next_in = Z_NULL;
	res = inflateInit2 (&zs, -MAX_WBITS);

	if (res != Z_OK)
		return 0;

	/*** Set ZipChunk for first pass ***/
	zipoffset =
	(sizeof (PKZIPHEADER) + FLIP16 (pkzip.filenameLength) +
	FLIP16 (pkzip.extraDataLength));
	zipchunk = ZIPCHUNK - zipoffset;

	/*** Now do it! ***/
	do
	{
		zs.avail_in = zipchunk;
		zs.next_in = (Bytef *) & readbuffer[zipoffset];

		/*** Now inflate until input buffer is exhausted ***/
		do
		{
			zs.avail_out = ZIPCHUNK;
			zs.next_out = (Bytef *) & out;

			res = inflate (&zs, Z_NO_FLUSH);

			if (res == Z_MEM_ERROR)
			{
				inflateEnd (&zs);
				return 0;
			}

			have = ZIPCHUNK - zs.avail_out;
			if (have)
			{
				/*** Copy to normal block buffer ***/
				memcpy (&outbuffer[bufferoffset], &out, have);
				bufferoffset += have;
			}
		}
		while (zs.avail_out == 0);

		// Readup the next 2k block
		zipoffset = 0;
		zipchunk = ZIPCHUNK;

		switch (method)
		{
			case METHOD_SD:
			case METHOD_USB:
				fread (readbuffer, 1, ZIPCHUNK, fatfile);
				break;

			case METHOD_DVD:
				readoffset += ZIPCHUNK;
				dvd_read (readbuffer, ZIPCHUNK, discoffset+readoffset);
				break;

			case METHOD_SMB:
				readoffset += ZIPCHUNK;
				SMB_ReadFile(readbuffer, ZIPCHUNK, readoffset, smbfile);
				break;
		}
	}
	while (res != Z_STREAM_END);

	inflateEnd (&zs);

	if (res == Z_STREAM_END)
	{
		if (pkzip.uncompressedSize == (u32) bufferoffset)
			return bufferoffset;
		else
			return pkzip.uncompressedSize;
	}

	return 0;
}

/****************************************************************************
* GetFirstZipFilename
*
* Returns the filename of the first file in the zipped archive
* The idea here is to do the least amount of work required
***************************************************************************/

char *
GetFirstZipFilename (int method)
{
	char * firstFilename = NULL;
	char tempbuffer[ZIPCHUNK];

	// read start of ZIP
	switch (method)
	{
		case METHOD_SD:	// SD Card
		case METHOD_USB: // USB
			LoadFATFile (tempbuffer, ZIPCHUNK);
			break;

		case METHOD_DVD: // DVD
			LoadDVDFile ((unsigned char *)tempbuffer, ZIPCHUNK);
			break;

		case METHOD_SMB: // From SMB
			LoadSMBFile (tempbuffer, ZIPCHUNK);
			break;
	}

	tempbuffer[28] = 0; // truncate - filename length is 2 bytes long (bytes 26-27)
	int namelength = tempbuffer[26]; // filename length starts 26 bytes in

	firstFilename = &tempbuffer[30]; // first filename of a ZIP starts 31 bytes in
	firstFilename[namelength] = 0; // truncate at filename length

	return firstFilename;
}

/****************************************************************************
* 7z functions
***************************************************************************/

typedef struct _SzFileInStream
{
   ISzInStream InStream;
   u64 offset; // offset of the file
   unsigned int len; // length of the file
   u64 pos;  // current position of the file pointer
} SzFileInStream;

// 7zip error list
char szerrormsg[][30] = {
   "7z: Data error",
   "7z: Out of memory",
   "7z: CRC Error",
   "7z: Not implemented",
   "7z: Fail",
   "7z: Archive error",
   "7z: Dictionary too large",
};

SZ_RESULT SzRes;

SzFileInStream SzArchiveStream;
CArchiveDatabaseEx SzDb;
ISzAlloc SzAllocImp;
ISzAlloc SzAllocTempImp;
UInt32 SzBlockIndex = 0xFFFFFFFF;
size_t SzBufferSize;
size_t SzOffset;
size_t SzOutSizeProcessed;
CFileItem *SzF;

char sz_buffer[2048];
int szMethod = 0;

/****************************************************************************
* Is7ZipFile
*
* Returns 1 when 7z signature is found
****************************************************************************/
int
Is7ZipFile (char *buffer)
{
	unsigned int *check;
	check = (unsigned int *) buffer;

	// 7z signature
	static Byte Signature[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

	int i;
	for(i = 0; i < 6; i++)
		if(buffer[i] != Signature[i])
			return 0;

	return 1; // 7z archive found
}

// display an error message
void SzDisplayError(SZ_RESULT res)
{
	WaitPrompt(szerrormsg[(res - 1)]);
}

// function used by the 7zip SDK to read data from SD/USB/DVD/SMB
SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxRequiredSize, size_t *processedSize)
{
   // the void* object is a SzFileInStream
   SzFileInStream *s = (SzFileInStream *)object;

   // calculate offset
   u64 offset = (u64)(s->offset + s->pos);

	if(maxRequiredSize > 2048)
		maxRequiredSize = 2048;

   // read data
	switch(szMethod)
	{
		case METHOD_SD:
		case METHOD_USB:
			fseek(fatfile, offset, SEEK_SET);
			fread (sz_buffer, 1, maxRequiredSize, fatfile);
			break;
		case METHOD_DVD:
			dvd_safe_read(sz_buffer, maxRequiredSize, offset);
			break;
		case METHOD_SMB:
			SMB_ReadFile(sz_buffer, maxRequiredSize, offset, smbfile);
			break;
	}

   *buffer = sz_buffer;
   *processedSize = maxRequiredSize;
   s->pos += *processedSize;

   return SZ_OK;
}

// function used by the 7zip SDK to change the filepointer
SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
   // the void* object is a SzFileInStream
   SzFileInStream *s = (SzFileInStream *)object;

   // check if the 7z SDK wants to move the pointer to somewhere after the EOF
   if(pos >= s->len)
   {
       WaitPrompt((char *)"7z Error: The 7z SDK wants to start reading somewhere behind the EOF...");
       return SZE_FAIL;
   }

   // save new position and return
   s->pos = pos;
   return SZ_OK;
}

/****************************************************************************
* SzParse
*
* Opens a 7z file, and parses it
* Right now doesn't parse 7z, since we'll always use the first file
* But it could parse the entire 7z for full browsing capability
***************************************************************************/

int SzParse(char * filepath, int method)
{
	int nbfiles = 0;

	// save the offset and the length of this file inside the archive stream structure
	SzArchiveStream.offset = filelist[selection].offset;
	SzArchiveStream.len = filelist[selection].length;
	SzArchiveStream.pos = 0;

	// open file
	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
			fatfile = fopen (filepath, "rb");
			if(!fatfile)
				return 0;
			break;
		case METHOD_SMB:
			smbfile = OpenSMBFile(filepath);
			if(!smbfile)
				return 0;
			break;
	}

	// set szMethod to current chosen load method
	szMethod = method;

	// set handler functions for reading data from FAT/SMB/DVD
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

			// store the current 7z data
			unsigned int oldLength = filelist[selection].length;
			u64 oldOffset = filelist[selection].offset;

			// erase all previous entries
			memset(&filelist, 0, sizeof(FILEENTRIES) * MAXFILES);

			// add '..' folder
			strncpy(filelist[0].displayname, "..", 2);
			filelist[0].flags = 1;
			filelist[0].length = oldLength;
			filelist[0].offset = oldOffset; // in case the user wants exit 7z

			// get contents and parse them into file list structure
			unsigned int SzI, SzJ;
			SzJ = 1;
			for (SzI = 0; SzI < SzDb.Database.NumFiles; SzI++)
			{
				SzF = SzDb.Database.Files + SzI;

				// skip directories
				if (SzF->IsDirectory)
					continue;

				// do not exceed MAXFILES to avoid possible buffer overflows
				if (SzJ == (MAXFILES - 1))
					break;

				// parse information about this file to the dvd file list structure
				strncpy(filelist[SzJ].filename, SzF->Name, MAXJOLIET); // copy joliet name (useless...)
				filelist[SzJ].filename[MAXJOLIET] = 0; // terminate string
				strncpy(filelist[SzJ].displayname, SzF->Name, MAXDISPLAY+1);	// crop name for display
				filelist[SzJ].length = SzF->Size; // filesize
				filelist[SzJ].offset = SzI; // the extraction function identifies the file with this number
				filelist[SzJ].flags = 0; // only files will be displayed (-> no flags)
				SzJ++;
			}

			// update maxfiles and select the first entry
			offset = selection = 0;
			nbfiles = SzJ;
		}
		else
		{
			SzArDbExFree(&SzDb, SzAllocImp.Free);
		}
	}

	// close file
	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
			fclose(fatfile);
			break;
		case METHOD_SMB:
			SMB_CloseFile (smbfile);
			break;
	}
	return nbfiles;
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
* SzExtractFile
*
* Extracts the given file # into the buffer specified
* Must parse the 7z BEFORE running this function
***************************************************************************/

int SzExtractFile(int i, unsigned char *buffer)
{
   // prepare some variables
   SzBlockIndex = 0xFFFFFFFF;
   SzOffset = 0;

   // Unzip the file
   ShowAction((char *)"Unzipping file. Please wait...");

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

   // close 7Zip archive and free memory
	SzClose();

   // check for errors
   if(SzRes != SZ_OK)
   {
   	// display error message
   	SzDisplayError(SzRes);
       return 0;
   }
   else
   {
   	return SzOutSizeProcessed;
   }
}
