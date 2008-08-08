/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * fileop.h
 *
 * File operations
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

bool fat_remount(PARTITION_INTERFACE partition);
bool fat_is_mounted(PARTITION_INTERFACE partition);
bool changeFATInterface(int method);
static int FileSortCallback(const void *f1, const void *f2);
int updateFATdirname(int method);
int parseFATdirectory(int method);
int LoadFATFile (char *filename, int length);
int SaveBufferToFAT (char *filepath, int datasize, bool silent);
int LoadBufferFromFAT (char *filepath, bool silent);

extern char currFATdir[MAXPATHLEN];

#endif
