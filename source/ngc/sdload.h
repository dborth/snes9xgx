/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * sdload.cpp
 *
 * Load ROMS from SD Card
 ****************************************************************************/

#ifndef _LOADFROMSDC_
#define _LOADFROMSDC_
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>

#define ROOTSDDIR "fat3:/"
#define ROOTUSBDIR "fat4:/"

bool fat_remount(PARTITION_INTERFACE partition);
static int FileSortCallback(const void *f1, const void *f2);
int updateFATdirname(int method);
int parseFATdirectory(int method);
int LoadFATFile (char *filename, int length);
int SaveBufferToFAT (char *filepath, int datasize, bool silent);
int LoadBufferFromFAT (char *filepath, bool silent);

extern char currFATdir[MAXPATHLEN];

#endif
