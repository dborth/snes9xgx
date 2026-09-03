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

#include "snes9xgx.h"
#include "system.h"
#include "menu.h"
#include "fileop.h"
#include "drivers/ogc/videofilters.h"
#include "video.h"
#include "filebrowser.h"
#include "input.h"
#include "button_mapping.h"
#include "libgui/Gui.h"

#include "snes9x/apu/apu.h"

struct SGCSettings GCSettings;

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

	createXMLSetting("AutoLoad", "Auto Load", toStr(GCSettings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(GCSettings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(GCSettings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(GCSettings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", GCSettings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", GCSettings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", GCSettings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", BtoStr(GCSettings.AppendAuto));
	createXMLSetting("CheatFolder", "Cheats Folder", GCSettings.CheatFolder);
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", GCSettings.ScreenshotsFolder);
	createXMLSetting("CoverFolder", "Covers Folder", GCSettings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", GCSettings.ArtworkFolder);
	
	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", GCSettings.smbip);
	createXMLSetting("smbshare", "Share Name", GCSettings.smbshare);
	createXMLSetting("smbuser", "Share Username", GCSettings.smbuser);
	createXMLSetting("smbpwd", "Share Password", GCSettings.smbpwd);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videoMode", "Output Mode", toStr(GCSettings.videoMode));
	createXMLSetting("videoAspectRatioCorrection", "Aspect Ratio Correction", toStr(GCSettings.videoAspectRatioCorrection));
	createXMLSetting("videoBilinearFilter", "Bilinear Filtering", BtoStr(GCSettings.videoBilinearFilter));
	createXMLSetting("videoHardwareSoften", "Hardware Soften", toStr(GCSettings.videoHardwareSoften));
	createXMLSetting("videoScanlines", "Scanlines", BtoStr(GCSettings.videoScanlines));
	createXMLSetting("videoUpscalingFilter", "Upscaling Filter Method", toStr(GCSettings.videoUpscalingFilter));
	createXMLSetting("videoZoomHor", "Horizontal Zoom Level", FtoStr(GCSettings.videoZoomHor));
	createXMLSetting("videoZoomVert", "Vertical Zoom Level", FtoStr(GCSettings.videoZoomVert));
	createXMLSetting("videoXshift", "Horizontal Video Shift", toStr(GCSettings.videoXshift));
	createXMLSetting("videoYshift", "Vertical Video Shift", toStr(GCSettings.videoYshift));

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("crosshair", "Crosshair", BtoStr(GCSettings.crosshair));
	createXMLSetting("HiResolution", "SNES Hi-Res Mode", BtoStr(GCSettings.HiResolution));
	createXMLSetting("SpriteLimit", "Sprites per-line Limit", BtoStr(GCSettings.SpriteLimit));
	createXMLSetting("FrameSkip", "Frame Skipping", BtoStr(GCSettings.FrameSkip));
	createXMLSetting("sfxOverclock", "SuperFX Overclock", toStr(GCSettings.sfxOverclock));
	createXMLSetting("Interpolation", "Interpolation", toStr(GCSettings.Interpolation));
	createXMLSetting("MuteAudio", "Mute", BtoStr(GCSettings.MuteAudio));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(GCSettings.wiimoteOrientation));
#endif
	createXMLSetting("ExitAction", "Exit Action", toStr(GCSettings.ExitAction));
	createXMLSetting("MusicVolume", "Music Volume", toStr(GCSettings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(GCSettings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", BtoStr(GCSettings.Rumble));
	createXMLSetting("language", "Language", toStr(GCSettings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(GCSettings.PreviewImage));
	createXMLSetting("HideSRAMSaving", "Hide SRAM Saving", BtoStr(GCSettings.HideSRAMSaving));
	
	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("Controller", "Controller", toStr(GCSettings.Controller));
	createXMLSetting("TurboModeEnabled", "Turbo Mode Enabled", BtoStr(GCSettings.TurboModeEnabled));
	createXMLSetting("TurboModeButton", "Turbo Mode Button", toStr(GCSettings.TurboModeButton));
	createXMLSetting("GamepadMenuToggle", "Gamepad Menu Toggle", toStr(GCSettings.GamepadMenuToggle));
	createXMLSetting("MapABXYRightStick", "Map ABXY Right Stick", BtoStr(GCSettings.MapABXYRightStick));

	createXMLController(btnmap[CTRL_PAD][GUI_HW_GAMECUBE], "btnmapping_pad_gcpad", "SNES Pad - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_PAD][GUI_HW_WIIMOTE], "btnmapping_pad_wiimote", "SNES Pad - Wiimote");
	createXMLController(btnmap[CTRL_PAD][GUI_HW_CLASSIC], "btnmapping_pad_classic", "SNES Pad - Classic Controller");
	createXMLController(btnmap[CTRL_PAD][GUI_HW_WUPC], "btnmapping_pad_wupc", "SNES Pad - Wii U Pro Controller");
	createXMLController(btnmap[CTRL_PAD][GUI_HW_DRC], "btnmapping_pad_wiidrc", "SNES Pad - Wii U Gamepad");
	createXMLController(btnmap[CTRL_PAD][GUI_HW_NUNCHUK], "btnmapping_pad_nunchuk", "SNES Pad - Nunchuk + Wiimote");
#endif
	createXMLController(btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE], "btnmapping_scope_gcpad", "Superscope - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE], "btnmapping_scope_wiimote", "Superscope - Wiimote");
#endif
	createXMLController(btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE], "btnmapping_mouse_gcpad", "Mouse - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE], "btnmapping_mouse_wiimote", "Mouse - Wiimote");
#endif
	createXMLController(btnmap[CTRL_JUST][GUI_HW_GAMECUBE], "btnmapping_just_gcpad", "Justifier - GameCube Controller");
#ifdef HW_RVL
	createXMLController(btnmap[CTRL_JUST][GUI_HW_WIIMOTE], "btnmapping_just_wiimote", "Justifier - Wiimote");
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
	platform->getInput()->setWiimoteOrientation(GCSettings.wiimoteOrientation);
	platform->getInput()->setRumbleEnabled(GCSettings.Rumble);
	GuiSound::setDefaultVolume(SOUND::OGG, GCSettings.MusicVolume);
	GuiSound::setDefaultVolume(SOUND::PCM, GCSettings.SFXVolume);
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

	loadXMLSetting(&GCSettings.AutoLoad, "AutoLoad");
	loadXMLSetting(&GCSettings.AutoSave, "AutoSave");
	loadXMLSetting(&GCSettings.LoadMethod, "LoadMethod");
	loadXMLSetting(&GCSettings.SaveMethod, "SaveMethod");
	loadXMLSetting(GCSettings.LoadFolder, "LoadFolder", sizeof(GCSettings.LoadFolder));
	loadXMLSetting(GCSettings.LastFileLoaded, "LastFileLoaded", sizeof(GCSettings.LastFileLoaded));
	loadXMLSetting(GCSettings.SaveFolder, "SaveFolder", sizeof(GCSettings.SaveFolder));
	loadXMLSetting(&GCSettings.AppendAuto, "AppendAuto");
	loadXMLSetting(GCSettings.CheatFolder, "CheatFolder", sizeof(GCSettings.CheatFolder));
	loadXMLSetting(GCSettings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(GCSettings.ScreenshotsFolder));
	loadXMLSetting(GCSettings.CoverFolder, "CoverFolder", sizeof(GCSettings.CoverFolder));
	loadXMLSetting(GCSettings.ArtworkFolder, "ArtworkFolder", sizeof(GCSettings.ArtworkFolder));

	// Network Settings

	loadXMLSetting(GCSettings.smbip, "smbip", sizeof(GCSettings.smbip));
	loadXMLSetting(GCSettings.smbshare, "smbshare", sizeof(GCSettings.smbshare));
	loadXMLSetting(GCSettings.smbuser, "smbuser", sizeof(GCSettings.smbuser));
	loadXMLSetting(GCSettings.smbpwd, "smbpwd", sizeof(GCSettings.smbpwd));

	// Video Settings

	loadXMLSetting(&GCSettings.videoMode, "videoMode");
	loadXMLSetting(&GCSettings.videoAspectRatioCorrection, "videoAspectRatioCorrection");
	loadXMLSetting(&GCSettings.videoBilinearFilter, "videoBilinearFilter");
	loadXMLSetting(&GCSettings.videoHardwareSoften, "videoHardwareSoften");
	loadXMLSetting(&GCSettings.videoUpscalingFilter, "videoUpscalingFilter");
	loadXMLSetting(&GCSettings.videoScanlines, "videoScanlines");
	loadXMLSetting(&GCSettings.videoZoomHor, "videoZoomHor");
	loadXMLSetting(&GCSettings.videoZoomVert, "videoZoomVert");
	loadXMLSetting(&GCSettings.videoXshift, "videoXshift");
	loadXMLSetting(&GCSettings.videoYshift, "videoYshift");

	// Emulation Settings
	loadXMLSetting(&GCSettings.sfxOverclock, "sfxOverclock");
	loadXMLSetting(&GCSettings.crosshair, "crosshair");
	loadXMLSetting(&GCSettings.HiResolution, "HiResolution");
	loadXMLSetting(&GCSettings.SpriteLimit, "SpriteLimit");
	loadXMLSetting(&GCSettings.FrameSkip, "FrameSkip");
	loadXMLSetting(&GCSettings.Interpolation, "Interpolation");
	loadXMLSetting(&GCSettings.MuteAudio, "MuteAudio");

	// Menu Settings

	loadXMLSetting(&GCSettings.wiimoteOrientation, "WiimoteOrientation");
	loadXMLSetting(&GCSettings.ExitAction, "ExitAction");
	loadXMLSetting(&GCSettings.MusicVolume, "MusicVolume");
	loadXMLSetting(&GCSettings.SFXVolume, "SFXVolume");
	loadXMLSetting(&GCSettings.Rumble, "Rumble");
	loadXMLSetting(&GCSettings.language, "language");
	loadXMLSetting(&GCSettings.PreviewImage, "PreviewImage");
	loadXMLSetting(&GCSettings.HideSRAMSaving, "HideSRAMSaving");

	// Controller Settings

	loadXMLSetting(&GCSettings.Controller, "Controller");
	loadXMLSetting(&GCSettings.TurboModeEnabled, "TurboModeEnabled");
	loadXMLSetting(&GCSettings.TurboModeButton, "TurboModeButton");
	loadXMLSetting(&GCSettings.GamepadMenuToggle, "GamepadMenuToggle");
	loadXMLSetting(&GCSettings.MapABXYRightStick, "MapABXYRightStick");

	loadXMLController(btnmap[CTRL_PAD][GUI_HW_GAMECUBE], "btnmapping_pad_gcpad");
	loadXMLController(btnmap[CTRL_PAD][GUI_HW_WIIMOTE], "btnmapping_pad_wiimote");
	loadXMLController(btnmap[CTRL_PAD][GUI_HW_CLASSIC], "btnmapping_pad_classic");
	loadXMLController(btnmap[CTRL_PAD][GUI_HW_WUPC], "btnmapping_pad_wupc");
	loadXMLController(btnmap[CTRL_PAD][GUI_HW_DRC], "btnmapping_pad_wiidrc");
	loadXMLController(btnmap[CTRL_PAD][GUI_HW_NUNCHUK], "btnmapping_pad_nunchuk");
	loadXMLController(btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE], "btnmapping_scope_gcpad");
	loadXMLController(btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE], "btnmapping_scope_wiimote");
	loadXMLController(btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE], "btnmapping_mouse_gcpad");
	loadXMLController(btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE], "btnmapping_mouse_wiimote");
	loadXMLController(btnmap[CTRL_JUST][GUI_HW_GAMECUBE], "btnmapping_just_gcpad");
	loadXMLController(btnmap[CTRL_JUST][GUI_HW_WIIMOTE], "btnmapping_just_wiimote");

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
	if(!isValidLoadDevice(GCSettings.LoadMethod))
		GCSettings.LoadMethod = DEVICE_AUTO;
	if(!isValidSaveDevice(GCSettings.SaveMethod))
		GCSettings.SaveMethod = DEVICE_AUTO;

	if(strlen(GCSettings.smbshare) == 0 || strlen(GCSettings.smbip) == 0) {
		if(GCSettings.LoadMethod == DEVICE_SMB) {
			GCSettings.LoadMethod = DEVICE_AUTO;
		}
		if(GCSettings.SaveMethod == DEVICE_SMB) {
			GCSettings.SaveMethod = DEVICE_AUTO;
		}
	}

	if(!(GCSettings.videoZoomHor > 0.5 && GCSettings.videoZoomHor < 1.5))
		GCSettings.videoZoomHor = 1.0;
	if(!(GCSettings.videoZoomVert > 0.5 && GCSettings.videoZoomVert < 1.5))
		GCSettings.videoZoomVert = 1.0;
	if(!(GCSettings.videoXshift > -50 && GCSettings.videoXshift < 50))
		GCSettings.videoXshift = 0;
	if(!(GCSettings.videoYshift > -50 && GCSettings.videoYshift < 50))
		GCSettings.videoYshift = 0;
	if(!(GCSettings.MusicVolume >= 0 && GCSettings.MusicVolume <= 100))
		GCSettings.MusicVolume = 20;
	if(!(GCSettings.SFXVolume >= 0 && GCSettings.SFXVolume <= 100))
		GCSettings.SFXVolume = 40;
	if(GCSettings.language < 0 || GCSettings.language >= LANG_LENGTH)
		GCSettings.language = LANG_ENGLISH;
	if(GCSettings.Controller > CTRL_PAD4 || GCSettings.Controller < CTRL_SCOPE)
		GCSettings.Controller = CTRL_PAD2;
	if(!(GCSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && GCSettings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		GCSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
	if(!(GCSettings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_NONE && GCSettings.videoAspectRatioCorrection < VIDEO_ASPECT_RATIO_CORRECTION_LENGTH))
		GCSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
	if(!(GCSettings.videoMode >= VIDEOMODE_AUTO && GCSettings.videoMode < VIDEOMODE_LENGTH))
		GCSettings.videoMode = VIDEOMODE_AUTO;
	if(!(GCSettings.videoUpscalingFilter >= FILTER_NONE && GCSettings.videoUpscalingFilter <= NUM_FILTERS))
		GCSettings.videoUpscalingFilter = FILTER_NONE;
	if(!(GCSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_AUTO && GCSettings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		GCSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
}

/****************************************************************************
 * DefaultSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
void DefaultSettings()
{
	memset (&GCSettings, 0, sizeof (GCSettings));

	ResetControls(); // controller button mappings

	GCSettings.LoadMethod = DEVICE_AUTO;
	GCSettings.SaveMethod = DEVICE_AUTO;
	sprintf (GCSettings.LoadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (GCSettings.SaveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (GCSettings.CheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (GCSettings.ScreenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (GCSettings.CoverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (GCSettings.ArtworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files
	GCSettings.AutoLoad = true;
	GCSettings.AutoSave = true;

	GCSettings.Controller = CTRL_PAD2;

	GCSettings.videoMode = VIDEOMODE_AUTO;
	GCSettings.videoBilinearFilter = true;
	GCSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
	GCSettings.videoScanlines = false;
	GCSettings.videoUpscalingFilter = FILTER_NONE;

#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		GCSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_16_9;
	else
		GCSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#else
	GCSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#endif

	GCSettings.videoZoomHor = 1.0; // horizontal zoom level
	GCSettings.videoZoomVert = 1.0; // vertical zoom level
	GCSettings.videoXshift = 0; // horizontal video shift
	GCSettings.videoYshift = 0; // vertical video shift
	GCSettings.crosshair = true;

	GCSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
#ifdef HW_RVL
	GCSettings.ExitAction = EXITACTION_WII_AUTO;
#else
	GCSettings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	GCSettings.AutoloadGame = false;
	GCSettings.MusicVolume = 20;
	GCSettings.SFXVolume = 40;
	GCSettings.Rumble = true;
	GCSettings.PreviewImage = PREVIEWIMAGE_COVER;
	GCSettings.HideSRAMSaving = false;
	
#ifdef HW_RVL
	GCSettings.language = CONF_GetLanguage();

	if(GCSettings.language == LANG_TRAD_CHINESE)
		GCSettings.language = LANG_SIMP_CHINESE;
#else
	GCSettings.language = SYS_GetLanguage() + LANG_ENGLISH;
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
	GCSettings.MuteAudio = false;
	GCSettings.Interpolation = 0;
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
	GCSettings.HiResolution = true; // Enabled by default
	GCSettings.SpriteLimit = true; // Enabled by default
	GCSettings.FrameSkip = true; // Enabled by default

	// Frame timings in 50hz and 60hz cpu mode
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;

	GCSettings.sfxOverclock = 0;
	/* Initialize Super FX CPU to normal speed by default */
	Settings.SuperFXSpeedPerLine = 5823405;
	
	Settings.SuperFXClockMultiplier = 100;
	Settings.OverclockMode = 0;
	Settings.OneClockCycle = 6;
	Settings.OneSlowClockCycle = 8;
	Settings.TwoClockCycles = 12;

	GCSettings.TurboModeEnabled = true; // Enabled by default
	GCSettings.TurboModeButton = 0; // Default is Right Analog Stick (0)
	GCSettings.GamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
	GCSettings.MapABXYRightStick = false;
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
		sprintf(filepath, "%s/%s", prefpath, PREF_FILE_NAME);
		FindDevice(filepath, &device);
	}
	else if(appPath[0] != 0)
	{
		sprintf(filepath, "%s/%s", appPath, PREF_FILE_NAME);
		strcpy(prefpath, appPath);
		FindDevice(filepath, &device);
	}
	else
	{
		autoSaveMethod();
		device = GCSettings.SaveMethod;

		if(!ChangeInterface(device, true)) {
			return false;
		}
		
		sprintf(filepath, "%s%s", pathPrefix[device], APPFOLDER);
		if(!CreateDirectory(filepath)) {
			return false;
		}

		sprintf(filepath, "%s%s/%s", pathPrefix[device], APPFOLDER, PREF_FILE_NAME);
		sprintf(prefpath, "%s%s", pathPrefix[device], APPFOLDER);
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
#else
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

    if (GCSettings.SaveMethod > DEVICE_AUTO && ChangeInterface(GCSettings.SaveMethod, NOTSILENT)) {
        const char* savePointers[] = { GCSettings.SaveFolder, GCSettings.CheatFolder };

        for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
            const char* currentPath = savePointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(GCSettings.SaveMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(GCSettings.SaveMethod, currentPath);
            }
        }
    }

    if (GCSettings.LoadMethod > DEVICE_AUTO && GCSettings.LoadMethod != DEVICE_DVD && ChangeInterface(GCSettings.LoadMethod, NOTSILENT)) {
        const char* loadPointers[] = {
            GCSettings.LoadFolder,
            GCSettings.ScreenshotsFolder,
            GCSettings.CoverFolder,
            GCSettings.ArtworkFolder
        };

        for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
            const char* currentPath = loadPointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(GCSettings.LoadMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(GCSettings.LoadMethod, currentPath);
            }
        }
    }
}
