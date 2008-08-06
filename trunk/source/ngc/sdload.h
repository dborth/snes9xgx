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

static int FileSortCallback(const void *f1, const void *f2);
int updateSDdirname();
int parseSDdirectory();
int LoadSDFile (char *filename, int length);
void SaveSRAMToSD (bool silent);
void LoadSRAMFromSD (bool silent);
void SavePrefsToSD (bool silent);
void LoadPrefsFromSD (bool silent);

extern char currSDdir[MAXPATHLEN];

#endif
