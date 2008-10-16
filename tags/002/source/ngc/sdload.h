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

int updateSDdirname();
int parseSDdirectory();
int LoadSDFile (char *filename, int length);
void SaveSRAMToSD (int slot, bool silent);
void LoadSRAMFromSD (int slot, bool silent);
void SavePrefsToSD (int slot, bool silent);
void LoadPrefsFromSD (int slot, bool silent);

extern char rootSDdir[MAXPATHLEN];
extern char currSDdir[MAXPATHLEN];

#endif
