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

#include <stdio.h>
#include <stddef.h>

int IsZipFile (char *buffer);
char * GetFirstZipFilename();
size_t UnZipBuffer (FILE * fp, unsigned char *outbuffer, size_t buffersize);
int SzParse(char * filepath);
size_t SzExtractFile(FILE * fp, int i, unsigned char *buffer);
void SzClose();

#endif
