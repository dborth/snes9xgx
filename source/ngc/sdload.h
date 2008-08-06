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
#include <sdcard.h>

int updateSDdirname();
int parseSDdirectory();
int LoadSDFile (char *filename, int length);
void SaveSRAMToSD (uint8 slot, bool8 silent);
void LoadSRAMFromSD (uint8 slot, bool8 silent);
void SavePrefsToSD (uint8 slot, bool8 silent);
void LoadPrefsFromSD (uint8 slot, bool8 silent);

extern char rootSDdir[SDCARD_MAX_PATH_LEN];

#endif
