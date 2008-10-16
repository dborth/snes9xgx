/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Filesel - borrowed from GPP
 *
 * softdev July 2006
 * svpe June 2007
 * crunchy2 May-July 2007
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "snes9x.h"
#include "memmap.h"
#include "debug.h"
#include "cpuexec.h"
#include "ppu.h"
#include "apu.h"
#include "display.h"
#include "gfx.h"
#include "soundux.h"
#include "spc700.h"
#include "spc7110.h"
#include "controls.h"

#include "snes9xGx.h"
#include "dvd.h"
#include "ftfont.h"
#include "video.h"
#include "aram.h"
#include "unzip.h"
#include "filesel.h"
#include "smbload.h"
#include "sdload.h"
#include "mcsave.h"

#define PAGESIZE 17
int maxfiles;
int havedir = 0;
int hasloaded = 0;
int loadtype = 0;
int LoadDVDFile (unsigned char *buffer);
int haveSDdir = 0;
extern unsigned long ARAM_ROMSIZE;
extern int screenheight;

extern FILEENTRIES filelist[MAXFILES];

/**
 * Showfile screen
 *
 * Display the file selection to the user
 */
static void
ShowFiles (int offset, int selection)
{
  int i, j;
  char text[MAXPATHLEN];
  int ypos;
  int w;
  
  setfontsize(18);
  clearscreen ();

  ypos = (screenheight - ((PAGESIZE - 1) * 20)) >> 1;

  if (screenheight == 480)
    ypos += 24;
  else
    ypos += 10;


  j = 0;
  for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)

    {
      if (filelist[i].flags)	// if a dir

        {
          strcpy (text, "[");
          strcat (text, filelist[i].displayname);
          strcat (text, "]");
        }

      else
        strcpy (text, filelist[i].displayname);
      if (j == (selection - offset))

        {

                        /*** Highlighted text entry ***/
          for ( w = 0; w < 20; w++ )
                DrawLineFast( 30, 610, ( j * 20 ) + (ypos-16) + w, 0x80, 0x80, 0x80 );

          setfontcolour (0x00, 0x00, 0xe0);
          DrawText (-1, (j * 20) + ypos, text);
          setfontcolour (0x00, 0x00, 0x00);
        }

      else

        {
                        /*** Normal entry ***/
          DrawText (-1, (j * 20) + ypos, text);
        }
      j++;
    }
  showscreen ();

}

/**
* SNESROMSOffset
*
* Function to check for and return offset to a directory called SNESROMS, if
* any
*/
int SNESROMSOffset()
{
    int i;
    
    for ( i = 0; i < maxfiles; i++ )
        if (strcmp(filelist[i].filename, "SNESROMS") == 0)
            return i;
    return 0;
}

/**
 * FileSelector
 *
 * Let user select a file from the DVD listing
 */
int offset = 0;
int selection = 0;

