/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric September 2008
 *
 * snes9xGX.h
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#ifndef _SNES9XGX_H_
#define _SNES9XGX_H_

#include "FreeTypeGX.h"
#include "snes9x.h"
#include "filter.h"
#include "filelist.h"

#define APPNAME 		"Snes9x GX"
#define APPVERSION 		"4.0.6"
#define PREF_FILE_NAME 	"settings.xml"

#define NOTSILENT 0
#define SILENT 1

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB,
	METHOD_SD_SLOTA,
	METHOD_SD_SLOTB
};

enum {
	FILE_SRAM,
	FILE_SNAPSHOT,
	FILE_ROM,
	FILE_CHEAT,
	FILE_PREF,
	FILE_SCREEN
};

enum
{
	CTRL_PAD,
	CTRL_MOUSE,
	CTRL_SCOPE,
	CTRL_JUST,
	CTRL_PAD2,
	CTRL_PAD4,
	CTRL_LENGTH
};

const char ctrlName[6][24] =
{ "SNES Controller", "SNES Mouse", "Superscope", "Justifier", "SNES Controllers (2)", "SNES Controllers (4)" };

struct SGCSettings{
    int		AutoLoad;
    int		AutoSave;
    int		LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, SMB
	char	LoadFolder[256]; // Path to game files
	char	SaveFolder[256]; // Path to save files
	char	CheatFolder[256]; // Path to cheat files

	char	smbip[16];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbshare[20];

    float	ZoomLevel; // zoom amount
    int		VerifySaves;
    int		videomode; // 0 - automatic, 1 - NTSC (480i), 2 - Progressive (480p), 3 - PAL (50Hz), 4 - PAL (60Hz)
	int		render;		// 0 - original, 1 - filtered, 2 - unfiltered
	int		FilterMethod; // convert to RenderFilter
	int		Controller;
	int		widescreen;	// 0 - 4:3 aspect, 1 - 16:9 aspect
	int		xshift;	// video output shift
	int		yshift;
	int		WiimoteOrientation;
	int		ExitAction;
	int		MusicVolume;
	int		SFXVolume;
	int		Rumble;
};

void ExitApp();
void ShutdownWii();
extern struct SGCSettings GCSettings;
extern int ScreenshotRequested;
extern int ConfigRequested;
extern int ShutdownRequested;
extern int ExitRequested;
extern char appPath[];
extern int appLoadMethod;
extern FreeTypeGX *fontSystem;

#endif
