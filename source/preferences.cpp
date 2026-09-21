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
#ifdef __WIIU__
#include <gx2/display.h>
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
#ifdef __WIIU__
#include "drivers/wut/WutUpscaleFilters.h"
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

	createXMLSetting("autoLoad", "Auto Load", toStr(EmuSettings.autoLoad));
	createXMLSetting("autoSave", "Auto Save", toStr(EmuSettings.autoSave));
	createXMLSetting("loadDevice", "Load Device", toStr(EmuSettings.loadDevice));
	createXMLSetting("saveDevice", "Save Device", toStr(EmuSettings.saveDevice));
	createXMLSetting("loadFolder", "Load Folder", EmuSettings.loadFolder);
	createXMLSetting("lastFileLoaded", "Last File Loaded", EmuSettings.lastFileLoaded);
	createXMLSetting("saveFolder", "Save Folder", EmuSettings.saveFolder);
	createXMLSetting("appendAuto", "Append Auto to .SAV Files", BtoStr(EmuSettings.appendAuto));
	createXMLSetting("cheatFolder", "Cheats Folder", EmuSettings.cheatFolder);
	createXMLSetting("screenshotsFolder", "Screenshots Folder", EmuSettings.screenshotsFolder);
	createXMLSetting("coverFolder", "Covers Folder", EmuSettings.coverFolder);
	createXMLSetting("artworkFolder", "Artwork Folder", EmuSettings.artworkFolder);
	
	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbHost", "Share Computer IP", EmuSettings.smbShare.host);
	createXMLSetting("smbShare", "Share Name", EmuSettings.smbShare.share);
	createXMLSetting("smbUser", "Share Username", EmuSettings.smbShare.user);
	createXMLSetting("smbPassword", "Share Password", EmuSettings.smbShare.password);

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
	createXMLSetting("hiResolution", "SNES Hi-Res Mode", BtoStr(EmuSettings.hiResolution));
	createXMLSetting("spriteLimit", "Sprites per-line Limit", BtoStr(EmuSettings.spriteLimit));
	createXMLSetting("frameSkip", "Frame Skipping", BtoStr(EmuSettings.frameSkip));
	createXMLSetting("sfxOverclock", "SuperFX Overclock", toStr(EmuSettings.sfxOverclock));
	createXMLSetting("interpolation", "Interpolation", toStr(EmuSettings.interpolation));
	createXMLSetting("muteAudio", "Mute", BtoStr(EmuSettings.muteAudio));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(EmuSettings.wiimoteOrientation));
