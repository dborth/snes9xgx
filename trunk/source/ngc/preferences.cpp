/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * crunchy2 April 2007-July 2007
 *
 * preferences.cpp
 *
 * Preferences save/load preferences utilities
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include "mxml.h"

#include "snes9x.h"
#include "memmap.h"
#include "srtc.h"

#include "snes9xGx.h"
#include "images/saveicon.h"
#include "menudraw.h"
#include "memcardop.h"
#include "fileop.h"
#include "smbop.h"
#include "filesel.h"

extern unsigned char savebuffer[];
extern int currconfig[4];

// button map configurations
extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

#define PREFS_FILE_NAME "SNES9xGX.xml"
#define PREFSVERSTRING "Snes9x GX 005 Prefs"
#define VERSIONSTRING "005"

char prefscomment[2][32] = { {PREFSVERSTRING}, {"Preferences"} };

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ****************************************************************************/
mxml_node_t *xml;
mxml_node_t *data;
mxml_node_t *section;
mxml_node_t *item;
mxml_node_t *elem;

char temp[200];

const char * toStr(int i)
{
	sprintf(temp, "%d", i);
	return temp;
}

void createXMLSection(const char * name, const char * description)
{
	section = mxmlNewElement(data, "section");
	mxmlElementSetAttr(section, "name", name);
	mxmlElementSetAttr(section, "description", description);
}

void createXMLSetting(const char * name, const char * description, const char * value)
{
	item = mxmlNewElement(section, "setting");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "value", value);
	mxmlElementSetAttr(item, "description", description);
}

void createXMLController(unsigned int controller[], const char * name, const char * description)
{
	item = mxmlNewElement(section, "controller");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "description", description);

	// create buttons
	for(int i=0; i < 12; i++)
	{
		elem = mxmlNewElement(item, "button");
		mxmlElementSetAttr(elem, "number", toStr(i));
		mxmlElementSetAttr(elem, "assignment", toStr(controller[i]));
	}
}

