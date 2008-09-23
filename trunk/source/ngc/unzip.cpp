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

#include "dvd.h"
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
 * unzip
 *
 * It should be noted that there is a limit of 5MB total size for any ROM
 ******************************************************************************/
FILE* fatfile; // FAT
u64 discoffset; // DVD
SMBFILE smbfile; // SMB

int
UnZipBuffer (unsigned char *outbuffer, short where)
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

	/*** Read Zip Header ***/
	switch (where)
	{
		case 0:	// SD Card
		fseek(fatfile, 0, SEEK_SET);
		fread (readbuffer, 1, ZIPCHUNK, fatfile);
		break;

		case 1: // DVD
		dvd_read (readbuffer, ZIPCHUNK, discoffset);
		break;

		case 2: // From SMB
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

		/*** Readup the next 2k block ***/
		zipoffset = 0;
		zipchunk = ZIPCHUNK;

		switch (where)
		{
			case 0:	// SD Card
			fread (readbuffer, 1, ZIPCHUNK, fatfile);
			break;

			case 1:	// DVD
			readoffset += ZIPCHUNK;
			dvd_read (readbuffer, ZIPCHUNK, discoffset+readoffset);
			break;

			case 2: // From SMB
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
// Reading from FAT
int
UnZipFile (unsigned char *outbuffer, FILE* infile)
{
	fatfile = infile;
	return UnZipBuffer(outbuffer, 0);
}
// Reading from DVD
int
UnZipFile (unsigned char *outbuffer, u64 inoffset)
{
	discoffset = inoffset;
	return UnZipBuffer(outbuffer, 1);
}
// Reading from SMB
int
UnZipFile (unsigned char *outbuffer, SMBFILE infile)
{
	smbfile = infile;
	return UnZipBuffer(outbuffer, 2);
}
