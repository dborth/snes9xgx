/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * svpe June 2007
 * crunchy2 May-July 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * filesel.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <malloc.h>

#ifdef HW_RVL
extern "C" {
#include <di/di.h>
}
#endif

#include "snes9x.h"
#include "memmap.h"

#include "filebrowser.h"
#include "snes9xGX.h"
#include "dvd.h"
#include "menu.h"
#include "video.h"
#include "aram.h"
#include "networkop.h"
#include "fileop.h"
#include "memcardop.h"
#include "input.h"
#include "gcunzip.h"
#include "cheatmgr.h"
#include "patch.h"
#include "freeze.h"
#include "sram.h"

BROWSERINFO browser;
BROWSERENTRY * browserList = NULL; // list of files/folders in browser

char rootdir[10];
static char szpath[MAXPATHLEN];
static bool inSz = false;

unsigned long SNESROMSize = 0;

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load method
* Returns method set
****************************************************************************/
int autoLoadMethod()
{
	ShowAction ("Attempting to determine load method...");

	int method = 0;

	if(ChangeInterface(METHOD_SD, SILENT))
		method = METHOD_SD;
	else if(ChangeInterface(METHOD_USB, SILENT))
		method = METHOD_USB;
	else if(ChangeInterface(METHOD_DVD, SILENT))
		method = METHOD_DVD;
	else if(ChangeInterface(METHOD_SMB, SILENT))
		method = METHOD_SMB;
	else
		ErrorPrompt("Unable to auto-determine load method!");

	if(GCSettings.LoadMethod == METHOD_AUTO)
		GCSettings.LoadMethod = method; // save method found for later use
	CancelAction();
	return method;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save method
* Returns method set
****************************************************************************/
int autoSaveMethod(bool silent)
{
	if(!silent)
		ShowAction ("Attempting to determine save method...");

	int method = 0;

	if(ChangeInterface(METHOD_SD, SILENT))
		method = METHOD_SD;
	else if(ChangeInterface(METHOD_USB, SILENT))
		method = METHOD_USB;
	else if(TestCard(CARD_SLOTA, SILENT))
		method = METHOD_MC_SLOTA;
	else if(TestCard(CARD_SLOTB, SILENT))
		method = METHOD_MC_SLOTB;
	else if(ChangeInterface(METHOD_SMB, SILENT))
		method = METHOD_SMB;
	else if(!silent)
		ErrorPrompt("Unable to auto-determine save method!");

	if(GCSettings.SaveMethod == METHOD_AUTO)
		GCSettings.SaveMethod = method; // save method found for later use

	CancelAction();
	return method;
}

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser()
{
	browser.selIndex = browser.pageIndex = 0;

	// Clear any existing values
	if(browserList != NULL)
	{
		free(browserList);
		browserList = NULL;
	}
	// set aside space for 1 entry
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY));
	memset(browserList, 0, sizeof(BROWSERENTRY));
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName(int method)
{
	int size=0;
	char * test;
	char temp[1024];

	// update DVD directory
	if(method == METHOD_DVD)
		SetDVDdirectory(browserList[browser.selIndex].offset, browserList[browser.selIndex].length);

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
	{
		/* determine last subdirectory namelength */
		sprintf(temp,"%s",browser.dir);
		test = strtok(temp,"/");
		while (test != NULL)
		{
			size = strlen(test);
			test = strtok(NULL,"/");
		}

		/* remove last subdirectory name */
		size = strlen(browser.dir) - size - 1;
		browser.dir[size] = 0;

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser.dir, "%s/%s",browser.dir, browserList[browser.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

bool MakeFilePath(char filepath[], int type, int method)
{
	char file[512];
	char folder[1024];
	char temppath[MAXPATHLEN];

	if(type == FILE_ROM)
	{
		// Check path length
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) >= MAXPATHLEN)
		{
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s/%s",browser.dir,browserList[browser.selIndex].filename);
		}
	}
	else
	{
		switch(type)
		{
			case FILE_SRAM:
				sprintf(folder, GCSettings.SaveFolder);
				sprintf(file, "%s.srm", Memory.ROMFilename);
				break;
			case FILE_SNAPSHOT:
				sprintf(folder, GCSettings.SaveFolder);
				sprintf(file, "%s.frz", Memory.ROMFilename);
				break;
			case FILE_CHEAT:
				sprintf(folder, GCSettings.CheatFolder);
				sprintf(file, "%s.cht", Memory.ROMFilename);
				break;
			case FILE_PREF:
				sprintf(folder, appPath);
				sprintf(file, "%s", PREF_FILE_NAME);
				break;
			case FILE_SCREEN:
				sprintf(folder, GCSettings.SaveFolder);	// screenshot dir?
				sprintf(file, "%s.png", Memory.ROMFilename);
				break;
		}
		switch(method)
		{
			case METHOD_MC_SLOTA:
			case METHOD_MC_SLOTB:
				sprintf (temppath, "%s", file);
				temppath[31] = 0; // truncate filename
				break;
			default:
				sprintf (temppath, "%s/%s", folder, file);
				break;
		}
	}
	strcpy(filepath, temppath);
	return true;
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY *)f1)->filename[0] == '.' || ((BROWSERENTRY *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY *)f1)->isdir && !(((BROWSERENTRY *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY *)f1)->isdir) && ((BROWSERENTRY *)f2)->isdir) return 1;

	return stricmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/