int
preparePrefsData (int method)
{
	int offset = 0;
	memset (savebuffer, 0, SAVEBUFFERSIZE);

	// add save icon and comments for Memory Card saves
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		offset = sizeof (saveicon);

		// Copy in save icon
		memcpy (savebuffer, saveicon, offset);

		// And the comments
		memcpy (savebuffer + offset, prefscomment, 64);
		offset += 64;
	}

	xml = mxmlNewXML("1.0");

	data = mxmlNewElement(xml, "file");
	mxmlElementSetAttr(data, "version",VERSIONSTRING);

	createXMLSection("File", "File Settings");

	createXMLSetting("AutoLoad", "Auto Load", toStr(GCSettings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(GCSettings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(GCSettings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(GCSettings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", GCSettings.LoadFolder);
	createXMLSetting("SaveFolder", "Save Folder", GCSettings.SaveFolder);
	createXMLSetting("CheatFolder", "Cheats Folder", GCSettings.CheatFolder);
	createXMLSetting("VerifySaves", "Verify Memory Card Saves", toStr(GCSettings.VerifySaves));

	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", GCSettings.smbip);
	createXMLSetting("smbshare", "Share Name", GCSettings.smbshare);
	createXMLSetting("smbuser", "Share Username", GCSettings.smbuser);
	createXMLSetting("smbpwd", "Share Password", GCSettings.smbpwd);

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("ReverseStereo", "Reverse Stereo", toStr(Settings.ReverseStereo));
	createXMLSetting("InterpolatedSound", "Interpolated Sound", toStr(Settings.InterpolatedSound));
	createXMLSetting("Transparency", "Transparency", toStr(Settings.Transparency));
	createXMLSetting("DisplayFrameRate", "Display Frame Rate", toStr(Settings.DisplayFrameRate));
	createXMLSetting("NGCZoom", "C-Stick Zoom", toStr(GCSettings.NGCZoom));
	createXMLSetting("render", "Video Filtering", toStr(GCSettings.render));

	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("MultiTap", "MultiTap", toStr(Settings.MultiPlayer5Master));
	createXMLSetting("Superscope", "Superscope", toStr(GCSettings.Superscope));
	createXMLSetting("Mice", "Mice", toStr(GCSettings.Mouse));
	createXMLSetting("Justifiers", "Justifiers", toStr(GCSettings.Justifier));

	createXMLController(gcpadmap, "gcpadmap", "GameCube Pad");
	createXMLController(wmpadmap, "wmpadmap", "Wiimote");
	createXMLController(ccpadmap, "ccpadmap", "Classic Controller");
	createXMLController(ncpadmap, "ncpadmap", "Nunchuk");

	memset (savebuffer + offset, 0, SAVEBUFFERSIZE);
	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, MXML_NO_CALLBACK);

	mxmlDelete(xml);

	return datasize;

	/*
	int offset = sizeof (saveicon);
	int size;

	memset (savebuffer, 0, SAVEBUFFERSIZE);

	// Copy in save icon
	memcpy (savebuffer, saveicon, offset);

	// And the prefscomments
	memcpy (savebuffer + offset, prefscomment, 64);
	offset += 64;

	// Save all settings
	size = sizeof (Settings);
	memcpy (savebuffer + offset, &Settings, size);
	offset += size;

	// Save GC specific settings
	size = sizeof (GCSettings);
	memcpy (savebuffer + offset, &GCSettings, size);
	offset += size;

	// Save buttonmaps
	size = sizeof (unsigned int) *12;	// this size applies to all padmaps
	memcpy (savebuffer + offset, &gcpadmap, size);
	offset += size;
	memcpy (savebuffer + offset, &wmpadmap, size);
	offset += size;
	memcpy (savebuffer + offset, &ccpadmap, size);
	offset += size;
	memcpy (savebuffer + offset, &ncpadmap, size);
	offset += size;

	return offset;
	*/
}


/****************************************************************************
 * Decode Preferences Data
 ****************************************************************************/
void loadXMLSetting(char * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
		sprintf(var, "%s", mxmlElementGetAttr(item, "value"));
}
void loadXMLSetting(int * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
		*var = atoi(mxmlElementGetAttr(item, "value"));
}
void loadXMLSetting(bool8 * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
		*var = atoi(mxmlElementGetAttr(item, "value"));
}

void loadXMLController(unsigned int controller[], const char * name)
{
	item = mxmlFindElement(xml, xml, "controller", "name", name, MXML_DESCEND);

	if(item)
	{
		WaitPrompt((char *)name);
		// populate buttons
		for(int i=0; i < 12; i++)
		{
			elem = mxmlFindElement(item, xml, "button", "number", toStr(i), MXML_DESCEND);
			if(elem)
				controller[i] = atoi(mxmlElementGetAttr(elem, "assignment"));
		}
	}
}

bool
decodePrefsData (int method)
{
	int offset = 0;

	// skip save icon and comments for Memory Card saves
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		offset = sizeof (saveicon);
		offset += 64; // sizeof prefscomment
	}

	xml = mxmlLoadString(NULL, (char *)savebuffer+offset, MXML_TEXT_CALLBACK);

	// check settings version
	// we don't do anything with the version #, but we'll store it anyway
	char * version;
	item = mxmlFindElement(xml, xml, "file", "version", NULL, MXML_DESCEND);
	if(item) // a version entry exists
		version = (char *)mxmlElementGetAttr(item, "version");
	else // version # not found, must be invalid
		return false;

	// File Settings

	loadXMLSetting(&GCSettings.AutoLoad, "AutoLoad");
	loadXMLSetting(&GCSettings.AutoSave, "AutoSave");
	loadXMLSetting(&GCSettings.LoadMethod, "LoadMethod");
	loadXMLSetting(&GCSettings.SaveMethod, "SaveMethod");
	loadXMLSetting(GCSettings.LoadFolder, "LoadFolder");
	loadXMLSetting(GCSettings.SaveFolder, "SaveFolder");
	loadXMLSetting(GCSettings.CheatFolder, "CheatFolder");
	loadXMLSetting(&GCSettings.VerifySaves, "VerifySaves");

	// Network Settings

	loadXMLSetting(GCSettings.smbip, "smbip");
	loadXMLSetting(GCSettings.smbshare, "smbshare");
	loadXMLSetting(GCSettings.smbuser, "smbuser");
	loadXMLSetting(GCSettings.smbpwd, "smbpwd");

	// Emulation Settings

	loadXMLSetting(&Settings.ReverseStereo, "ReverseStereo");
	loadXMLSetting(&Settings.InterpolatedSound, "InterpolatedSound");
	loadXMLSetting(&Settings.Transparency, "Transparency");
	loadXMLSetting(&Settings.DisplayFrameRate, "DisplayFrameRate");
	loadXMLSetting(&GCSettings.NGCZoom, "NGCZoom");
	loadXMLSetting(&GCSettings.render, "render");

	// Controller Settings

	loadXMLSetting(&Settings.MultiPlayer5Master, "MultiTap");
	loadXMLSetting(&GCSettings.Superscope, "Superscope");
	loadXMLSetting(&GCSettings.Mouse, "Mice");
	loadXMLSetting(&GCSettings.Justifier, "Justifiers");

	loadXMLController(gcpadmap, "gcpadmap");
	loadXMLController(wmpadmap, "wmpadmap");
	loadXMLController(ccpadmap, "ccpadmap");
	loadXMLController(ncpadmap, "ncpadmap");

	mxmlDelete(xml);

	return true;

	/*
	int offset;
	char prefscomment[32];
	int size;

	offset = sizeof (saveicon);
	memcpy (prefscomment, savebuffer + offset, 32);

	if ( strcmp (prefscomment, PREFSVERSTRING) == 0 )
	{
		offset += 64;
		memcpy (&Settings, savebuffer + offset, sizeof (Settings));
		offset += sizeof (Settings);
		memcpy (&GCSettings, savebuffer + offset, sizeof (GCSettings));
		offset += sizeof (GCSettings);
		// load padmaps (order important)
		size = sizeof (unsigned int) *12;
		memcpy (&gcpadmap, savebuffer + offset, size);
		offset += size;
		memcpy (&wmpadmap, savebuffer + offset, size);
		offset += size;
		memcpy (&ccpadmap, savebuffer + offset, size);
		offset += size;
		memcpy (&ncpadmap, savebuffer + offset, size);

		return true;
	}
	else
		return false;
	*/
}

/****************************************************************************
 * Save Preferences
 ****************************************************************************/
bool
SavePrefs (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	char filepath[1024];
	int datasize;
	int offset = 0;

	datasize = preparePrefsData (method);

	if (!silent)
		ShowAction ((char*) "Saving preferences...");

	if(method == METHOD_SD || method == METHOD_USB)
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filepath, "%s/%s/%s", ROOTFATDIR, GCSettings.SaveFolder, PREFS_FILE_NAME);
			offset = SaveBufferToFAT (filepath, datasize, silent);
		}
	}
	else if(method == METHOD_SMB)
	{
		sprintf (filepath, "%s/%s", GCSettings.SaveFolder, PREFS_FILE_NAME);
		offset = SaveBufferToSMB (filepath, datasize, silent);
	}
	else if(method == METHOD_MC_SLOTA)
	{
		offset = SaveBufferToMC (savebuffer, CARD_SLOTA, (char *)PREFS_FILE_NAME, datasize, silent);
	}
	else if(method == METHOD_MC_SLOTB)
	{
		offset = SaveBufferToMC (savebuffer, CARD_SLOTB, (char *)PREFS_FILE_NAME, datasize, silent);
	}

	if (offset > 0)
	{
		if (!silent)
			WaitPrompt ((char *)"Preferences saved");
		return true;
	}
	return false;
}

