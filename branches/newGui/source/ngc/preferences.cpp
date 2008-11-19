/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric September 2008
 *
 * preferences.cpp
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <mxml.h>

#include "snes9xGX.h"
#include "images/saveicon.h"
#include "menudraw.h"
#include "memcardop.h"
#include "fileop.h"
#include "smbop.h"
#include "filesel.h"
#include "input.h"

extern int currconfig[4];

#define PREFS_FILE_NAME "SNES9xGX.xml"

char prefscomment[2][32];

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ***************************************************************************/
mxml_node_t *xml;
mxml_node_t *data;
mxml_node_t *section;
mxml_node_t *item;
mxml_node_t *elem;

char temp[20];

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
	for(int i=0; i < MAXJP; i++)
	{
		elem = mxmlNewElement(item, "button");
		mxmlElementSetAttr(elem, "number", toStr(i));
		mxmlElementSetAttr(elem, "assignment", toStr(controller[i]));
	}
}

const char * XMLSaveCallback(mxml_node_t *node, int where)
{
	const char *name;

	name = node->value.element.name;

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
	return (NULL);
}


int
preparePrefsData (int method)
{
	int offset = 0;

	// add save icon and comments for Memory Card saves
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		offset = sizeof (saveicon);

		// Copy in save icon
		memcpy (savebuffer, saveicon, offset);

		// And the comments
		sprintf (prefscomment[0], "%s Prefs", VERSIONSTR);
		sprintf (prefscomment[1], "Preferences");
		memcpy (savebuffer + offset, prefscomment, 64);
		offset += 64;
	}

	xml = mxmlNewXML("1.0");
	mxmlSetWrapMargin(0); // disable line wrapping

	data = mxmlNewElement(xml, "file");
	mxmlElementSetAttr(data, "version",VERSIONSTR);

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
	createXMLSetting("widescreen", "Aspect Ratio Correction", toStr(GCSettings.widescreen));
	createXMLSetting("xshift", "Horizontal Video Shift", toStr(GCSettings.xshift));
	createXMLSetting("yshift", "Vertical Video Shift", toStr(GCSettings.yshift));

	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("MultiTap", "MultiTap", toStr(Settings.MultiPlayer5Master));
	createXMLSetting("Superscope", "Superscope", toStr(GCSettings.Superscope));
	createXMLSetting("Mice", "Mice", toStr(GCSettings.Mouse));
	createXMLSetting("Justifiers", "Justifiers", toStr(GCSettings.Justifier));

	createXMLController(gcpadmap, "gcpadmap", "GameCube Pad");
	createXMLController(wmpadmap, "wmpadmap", "Wiimote");
	createXMLController(ccpadmap, "ccpadmap", "Classic Controller");
	createXMLController(ncpadmap, "ncpadmap", "Nunchuk");

	int datasize = mxmlSaveString(xml, (char *)savebuffer+offset, (SAVEBUFFERSIZE-offset), XMLSaveCallback);

	mxmlDelete(xml);

	return datasize;
}


/****************************************************************************
 * loadXMLSetting
 *
 * Load XML elements into variables for an individual variable
 ***************************************************************************/

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

/****************************************************************************
 * loadXMLController
 *
 * Load XML elements into variables for a controller mapping
 ***************************************************************************/

void loadXMLController(unsigned int controller[], const char * name)
{
	item = mxmlFindElement(xml, xml, "controller", "name", name, MXML_DESCEND);

	if(item)
	{
		// populate buttons
		for(int i=0; i < MAXJP; i++)
		{
			elem = mxmlFindElement(item, xml, "button", "number", toStr(i), MXML_DESCEND);
			if(elem)
				controller[i] = atoi(mxmlElementGetAttr(elem, "assignment"));
		}
	}
}

/****************************************************************************
 * decodePrefsData
 *
 * Decodes preferences - parses XML and loads preferences into the variables
 ***************************************************************************/

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
	loadXMLSetting(&GCSettings.widescreen, "widescreen");
	loadXMLSetting(&GCSettings.xshift, "xshift");
	loadXMLSetting(&GCSettings.yshift, "yshift");

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
}

/****************************************************************************
 * Save Preferences
 ***************************************************************************/
bool
SavePrefs (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	char filepath[1024];
	int datasize;
	int offset = 0;

	AllocSaveBuffer ();
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

	FreeSaveBuffer ();

	if (offset > 0)
	{
		if (!silent)
			WaitPrompt ((char *)"Preferences saved");
		return true;
	}
	return false;
}

/****************************************************************************
 * Load Preferences from specified method
 ***************************************************************************/
bool
LoadPrefsFromMethod (int method)
{
	bool retval = false;
	char filepath[1024];
	int offset = 0;

	AllocSaveBuffer ();

	if(method == METHOD_SD || method == METHOD_USB)
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filepath, "%s/%s/%s", ROOTFATDIR, GCSettings.SaveFolder, PREFS_FILE_NAME);
			offset = LoadBufferFromFAT (filepath, SILENT);
		}
	}
	else if(method == METHOD_SMB)
	{
		sprintf (filepath, "%s/%s", GCSettings.SaveFolder, PREFS_FILE_NAME);
		offset = LoadBufferFromSMB (filepath, SILENT);
	}
	else if(method == METHOD_MC_SLOTA)
	{
		offset = LoadBufferFromMC (savebuffer, CARD_SLOTA, (char *)PREFS_FILE_NAME, SILENT);
	}
	else if(method == METHOD_MC_SLOTB)
	{
		offset = LoadBufferFromMC (savebuffer, CARD_SLOTB, (char *)PREFS_FILE_NAME, SILENT);
	}

	if (offset > 0)
		retval = decodePrefsData (method);

	FreeSaveBuffer ();

	return retval;
}

/****************************************************************************
 * Load Preferences
 * Checks sources consecutively until we find a preference file
 ***************************************************************************/
bool LoadPrefs()
{
	ShowAction ((char*) "Loading preferences...");
	bool prefFound = false;
	if(ChangeFATInterface(METHOD_SD, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_SD);
	if(!prefFound && ChangeFATInterface(METHOD_USB, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_USB);
	if(!prefFound && TestCard(CARD_SLOTA, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_MC_SLOTA);
	if(!prefFound && TestCard(CARD_SLOTB, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_MC_SLOTB);
	if(!prefFound && ConnectShare (SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_SMB);

	return prefFound;
}
