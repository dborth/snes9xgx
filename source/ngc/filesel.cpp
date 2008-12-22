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

#ifdef WII_DVD
extern "C" {
#include <di/di.h>
}
#endif

#include "snes9x.h"
#include "memmap.h"

#include "snes9xGX.h"
#include "dvd.h"
#include "menudraw.h"
#include "video.h"
#include "aram.h"
#include "smbop.h"
#include "fileop.h"
#include "memcardop.h"
#include "input.h"
#include "unzip.h"

int offset;
int selection;
char rootdir[10];
char currentdir[MAXPATHLEN];
char szpath[MAXPATHLEN];
int maxfiles;
extern int screenheight;
unsigned long SNESROMSize = 0;

int havedir = -1;
extern u64 dvddir;
extern int dvddirlength;

// Global file entry table
FILEENTRIES filelist[MAXFILES];
bool inSz = false;

unsigned char *savebuffer = NULL;

/****************************************************************************
 * AllocSaveBuffer ()
 * Allocate and clear the savebuffer
 ***************************************************************************/
void
AllocSaveBuffer ()
{
	if (savebuffer != NULL)
		free(savebuffer);

	savebuffer = (unsigned char *) malloc(SAVEBUFFERSIZE);
	memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
 * FreeSaveBuffer ()
 * Free the savebuffer memory
 ***************************************************************************/
void
FreeSaveBuffer ()
{
	if (savebuffer != NULL)
		free(savebuffer);

	savebuffer = NULL;
}

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
		WaitPrompt("Unable to auto-determine load method!");

	GCSettings.LoadMethod = method; // save method found for later use
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
		WaitPrompt("Unable to auto-determine save method!");

	GCSettings.SaveMethod = method; // save method found for later use
	return method;
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
	{
		dvddir = filelist[selection].offset;
		dvddirlength = filelist[selection].length;
	}

	/* current directory doesn't change */
	if (strcmp(filelist[selection].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(filelist[selection].filename,"..") == 0)
	{
		/* determine last subdirectory namelength */
		sprintf(temp,"%s",currentdir);
		test = strtok(temp,"/");
		while (test != NULL)
		{
			size = strlen(test);
			test = strtok(NULL,"/");
		}

		/* remove last subdirectory name */
		size = strlen(currentdir) - size - 1;
		currentdir[size] = 0;

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(currentdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(currentdir, "%s/%s",currentdir, filelist[selection].filename);
			return 1;
		}
		else
		{
			WaitPrompt("Directory name is too long!");
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
		if ((strlen(currentdir)+1+strlen(filelist[selection].filename)) >= MAXPATHLEN)
		{
			WaitPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s/%s",currentdir,filelist[selection].filename);
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
	if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
	{
		if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((FILEENTRIES *)f1)->flags && !(((FILEENTRIES *)f2)->flags)) return -1;
	if(!(((FILEENTRIES *)f1)->flags) && ((FILEENTRIES *)f2)->flags) return 1;

	return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/

bool IsValidROM(int method)
{
	// file size should be between 96K and 8MB
	if(filelist[selection].length < (1024*96) ||
		filelist[selection].length > (1024*1024*8))
	{
		WaitPrompt("Invalid file size!");
		return false;
	}

	if (strlen(filelist[selection].filename) > 4)
	{
		char * p = strrchr(filelist[selection].filename, '.');

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
	WaitPrompt("Unknown file type!");
	return false;
}

/****************************************************************************
 * IsSz
 *
 * Checks if the specified file is a 7z
 ***************************************************************************/

bool IsSz()
{
	if (strlen(filelist[selection].filename) > 4)
	{
		char * p = strrchr(filelist[selection].filename, '.');

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

	strcpy (returnstring, inputstring);
	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

/****************************************************************************
 * FileSelector
 *
 * Let user select a file from the listing
 ***************************************************************************/
int FileSelector (int method)
{
    u32 p = 0;
	u32 wp = 0;
	u32 ph = 0;
	u32 wh = 0;
    signed char gc_ay = 0;
	signed char gc_sx = 0;
	signed char wm_ay = 0;
	signed char wm_sx = 0;

    int haverom = 0;
    int redraw = 1;
    int selectit = 0;

	int scroll_delay = 0;
	bool move_selection = 0;
	#define SCROLL_INITIAL_DELAY	15
	#define SCROLL_LOOP_DELAY		2

    while (haverom == 0)
    {
        if (redraw)
            ShowFiles (filelist, maxfiles, offset, selection);
        redraw = 0;

		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads

		gc_ay = PAD_StickY (0);
		gc_sx = PAD_SubStickX (0);

        p = PAD_ButtonsDown (0);
		ph = PAD_ButtonsHeld (0);
#ifdef HW_RVL
		wm_ay = WPAD_Stick (0, 0, 1);
		wm_sx = WPAD_Stick (0, 1, 0);

		wp = WPAD_ButtonsDown (0);
		wh = WPAD_ButtonsHeld (0);
#endif

		/*** Check for exit combo ***/
		if ( (gc_sx < -70) || (wm_sx < -70) || (wp & WPAD_BUTTON_HOME) || (wp & WPAD_CLASSIC_BUTTON_HOME) )
			return 0;

		/*** Check buttons, perform actions ***/
		if ( (p & PAD_BUTTON_A) || selectit || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) )
		{
			if ( selectit )
				selectit = 0;
			if (filelist[selection].flags) // This is directory
			{
				/* update current directory and set new entry list if directory has changed */
				int status;

				if(inSz && selection == 0) // inside a 7z, requesting to leave
				{
					if(method == METHOD_DVD)
					{
						// go to directory the 7z was in
						dvddir = filelist[0].offset;
						dvddirlength = filelist[0].length;
					}
					inSz = false;
					status = 1;
					SzClose();
				}
				else
				{
					status = UpdateDirName(method);
				}

				if (status == 1) // ok, open directory
				{
					switch (method)
					{
						case METHOD_DVD:
							maxfiles = ParseDVDdirectory();
							break;

						default:
							maxfiles = ParseDirectory();
							break;
					}

					if (!maxfiles)
					{
						WaitPrompt ("Error reading directory!");
						haverom = 1; // quit menu
					}
				}
				else if (status == -1)	// directory name too long
				{
					return 0; // quit menu
				}
			}
			else	// this is a file
			{
				// 7z file - let's open it up to select a file inside
				if(IsSz())
				{
					// we'll store the 7z filepath for extraction later
					if(!MakeFilePath(szpath, FILE_ROM, method))
						return 0;

					// add device to filepath
					char fullpath[1024];
					sprintf(fullpath, "%s%s", rootdir, szpath);
					strcpy(szpath, fullpath);

					int szfiles = SzParse(szpath, method);
					if(szfiles)
					{
						maxfiles = szfiles;
						inSz = true;
					}
					else
						WaitPrompt("Error opening archive!");
				}
				else
				{
					// check that this is a valid ROM
					if(!IsValidROM(method))
						return 0;

					// store the filename (w/o ext) - used for sram/freeze naming
					StripExt(Memory.ROMFilename, filelist[selection].filename);

					ShowAction ("Loading...");

					SNESROMSize = 0;

					if(!inSz)
					{
						char filepath[1024];

						if(!MakeFilePath(filepath, FILE_ROM, method))
							return 0;

						SNESROMSize = LoadFile ((char *)Memory.ROM, filepath, filelist[selection].length, method, NOTSILENT);
					}
					else
					{
						switch (method)
						{
							case METHOD_DVD:
								SNESROMSize = SzExtractFile(filelist[selection].offset, (unsigned char *)Memory.ROM);
								break;
							default:
								SNESROMSize = LoadSzFile(szpath, (unsigned char *)Memory.ROM);
								break;
						}
					}
					inSz = false;

					if (SNESROMSize > 0)
						return 1;
					else
						WaitPrompt("Error loading ROM!");
				}
			}
			redraw = 1;
		}	// End of A
        if ( (p & PAD_BUTTON_B) || (wp & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)) )
        {
            while ( (PAD_ButtonsDown(0) & PAD_BUTTON_B)
#ifdef HW_RVL
					|| (WPAD_ButtonsDown(0) & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B))
#endif
					)
                VIDEO_WaitVSync();
			if ( strcmp(filelist[0].filename,"..") == 0 )
			{
				selection = 0;
				selectit = 1;
			}
			else if ( strcmp(filelist[1].filename,"..") == 0 )
			{
                selection = selectit = 1;
			}
			else
			{
                return 0;
			}
        }	// End of B
        if ( ((p | ph) & PAD_BUTTON_DOWN) || ((wp | wh) & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (gc_ay < -PADCAL) || (wm_ay < -PADCAL) )
        {
			if ( (p & PAD_BUTTON_DOWN) || (wp & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) ) { /*** Button just pressed ***/
				scroll_delay = SCROLL_INITIAL_DELAY;	// reset scroll delay.
				move_selection = 1;	//continue (move selection)
			}
			else if (scroll_delay == 0) { 		/*** Button is held ***/
				scroll_delay = SCROLL_LOOP_DELAY;
				move_selection = 1;	//continue (move selection)
			} else {
				scroll_delay--;	// wait
			}

			if (move_selection)
			{
	            selection++;
	            if (selection == maxfiles)
	                selection = offset = 0;
	            if ((selection - offset) >= PAGESIZE)
	                offset += PAGESIZE;
	            redraw = 1;
				move_selection = 0;
			}
        }	// End of down
        if ( ((p | ph) & PAD_BUTTON_UP) || ((wp | wh) & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (gc_ay > PADCAL) || (wm_ay > PADCAL) )
        {
			if ( (p & PAD_BUTTON_UP) || (wp & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) ) { /*** Button just pressed***/
				scroll_delay = SCROLL_INITIAL_DELAY;	// reset scroll delay.
				move_selection = 1;	//continue (move selection)
			}
			else if (scroll_delay == 0) { 		/*** Button is held ***/
				scroll_delay = SCROLL_LOOP_DELAY;
				move_selection = 1;	//continue (move selection)
			} else {
				scroll_delay--;	// wait
			}

			if (move_selection)
			{
	            selection--;
	            if (selection < 0) {
	                selection = maxfiles - 1;
	                offset = selection - PAGESIZE + 1;
	            }
	            if (selection < offset)
	                offset -= PAGESIZE;
	            if (offset < 0)
	                offset = 0;
	            redraw = 1;
				move_selection = 0;
			}
        }	// End of Up
        if ( (p & PAD_BUTTON_LEFT) || (wp & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)) )
        {
            /*** Go back a page ***/
            selection -= PAGESIZE;
            if (selection < 0)
            {
                selection = maxfiles - 1;
                offset = selection - PAGESIZE + 1;
            }
            if (selection < offset)
                offset -= PAGESIZE;
            if (offset < 0)
                offset = 0;
            redraw = 1;
        }
        if ( (p & PAD_BUTTON_RIGHT) || (wp & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)) )
        {
            /*** Go forward a page ***/
            selection += PAGESIZE;
            if (selection > maxfiles - 1)
                selection = offset = 0;
            if ((selection - offset) >= PAGESIZE)
                offset += PAGESIZE;
            redraw = 1;
        }
    }
    return 0;
}

/****************************************************************************
 * OpenROM
 * Opens device specified by method, displays a list of ROMS
 ***************************************************************************/

int
OpenROM (int method)
{
	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	if(ChangeInterface(method, NOTSILENT))
	{
		// change current dir to roms directory
		switch(method)
		{
			case METHOD_DVD:
				currentdir[0] = 0;
				maxfiles = ParseDVDdirectory (); // Parse root directory
				SwitchDVDFolder(GCSettings.LoadFolder); // switch to ROM folder
				break;
			default:
				sprintf(currentdir, "/%s", GCSettings.LoadFolder);
				maxfiles = ParseDirectory(); // Parse root directory
				break;
		}

		if (maxfiles > 0)
		{
			// Select an entry
			return FileSelector (method);
		}
		else
		{
			// no entries found
			WaitPrompt ("No Files Found!");
			return 0;
		}
	}
	return 0;
}