/****************************************************************************
 * Load Preferences
 ****************************************************************************/
bool
LoadPrefs (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' folder because preferences need R/W

	bool retval = false;
	char filepath[1024];
	int offset = 0;

	if ( !silent )
		ShowAction ((char*) "Loading preferences...");

	if(method == METHOD_SD || method == METHOD_USB)
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filepath, "%s/%s/%s", ROOTFATDIR, GCSettings.SaveFolder, PREFS_FILE_NAME);
			offset = LoadBufferFromFAT (filepath, silent);
		}
	}
	else if(method == METHOD_SMB)
	{
		sprintf (filepath, "%s/%s", GCSettings.SaveFolder, PREFS_FILE_NAME);
		offset = LoadBufferFromSMB (filepath, silent);
	}
	else if(method == METHOD_MC_SLOTA)
	{
		offset = LoadBufferFromMC (savebuffer, CARD_SLOTA, (char *)PREFS_FILE_NAME, silent);
	}
	else if(method == METHOD_MC_SLOTB)
	{
		offset = LoadBufferFromMC (savebuffer, CARD_SLOTB, (char *)PREFS_FILE_NAME, silent);
	}

	if (offset > 0)
	{
		retval = decodePrefsData (method);
		if ( !silent )
			WaitPrompt((char *)"Preferences loaded");
	}
	return retval;
}
