/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * Tantric September 2008
 *
 * unzip.h
 *
 * File unzip routines
 ****************************************************************************/
#ifndef _UNZIP_
#define _UNZIP_

#include <smb.h>

extern int IsZipFile (char *buffer);

int UnZipFile (unsigned char *outbuffer, FILE* infile); // Reading from FAT
int UnZipFile (unsigned char *outbuffer, u64 inoffset); // Reading from DVD
int UnZipFile (unsigned char *outbuffer, SMBFILE infile); // Reading from SMB

/*
 * Zip file header definition
 */
typedef struct
{
  unsigned int zipid __attribute__ ((__packed__));	// 0x04034b50
  unsigned short zipversion __attribute__ ((__packed__));
  unsigned short zipflags __attribute__ ((__packed__));
  unsigned short compressionMethod __attribute__ ((__packed__));
  unsigned short lastmodtime __attribute__ ((__packed__));
  unsigned short lastmoddate __attribute__ ((__packed__));
  unsigned int crc32 __attribute__ ((__packed__));
  unsigned int compressedSize __attribute__ ((__packed__));
  unsigned int uncompressedSize __attribute__ ((__packed__));
  unsigned short filenameLength __attribute__ ((__packed__));
  unsigned short extraDataLength __attribute__ ((__packed__));
}
PKZIPHEADER;

u32 FLIP32 (u32 b);
u16 FLIP16 (u16 b);

#endif
