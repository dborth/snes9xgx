/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * decompress.h
 *
 * File decompression routines
 ****************************************************************************/
#ifndef _DECOMPRESS_H_
#define _DECOMPRESS_H_

int IsZipFile (char *buffer);
char * GetFirstZipFilename();
size_t UnZipBuffer (unsigned char *outbuffer, size_t buffersize);
int SzParse(char * filepath);
size_t SzExtractFile(int i, unsigned char *buffer);
void SzClose();

#endif
