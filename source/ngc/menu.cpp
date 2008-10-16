/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Menu
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 ****************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snes9x.h"
#include "snes9xGx.h"
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
#include "aram.h"
#include "ftfont.h"
#include "video.h"
#include "mcsave.h"
#include "filesel.h"
#include "ngcunzip.h"
#include "smbload.h"
#include "mcsave.h"
#include "sdload.h"
#include "memfile.h"
#include "dvd.h"
#include "s9xconfig.h"
#include "sram.h"
#include "preferences.h"

#define PSOSDLOADID 0x7c6000a6
extern int menu;
extern unsigned long ARAM_ROMSIZE;

#define SOFTRESET_ADR ((volatile u32*)0xCC003024)


/****************************************************************************
 * Freeze Manager
 ****************************************************************************/
int freezecountwii = 9;
char freezemenuwii[][20] = { "Freeze to MC Slot A", "Thaw from MC Slot A",
     "Freeze to MC Slot B", "Thaw from MC Slot B",
     "Freeze to SD Slot A", "Thaw from SD Slot A",
     "Freeze to SD Slot B", "Thaw from SD Slot B",
     "Return to previous"
};
int freezecount = 11;
char freezemenu[][20] = { "Freeze to MC Slot A", "Thaw from MC Slot A",
     "Freeze to MC Slot B", "Thaw from MC Slot B",
     "Freeze to SD Slot A", "Thaw from SD Slot A",
     "Freeze to SD Slot B", "Thaw from SD Slot B",
     "Freeze to SMB", "Thaw from SMB",
     "Return to previous"
};
int
FreezeManager ()
{
    int ret;
    int loaded = 0;
    int quit = 0;
    int oldmenu = menu;
    menu = 0;
    
    
    while (quit == 0)
    {
        if ( isWii )   /* Wii menu */
        {
            ret = RunMenu (freezemenuwii, freezecountwii, "Freeze Manager");
            if (ret >= freezecountwii-1)
                ret = freezecount-1;
        }
        else           /* Gamecube menu */
            ret = RunMenu (freezemenu, freezecount, "Freeze Manager");
        
        switch (ret)
        {
            case 0:/*** Freeze to MC in slot A ***/
                NGCFreezeGame (0, NOTSILENT);
                break;
            
            case 1:/*** Thaw from MC in slot A ***/
                quit = loaded = NGCUnfreezeGame (0, NOTSILENT);
                break;
             
            case 2:/*** Freeze to MC in slot B ***/
                NGCFreezeGame (1, NOTSILENT);
                break;

            case 3:/*** Thaw from MC in slot B ***/
                quit = loaded = NGCUnfreezeGame (1, NOTSILENT);
                break;
            
            case 4:/*** Freeze to SDCard in slot A ***/
                NGCFreezeGame (2, NOTSILENT);
                break;
            
            case 5:/*** Thaw from SDCard in slot A ***/
                quit = loaded = NGCUnfreezeGame (2, NOTSILENT);
                break;
            
            case 6:/*** Freeze to SDCard in slot B ***/
                NGCFreezeGame (3, NOTSILENT);
                break;
            
            case 7:/*** Thaw from SDCard in slot B ***/
                quit = loaded = NGCUnfreezeGame (3, NOTSILENT);
                break;
            
            case 8:/*** Freeze to SMB ***/
                if ( !isWii )
                    NGCFreezeGame (4, NOTSILENT);
                break;
            
            case 9:/*** Thaw from SMB ***/
                if ( !isWii )
                    quit = loaded = NGCUnfreezeGame (4, NOTSILENT);
                break;
            
			case -1: /*** Button B ***/
            case 10:
                quit = 1;
                break;
        }
    
    }
    
    menu = oldmenu;
    return loaded;
}

/****************************************************************************
 * Load Manager
 ****************************************************************************/
