/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2022
 *
 * snes9xgx.h
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#ifndef _SNES9XGX_H_
#define _SNES9XGX_H_

#include "utils/FreeTypeGX.h"
#include "snes9x.h"
#include "filter.h"
#include "filelist.h"

#define APPNAME 			"Snes9x GX"
#define APPVERSION 			"4.5.2"
#define APPFOLDER 			"snes9xgx"
#define PREF_FILE_NAME		"settings.xml"

#define MAXPATHLEN 1024
#define NOTSILENT 0
#define SILENT 1

const char pathPrefix[9][8] =
{ "", "sd:/", "usb:/", "dvd:/", "smb:/", "carda:/", "cardb:/", "port2:/" };

enum {
	DEVICE_AUTO,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_SD_SLOTA,
	DEVICE_SD_SLOTB,
	DEVICE_SD_PORT2
};

enum {
	FILE_SRAM,
	FILE_SNAPSHOT,
	FILE_ROM,
	FILE_CHEAT
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
{ "SNES Controller", "SNES Mouse", "Super Scope", "Justifier", "SNES Controllers (2)", "SNES Controllers (4)" };

enum {
	TURBO_BUTTON_RSTICK = 0,
	TURBO_BUTTON_A,
	TURBO_BUTTON_B,
	TURBO_BUTTON_X,
	TURBO_BUTTON_Y,
	TURBO_BUTTON_L,
	TURBO_BUTTON_R,
	TURBO_BUTTON_ZL,
	TURBO_BUTTON_ZR,
	TURBO_BUTTON_Z,
	TURBO_BUTTON_C,
	TURBO_BUTTON_1,
	TURBO_BUTTON_2,
	TURBO_BUTTON_PLUS,
	TURBO_BUTTON_MINUS,
};

enum {
	LANG_JAPANESE = 0,
	LANG_ENGLISH,
	LANG_GERMAN,
	LANG_FRENCH,
	LANG_SPANISH,
	LANG_ITALIAN,
	LANG_DUTCH,
	LANG_SIMP_CHINESE,
	LANG_TRAD_CHINESE,
	LANG_KOREAN,
	LANG_PORTUGUESE,
	LANG_BRAZILIAN_PORTUGUESE,
	LANG_CATALAN,
	LANG_TURKISH,
	LANG_LENGTH
};

struct SGCSettings{
	int		AutoLoad;
	int		AutoSave;
	int		LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, USB, SMB
	int		AppendAuto; // 0 - no, 1 - yes
	char	LoadFolder[MAXPATHLEN]; 	// Path to game files
	char	LastFileLoaded[MAXPATHLEN]; // Last file loaded filename
	char	SaveFolder[MAXPATHLEN]; 	// Path to save files
	char	CheatFolder[MAXPATHLEN]; 	// Path to cheat files
	char	ScreenshotsFolder[MAXPATHLEN]; // Path to screenshots files
	char	CoverFolder[MAXPATHLEN]; 	// Path to cover files
	char	ArtworkFolder[MAXPATHLEN]; 	// Path to artwork files
	int		HideSRAMSaving;
	int		AutoloadGame;

	char	smbip[80];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbshare[20];

	float	zoomHor; // horizontal zoom amount
	float	zoomVert; // vertical zoom amount
	int		videomode; // 0 - automatic, 1 - NTSC (480i), 2 - Progressive (480p), 3 - PAL (50Hz), 4 - PAL (60Hz)
	int		render;		// 0 - original, 1 - filtered, 2 - unfiltered
	int		FilterMethod; // convert to RenderFilter
	int		Controller;
	int		crosshair;
	int		widescreen;	// 0 - 4:3 aspect, 1 - 16:9 aspect
	int		xshift;	// video output shift
	int		yshift;
	int		WiimoteOrientation;
	int		ExitAction;
	int		MusicVolume;
	int		SFXVolume;
	int		Rumble;
	int		language;
	int		PreviewImage;

	int		sfxOverclock;
	
	int		Interpolation;

	int		TurboModeEnabled; // 0 - disabled, 1 - enabled
	int		TurboModeButton;
};

void ExitApp();
void ShutdownWii();
bool SupportedIOS(u32 ios);
bool SaneIOS(u32 ios);
extern struct SGCSettings GCSettings;
extern int ScreenshotRequested;
extern int ConfigRequested;
extern int ShutdownRequested;
extern int ExitRequested;
extern char appPath[];
extern FreeTypeGX *fontSystem[];
extern bool isWiiVC;
static inline bool IsWiiU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) || isWiiVC);
}
static inline bool IsWiiUFastCPU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) && ((*(vu32*)0xCD8005B0 & 0x20) == 0));
}
#endif