#endif
	createXMLSetting("exitAction", "Exit Action", toStr(EmuSettings.exitAction));
	createXMLSetting("musicVolume", "Music Volume", toStr(EmuSettings.musicVolume));
	createXMLSetting("sfxVolume", "Sound Effects Volume", toStr(EmuSettings.sfxVolume));
	createXMLSetting("rumble", "Rumble", BtoStr(EmuSettings.rumble));
	createXMLSetting("language", "Language", toStr(EmuSettings.language));
	createXMLSetting("previewImage", "Preview Image", toStr(EmuSettings.previewImage));
	createXMLSetting("hideSramSaving", "Hide SRAM Saving", BtoStr(EmuSettings.hideSramSaving));
	
	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("controller", "Controller", toStr(EmuSettings.controller));
	createXMLSetting("turboModeEnabled", "Turbo Mode Enabled", BtoStr(EmuSettings.turboModeEnabled));
	createXMLSetting("turboModeButton", "Turbo Mode Button", toStr(EmuSettings.turboModeButton));
	createXMLSetting("gamepadMenuToggle", "Gamepad Menu Toggle", toStr(EmuSettings.gamepadMenuToggle));
	createXMLSetting("mapAbxyRightStick", "Map ABXY Right Stick", BtoStr(EmuSettings.mapAbxyRightStick));

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
	platform->getInput()->setRumbleEnabled(EmuSettings.rumble);
	GuiSound::setDefaultVolume(VOLUME_TYPE::MUSIC, EmuSettings.musicVolume);
	GuiSound::setDefaultVolume(VOLUME_TYPE::SFX, EmuSettings.sfxVolume);
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

	loadXMLSetting(&EmuSettings.autoLoad, "autoLoad");
	loadXMLSetting(&EmuSettings.autoSave, "autoSave");
	loadXMLSetting(&EmuSettings.loadDevice, "loadDevice");
	loadXMLSetting(&EmuSettings.saveDevice, "saveDevice");
	loadXMLSetting(EmuSettings.loadFolder, "loadFolder", sizeof(EmuSettings.loadFolder));
	loadXMLSetting(EmuSettings.lastFileLoaded, "lastFileLoaded", sizeof(EmuSettings.lastFileLoaded));
	loadXMLSetting(EmuSettings.saveFolder, "saveFolder", sizeof(EmuSettings.saveFolder));
	loadXMLSetting(&EmuSettings.appendAuto, "appendAuto");
	loadXMLSetting(EmuSettings.cheatFolder, "cheatFolder", sizeof(EmuSettings.cheatFolder));
	loadXMLSetting(EmuSettings.screenshotsFolder, "screenshotsFolder", sizeof(EmuSettings.screenshotsFolder));
	loadXMLSetting(EmuSettings.coverFolder, "coverFolder", sizeof(EmuSettings.coverFolder));
	loadXMLSetting(EmuSettings.artworkFolder, "artworkFolder", sizeof(EmuSettings.artworkFolder));

	// Network Settings

	loadXMLSetting(EmuSettings.smbShare.host, "smbHost", sizeof(EmuSettings.smbShare.host));
	loadXMLSetting(EmuSettings.smbShare.share, "smbShare", sizeof(EmuSettings.smbShare.share));
	loadXMLSetting(EmuSettings.smbShare.user, "smbUser", sizeof(EmuSettings.smbShare.user));
	loadXMLSetting(EmuSettings.smbShare.password, "smbPassword", sizeof(EmuSettings.smbShare.password));

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
	loadXMLSetting(&EmuSettings.hiResolution, "hiResolution");
	loadXMLSetting(&EmuSettings.spriteLimit, "spriteLimit");
	loadXMLSetting(&EmuSettings.frameSkip, "frameSkip");
	loadXMLSetting(&EmuSettings.interpolation, "interpolation");
	loadXMLSetting(&EmuSettings.muteAudio, "muteAudio");

	// Menu Settings

	loadXMLSetting(&EmuSettings.wiimoteOrientation, "wiimoteOrientation");
	loadXMLSetting(&EmuSettings.exitAction, "exitAction");
	loadXMLSetting(&EmuSettings.musicVolume, "musicVolume");
	loadXMLSetting(&EmuSettings.sfxVolume, "sfxVolume");
	loadXMLSetting(&EmuSettings.rumble, "rumble");
	loadXMLSetting(&EmuSettings.language, "language");
	loadXMLSetting(&EmuSettings.previewImage, "previewImage");
	loadXMLSetting(&EmuSettings.hideSramSaving, "hideSramSaving");

	// Controller Settings

	loadXMLSetting(&EmuSettings.controller, "controller");
	loadXMLSetting(&EmuSettings.turboModeEnabled, "turboModeEnabled");
	loadXMLSetting(&EmuSettings.turboModeButton, "turboModeButton");
	loadXMLSetting(&EmuSettings.gamepadMenuToggle, "gamepadMenuToggle");
	loadXMLSetting(&EmuSettings.mapAbxyRightStick, "mapAbxyRightStick");

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
	if(!isValidLoadDevice(EmuSettings.loadDevice))
		EmuSettings.loadDevice = DEVICE_AUTO;
	if(!isValidSaveDevice(EmuSettings.saveDevice))
		EmuSettings.saveDevice = DEVICE_AUTO;

	if(strlen(EmuSettings.smbShare.share) == 0 || strlen(EmuSettings.smbShare.host) == 0) {
		if(EmuSettings.loadDevice == DEVICE_SMB) {
			EmuSettings.loadDevice = DEVICE_AUTO;
		}
		if(EmuSettings.saveDevice == DEVICE_SMB) {
			EmuSettings.saveDevice = DEVICE_AUTO;
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
	if(!(EmuSettings.musicVolume >= 0 && EmuSettings.musicVolume <= 100))
		EmuSettings.musicVolume = 20;
	if(!(EmuSettings.sfxVolume >= 0 && EmuSettings.sfxVolume <= 100))
		EmuSettings.sfxVolume = 40;
	if(EmuSettings.language < 0 || EmuSettings.language >= LANG_LENGTH)
		EmuSettings.language = LANG_ENGLISH;
	if(EmuSettings.controller > CTRL_PAD4 || EmuSettings.controller < CTRL_SCOPE)
		EmuSettings.controller = CTRL_PAD2;
	if(!(EmuSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && EmuSettings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
	if(!(EmuSettings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_NONE && EmuSettings.videoAspectRatioCorrection < VIDEO_ASPECT_RATIO_CORRECTION_LENGTH))
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
	if(!(EmuSettings.videoMode >= VIDEOMODE_AUTO && EmuSettings.videoMode < VIDEOMODE_LENGTH))
		EmuSettings.videoMode = VIDEOMODE_AUTO;
#if defined(HW_RVL) || defined(HW_DOL) || defined(__WIIU__)
	if(!(EmuSettings.videoUpscalingFilter >= UPSCALE_NONE && EmuSettings.videoUpscalingFilter <= NUM_UPSCALE_FILTERS))
		EmuSettings.videoUpscalingFilter = UPSCALE_NONE;
#endif
	if(!(EmuSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_VERTICAL && EmuSettings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_VERTICAL;
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

	EmuSettings.loadDevice = DEVICE_AUTO;
	EmuSettings.saveDevice = DEVICE_AUTO;
	sprintf (EmuSettings.loadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (EmuSettings.saveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (EmuSettings.cheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (EmuSettings.screenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (EmuSettings.coverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (EmuSettings.artworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files
	EmuSettings.autoLoad = true;
	EmuSettings.autoSave = true;

	EmuSettings.controller = CTRL_PAD2;

	EmuSettings.videoMode = VIDEOMODE_AUTO;
	EmuSettings.videoBilinearFilter = false;
	EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
	EmuSettings.videoScanlines = false;
#if defined(HW_RVL) || defined(HW_DOL) || defined(__WIIU__)
	EmuSettings.videoUpscalingFilter = UPSCALE_NONE;
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
#elif defined(__WIIU__)
	if (GX2GetSystemTVAspectRatio() == GX2_ASPECT_RATIO_16_9)
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_16_9;
	else
		EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
#endif

	EmuSettings.videoZoomHor = 1.0; // horizontal zoom level
	EmuSettings.videoZoomVert = 1.0; // vertical zoom level
	EmuSettings.videoXshift = 0; // horizontal video shift
	EmuSettings.videoYshift = 0; // vertical video shift
	EmuSettings.crosshair = true;

	EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_VERTICAL;
#ifdef HW_RVL
	EmuSettings.exitAction = EXITACTION_WII_AUTO;
#elif HW_DOL
	EmuSettings.exitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	EmuSettings.autoloadGame = false;
	EmuSettings.musicVolume = 20;
	EmuSettings.sfxVolume = 40;
	EmuSettings.rumble = true;
	EmuSettings.previewImage = PREVIEWIMAGE_COVER;
	EmuSettings.hideSramSaving = false;
	
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
	EmuSettings.muteAudio = false;
	EmuSettings.interpolation = 0;
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
	EmuSettings.hiResolution = true; // Enabled by default
	EmuSettings.spriteLimit = true; // Enabled by default
	EmuSettings.frameSkip = true; // Enabled by default

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

	EmuSettings.turboModeEnabled = true; // Enabled by default
	EmuSettings.turboModeButton = 0; // Default is Right Analog Stick (0)
	EmuSettings.gamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
	EmuSettings.mapAbxyRightStick = false;
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
static uint32_t prefsHash = 0;
static bool prefsHashKnown = false;

static uint32_t HashBytes(const void * data, size_t size)
{
	const uint8_t * bytes = (const uint8_t *)data;
	uint32_t hash = 2166136261u; // FNV-1a
	for(size_t i = 0; i < size; i++)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static bool SavePrefsNow()
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
		device = EmuSettings.saveDevice;

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

	uint32_t hash = HashBytes(savebuffer, datasize);
	if(prefsHashKnown && hash == prefsHash)
	{
		offset = datasize; // unchanged - nothing to write
	}
	else
	{
		offset = SaveFile(filepath, datasize, true);
		prefsHash = hash;
		prefsHashKnown = (offset > 0); // if it failed, try again next time
	}

	FreeSaveBuffer ();

	if (offset > 0)
	{
		if(appPath[0] == 0)
			strcpy(appPath, prefpath);
		return true;
	}
	return false;
}

static int SavePrefsTask(void *)
{
	SavePrefsNow();
	return 0;
}

// Asynchronous and silent: queued for the worker thread, so the caller never
// waits on storage and nothing is shown. Saving again while a save is still
// queued does nothing extra - it will save whatever the settings are by then.
bool SavePrefs()
{
	if(QueueBackgroundTask(SavePrefsTask))
		return true;

	return SavePrefsNow(); // worker unavailable or queue full
}

// For exit: lets any queued save finish, then saves right here.
bool SavePrefsAndWait()
{
	if(!FlushBackgroundTasks(15000)) // don't hang the exit forever on a stalled device
		return false;

	return SavePrefsNow();
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

	if(retval)
	{
		prefsHash = HashBytes(savebuffer, offset);
		prefsHashKnown = true;
	}

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
	RemapUsbPathIfNeeded(EmuSettings.loadFolder, sizeof(EmuSettings.loadFolder));
	RemapUsbPathIfNeeded(EmuSettings.lastFileLoaded, sizeof(EmuSettings.lastFileLoaded));
	RemapUsbPathIfNeeded(EmuSettings.saveFolder, sizeof(EmuSettings.saveFolder));
	RemapUsbPathIfNeeded(EmuSettings.cheatFolder, sizeof(EmuSettings.cheatFolder));
	RemapUsbPathIfNeeded(EmuSettings.screenshotsFolder, sizeof(EmuSettings.screenshotsFolder));
	RemapUsbPathIfNeeded(EmuSettings.coverFolder, sizeof(EmuSettings.coverFolder));
	RemapUsbPathIfNeeded(EmuSettings.artworkFolder, sizeof(EmuSettings.artworkFolder));
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
	if (EmuSettings.saveDevice > DEVICE_AUTO) {
		const char* savePointers[] = { EmuSettings.saveFolder, EmuSettings.cheatFolder };
		bool appFolderChecked = false; // only hit the device once per pass

		for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
			const char* currentPath = savePointers[i];

			if (!appFolderChecked && strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
				CreatePathWithPrefix(EmuSettings.saveDevice, APPFOLDER);
				appFolderChecked = true;
			}

			GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
			if (strcmp(currentPath, defaultFolder) == 0) {
				CreatePathWithPrefix(EmuSettings.saveDevice, currentPath);
			}
		}
	}

	if (EmuSettings.loadDevice > DEVICE_AUTO && EmuSettings.loadDevice != DEVICE_DVD) {
		const char* loadPointers[] = {
			EmuSettings.loadFolder,
			EmuSettings.screenshotsFolder,
			EmuSettings.coverFolder,
			EmuSettings.artworkFolder
		};
		bool appFolderChecked = false; // only hit the device once per pass

		for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
			const char* currentPath = loadPointers[i];
			if (!appFolderChecked && strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
				CreatePathWithPrefix(EmuSettings.loadDevice, APPFOLDER);
				appFolderChecked = true;
			}

			GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
			if (strcmp(currentPath, defaultFolder) == 0) {
				CreatePathWithPrefix(EmuSettings.loadDevice, currentPath);
			}
		}
	}
}