int loadmancountwii = 4;
char loadmanwii[][20] = { "Load from DVD", "Load from SD Slot A",
  "Load from SD Slot B", "Return to previous"
};
int loadmancount = 5;
char loadman[][20] = { "Load from DVD", "Load from SD Slot A",
  "Load from SD Slot B", "Load from SMB", "Return to previous"
};
int
LoadManager ()
{
    int ret;
    int quit = 0;
    int oldmenu = menu;
    int retval = 0;
    menu = 0;
    
    while (quit == 0)
    {
        if ( LOAD_TYPE )
            ret = LOAD_TYPE-1;
        else
        {
            if ( isWii )   /* Wii menu */
            {
                ret = RunMenu (loadmanwii, loadmancountwii, "Load Manager");
                if (ret >= loadmancountwii-1)
                    ret = loadmancount-1;
            }
            else           /* Gamecube menu */
                ret = RunMenu (loadman, loadmancount, "Load Manager");
        }
        
        switch (ret)
        {
            case 0:
                /*** Load from DVD ***/
                retval = OpenDVD ();
                if ( LOAD_TYPE )
                    quit = 1;
                else
                    quit = retval;
                break;
            
            case 1:
                retval = OpenSD (CARD_SLOTA);
                if ( LOAD_TYPE )
                    quit = 1;
                else
                    quit = retval;
                break;
            
            case 2:
                retval = OpenSD (CARD_SLOTB);
                if ( LOAD_TYPE )
                    quit = 1;
                else
                    quit = retval;
                break;
            
            case 3:
                retval = OpenSMB ();
                if ( LOAD_TYPE )
                    quit = 1;
                else
                    quit = retval;
                break;
            
			case -1: /*** Button B ***/
            case 4:
                retval = 0;
                quit = 1;
                break;
        }
    }
    
    menu = oldmenu;
    return retval;
}

/****************************************************************************
 * Save Manager
 ****************************************************************************/
int savecountwii = 9;
char savemenuwii[][20] = { "Save to MC SLOT A", "Load from MC SLOT A",
  "Save to MC SLOT B", "Load from MC SLOT B",
  "Save to SD Slot A", "Load from SD Slot A",
  "Save to SD Slot B", "Load from SD Slot B",
  "Return to previous"
};
int savecount = 11;
char savemenu[][20] = { "Save to MC SLOT A", "Load from MC SLOT A",
  "Save to MC SLOT B", "Load from MC SLOT B",
  "Save to SD Slot A", "Load from SD Slot A",
  "Save to SD Slot B", "Load from SD Slot B",
  "Save to SMB", "Load from SMB",
  "Return to previous"
};
void
SaveManager ()
{
    int ret;
    int quit = 0;
    int oldmenu = menu;
    menu = 0;
    
    while (quit == 0)
    {
        if ( isWii )   /* Wii menu */
        {
            ret = RunMenu (savemenuwii, savecountwii, "Save Manager");
            if (ret >= savecountwii-1)
                ret = savecount-1;
        }
        else           /* Gamecube menu */
            ret = RunMenu (savemenu, savecount, "Save Manager");

        switch (ret)
        {
            case 0:
                /*** Save to MC slot A ***/
                SaveSRAMToMC (CARD_SLOTA, NOTSILENT);
                break;
            
            case 1:
                /*** Load from MC slot A ***/
                LoadSRAMFromMC (CARD_SLOTA, NOTSILENT);
                break;
            
            case 2:
                /*** Save to MC slot B ***/
                SaveSRAMToMC (CARD_SLOTB, NOTSILENT);
                break;
            
            case 3:
                /*** Load from MC slot B ***/
                LoadSRAMFromMC (CARD_SLOTB, NOTSILENT);
                break;
            
            case 4:
                /*** Save to SD slot A ***/
                SaveSRAMToSD (CARD_SLOTA, NOTSILENT);
                break;
            
            case 5:
                /*** Load from SD slot A ***/
                LoadSRAMFromSD (CARD_SLOTA, NOTSILENT);
                break;
            
            case 6:
                /*** Save to SD slot B ***/
                SaveSRAMToSD (CARD_SLOTB, NOTSILENT);
                break;
            
            case 7:
                /*** Load from SD slot B ***/
                LoadSRAMFromSD (CARD_SLOTB, NOTSILENT);
                break;
            
            case 8:
                /*** Save to SMB **/
                SaveSRAMToSMB (NOTSILENT);
                break;
            
            case 9:
                /*** Load from SMB ***/
                LoadSRAMFromSMB (NOTSILENT);
                break;
            
			case -1: /*** Button B ***/
            case 10:
                /*** Return ***/
                quit = 1;
                break;
        }
    }
    
    menu = oldmenu;
}

/****************************************************************************
 * Emulator Options
 ****************************************************************************/
static int emuCount = 11;
static char emulatorOptions[][20] = { "Reverse Stereo OFF",
  "Interp. Sound ON", "Transparency ON", "FPS Display OFF",
  "MultiTap 5 OFF", "C-Stick Zoom OFF",
  "Auto Load OFF", "Auto Save OFF", "Verify MC Saves OFF",
  "Save Prefs Now", "Return to previous"
};

