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
#include <wiiuse/wpad.h>
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


void Reboot() {
#ifdef __wii__
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
#define SOFTRESET_ADR ((volatile u32*)0xCC003024)
    *SOFTRESET_ADR = 0x00000000;
#endif
}

/****************************************************************************
 * Freeze Manager
 ****************************************************************************/
int freezecountwii = 7;
char freezemenuwii[][20] = { "Freeze to SD", "Thaw from SD",
	 "Freeze to MC Slot A", "Thaw from MC Slot A",
     "Freeze to MC Slot B", "Thaw from MC Slot B",
     "Return to previous"
};
int freezecount = 9;
char freezemenu[][20] = { "Freeze to SD", "Thaw from SD",
	 "Freeze to MC Slot A", "Thaw from MC Slot A",
     "Freeze to MC Slot B", "Thaw from MC Slot B",
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
            case 0:/*** Freeze to SDCard ***/
                NGCFreezeGame (2, NOTSILENT);
                break;
            
            case 1:/*** Thaw from SDCard***/
                quit = loaded = NGCUnfreezeGame (2, NOTSILENT);
                break;
				
            case 2:/*** Freeze to MC in slot A ***/
                NGCFreezeGame (0, NOTSILENT);
                break;
            
            case 3:/*** Thaw from MC in slot A ***/
                quit = loaded = NGCUnfreezeGame (0, NOTSILENT);
                break;
             
            case 4:/*** Freeze to MC in slot B ***/
                NGCFreezeGame (1, NOTSILENT);
                break;

            case 5:/*** Thaw from MC in slot B ***/
                quit = loaded = NGCUnfreezeGame (1, NOTSILENT);
                break;
            
            case 6:/*** Freeze to SMB ***/
                if ( !isWii )
                    NGCFreezeGame (4, NOTSILENT);
                break;
            
            case 7:/*** Thaw from SMB ***/
                if ( !isWii )
                    quit = loaded = NGCUnfreezeGame (4, NOTSILENT);
                break;
            
			case -1: /*** Button B ***/
            case 8:
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
int loadmancountwii = 2;
char loadmanwii[][20] = { "Load from SD",
  "Return to previous"
};
int loadmancount = 4;
char loadman[][20] = {  "Load from SD",
  "Load from DVD", "Load from SMB", "Return to previous"
};
int
LoadManager ()
{
    int ret;
    int quit = 0;
    int oldmenu = menu;
    int retval = 1;
    menu = 0;
    
    while (quit == 0)
    {
		if ( isWii )   /* Wii menu */
		{
			ret = RunMenu (loadmanwii, loadmancountwii, (char*)"Load Manager");
			if (ret >= loadmancountwii-1)
				ret = loadmancount-1;
		}
		else           /* Gamecube menu */
			ret = RunMenu (loadman, loadmancount, (char*)"Load Manager");
        
        switch (ret)
        {
		    case 0:
				/*** Load from SD ***/
                quit = OpenSD (0);
                break;
				
            case 1:
                /*** Load from DVD ***/
                quit = OpenDVD ();
                break;
            
            case 2:
				/*** Load from SMB ***/ //(gamecube option)
	            quit = OpenSMB ();
	            break;
            
			case -1: /*** Button B ***/
            case 3:
                retval = 0;
                quit = 1;
                break;
        }
    }
	
	/*** 
	* check for autoloadsram / freeze 
	***/
	if ( retval == 1 ) // if ROM was loaded, load the SRAM & settings
	{
		if ( GCSettings.AutoLoad == 1 )
			quickLoadSRAM ( SILENT );
		else if ( GCSettings.AutoLoad == 2 )
		{
			/*** load SRAM first in order to get joypad config ***/
			quickLoadSRAM ( SILENT );
			quickLoadFreeze ( SILENT );
		}
		S9xSoftReset();	// reset after loading sram
	}
    
    menu = oldmenu;
    return retval;
}

/****************************************************************************
 * Save Manager
 ****************************************************************************/
int savecountwii = 7;
char savemenuwii[][20] = { "Save to SD", "Load from SD",
  "Save to MC SLOT A", "Load from MC SLOT A",
  "Save to MC SLOT B", "Load from MC SLOT B",
  "Return to previous"
};
int savecount = 9;
char savemenu[][20] = { "Save to SD", "Load from SD",
  "Save to MC SLOT A", "Load from MC SLOT A",
  "Save to MC SLOT B", "Load from MC SLOT B",
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
            ret = RunMenu (savemenuwii, savecountwii, (char*)"Save Manager");
            if (ret >= savecountwii-1)
                ret = savecount-1;
        }
        else           /* Gamecube menu */
            ret = RunMenu (savemenu, savecount, (char*)"Save Manager");

        switch (ret)
        {
		    case 0:
                /*** Save to SD***/
                SaveSRAMToSD (0, NOTSILENT);
                break;
            
            case 1:
                /*** Load from SD***/
                LoadSRAMFromSD (0, NOTSILENT);
                break;
            
            case 2:
                /*** Save to MC slot A ***/
                SaveSRAMToMC (CARD_SLOTA, NOTSILENT);
                break;
            
            case 3:
                /*** Load from MC slot A ***/
                LoadSRAMFromMC (CARD_SLOTA, NOTSILENT);
                break;
            
            case 4:
                /*** Save to MC slot B ***/
                SaveSRAMToMC (CARD_SLOTB, NOTSILENT);
                break;
            
            case 5:
                /*** Load from MC slot B ***/
                LoadSRAMFromMC (CARD_SLOTB, NOTSILENT);
                break;

            case 6:
                /*** Save to SMB **/
                SaveSRAMToSMB (NOTSILENT);
                break;
            
            case 7:
                /*** Load from SMB ***/
                LoadSRAMFromSMB (NOTSILENT);
                break;
            
			case -1: /*** Button B ***/
            case 8:
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
		
		ret = RunMenu (emulatorOptions, emuCount, (char*)"Emulator Options");
		
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
		
		ret = RunMenu (padmenu, padcount, (char*)"Configure Joypads");
		
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
				    WaitPrompt((char*) "No ROM loaded - can't save SRAM");
				
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
  "Reset Game", "Stop DVD Drive", "Exit to Loader",
  "Reboot System", "Return to Game"
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
		ret = RunMenu (menuitems, menucount, (char*)"Main Menu");
		
		switch (ret)
		{
			case 0:
				/*** Load ROM Menu ***/
				quit = LoadManager ();
				break;
			
			case 1:
				/*** SRAM Manager Menu ***/
				if ( ARAM_ROMSIZE > 0 )
                    SaveManager ();
                else
                    WaitPrompt((char*) "No ROM is loaded!");
                break;
			
			case 2:
				/*** Do Freeze / Thaw Menu ***/
				if ( ARAM_ROMSIZE > 0 )
                    quit = FreezeManager ();
                else
                    WaitPrompt((char*) "No ROM is loaded!");
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
				/*** Exit to Loader ***/
				#ifdef __wii__
				exit(0);
				#else	// gamecube
				if (psoid[0] == PSOSDLOADID)
				PSOReload ();
				#endif
				break;
				
		    case 8:
		        /*** Reset the Gamecube/Wii ***/
                Reboot();
                break;
			
			case -1: /*** Button B ***/
			case 9:
				/*** Return to Game ***/
				quit = 1;
				break;
			
		}
		
	}
	
	/*** Remove any still held buttons ***/
	while( PAD_ButtonsHeld(0) || WPAD_ButtonsHeld(0) )
	    VIDEO_WaitVSync();
	
}
