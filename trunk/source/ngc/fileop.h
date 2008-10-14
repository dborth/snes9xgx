/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
  * Michniewski 2008
 * Tantric August 2008
 *
 * fileop.h
 *
 * FAT File operations
 ****************************************************************************/

#ifndef _FATFILESC_
#define _FATFILESC_
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>

#define ROOTFATDIR "fat:/"

bool ChangeFATInterface(int method, bool silent);
int ParseFATdirectory(int method);
int LoadFATFile (char * fbuffer, int length);
int LoadFATSzFile(char * filepath, unsigned char * rbuffer);
int SaveBufferToFAT (char *filepath, int datasize, bool silent);
int LoadBufferFromFAT (char *filepath, bool silent);

extern char currFATdir[MAXPATHLEN];
extern FILE * fatfile;

#endif