void
EmulatorOptions ()
{
	int ret = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;
	
	while (quit == 0)
	{
		sprintf (emulatorOptions[0], "Reverse Stereo %s",
			Settings.ReverseStereo == true ? " ON" : "OFF");
			
		sprintf (emulatorOptions[1], "Interp. Sound %s",
			Settings.InterpolatedSound == true ? " ON" : "OFF");
			
		sprintf (emulatorOptions[2], "Transparency %s",
			Settings.Transparency == true ? " ON" : "OFF");
			
		sprintf (emulatorOptions[3], "FPS Display %s",
			Settings.DisplayFrameRate == true ? " ON" : "OFF");
			
		sprintf (emulatorOptions[4], "MultiTap 5 %s",
			Settings.MultiPlayer5Master == true ? " ON" : "OFF");
			
		sprintf (emulatorOptions[5], "C-Stick Zoom %s",
			GCSettings.NGCZoom == true ? " ON" : "OFF");
			
        if (GCSettings.AutoLoad == 0) sprintf (emulatorOptions[6],"Auto Load OFF");
        else if (GCSettings.AutoLoad == 1) sprintf (emulatorOptions[6],"Auto Load SRAM");
        else if (GCSettings.AutoLoad == 2) sprintf (emulatorOptions[6],"Auto Load FREEZE");
        
        if (GCSettings.AutoSave == 0) sprintf (emulatorOptions[7],"Auto Save OFF");
        else if (GCSettings.AutoSave == 1) sprintf (emulatorOptions[7],"Auto Save SRAM");
        else if (GCSettings.AutoSave == 2) sprintf (emulatorOptions[7],"Auto Save FREEZE");
        else if (GCSettings.AutoSave == 3) sprintf (emulatorOptions[7],"Auto Save BOTH");
			
		sprintf (emulatorOptions[8], "Verify MC Saves %s",
			GCSettings.VerifySaves == true ? " ON" : "OFF");
		
		ret = RunMenu (emulatorOptions, emuCount, "Emulator Options");
		
		switch (ret)
		{
			case 0:
				Settings.ReverseStereo =
					(Settings.ReverseStereo == false ? true : false);
				break;
			
			case 1:
				Settings.InterpolatedSound =
					(Settings.InterpolatedSound == false ? true : false);
				break;
			
			case 2:
				Settings.Transparency =
					(Settings.Transparency == false ? true : false);
				break;
			
			case 3:
				Settings.DisplayFrameRate =
					(Settings.DisplayFrameRate == false ? true : false);
				break;
			
			case 4:
				Settings.MultiPlayer5Master =
					(Settings.MultiPlayer5Master == false ? true : false);
				
				if (Settings.MultiPlayer5Master)
				{
					S9xSetController (1, CTL_MP5, 1, 2, 3, -1);
				}
				else
				{
					S9xSetController (1, CTL_JOYPAD, 1, 0, 0, 0);
				}
				break;
			
			case 5:
				GCSettings.NGCZoom =
					(GCSettings.NGCZoom == false ? true : false);
				break;
			
			case 6:
			    GCSettings.AutoLoad ++;
		        if (GCSettings.AutoLoad > 2)
		            GCSettings.AutoLoad = 0;
				break;
			
			case 7:
				GCSettings.AutoSave ++;
		        if (GCSettings.AutoSave > 3)
		            GCSettings.AutoSave = 0;
				break;
			
			case 8:
				GCSettings.VerifySaves =
					(GCSettings.VerifySaves == false ? true : false);
				break;
			
			case 9:
				quickSavePrefs(NOTSILENT);
				break;
			
			case -1: /*** Button B ***/
			case 10:
				quit = 1;
				break;
			
		}
	}
	
	menu = oldmenu;
}

/****************************************************************************
 * Configure Joypads
 *
 * Snes9x 1.50 uses a cmd system to work out which button has been pressed.
 * Here, I simply move the designated value to the gcpadmaps array, which saves
 * on updating the cmd sequences.
 ****************************************************************************/
int padcount = 7;
char padmenu[][20] = { "SNES BUTTON A - A",
	"SNES BUTTON B - B",
	"SNES BUTTON X - X",
	"SNES BUTTON Y - Y",
	"ANALOG CLIP   - 70",
	"Save SRAM/Config",
	"Return to previous"
};

unsigned short padmap[4] = { PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y
};
int currconfig[4] = { 0, 1, 2, 3 };
static char *padnames = "ABXY";

extern unsigned short gcpadmap[];
extern int padcal;

