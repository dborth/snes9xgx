/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * snes9xgx.h
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#ifndef _SNES9XGX_H_
#define _SNES9XGX_H_

#include "drivers/InputDriver.h"
#include "drivers/ogc/OgcDeviceTypes.h"
#include "snes9x.h"
#include "filelist.h"

#define APPNAME 			"Snes9x GX"
#define APPVERSION 			"5.0.2"
#define APPFOLDER 			"snes9xgx"
#define PREF_FILE_NAME		"settings.xml"

#define MAXPATHLEN 1024
#define NOTSILENT 0
#define SILENT 1

enum {
    SAVEFOLDER_SAVES = 0,
    SAVEFOLDER_CHEATS,
    SAVEFOLDER_LENGTH
};

enum {
    LOADFOLDER_ROMS = 0,
    LOADFOLDER_SCREENSHOTS,
    LOADFOLDER_COVERS,
    LOADFOLDER_ARTWORK,
    LOADFOLDER_LENGTH
};

typedef struct {
    int id;
    const char *name;
} FolderDef;

const FolderDef saveFolder[] = {
    { SAVEFOLDER_SAVES,  "saves" },
    { SAVEFOLDER_CHEATS, "cheats" }
};

const FolderDef loadFolder[] = {
    { LOADFOLDER_ROMS,        "roms" },
    { LOADFOLDER_SCREENSHOTS, "screenshots" },
    { LOADFOLDER_COVERS,      "covers" },
    { LOADFOLDER_ARTWORK,     "artwork" }
};

enum {
	FILE_SRAM,
	FILE_STATE,
	FILE_ROM,
	FILE_CHEAT
};

enum {
	AUTOLOAD_OFF = 0,
	AUTOLOAD_SRAM,
	AUTOLOAD_STATE
};

enum {
	AUTOSAVE_OFF = 0,
	AUTOSAVE_SRAM,
	AUTOSAVE_STATE,
	AUTOSAVE_BOTH
};

enum {
	PREVIEWIMAGE_SCREENSHOT = 0,
	PREVIEWIMAGE_COVER,
	PREVIEWIMAGE_ARTWORK,
	PREVIEWIMAGE_LENGTH
};

enum {
	VIDEO_ASPECT_RATIO_CORRECTION_NONE = 0,
	VIDEO_ASPECT_RATIO_CORRECTION_16_9,
	VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED,
	VIDEO_ASPECT_RATIO_CORRECTION_LENGTH
};

enum {
	VIDEO_HW_SOFTEN_OFF = 0,
	VIDEO_HW_SOFTEN_AUTO,
	VIDEO_HW_SOFTEN_SHARP,
	VIDEO_HW_SOFTEN_SOFT,
	VIDEO_HW_SOFTEN_LENGTH
};

enum {
	VIDEOMODE_AUTO = 0,
	VIDEOMODE_NTSC,
	VIDEOMODE_PROGRESSIVE,
	VIDEOMODE_PAL,
	VIDEOMODE_PAL60,
	VIDEOMODE_PROGRESSIVE_576P,
	VIDEOMODE_ORIGINAL_240P,
	VIDEOMODE_LENGTH
};

enum {
	SFXOVERCLOCK_OFF = 0,
	SFXOVERCLOCK_20MHZ,
	SFXOVERCLOCK_40MHZ,
	SFXOVERCLOCK_60MHZ,
	SFXOVERCLOCK_80MHZ,
	SFXOVERCLOCK_100MHZ,
	SFXOVERCLOCK_120MHZ,
	SFXOVERCLOCK_LENGTH
};

enum {
	GAMEPAD_MENU_TOGGLE_DEFAULT = 0,
	GAMEPAD_MENU_TOGGLE_HOME_RIGHTSTICK,
	GAMEPAD_MENU_TOGGLE_LRSTART_12PLUS,
	GAMEPAD_MENU_TOGGLE_LENGTH
};

enum
{
	CTRL_PAD,
	CTRL_SCOPE,
	CTRL_JUST,
	CTRL_MOUSE,
	CTRL_MOUSE_PORT2,
	CTRL_MOUSE_BOTH_PORTS,
	CTRL_PAD2,
	CTRL_PAD4,
	CTRL_LENGTH
};

const char ctrlName[8][24] =
{ 
	"SNES Controller", 
	"Super Scope", 
	"Justifier", 
	"SNES Mouse (Port 1)", 
	"SNES Mouse (Port 2)", 
	"SNES Mouse (Both Ports)",
	"SNES Controllers (2)", 
	"SNES Controllers (4)"
};

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
	LANG_SWEDISH,
	LANG_LENGTH
};

struct SGCSettings{
	int		AutoLoad;
	int		AutoSave;
	int		LoadMethod;
	int		SaveMethod;
	bool	AppendAuto;
	char	LoadFolder[MAXPATHLEN];
	char	LastFileLoaded[MAXPATHLEN];
	char	SaveFolder[MAXPATHLEN];
	char	CheatFolder[MAXPATHLEN];
	char	ScreenshotsFolder[MAXPATHLEN];
	char	CoverFolder[MAXPATHLEN];
	char	ArtworkFolder[MAXPATHLEN];
	bool	HideSRAMSaving;
	bool	AutoloadGame;

	char	smbip[80];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbshare[20];

	int		videoMode;
	int		videoAspectRatioCorrection;
	bool	videoBilinearFilter;
	int		videoHardwareSoften;
	bool	videoScanlines;
	int		videoUpscalingFilter;
	float	videoZoomHor;
	float	videoZoomVert;
	int		videoXshift;
	int		videoYshift;

	bool	HiResolution;
	bool	SpriteLimit;
	bool	FrameSkip;
	bool	crosshair;
	int		sfxOverclock;
	int		Interpolation;
	bool	MuteAudio;
	
	int		Controller;
	int		wiimoteOrientation;
	int		ExitAction;
	int		MusicVolume;
	int		SFXVolume;
	bool	Rumble;
	int		language;
	int		PreviewImage;


	bool	TurboModeEnabled;
	int		TurboModeButton;
	int		GamepadMenuToggle;
	bool	MapABXYRightStick;
};

void ExitApp();
extern struct SGCSettings GCSettings;
extern bool MenuRequested;
extern char appPath[];
#endif
