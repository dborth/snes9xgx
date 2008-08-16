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

#include "mxml.h"

extern unsigned char savebuffer[];
extern int currconfig[4];

// button map configurations
extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

#define PREFS_FILE_NAME "snes9xGx.prf"
#define PREFSVERSTRING "Snes9x GX 005 Prefs"

char prefscomment[2][32] = { {PREFSVERSTRING}, {"Preferences"} };

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ****************************************************************************/

void CreateXmlFile(char* filename)
{
   mxml_node_t *xml;
   mxml_node_t *data;
   mxml_node_t *group;

   xml = mxmlNewXML("1.0");

   data = mxmlNewElement(xml, "screen");

   //Create Some config value
   mxmlElementSetAttr(data, "height","480");

   mxmlElementSetAttr(data, "width","640");

   //Lets do some sub items for funs
   group = mxmlNewElement(data, "properties");
   mxmlElementSetAttr(group, "username", "beardface");

   mxmlElementSetAttr(group, "favorite_food", "dead babies");

   /* now lets save the xml file to a file! */
   FILE *fp;
   fp = fopen(filename, "w");

   mxmlSaveFile(xml, fp, MXML_NO_CALLBACK);

   /*Time to clean up!*/
   fclose(fp);
   mxmlDelete(group);
   mxmlDelete(data);
   mxmlDelete(xml);
}

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ****************************************************************************/

void LoadXmlFile(char* filename)
{
   FILE *fp;
   mxml_node_t *tree;
   mxml_node_t *data;
   mxml_node_t *group;

   /*Load our xml file! */
   fp = fopen(filename, "r");
   tree = mxmlLoadFile(NULL, fp, MXML_NO_CALLBACK);
   fclose(fp);

   /*Load and printf our values! */
   /* As a note, its a good idea to normally check if node* is NULL */
   data = mxmlFindElement(tree, tree, "screen", NULL, NULL, MXML_DESCEND);

   printf("Loaded following values from xml file:\n");
   printf("  Height: %s\n",mxmlElementGetAttr(data,"height"));
   printf("  Width: %s\n",mxmlElementGetAttr(data,"width"));

   group = mxmlFindElement(tree, tree, "properties", NULL, NULL, MXML_DESCEND);
   printf("  %s's favorite food is %s\n",mxmlElementGetAttr(group, "username"), mxmlElementGetAttr(group, "favorite_food"));

   /* Yay Done! Now lets be considerate programmers, and put memory back how
      we found it before we started playing with it...*/
   mxmlDelete(group);
   mxmlDelete(data);
   mxmlDelete(tree);
}

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ****************************************************************************/
int
preparePrefsData ()
{
	int offset = sizeof (saveicon);
	int size;

	memset (savebuffer, 0, SAVEBUFFERSIZE);

	/*** Copy in save icon ***/
	memcpy (savebuffer, saveicon, offset);

	/*** And the prefscomments ***/
	memcpy (savebuffer + offset, prefscomment, 64);
	offset += 64;

	/*** Save all settings ***/
	size = sizeof (Settings);
	memcpy (savebuffer + offset, &Settings, size);
	offset += size;

	/*** Save GC specific settings ***/
	size = sizeof (GCSettings);
	memcpy (savebuffer + offset, &GCSettings, size);
	offset += size;

	/*** Save buttonmaps ***/
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
}


/****************************************************************************
 * Decode Preferences Data
 ****************************************************************************/
bool
decodePrefsData ()
{
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
}

/****************************************************************************
 * Save Preferences
 ****************************************************************************/
bool
SavePrefs (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	bool retval = false;
	char filepath[1024];
	int datasize;
	int offset = 0;

	datasize = preparePrefsData ();

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
		retval = decodePrefsData ();
		if ( !silent )
			WaitPrompt ((char *)"Preferences saved");
	}
	return retval;
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
		retval = decodePrefsData ();
		if ( !silent )
			WaitPrompt((char *)"Preferences loaded");
	}
	return retval;
}
