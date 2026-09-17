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
#include "drivers/ogc/wii/WiiPlatform.h"
#include "drivers/ogc/gamecube/GameCubePlatform.h"
#include "drivers/ogc/videofilters.h"
#endif

struct SEmuSettings EmuSettings;

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

	createXMLSetting("AutoLoad", "Auto Load", toStr(EmuSettings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(EmuSettings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(EmuSettings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(EmuSettings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", EmuSettings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", EmuSettings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", EmuSettings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", BtoStr(EmuSettings.AppendAuto));
	createXMLSetting("CheatFolder", "Cheats Folder", EmuSettings.CheatFolder);
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", EmuSettings.ScreenshotsFolder);
	createXMLSetting("CoverFolder", "Covers Folder", EmuSettings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", EmuSettings.ArtworkFolder);
	
	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", EmuSettings.smbShare.host);
	createXMLSetting("smbshare", "Share Name", EmuSettings.smbShare.share);
	createXMLSetting("smbuser", "Share Username", EmuSettings.smbShare.user);
	createXMLSetting("smbpwd", "Share Password", EmuSettings.smbShare.password);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videoMode", "Output Mode", toStr(EmuSettings.videoMode));
	createXMLSetting("videoAspectRatioCorrection", "Aspect Ratio Correction", toStr(EmuSettings.videoAspectRatioCorrection));
	createXMLSetting("videoBilinearFilter", "Bilinear Filtering", BtoStr(EmuSettings.videoBilinearFilter));
	createXMLSetting("videoHardwareSoften", "Hardware Soften", toStr(EmuSettings.videoHardwareSoften));
	createXMLSetting("videoScanlines", "Scanlines", BtoStr(EmuSettings.videoScanlines));
	createXMLSetting("videoUpscalingFilter", "Upscaling Filter Method", toStr(EmuSettings.videoUpscalingFilter));
	createXMLSetting("videoZoomHor", "Horizontal Zoom Level", FtoStr(EmuSettings.videoZoomHor));
	createXMLSetting("videoZoomVert", "Vertical Zoom Level", FtoStr(EmuSettings.videoZoomVert));
	createXMLSetting("videoXshift", "Horizontal Video Shift", toStr(EmuSettings.videoXshift));
	createXMLSetting("videoYshift", "Vertical Video Shift", toStr(EmuSettings.videoYshift));

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("crosshair", "Crosshair", BtoStr(EmuSettings.crosshair));
	createXMLSetting("HiResolution", "SNES Hi-Res Mode", BtoStr(EmuSettings.HiResolution));
	createXMLSetting("SpriteLimit", "Sprites per-line Limit", BtoStr(EmuSettings.SpriteLimit));
	createXMLSetting("FrameSkip", "Frame Skipping", BtoStr(EmuSettings.FrameSkip));
	createXMLSetting("sfxOverclock", "SuperFX Overclock", toStr(EmuSettings.sfxOverclock));
	createXMLSetting("Interpolation", "Interpolation", toStr(EmuSettings.Interpolation));
	createXMLSetting("MuteAudio", "Mute", BtoStr(EmuSettings.MuteAudio));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(EmuSettings.wiimoteOrientation));
#endif
	createXMLSetting("ExitAction", "Exit Action", toStr(EmuSettings.ExitAction));
	createXMLSetting("MusicVolume", "Music Volume", toStr(EmuSettings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(EmuSettings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", BtoStr(EmuSettings.Rumble));
	createXMLSetting("language", "Language", toStr(EmuSettings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(EmuSettings.PreviewImage));
	createXMLSetting("HideSRAMSaving", "Hide SRAM Saving", BtoStr(EmuSettings.HideSRAMSaving));
	
	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("Controller", "Controller", toStr(EmuSettings.Controller));
	createXMLSetting("TurboModeEnabled", "Turbo Mode Enabled", BtoStr(EmuSettings.TurboModeEnabled));
	createXMLSetting("TurboModeButton", "Turbo Mode Button", toStr(EmuSettings.TurboModeButton));
	createXMLSetting("GamepadMenuToggle", "Gamepad Menu Toggle", toStr(EmuSettings.GamepadMenuToggle));
	createXMLSetting("MapABXYRightStick", "Map ABXY Right Stick", BtoStr(EmuSettings.MapABXYRightStick));

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
	platform->getInput()->setWiimoteOrientation(EmuSettings.wiimoteOrientation);
	platform->getInput()->setRumbleEnabled(EmuSettings.Rumble);
	GuiSound::setDefaultVolume(SOUND::OGG, EmuSettings.MusicVolume);
	GuiSound::setDefaultVolume(SOUND::PCM, EmuSettings.SFXVolume);
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

	loadXMLSetting(&EmuSettings.AutoLoad, "AutoLoad");
	loadXMLSetting(&EmuSettings.AutoSave, "AutoSave");
	loadXMLSetting(&EmuSettings.LoadMethod, "LoadMethod");
	loadXMLSetting(&EmuSettings.SaveMethod, "SaveMethod");
	loadXMLSetting(EmuSettings.LoadFolder, "LoadFolder", sizeof(EmuSettings.LoadFolder));
	loadXMLSetting(EmuSettings.LastFileLoaded, "LastFileLoaded", sizeof(EmuSettings.LastFileLoaded));
	loadXMLSetting(EmuSettings.SaveFolder, "SaveFolder", sizeof(EmuSettings.SaveFolder));
	loadXMLSetting(&EmuSettings.AppendAuto, "AppendAuto");
	loadXMLSetting(EmuSettings.CheatFolder, "CheatFolder", sizeof(EmuSettings.CheatFolder));
	loadXMLSetting(EmuSettings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(EmuSettings.ScreenshotsFolder));
	loadXMLSetting(EmuSettings.CoverFolder, "CoverFolder", sizeof(EmuSettings.CoverFolder));
	loadXMLSetting(EmuSettings.ArtworkFolder, "ArtworkFolder", sizeof(EmuSettings.ArtworkFolder));

	// Network Settings

	loadXMLSetting(EmuSettings.smbShare.host, "smbip", sizeof(EmuSettings.smbShare.host));
	loadXMLSetting(EmuSettings.smbShare.share, "smbshare", sizeof(EmuSettings.smbShare.share));
	loadXMLSetting(EmuSettings.smbShare.user, "smbuser", sizeof(EmuSettings.smbShare.user));
	loadXMLSetting(EmuSettings.smbShare.password, "smbpwd", sizeof(EmuSettings.smbShare.password));

	// Video Settings

	loadXMLSetting(&EmuSettings.videoMode, "videoMode");
	loadXMLSetting(&EmuSettings.videoAspectRatioCorrection, "videoAspectRatioCorrection");
	loadXMLSetting(&EmuSettings.videoBilinearFilter, "videoBilinearFilter");
	loadXMLSetting(&EmuSettings.videoHardwareSoften, "videoHardwareSoften");
	loadXMLSetting(&EmuSettings.videoUpscalingFilter, "videoUpscalingFilter");
	loadXMLSetting(&EmuSettings.videoScanlines, "videoScanlines");
	loadXMLSetting(&EmuSettings.videoZoomHor, "videoZoomHor");
	loadXMLSetting(&EmuSettings.videoZoomVert, "videoZoomVert");
	loadXMLSetting(&EmuSettings.videoXshift, "videoXshift");
	loadXMLSetting(&EmuSettings.videoYshift, "videoYshift");

	// Emulation Settings
	loadXMLSetting(&EmuSettings.sfxOverclock, "sfxOverclock");
	loadXMLSetting(&EmuSettings.crosshair, "crosshair");
	loadXMLSetting(&EmuSettings.HiResolution, "HiResolution");
	loadXMLSetting(&EmuSettings.SpriteLimit, "SpriteLimit");
	loadXMLSetting(&EmuSettings.FrameSkip, "FrameSkip");
	loadXMLSetting(&EmuSettings.Interpolation, "Interpolation");
	loadXMLSetting(&EmuSettings.MuteAudio, "MuteAudio");

	// Menu Settings

	loadXMLSetting(&EmuSettings.wiimoteOrientation, "wiimoteOrientation");
	loadXMLSetting(&EmuSettings.ExitAction, "ExitAction");
	loadXMLSetting(&EmuSettings.MusicVolume, "MusicVolume");
	loadXMLSetting(&EmuSettings.SFXVolume, "SFXVolume");
	loadXMLSetting(&EmuSettings.Rumble, "Rumble");
	loadXMLSetting(&EmuSettings.language, "language");
	loadXMLSetting(&EmuSettings.PreviewImage, "PreviewImage");
	loadXMLSetting(&EmuSettings.HideSRAMSaving, "HideSRAMSaving");

	// Controller Settings

	loadXMLSetting(&EmuSettings.Controller, "Controller");
	loadXMLSetting(&EmuSettings.TurboModeEnabled, "TurboModeEnabled");
	loadXMLSetting(&EmuSettings.TurboModeButton, "TurboModeButton");
	loadXMLSetting(&EmuSettings.GamepadMenuToggle, "GamepadMenuToggle");
	loadXMLSetting(&EmuSettings.MapABXYRightStick, "MapABXYRightStick");

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
	if(!isValidLoadDevice(EmuSettings.LoadMethod))
		EmuSettings.LoadMethod = DEVICE_AUTO;
	if(!isValidSaveDevice(EmuSettings.SaveMethod))
		EmuSettings.SaveMethod = DEVICE_AUTO;

	if(strlen(EmuSettings.smbShare.share) == 0 || strlen(EmuSettings.smbShare.host) == 0) {
		if(EmuSettings.LoadMethod == DEVICE_SMB) {
			EmuSettings.LoadMethod = DEVICE_AUTO;
		}
		if(EmuSettings.SaveMethod == DEVICE_SMB) {
			EmuSettings.SaveMethod = DEVICE_AUTO;
		}
	}

	if(!(EmuSettings.videoZoomHor > 0.5 && EmuSettings.videoZoomHor < 1.5))
		EmuSettings.videoZoomHor = 1.0;
	if(!(EmuSettings.videoZoomVert > 0.5 && EmuSettings.videoZoomVert < 1.5))
		EmuSettings.videoZoomVert = 1.0;
	if(!(EmuSettings.videoXshift > -50 && EmuSettings.videoXshift < 50))
		EmuSettings.videoXshift = 0;
	if(!(EmuSettings.videoYshift > -50 && EmuSettings.videoYshift < 50))
		EmuSettings.videoYshift = 0;
	if(!(EmuSettings.MusicVolume >= 0 && EmuSettings.MusicVolume <= 100))
		EmuSettings.MusicVolume = 20;
	if(!(EmuSettings.SFXVolume >= 0 && EmuSettings.SFXVolume <= 100))
		EmuSettings.SFXVolume = 40;
	if(EmuSettings.language < 0 || EmuSettings.language >= LANG_LENGTH)
		EmuSettings.language = LANG_ENGLISH;
	if(EmuSettings.Controller > CTRL_PAD4 || EmuSettings.Controller < CTRL_SCOPE)
		EmuSettings.Controller = CTRL_PAD2;
	if(!(EmuSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && EmuSettings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
	if(!(EmuSettings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_NONE && EmuSettings.videoAspectRatioCorrection < VIDEO_ASPECT_RATIO_CORRECTION_LENGTH))
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
	if(!(EmuSettings.videoMode >= VIDEOMODE_AUTO && EmuSettings.videoMode < VIDEOMODE_LENGTH))
		EmuSettings.videoMode = VIDEOMODE_AUTO;
#if defined(HW_RVL) || defined(HW_DOL)
	if(!(EmuSettings.videoUpscalingFilter >= FILTER_NONE && EmuSettings.videoUpscalingFilter <= NUM_FILTERS))
		EmuSettings.videoUpscalingFilter = FILTER_NONE;
#endif
	if(!(EmuSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_AUTO && EmuSettings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
}

/****************************************************************************
 * DefaultSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
void DefaultSettings()
{
	memset (&EmuSettings, 0, sizeof (EmuSettings));

	ResetControls(); // controller button mappings

	EmuSettings.LoadMethod = DEVICE_AUTO;
	EmuSettings.SaveMethod = DEVICE_AUTO;
	sprintf (EmuSettings.LoadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (EmuSettings.SaveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (EmuSettings.CheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (EmuSettings.ScreenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (EmuSettings.CoverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (EmuSettings.ArtworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files
	EmuSettings.AutoLoad = true;
	EmuSettings.AutoSave = true;

	EmuSettings.Controller = CTRL_PAD2;

	EmuSettings.videoMode = VIDEOMODE_AUTO;
	EmuSettings.videoBilinearFilter = true;
	EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
	EmuSettings.videoScanlines = false;
#if defined(HW_RVL) || defined(HW_DOL)
	EmuSettings.videoUpscalingFilter = FILTER_NONE;
#else
	EmuSettings.videoUpscalingFilter = 0;
#endif

#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_16_9;
	else
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#elif HW_DOL
	EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#endif

	EmuSettings.videoZoomHor = 1.0; // horizontal zoom level
	EmuSettings.videoZoomVert = 1.0; // vertical zoom level
	EmuSettings.videoXshift = 0; // horizontal video shift
	EmuSettings.videoYshift = 0; // vertical video shift
	EmuSettings.crosshair = true;

	EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
#ifdef HW_RVL
	EmuSettings.ExitAction = EXITACTION_WII_AUTO;
#elif HW_DOL
	EmuSettings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	EmuSettings.AutoloadGame = false;
	EmuSettings.MusicVolume = 20;
	EmuSettings.SFXVolume = 40;
	EmuSettings.Rumble = true;
	EmuSettings.PreviewImage = PREVIEWIMAGE_COVER;
	EmuSettings.HideSRAMSaving = false;
	
#ifdef HW_RVL
	EmuSettings.language = CONF_GetLanguage();

	if(EmuSettings.language == LANG_TRAD_CHINESE)
		EmuSettings.language = LANG_SIMP_CHINESE;
#elif HW_DOL
	EmuSettings.language = SYS_GetLanguage() + LANG_ENGLISH;
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
	EmuSettings.MuteAudio = false;
	EmuSettings.Interpolation = 0;
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
	EmuSettings.HiResolution = true; // Enabled by default
	EmuSettings.SpriteLimit = true; // Enabled by default
	EmuSettings.FrameSkip = true; // Enabled by default

	// Frame timings in 50hz and 60hz cpu mode
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;

	EmuSettings.sfxOverclock = 0;
	/* Initialize Super FX CPU to normal speed by default */
	Settings.SuperFXSpeedPerLine = 5823405;
	
	Settings.SuperFXClockMultiplier = 100;
	Settings.OverclockMode = 0;
	Settings.OneClockCycle = 6;
	Settings.OneSlowClockCycle = 8;
	Settings.TwoClockCycles = 12;

	EmuSettings.TurboModeEnabled = true; // Enabled by default
	EmuSettings.TurboModeButton = 0; // Default is Right Analog Stick (0)
	EmuSettings.GamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
	EmuSettings.MapABXYRightStick = false;
}

/****************************************************************************
 * Prefs storage location discovery
 *
 * Platform-agnostic: driven entirely through FileSystemDriver
 ***************************************************************************/

//! GameCube's SD-adapter card slots (carda/cardb/port2) have real hardware
//! presence detection; GCLoader doesn't - it can only be considered a
//! prefs candidate when none of the other three could possibly be what
//! the user means - not simply "next in priority order"
static bool AnyGameCubeSDCardPresent()
{
	return platform->getFileSystem()->isDevicePresent(DEVICE_SD_SLOTA) ||
			platform->getFileSystem()->isDevicePresent(DEVICE_SD_SLOTB) ||
			platform->getFileSystem()->isDevicePresent(DEVICE_SD_PORT2);
}

//! Ordered (most-preferred first) list of devices eligible to hold
//! settings.xml, derived from the platform's own save-device priority
static int GetPrefsDeviceCandidates(int outDevices[MAX_STORAGE_DEVICES])
{
	int numSaveDevices;
	const int * saveDevices = platform->getFileSystem()->getValidSaveDevices(numSaveDevices);

	bool gameCubeSDCardPresent = AnyGameCubeSDCardPresent();
	int count = 0;

	for(int i = 0; i < numSaveDevices; i++)
	{
		int device = saveDevices[i];

		if(device == DEVICE_AUTO || device == DEVICE_SMB || device == DEVICE_DVD)
			continue;

		if(device == DEVICE_SD_GCLOADER && gameCubeSDCardPresent)
			continue;

		outDevices[count++] = device;
	}

	return count;
}

//! Candidate subfolder(s) to check for settings.xml on device, most
//! canonical first. GameCube's card slots/GCLoader predate (and don't
//! use) the "apps/" loader convention. Wii/Wii U do - Wii U nests an
//! extra "wiiu/" underneath since its apps folder lives alongside vWii's
//! on the same SD card and the two must not collide.
static int GetPrefsSubfolderCandidates(int device, const char * outFolders[2])
{
	if(device == DEVICE_SD_SLOTA || device == DEVICE_SD_SLOTB ||
	   device == DEVICE_SD_PORT2 || device == DEVICE_SD_GCLOADER)
	{
		outFolders[0] = APPFOLDER;
		return 1;
	}

#ifdef __WIIU__
	outFolders[0] = "wiiu/apps/" APPFOLDER;
#else
	outFolders[0] = "apps/" APPFOLDER;
#endif
	outFolders[1] = APPFOLDER; // legacy fallback: pre-"apps/" installs
	return 2;
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

	// The remembered location might not be reachable anymore - eg. a USB
	// drive was unplugged, or moved to a different USB1/2/3 slot since
	// prefpath was last set. Rather than fail outright, forget it and
	// fall through to picking a fresh save location below, exactly as on
	// a first save.
	if(device != DEVICE_AUTO && !ChangeInterface(device, SILENT))
	{
		device = DEVICE_AUTO;
		prefpath[0] = 0;
	}

	if(device == DEVICE_AUTO)
	{
		autoSaveMethod();
		device = EmuSettings.SaveMethod;

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

//! Cycles through every connected candidate device (priority order) and
//! every subfolder convention it might use, looking for an existing
//! settings.xml. Stops - and leaves prefpath/appPath set via
//! LoadPrefsFromMethod()'s own side effects - at the first hit.
static bool ScanForExistingPrefs()
{
	int devices[MAX_STORAGE_DEVICES];
	int deviceCount = GetPrefsDeviceCandidates(devices);

	for(int i = 0; i < deviceCount; i++)
	{
		int device = devices[i];

		if(!ChangeInterface(device, SILENT))
			continue; // not physically present / couldn't mount

		const char * folders[2];
		int folderCount = GetPrefsSubfolderCandidates(device, folders);

		for(int f = 0; f < folderCount; f++)
		{
			char path[MAXPATHLEN];
			MakeFilePathForFolderPath(path, device, folders[f]);

			if(LoadPrefsFromMethod(path))
				return true;
		}
	}

	return false;
}

//! Recognizes a devoptab-style USB path prefix and maps it to the
//! corresponding Device id plus the path suffix after the prefix. Both
//! Wii's naming ("usb:/" for the first slot) and Wii U's ("usb1:/" for
//! the first slot) are recognized, since this needs to work unmodified
//! on both platforms.
static bool ParseUsbPath(const char * path, int * outDevice, const char ** outSuffix)
{
	static const struct { const char * prefix; int device; } usbPrefixes[] = {
		{ "usb:/",  DEVICE_USB  },
		{ "usb1:/", DEVICE_USB  },
		{ "usb2:/", DEVICE_USB2 },
		{ "usb3:/", DEVICE_USB3 },
	};

	if(!path)
		return false;

	for(size_t i = 0; i < sizeof(usbPrefixes) / sizeof(usbPrefixes[0]); i++)
	{
		size_t len = strlen(usbPrefixes[i].prefix);
		if(strncmp(path, usbPrefixes[i].prefix, len) == 0)
		{
			*outDevice = usbPrefixes[i].device;
			*outSuffix = path + len;
			return true;
		}
	}
	return false;
}

//! USB1/2/3 slot assignment isn't stable. If path's own device doesn't
//! currently resolve to that literal path, look for the same relative
//! path on another currently-mounted USB device and, if found, rewrite
//! path in place to point at it. No-op for any path that isn't on a
//! USB1/2/3 device - SD/DVD/SMB/GameCube card paths are left alone.
static void RemapUsbPathIfNeeded(char * path, size_t pathSize)
{
	int device;
	const char * suffix;

	if(!path || path[0] == 0 || !ParseUsbPath(path, &device, &suffix))
		return;

	struct stat st;

	// Already resolves as-is - nothing to do.
	if(ChangeInterface(device, SILENT) && stat(path, &st) == 0)
		return;

	static const int usbCandidates[] = { DEVICE_USB, DEVICE_USB2, DEVICE_USB3 };

	for(int i = 0; i < 3; i++)
	{
		if(usbCandidates[i] == device)
			continue; // already checked above

		if(!ChangeInterface(usbCandidates[i], SILENT))
			continue; // this slot isn't mounted right now

		char candidatePath[MAXPATHLEN];
		MakeFilePathForFolderPath(candidatePath, usbCandidates[i], suffix);

		if(stat(candidatePath, &st) == 0)
		{
			snprintf(path, pathSize, "%s", candidatePath);
			return;
		}
	}

	// No match on any other USB device - leave path as-is. Whatever
	// consumes it (eg. the file browser) already handles a folder or
	// file that doesn't exist.
}

//! Applies RemapUsbPathIfNeeded() to every stored path that can point at
//! removable storage. Called once, right after a settings.xml is
//! successfully loaded.
static void RemapUsbPathsIfNeeded()
{
	RemapUsbPathIfNeeded(EmuSettings.LoadFolder, sizeof(EmuSettings.LoadFolder));
	RemapUsbPathIfNeeded(EmuSettings.LastFileLoaded, sizeof(EmuSettings.LastFileLoaded));
	RemapUsbPathIfNeeded(EmuSettings.SaveFolder, sizeof(EmuSettings.SaveFolder));
	RemapUsbPathIfNeeded(EmuSettings.CheatFolder, sizeof(EmuSettings.CheatFolder));
	RemapUsbPathIfNeeded(EmuSettings.ScreenshotsFolder, sizeof(EmuSettings.ScreenshotsFolder));
	RemapUsbPathIfNeeded(EmuSettings.CoverFolder, sizeof(EmuSettings.CoverFolder));
	RemapUsbPathIfNeeded(EmuSettings.ArtworkFolder, sizeof(EmuSettings.ArtworkFolder));
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

	// Most likely correct location: wherever the app itself was loaded
	// from (CreateAppPath(), set from argv[0] at startup), if known.
	bool prefFound = (appPath[0] != 0) && LoadPrefsFromMethod(appPath);

	// Otherwise, cycle through every connected device in priority order
	// (SD before USB1/2/3 on Wii/Wii U; carda/cardb/port2 before
	// GCLoader on GameCube - see GetPrefsDeviceCandidates()) looking for
	// an existing settings.xml.
	if(!prefFound)
		prefFound = ScanForExistingPrefs();

	if(!prefFound) {
		return false;
	}

	RemapUsbPathsIfNeeded();

	FixInvalidSettings();
	ApplySettings();

	#ifndef HW_DOL
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
	if (EmuSettings.SaveMethod > DEVICE_AUTO) {
		const char* savePointers[] = { EmuSettings.SaveFolder, EmuSettings.CheatFolder };

		for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
			const char* currentPath = savePointers[i];

			if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
				CreatePathWithPrefix(EmuSettings.SaveMethod, APPFOLDER);
			}

			GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
			if (strcmp(currentPath, defaultFolder) == 0) {
				CreatePathWithPrefix(EmuSettings.SaveMethod, currentPath);
			}
		}
	}

	if (EmuSettings.LoadMethod > DEVICE_AUTO && EmuSettings.LoadMethod != DEVICE_DVD) {
		const char* loadPointers[] = {
			EmuSettings.LoadFolder,
			EmuSettings.ScreenshotsFolder,
			EmuSettings.CoverFolder,
			EmuSettings.ArtworkFolder
		};
		for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
			const char* currentPath = loadPointers[i];
			if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
				CreatePathWithPrefix(EmuSettings.LoadMethod, APPFOLDER);
			}

			GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
			if (strcmp(currentPath, defaultFolder) == 0) {
				CreatePathWithPrefix(EmuSettings.LoadMethod, currentPath);
			}
		}
	}
}