void
ConfigureJoyPads ()
{
	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;
	
	while (quit == 0)
	{
		/*** Update the menu information ***/
		for (ret = 0; ret < 4; ret++)
		padmenu[ret][16] = padnames[currconfig[ret]];
		
		sprintf (padmenu[4], "ANALOG CLIP - %d", padcal);
		
		ret = RunMenu (padmenu, padcount, "Configure Joypads");
		
		switch (ret)
		{
			case 0:
				/*** Configure Button A ***/
				currconfig[0]++;
				currconfig[0] &= 3;
				gcpadmap[0] = padmap[currconfig[0]];
				break;
			
			case 1:
				/*** Configure Button B ***/
				currconfig[1]++;
				currconfig[1] &= 3;
				gcpadmap[1] = padmap[currconfig[1]];
				break;
			
			case 2:
				/*** Configure Button X ***/
				currconfig[2]++;
				currconfig[2] &= 3;
				gcpadmap[2] = padmap[currconfig[2]];
				break;
			
			case 3:
				/*** Configure Button Y ***/
				currconfig[3]++;
				currconfig[3] &= 3;
				gcpadmap[3] = padmap[currconfig[3]];
				break;
			
			case 4:
				/*** Pad Calibration ***/
				padcal += 5;
				if (padcal > 80)
					padcal = 40;
				break;
						
			case 5:
				/*** Quick Save SRAM ***/
                if ( ARAM_ROMSIZE > 0 )
                    quickSaveSRAM(NOTSILENT);
				else
				    WaitPrompt ("No ROM loaded - can't save SRAM");
				
				break;

			case -1: /*** Button B ***/
			case 6:
				/*** Return ***/
				quit = 1;
				break;
		}
	}
	
	menu = oldmenu;
}

/****************************************************************************
 * Main Menu
 ****************************************************************************/
int menucount = 10;
char menuitems[][20] = { "Choose Game",
  "SRAM Manager", "Freeze Manager",
  "Configure Joypads", "Emulator Options",
  "Reset Game", "Stop DVD Drive", "PSO Reload",
  "Reset Gamecube", "Return to Game"
};

void
mainmenu ()
{
	int quit = 0;
	int ret;
	int *psoid = (int *) 0x80001800;
	void (*PSOReload) () = (void (*)()) 0x80001800;
	
	if ( isWii )
    	sprintf (menuitems[8],"Reset Wii");
	else
    	sprintf (menuitems[8],"Reset Gamecube");
	
	VIDEO_WaitVSync ();
	
	while (quit == 0)
	{
		ret = RunMenu (menuitems, menucount, "Main Menu");
		
		switch (ret)
		{
			case 0:
				/*** Load ROM Menu ***/
				quit = LoadManager ();
				if ( quit ) // if ROM was loaded, load the SRAM & settings
				{
					if ( GCSettings.AutoLoad == 1 )
						quickLoadSRAM ( SILENT );
					else if ( GCSettings.AutoLoad == 2 )
					{
					    /*** load SRAM first in order to get joypad config ***/
						quickLoadSRAM ( SILENT );
                        quickLoadFreeze ( SILENT );
                    }
				}
				break;
			
			case 1:
				/*** SRAM Manager Menu ***/
				if ( ARAM_ROMSIZE > 0 )
                    SaveManager ();
                else
                    WaitPrompt ("No ROM is loaded!");
                break;
			
			case 2:
				/*** Do Freeze / Thaw Menu ***/
				if ( ARAM_ROMSIZE > 0 )
                    quit = FreezeManager ();
                else
                    WaitPrompt ("No ROM is loaded!");
                break;
			
			case 3:
				/*** Configure Joypads ***/
				ConfigureJoyPads ();
				break;
			
			case 4:
				/*** Emulator Options ***/
				EmulatorOptions ();
				break;
			
			case 5:
				/*** Soft reset ***/
				S9xSoftReset ();
				quit = 1;
				break;
			
			case 6:
				/*** Turn off DVD motor ***/
				dvd_motor_off();
				break;
			
			case 7:
				/*** PSO Reload ***/
				if (psoid[0] == PSOSDLOADID)
				PSOReload ();
				break;
				
		    case 8:
		        /*** Reset the Gamecube ***/
                *SOFTRESET_ADR = 0x00000000;
                break;
			
			case -1: /*** Button B ***/
			case 9:
				/*** Return to Game ***/
				quit = 1;
				break;
			
		}
		
	}
	
	/*** Remove any still held buttons ***/
	while( PAD_ButtonsHeld(0) )
	    VIDEO_WaitVSync();
	
}
