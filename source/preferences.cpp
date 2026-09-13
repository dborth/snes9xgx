/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.cpp
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <mxml.h>
#if defined(HW_RVL) || defined(HW_DOL)
#include <ogc/conf.h>
#include <ogc/system.h>
#endif

#include "snes9xgx.h"
#include "menu.h"
#include "fileop.h"
#include "video.h"
#include "filebrowser.h"
#include "input.h"
#include "button_mapping.h"
#include "libgui/Gui.h"
#include "snes9x/apu/apu.h"

#if defined(HW_RVL) || defined(HW_DOL)
#include "drivers/ogc/WiiPlatform.h"
#include "drivers/ogc/GameCubePlatform.h"
#include "drivers/ogc/videofilters.h"
#endif

struct SSettings Settings;

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ***************************************************************************/
static mxml_node_t *xml = nullptr;
static mxml_node_t *data = nullptr;
static mxml_node_t *section = nullptr;
static mxml_node_t *item = nullptr;
static mxml_node_t *elem = nullptr;

static char temp[200];

static const char* BtoStr(bool b)
{
    return b ? "1" : "0";
}
static const char * toStr(int i)
{
	sprintf(temp, "%d", i);
	return temp;
}

static const char * FtoStr(float i)
{
	sprintf(temp, "%.2f", i);
	return temp;
}

static void createXMLSection(const char * name, const char * description)
{
	section = mxmlNewElement(data, "section");
	mxmlElementSetAttr(section, "name", name);
	mxmlElementSetAttr(section, "description", description);
}

static void createXMLSetting(const char * name, const char * description, const char * value)
{
	item = mxmlNewElement(section, "setting");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "value", value);
	mxmlElementSetAttr(item, "description", description);
}

static void createXMLController(uint32_t controller[], const char * name, const char * description)
{
	item = mxmlNewElement(section, "controller");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "description", description);

	// create buttons
	for(int i=0; i < MAXJP; i++)
	{
		elem = mxmlNewElement(item, "button");
		mxmlElementSetAttr(elem, "number", toStr(i));
		mxmlElementSetAttr(elem, "assignment", toStr(controller[i]));
	}
}

static const char * XMLSaveCallback(mxml_node_t *node, int where)
{
	const char *name;

	name = mxmlGetElement(node);

	if(where == MXML_WS_BEFORE_CLOSE)
	{
		if(!strcmp(name, "file") || !strcmp(name, "section"))
			return ("\n");
		else if(!strcmp(name, "controller"))
			return ("\n\t");
	}
	if (where == MXML_WS_BEFORE_OPEN)
	{
		if(!strcmp(name, "file"))
			return ("\n");
		else if(!strcmp(name, "section"))
			return ("\n\n");
		else if(!strcmp(name, "setting") || !strcmp(name, "controller"))
			return ("\n\t");
		else if(!strcmp(name, "button"))
			return ("\n\t\t");
	}
	return (nullptr);
}