static bool IsValidROM(int method)
{
	// file size should be between 96K and 8MB
	if(browserList[browser.selIndex].length < (1024*96) ||
		browserList[browser.selIndex].length > (1024*1024*8))
	{
		ErrorPrompt("Invalid file size!");
		return false;
	}

	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != NULL)
		{
			if(stricmp(p, ".zip") == 0 && !inSz)
			{
				// we need to check the file extension of the first file in the archive
				char * zippedFilename = GetFirstZipFilename (method);

				if(zippedFilename == NULL) // we don't want to run strlen on NULL
					p = NULL;
				else if(strlen(zippedFilename) > 4)
					p = strrchr(zippedFilename, '.');
				else
					p = NULL;
			}

			if(p != NULL)
			{
				if (stricmp(p, ".smc") == 0 ||
					stricmp(p, ".fig") == 0 ||
					stricmp(p, ".sfc") == 0 ||
					stricmp(p, ".swc") == 0)
				{
					return true;
				}
			}
		}
	}
	ErrorPrompt("Unknown file type!");
	return false;
}

/****************************************************************************
 * IsSz
 *
 * Checks if the specified file is a 7z
 ***************************************************************************/

bool IsSz()
{
	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != NULL)
			if(stricmp(p, ".7z") == 0)
				return true;
	}
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/

void StripExt(char* returnstring, char * inputstring)
{
	char* loc_dot;

	strncpy (returnstring, inputstring, 255);

	if(inputstring == NULL || strlen(inputstring) < 4)
		return;

	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

// 7z file - let's open it up to select a file inside
int BrowserLoadSz(int method)
{
	char filepath[1024];

	// we'll store the 7z filepath for extraction later
	if(!MakeFilePath(szpath, FILE_ROM, method))
		return 0;

	// add device to filepath
	sprintf(filepath, "%s%s", rootdir, szpath);
	strcpy(szpath, filepath);

	int szfiles = SzParse(szpath, method);
	if(szfiles)
	{
		browser.numEntries = szfiles;
		inSz = true;
	}
	else
		ErrorPrompt("Error opening archive!");

	return szfiles;
}

int BrowserLoadFile(int method)
{
	char filepath[1024];
	int loaded = 0;

	// check that this is a valid ROM
	if(!IsValidROM(method))
		goto done;

	// store the filename (w/o ext) - used for sram/freeze naming
	StripExt(Memory.ROMFilename, browserList[browser.selIndex].filename);

	SNESROMSize = 0;

	if(!inSz)
	{
		if(!MakeFilePath(filepath, FILE_ROM, method))
			goto done;

		SNESROMSize = LoadFile ((char *)Memory.ROM, filepath, browserList[browser.selIndex].length, method, NOTSILENT);
	}
	else
	{
		switch (method)
		{
			case METHOD_DVD:
				SNESROMSize = SzExtractFile(browserList[browser.selIndex].offset, (unsigned char *)Memory.ROM);
				break;
			default:
				SNESROMSize = LoadSzFile(szpath, (unsigned char *)Memory.ROM);
				break;
		}
	}
	inSz = false;

	if (SNESROMSize <= 0)
		ErrorPrompt("Error loading ROM!");
	else
	{
		ResetBrowser();

		// load UPS/IPS/PPF patch
		LoadPatch(GCSettings.LoadMethod);

		Memory.LoadROM ("BLANK.SMC");
		Memory.LoadSRAM ("BLANK");

		// load SRAM or snapshot
		if ( GCSettings.AutoLoad == 1 )
			LoadSRAMAuto(GCSettings.SaveMethod, SILENT);
		else if ( GCSettings.AutoLoad == 2 )
			NGCUnfreezeGameAuto(GCSettings.SaveMethod, SILENT);

		// setup cheats
		SetupCheats();

		loaded = 1;
	}
done:
	CancelAction();
	return loaded;
}

/* update current directory and set new entry list if directory has changed */
int BrowserChangeFolder(int method)
{
	if(inSz && browser.selIndex == 0) // inside a 7z, requesting to leave
	{
		if(method == METHOD_DVD)
			SetDVDdirectory(browserList[0].offset, browserList[0].length);

		inSz = false;
		SzClose();
	}

	if(!UpdateDirName(method))
		return -1;

	switch (method)
	{
		case METHOD_DVD:
			browser.numEntries = ParseDVDdirectory();
			break;

		default:
			browser.numEntries = ParseDirectory();
			break;
	}

	if (!browser.numEntries)
	{
		ErrorPrompt("Error reading directory!");
	}

	return browser.numEntries;
}

/****************************************************************************
 * OpenROM
 * Opens device specified by method, displays a list of ROMS
 ***************************************************************************/

int
OpenGameList (int method)
{
	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	ResetBrowser();

	if(ChangeInterface(method, NOTSILENT))
	{
		// change current dir to roms directory
		switch(method)
		{
			case METHOD_DVD:
				browser.dir[0] = 0;
				browser.numEntries = ParseDVDdirectory(); // Parse root directory
				SwitchDVDFolder(GCSettings.LoadFolder); // switch to ROM folder
				break;
			default:
				sprintf(browser.dir, "/%s", GCSettings.LoadFolder);
				browser.numEntries = ParseDirectory(); // Parse root directory
				break;
		}
	}
	return browser.numEntries;
}