#define PADCAL 40
int
FileSelector ()
{
    u32 p, wp, ph, wh;
    signed char a, c;
    int haverom = 0;
    int redraw = 1;
    int selectit = 0;
	float mag = 0;
	u16 ang = 0;
	int scroll_delay = 0;
	bool move_selection = 0;
	#define SCROLL_INITIAL_DELAY	15
	#define SCROLL_LOOP_DELAY		4
    
    while (haverom == 0)    
    {
        if (redraw)
            ShowFiles (offset, selection);
        redraw = 0;

		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads
		
        p = PAD_ButtonsDown (0);
		ph = PAD_ButtonsHeld (0);
#ifdef HW_RVL
		wp = WPAD_ButtonsDown (0);
		wh = WPAD_ButtonsHeld (0);
		wpad_get_analogues(0, &mag, &ang);		// get joystick info from wii expansions
#else
		wp = 0;
		wh = 0;
#endif
		a = PAD_StickY (0);
		c = PAD_SubStickX (0);
        
		/*** Check for exit combo ***/
		if ( (c < -70) || (wp & WPAD_BUTTON_HOME) || (wp & WPAD_CLASSIC_BUTTON_HOME) ) return 0;
		
		/*** Check buttons, perform actions ***/
        if ( (p & PAD_BUTTON_A) || selectit || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) )
        {
            if ( selectit )
                selectit = 0;
            if (filelist[selection].flags)                        /*** This is directory ***/
            {
                if (loadtype == LOAD_SDC)
                {
                    /* memorize last entries list, actual root directory and selection for next access */
                    haveSDdir = 1;
                    
                    /* update current directory and set new entry list if directory has changed */
                    int status = updateSDdirname();
                    if (status == 1)	// ok, open directory
                    {
                        maxfiles = parseSDdirectory();
                        if (!maxfiles)
                        {
                            WaitPrompt ((char*) "Error reading directory !");
                            haverom   = 1; // quit SD menu
                            haveSDdir = 0; // reset everything at next access
                        }
                    }
                    else if (status == -1)	// directory name too long
                    {
                        haverom   = 1; // quit SD menu
                        haveSDdir = 0; // reset everything at next access
                    }
                }
                else
                {
                    if ( (strcmp (filelist[selection].filename, "..") == 0)
                        &&  ((unsigned int)rootdir == filelist[selection].offset) )
                        return 0;
                    else
                    {
                        rootdir = filelist[selection].offset;
                        rootdirlength = filelist[selection].length;
                        offset = selection = 0;
                        maxfiles = parsedirectory ();
                    }
                }
            }
            else	// this is a file
            {
                rootdir = filelist[selection].offset;
                rootdirlength = filelist[selection].length;
                
                switch (loadtype)
                {
                    case LOAD_DVD:
                        /*** Now load the DVD file to it's offset ***/
                        ARAM_ROMSIZE = LoadDVDFile (Memory.ROM);
                        break;
                    
                    case LOAD_SMB:
                        /*** Load from SMB ***/
                        ARAM_ROMSIZE =
                        LoadSMBFile (filelist[selection].filename,
                             filelist[selection].length);
                        break;
                    
                    case LOAD_SDC:
                        /*** Load from SD Card ***/
                        /* memorize last entries list, actual root directory and selection for next access */
                        haveSDdir = 1;
                        ARAM_ROMSIZE = LoadSDFile (filelist[selection].filename,
                                         filelist[selection].length);
                        break;
                }
                
                if (ARAM_ROMSIZE > 0)
                {
                    hasloaded = 1;
                    Memory.LoadROM ("BLANK.SMC");
                
                    Memory.LoadSRAM ("BLANK");
                    haverom = 1;
                    
                    return 1;
                }
                else
                {
                    WaitPrompt((char*) "Error loading ROM!");
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
            //if ((strcmp(filelist[1].filename,"..") == 0) && (strlen (filelist[0].filename) != 0))
			if ( strcmp(filelist[0].filename,"..") == 0 ) 
			{
				selection = 0;
				selectit = 1;
			}
			else if ( strcmp(filelist[1].filename,"..") == 0 ) 
			{
                selection = selectit = 1;
			} else {
                return 0;
			}
        }	// End of B
        if ( ((p | ph) & PAD_BUTTON_DOWN) || ((wp | wh) & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (a < -PADCAL) || (mag>JOY_THRESHOLD && (ang>130 && ang<230)) )
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
        if ( ((p | ph) & PAD_BUTTON_UP) || ((wp | wh) & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (a > PADCAL) || (mag>JOY_THRESHOLD && (ang>300 || ang<50)) )
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

/**
 * OpenDVD
 *
 * Function to load a DVD directory and display to user.
 */
int
OpenDVD ()
{
    int romsdiroffset = 0;
    
    loadtype = LOAD_DVD;
    
    if (!getpvd())
    {
        ShowAction((char*) "Mounting DVD ... Wait");
        DVD_Mount();             /* mount the DVD unit again */
        havedir = 0;             /* this may be a new DVD: content need to be parsed again */
        if (!getpvd())
            return 0; /* no correct ISO9660 DVD */
    }
    
    if (havedir == 0)
    {
        offset = selection = 0; /* reset file selector */
        haveSDdir = 0;  /* prevent conflicts with SDCARD file selector */
        
        if ((maxfiles = parsedirectory ()))
        {
            if ( romsdiroffset = SNESROMSOffset() )
            {
                rootdir = filelist[romsdiroffset].offset;
                rootdirlength = filelist[romsdiroffset].length;
                offset = selection = 0;
                maxfiles = parsedirectory ();
            }
            
            int ret = FileSelector ();
            havedir = 1;
            return ret;
        }
    }
    
    else
        return FileSelector ();
    
    return 0;
}

/**
 * OpenSMB
 *
 * Function to load from an SMB share
 */
int
OpenSMB ()
{
    loadtype = LOAD_SMB;
    
    if ((maxfiles = parseSMBDirectory ()))
    {
        char txt[80];
        sprintf(txt,"maxfiles = %d", maxfiles);
        
        return FileSelector ();
    }
    return 0;
}

/**
 * OpenSD
 *
 * Function to load from an SD Card
 */
int
OpenSD ()
{
    char msg[80];
	char buf[50] = "";
    
    loadtype = LOAD_SDC;
    
    if (haveSDdir == 0)
    {
        /* don't mess with DVD entries */
        havedir = 0;	// gamecube only
        
        /* change current dir to snes roms directory */
        sprintf ( currSDdir, "%s/%s", ROOTSDDIR, SNESROMDIR );
		
        
        /* Parse initial root directory and get entries list */
        if ((maxfiles = parseSDdirectory ()))
        {
            /* Select an entry */
            return FileSelector ();
        }
        else
        {
            /* no entries found */
            sprintf (msg, "No Files Found!");
            WaitPrompt (msg);
            return 0;
        }
    }
    /* Retrieve previous entries list and made a new selection */
    else
        return FileSelector ();
    
    return 0;
}


/**
 * LoadDVDFile
 *
 * This function will load a file from DVD, in BIN, SMD or ZIP format.
 * The values for offset and length are inherited from rootdir and 
 * rootdirlength.
 *
 * The buffer parameter should re-use the initial ROM buffer.
 */
int
LoadDVDFile (unsigned char *buffer)
{
  int offset;
  int blocks;
  int i;
  u64 discoffset;
  char readbuffer[2048];

        /*** SDCard Addition ***/
  if (rootdirlength == 0)
    return 0;

        /*** How many 2k blocks to read ***/
  blocks = rootdirlength / 2048;
  offset = 0;
  discoffset = rootdir;
  ShowAction ((char*) "Loading ... Wait");
  dvd_read (readbuffer, 2048, discoffset);

  if (!IsZipFile (readbuffer))

    {
      for (i = 0; i < blocks; i++)

        {
          dvd_read (readbuffer, 2048, discoffset);
          memcpy (buffer + offset, readbuffer, 2048);
          offset += 2048;
          discoffset += 2048;
        }

                /*** And final cleanup ***/
      if (rootdirlength % 2048)

        {
          i = rootdirlength % 2048;
          dvd_read (readbuffer, 2048, discoffset);
          memcpy (buffer + offset, readbuffer, i);
        }
    }

  else

    {
      return UnZipBuffer (buffer, discoffset, 1, NULL);	// unzip from dvd
    }
  return rootdirlength;
}