static int
preparePrefsData ()
{
	xml = mxmlNewXML("1.0");
	mxmlSetWrapMargin(0); // disable line wrapping

	data = mxmlNewElement(xml, "file");
	mxmlElementSetAttr(data, "app", APPNAME);
	mxmlElementSetAttr(data, "version", APPVERSION);

	createXMLSection("File", "File Settings");

	createXMLSetting("AutoLoad", "Auto Load", toStr(Settings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(Settings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(Settings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(Settings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", Settings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", Settings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", Settings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", BtoStr(Settings.AppendAuto));
	createXMLSetting("CheatFolder", "Cheats Folder", Settings.CheatFolder);
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", Settings.ScreenshotsFolder);
	createXMLSetting("CoverFolder", "Covers Folder", Settings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", Settings.ArtworkFolder);
	
	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", Settings.smbip);
	createXMLSetting("smbshare", "Share Name", Settings.smbshare);
	createXMLSetting("smbuser", "Share Username", Settings.smbuser);
	createXMLSetting("smbpwd", "Share Password", Settings.smbpwd);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videoMode", "Output Mode", toStr(Settings.videoMode));
	createXMLSetting("videoAspectRatioCorrection", "Aspect Ratio Correction", toStr(Settings.videoAspectRatioCorrection));
	createXMLSetting("videoBilinearFilter", "Bilinear Filtering", BtoStr(Settings.videoBilinearFilter));
	createXMLSetting("videoHardwareSoften", "Hardware Soften", toStr(Settings.videoHardwareSoften));
	createXMLSetting("videoScanlines", "Scanlines", BtoStr(Settings.videoScanlines));
	createXMLSetting("videoUpscalingFilter", "Upscaling Filter Method", toStr(Settings.videoUpscalingFilter));
	createXMLSetting("videoZoomHor", "Horizontal Zoom Level", FtoStr(Settings.videoZoomHor));
	createXMLSetting("videoZoomVert", "Vertical Zoom Level", FtoStr(Settings.videoZoomVert));
	createXMLSetting("videoXshift", "Horizontal Video Shift", toStr(Settings.videoXshift));
	createXMLSetting("videoYshift", "Vertical Video Shift", toStr(Settings.videoYshift));

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("crosshair", "Crosshair", BtoStr(Settings.crosshair));
	createXMLSetting("HiResolution", "SNES Hi-Res Mode", BtoStr(Settings.HiResolution));
	createXMLSetting("SpriteLimit", "Sprites per-line Limit", BtoStr(Settings.SpriteLimit));
	createXMLSetting("FrameSkip", "Frame Skipping", BtoStr(Settings.FrameSkip));
	createXMLSetting("sfxOverclock", "SuperFX Overclock", toStr(Settings.sfxOverclock));
	createXMLSetting("Interpolation", "Interpolation", toStr(Settings.Interpolation));
	createXMLSetting("MuteAudio", "Mute", BtoStr(Settings.MuteAudio));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(Settings.wiimoteOrientation));
#endif
	createXMLSetting("ExitAction", "Exit Action", toStr(Settings.ExitAction));
	createXMLSetting("MusicVolume", "Music Volume", toStr(Settings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(Settings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", BtoStr(Settings.Rumble));
	createXMLSetting("language", "Language", toStr(Settings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(Settings.PreviewImage));
	createXMLSetting("HideSRAMSaving", "Hide SRAM Saving", BtoStr(Settings.HideSRAMSaving));
	
	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("Controller", "Controller", toStr(Settings.Controller));
	createXMLSetting("TurboModeEnabled", "Turbo Mode Enabled", BtoStr(Settings.TurboModeEnabled));
	createXMLSetting("TurboModeButton", "Turbo Mode Button", toStr(Settings.TurboModeButton));
	createXMLSetting("GamepadMenuToggle", "Gamepad Menu Toggle", toStr(Settings.GamepadMenuToggle));
	createXMLSetting("MapABXYRightStick", "Map ABXY Right Stick", BtoStr(Settings.MapABXYRightStick));

	createXMLController(btnmap[CTRL_PAD][INPUT_HW_GAMECUBE], "btnmapping_pad_gcpad", "SNES Pad - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_PAD][INPUT_HW_WIIMOTE], "btnmapping_pad_wiimote", "SNES Pad - Wiimote");
	createXMLController(btnmap[CTRL_PAD][INPUT_HW_CLASSIC], "btnmapping_pad_classic", "SNES Pad - Classic Controller");
	createXMLController(btnmap[CTRL_PAD][INPUT_HW_WUPC], "btnmapping_pad_wupc", "SNES Pad - Wii U Pro Controller");
	createXMLController(btnmap[CTRL_PAD][INPUT_HW_DRC], "btnmapping_pad_wiidrc", "SNES Pad - Wii U Gamepad");
	createXMLController(btnmap[CTRL_PAD][INPUT_HW_NUNCHUK], "btnmapping_pad_nunchuk", "SNES Pad - Nunchuk + Wiimote");
#endif
	createXMLController(btnmap[CTRL_SCOPE][INPUT_HW_GAMECUBE], "btnmapping_scope_gcpad", "Superscope - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_SCOPE][INPUT_HW_WIIMOTE], "btnmapping_scope_wiimote", "Superscope - Wiimote");
#endif
	createXMLController(btnmap[CTRL_MOUSE][INPUT_HW_GAMECUBE], "btnmapping_mouse_gcpad", "Mouse - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_MOUSE][INPUT_HW_WIIMOTE], "btnmapping_mouse_wiimote", "Mouse - Wiimote");
#endif
	createXMLController(btnmap[CTRL_JUST][INPUT_HW_GAMECUBE], "btnmapping_just_gcpad", "Justifier - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_JUST][INPUT_HW_WIIMOTE], "btnmapping_just_wiimote", "Justifier - Wiimote");
#endif
	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSaveCallback);

	mxmlDelete(xml);

	return datasize;
}

/****************************************************************************
 * loadXMLSetting
 *
 * Load XML elements into variables for an individual variable
 ***************************************************************************/

static void loadXMLSetting(char * var, const char * name, int maxsize)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			snprintf(var, maxsize, "%s", tmp);
	}
}
static void loadXMLSetting(bool * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp) {
			if (strcmp(tmp, "1") == 0 || strcasecmp(tmp, "true") == 0)
				*var = true;
			else
				*var = false;
		}
	}
}
static void loadXMLSetting(int * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			*var = atoi(tmp);
	}
}
static void loadXMLSetting(float * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			*var = atof(tmp);
	}
}

/****************************************************************************
 * loadXMLController
 *
 * Load XML elements into variables for a controller mapping
 ***************************************************************************/

static void loadXMLController(uint32_t controller[], const char * name)
{
	item = mxmlFindElement(xml, xml, "controller", "name", name, MXML_DESCEND);

	if(item)
	{
		// populate buttons
		for(int i=0; i < MAXJP; i++)
		{
			elem = mxmlFindElement(item, xml, "button", "number", toStr(i), MXML_DESCEND);
			if(elem)
			{
				const char * tmp = mxmlElementGetAttr(elem, "assignment");
				if(tmp)
					controller[i] = atoi(tmp);
			}
		}
	}
}

void ApplySettings() {
	platform->getInput()->setWiimoteOrientation(Settings.wiimoteOrientation);
	platform->getInput()->setRumbleEnabled(Settings.Rumble);
	GuiSound::setDefaultVolume(SOUND::OGG, Settings.MusicVolume);
	GuiSound::setDefaultVolume(SOUND::PCM, Settings.SFXVolume);
	platform->getVideo()->startMenuVideo();
	ChangeLanguage();
}

/****************************************************************************
 * decodePrefsData
 *
 * Decodes preferences - parses XML and loads preferences into the variables
 ***************************************************************************/

static bool
decodePrefsData ()
{
	xml = mxmlLoadString(nullptr, (char *)savebuffer, MXML_TEXT_CALLBACK);

	if(!xml) {
		return false;
	}

	// File Settings

	loadXMLSetting(&Settings.AutoLoad, "AutoLoad");
	loadXMLSetting(&Settings.AutoSave, "AutoSave");
	loadXMLSetting(&Settings.LoadMethod, "LoadMethod");
	loadXMLSetting(&Settings.SaveMethod, "SaveMethod");
	loadXMLSetting(Settings.LoadFolder, "LoadFolder", sizeof(Settings.LoadFolder));
	loadXMLSetting(Settings.LastFileLoaded, "LastFileLoaded", sizeof(Settings.LastFileLoaded));
	loadXMLSetting(Settings.SaveFolder, "SaveFolder", sizeof(Settings.SaveFolder));
	loadXMLSetting(&Settings.AppendAuto, "AppendAuto");
	loadXMLSetting(Settings.CheatFolder, "CheatFolder", sizeof(Settings.CheatFolder));
	loadXMLSetting(Settings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(Settings.ScreenshotsFolder));
	loadXMLSetting(Settings.CoverFolder, "CoverFolder", sizeof(Settings.CoverFolder));
	loadXMLSetting(Settings.ArtworkFolder, "ArtworkFolder", sizeof(Settings.ArtworkFolder));

	// Network Settings

	loadXMLSetting(Settings.smbip, "smbip", sizeof(Settings.smbip));
	loadXMLSetting(Settings.smbshare, "smbshare", sizeof(Settings.smbshare));
	loadXMLSetting(Settings.smbuser, "smbuser", sizeof(Settings.smbuser));
	loadXMLSetting(Settings.smbpwd, "smbpwd", sizeof(Settings.smbpwd));

	// Video Settings

	loadXMLSetting(&Settings.videoMode, "videoMode");
	loadXMLSetting(&Settings.videoAspectRatioCorrection, "videoAspectRatioCorrection");
	loadXMLSetting(&Settings.videoBilinearFilter, "videoBilinearFilter");
	loadXMLSetting(&Settings.videoHardwareSoften, "videoHardwareSoften");
	loadXMLSetting(&Settings.videoUpscalingFilter, "videoUpscalingFilter");
	loadXMLSetting(&Settings.videoScanlines, "videoScanlines");
	loadXMLSetting(&Settings.videoZoomHor, "videoZoomHor");
	loadXMLSetting(&Settings.videoZoomVert, "videoZoomVert");
	loadXMLSetting(&Settings.videoXshift, "videoXshift");
	loadXMLSetting(&Settings.videoYshift, "videoYshift");

	// Emulation Settings
	loadXMLSetting(&Settings.sfxOverclock, "sfxOverclock");
	loadXMLSetting(&Settings.crosshair, "crosshair");
	loadXMLSetting(&Settings.HiResolution, "HiResolution");
	loadXMLSetting(&Settings.SpriteLimit, "SpriteLimit");
	loadXMLSetting(&Settings.FrameSkip, "FrameSkip");
	loadXMLSetting(&Settings.Interpolation, "Interpolation");
	loadXMLSetting(&Settings.MuteAudio, "MuteAudio");

	// Menu Settings

	loadXMLSetting(&Settings.wiimoteOrientation, "WiimoteOrientation");
	loadXMLSetting(&Settings.ExitAction, "ExitAction");
	loadXMLSetting(&Settings.MusicVolume, "MusicVolume");
	loadXMLSetting(&Settings.SFXVolume, "SFXVolume");
	loadXMLSetting(&Settings.Rumble, "Rumble");
	loadXMLSetting(&Settings.language, "language");
	loadXMLSetting(&Settings.PreviewImage, "PreviewImage");
	loadXMLSetting(&Settings.HideSRAMSaving, "HideSRAMSaving");

	// Controller Settings

	loadXMLSetting(&Settings.Controller, "Controller");
	loadXMLSetting(&Settings.TurboModeEnabled, "TurboModeEnabled");
	loadXMLSetting(&Settings.TurboModeButton, "TurboModeButton");
	loadXMLSetting(&Settings.GamepadMenuToggle, "GamepadMenuToggle");
	loadXMLSetting(&Settings.MapABXYRightStick, "MapABXYRightStick");

	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_GAMECUBE], "btnmapping_pad_gcpad");
	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_WIIMOTE], "btnmapping_pad_wiimote");
	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_CLASSIC], "btnmapping_pad_classic");
	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_WUPC], "btnmapping_pad_wupc");
	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_DRC], "btnmapping_pad_wiidrc");
	loadXMLController(btnmap[CTRL_PAD][INPUT_HW_NUNCHUK], "btnmapping_pad_nunchuk");
	loadXMLController(btnmap[CTRL_SCOPE][INPUT_HW_GAMECUBE], "btnmapping_scope_gcpad");
	loadXMLController(btnmap[CTRL_SCOPE][INPUT_HW_WIIMOTE], "btnmapping_scope_wiimote");
	loadXMLController(btnmap[CTRL_MOUSE][INPUT_HW_GAMECUBE], "btnmapping_mouse_gcpad");
	loadXMLController(btnmap[CTRL_MOUSE][INPUT_HW_WIIMOTE], "btnmapping_mouse_wiimote");
	loadXMLController(btnmap[CTRL_JUST][INPUT_HW_GAMECUBE], "btnmapping_just_gcpad");
	loadXMLController(btnmap[CTRL_JUST][INPUT_HW_WIIMOTE], "btnmapping_just_wiimote");

	mxmlDelete(xml);
	return true;
}

