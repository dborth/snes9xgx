/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Unzip - borrowed from the GPP
 *
 * softdev July 2006
 ****************************************************************************/
#ifndef _NGCUNZIP_
#define _NGCUNZIP_

extern int IsZipFile (char *buffer);
int UnZipBuffer (unsigned char *outbuffer, u64 discoffset, int length);
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