/****************************************************************************
 * FixInvalidSettings
 *
 * Attempts to correct at least some invalid settings - the ones that
 * might cause crashes
 ***************************************************************************/
void FixInvalidSettings()
{
	if(!isValidLoadDevice(Settings.LoadMethod))
		Settings.LoadMethod = DEVICE_AUTO;
	if(!isValidSaveDevice(Settings.SaveMethod))
		Settings.SaveMethod = DEVICE_AUTO;

	if(strlen(Settings.smbshare) == 0 || strlen(Settings.smbip) == 0) {
		if(Settings.LoadMethod == DEVICE_SMB) {
			Settings.LoadMethod = DEVICE_AUTO;
		}
		if(Settings.SaveMethod == DEVICE_SMB) {
			Settings.SaveMethod = DEVICE_AUTO;
		}
	}

	if(!(Settings.videoZoomHor > 0.5 && Settings.videoZoomHor < 1.5))
		Settings.videoZoomHor = 1.0;
	if(!(Settings.videoZoomVert > 0.5 && Settings.videoZoomVert < 1.5))
		Settings.videoZoomVert = 1.0;
	if(!(Settings.videoXshift > -50 && Settings.videoXshift < 50))
		Settings.videoXshift = 0;
	if(!(Settings.videoYshift > -50 && Settings.videoYshift < 50))
		Settings.videoYshift = 0;
	if(!(Settings.MusicVolume >= 0 && Settings.MusicVolume <= 100))
		Settings.MusicVolume = 20;
	if(!(Settings.SFXVolume >= 0 && Settings.SFXVolume <= 100))
		Settings.SFXVolume = 40;
	if(Settings.language < 0 || Settings.language >= LANG_LENGTH)
		Settings.language = LANG_ENGLISH;
	if(Settings.Controller > CTRL_PAD4 || Settings.Controller < CTRL_SCOPE)
		Settings.Controller = CTRL_PAD2;
	if(!(Settings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && Settings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		Settings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
	if(!(Settings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_NONE && Settings.videoAspectRatioCorrection < VIDEO_ASPECT_RATIO_CORRECTION_LENGTH))
		Settings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
	if(!(Settings.videoMode >= VIDEOMODE_AUTO && Settings.videoMode < VIDEOMODE_LENGTH))
		Settings.videoMode = VIDEOMODE_AUTO;
#if defined(HW_RVL) || defined(HW_DOL)
	if(!(Settings.videoUpscalingFilter >= FILTER_NONE && Settings.videoUpscalingFilter <= NUM_FILTERS))
		Settings.videoUpscalingFilter = FILTER_NONE;
#endif
	if(!(Settings.wiimoteOrientation >= WIIMOTE_ORIENTATION_AUTO && Settings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		Settings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
}

/****************************************************************************
 * DefaultSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
void DefaultSettings()
{
	memset (&Settings, 0, sizeof (Settings));

	ResetControls(); // controller button mappings

	Settings.LoadMethod = DEVICE_AUTO;
	Settings.SaveMethod = DEVICE_AUTO;
	sprintf (Settings.LoadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (Settings.SaveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (Settings.CheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (Settings.ScreenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (Settings.CoverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (Settings.ArtworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files
	Settings.AutoLoad = true;
	Settings.AutoSave = true;

	Settings.Controller = CTRL_PAD2;

	Settings.videoMode = VIDEOMODE_AUTO;
	Settings.videoBilinearFilter = true;
	Settings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
	Settings.videoScanlines = false;
#if defined(HW_RVL) || defined(HW_DOL)
	Settings.videoUpscalingFilter = FILTER_NONE;
#else
	Settings.videoUpscalingFilter = 0;
#endif

#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		Settings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_16_9;
	else
		Settings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#elif HW_DOL
	Settings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#endif

	Settings.videoZoomHor = 1.0; // horizontal zoom level
	Settings.videoZoomVert = 1.0; // vertical zoom level
	Settings.videoXshift = 0; // horizontal video shift
	Settings.videoYshift = 0; // vertical video shift
	Settings.crosshair = true;

	Settings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
#ifdef HW_RVL
	Settings.ExitAction = EXITACTION_WII_AUTO;
#elif HW_DOL
	Settings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	Settings.AutoloadGame = false;
	Settings.MusicVolume = 20;
	Settings.SFXVolume = 40;
	Settings.Rumble = true;
	Settings.PreviewImage = PREVIEWIMAGE_COVER;
	Settings.HideSRAMSaving = false;
	
#ifdef HW_RVL
	Settings.language = CONF_GetLanguage();

	if(Settings.language == LANG_TRAD_CHINESE)
		Settings.language = LANG_SIMP_CHINESE;
#elif HW_DOL
	Settings.language = SYS_GetLanguage() + LANG_ENGLISH;
#endif

	/****************** SNES9x Settings ***********************/

	// Default ALL to false
	memset (&Settings, 0, sizeof (Settings));

	// General

	Settings.MouseMaster = false;
	Settings.SuperScopeMaster = false;
	Settings.JustifierMaster = false;
	Settings.MultiPlayer5Master = false;
	Settings.DontSaveOopsSnapshot = true;
	Settings.ApplyCheats = true;

	Settings.HDMATimingHack = 100;
	Settings.BlockInvalidVRAMAccessMaster = true;
	
	Settings.IsPatched = 0;

	// Sound
	Settings.SoundSync = true;
	Settings.SixteenBitSound = true;
	Settings.Stereo = true;
	Settings.ReverseStereo = true;
	Settings.SoundPlaybackRate = 48000;
	Settings.SoundInputRate = 31920;
	Settings.DynamicRateControl = true;
	Settings.SeparateEchoBuffer = false;
	Settings.MuteAudio = false;
	Settings.Interpolation = 0;
	Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN;

	// Graphics
	Settings.Transparency = true;
	Settings.MaxSpriteTilesPerLine = 34;
	Settings.SkipFrames = AUTO_FRAMERATE;
	Settings.TurboSkipFrames = 19;
	Settings.AutoDisplayMessages = false;
	Settings.InitialInfoStringTimeout = 200; // # frames to display messages for
	Settings.DisplayFrameRate = false;
	Settings.DisplayTime = false;
	Settings.HiResolution = true; // Enabled by default
	Settings.SpriteLimit = true; // Enabled by default
	Settings.FrameSkip = true; // Enabled by default

	// Frame timings in 50hz and 60hz cpu mode
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;

	Settings.sfxOverclock = 0;
	/* Initialize Super FX CPU to normal speed by default */
	Settings.SuperFXSpeedPerLine = 5823405;
	
	Settings.SuperFXClockMultiplier = 100;
	Settings.OverclockMode = 0;
	Settings.OneClockCycle = 6;
	Settings.OneSlowClockCycle = 8;
	Settings.TwoClockCycles = 12;

	Settings.TurboModeEnabled = true; // Enabled by default
	Settings.TurboModeButton = 0; // Default is Right Analog Stick (0)
	Settings.GamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
	Settings.MapABXYRightStick = false;
}

/****************************************************************************
 * Save Preferences
 ***************************************************************************/
static char prefpath[MAXPATHLEN] = { 0 };

bool SavePrefs()
{
	char filepath[MAXPATHLEN];
	int datasize;
	int offset = 0;
	int device = DEVICE_AUTO;
	
	if(prefpath[0] != 0)
	{
		snprintf(filepath, sizeof(filepath), "%s/%s", prefpath, PREF_FILE_NAME);
		FindDevice(filepath, &device);
	}
	else if(appPath[0] != 0)
	{
		snprintf(filepath, sizeof(filepath), "%s/%s", appPath, PREF_FILE_NAME);
		strcpy(prefpath, appPath);
		FindDevice(filepath, &device);
	}
	else
	{
		autoSaveMethod();
		device = Settings.SaveMethod;

		if(!ChangeInterface(device, true)) {
			return false;
		}
		
		platform->getFileSystem()->getPath(filepath, device, APPFOLDER);
		if(!CreateDirectory(filepath)) {
			return false;
		}

		platform->getFileSystem()->getPath(filepath, device, APPFOLDER, PREF_FILE_NAME);
		platform->getFileSystem()->getPath(prefpath, device, APPFOLDER);
	}
	
	if(device == DEVICE_AUTO)
		return false;

	FixInvalidSettings();

	AllocSaveBuffer ();
	datasize = preparePrefsData ();

	offset = SaveFile(filepath, datasize, true);

	FreeSaveBuffer ();

	CancelAction();

	if (offset > 0)
	{
		if(appPath[0] == 0)
			strcpy(appPath, prefpath);
		return true;
	}
	return false;
}

/****************************************************************************
 * Load Preferences from specified filepath
 ***************************************************************************/
bool
LoadPrefsFromMethod (char * path)
{
	bool retval = false;
	int offset = 0;
	char filepath[MAXPATHLEN];
	sprintf(filepath, "%s/%s", path, PREF_FILE_NAME);

	AllocSaveBuffer ();

	offset = LoadFile(filepath, SILENT);

	if (offset > 0)
		retval = decodePrefsData ();

	FreeSaveBuffer ();
	
	if(retval)
	{
		strcpy(prefpath, path);

		if(appPath[0] == 0)
			strcpy(appPath, prefpath);
	}

	return retval;
}

/****************************************************************************
 * Load Preferences
 * Checks sources consecutively until we find a preference file
 ***************************************************************************/
static bool prefLoadAttempted = false;

bool LoadPrefs()
{
	if(prefLoadAttempted) // already attempted loading
		return true;

	prefLoadAttempted = true;

	bool prefFound = false;
	char filepath[5][MAXPATHLEN];
	int numDevices;

#ifdef HW_RVL
	numDevices = 5;
	sprintf(filepath[0], "%s", appPath);
	sprintf(filepath[1], "sd:/apps/%s", APPFOLDER);
	sprintf(filepath[2], "usb:/apps/%s", APPFOLDER);
	sprintf(filepath[3], "sd:/%s", APPFOLDER);
	sprintf(filepath[4], "usb:/%s", APPFOLDER);
#elif HW_DOL
	numDevices = 4;
	sprintf(filepath[0], "carda:/%s", APPFOLDER);
	sprintf(filepath[1], "cardb:/%s", APPFOLDER);
	sprintf(filepath[2], "port2:/%s", APPFOLDER);
	sprintf(filepath[3], "gcloader:/%s", APPFOLDER);
#endif

	for(int i=0; i<numDevices; i++) {
		prefFound = LoadPrefsFromMethod(filepath[i]);

		if(prefFound)
			break;
	}

	if(!prefFound) {
		return false;
	}

	FixInvalidSettings();
	ApplySettings();

#ifdef HW_RVL
	bg_music = (uint8_t * )bg_music_ogg;
	bg_music_size = bg_music_ogg_size;
	LoadBgMusic();
#endif
	return true;
}

void CreatePathWithPrefix(int device, const char* folder) {
    char fullPath[MAXPATHLEN];
    MakeFilePathForFolderPath(fullPath, device, folder);
    CreateDirectory(fullPath);
}

void CreateMissingDirectories() {
    char defaultFolder[MAXPATHLEN];

    if (Settings.SaveMethod > DEVICE_AUTO && ChangeInterface(Settings.SaveMethod, NOTSILENT)) {
        const char* savePointers[] = { Settings.SaveFolder, Settings.CheatFolder };

        for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
            const char* currentPath = savePointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(Settings.SaveMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(Settings.SaveMethod, currentPath);
            }
        }
    }

    if (Settings.LoadMethod > DEVICE_AUTO && Settings.LoadMethod != DEVICE_DVD && ChangeInterface(Settings.LoadMethod, NOTSILENT)) {
        const char* loadPointers[] = {
            Settings.LoadFolder,
            Settings.ScreenshotsFolder,
            Settings.CoverFolder,
            Settings.ArtworkFolder
        };

        for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
            const char* currentPath = loadPointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(Settings.LoadMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(Settings.LoadMethod, currentPath);
            }
        }
    }
}
