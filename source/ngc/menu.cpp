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
#include "unzip.h"
#include "smbload.h"
#include "mcsave.h"
#include "sdload.h"
#include "memfile.h"
#include "dvd.h"
#include "s9xconfig.h"
#include "sram.h"
#include "preferences.h"

#include "button_mapping.h"
#include "ftfont.h"

extern void DrawMenu (char items[][20], char *title, int maxitems, int selected, int fontsize);

#define PSOSDLOADID 0x7c6000a6
extern int menu;
extern unsigned long ARAM_ROMSIZE;

#define SOFTRESET_ADR ((volatile u32*)0xCC003024)


void Reboot() {
#ifdef HW_RVL
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
            ret = RunMenu (freezemenuwii, freezecountwii, (char*)"Freeze Manager");
            if (ret >= freezecountwii-1)
                ret = freezecount-1;
        }
        else           /* Gamecube menu */
            ret = RunMenu (freezemenu, freezecount, (char*)"Freeze Manager");
        
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
int loadmancountwii = 3;
char loadmanwii[][20] = { "Load from SD",
  "Load from USB", "Return to previous"
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
#ifdef HW_RVL
	/* Wii menu */
		ret = RunMenu (loadmanwii, loadmancountwii, (char*)"Load Manager");

        switch (ret)
        {
		    case 0:
				/*** Load from SD ***/
                quit = OpenSD ();
                break;
				
            case 1:
                /*** Load from USB ***/
                quit = OpenUSB ();
                break;
            
			case -1: /*** Button B ***/
            case 2:
                retval = 0;
                quit = 1;
                break;
        }
#else 
		/* Gamecube menu */
		ret = RunMenu (loadman, loadmancount, (char*)"Load Manager");

        switch (ret)
        {
		    case 0:
				/*** Load from SD ***/
                quit = OpenSD ();
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
#endif
    }
	
	/*** 
	* check for autoloadsram / freeze 
	***/
	if ( retval == 1 ) // if ROM was loaded, load the SRAM & settings
	{
		if ( GCSettings.AutoLoad == 1 )
		{
			quickLoadSRAM ( SILENT );
			S9xSoftReset();	// reset after loading sram
		}
		else if ( GCSettings.AutoLoad == 2 )
		{
			quickLoadFreeze ( SILENT );
		}
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
            ret = RunMenu (savemenuwii, savecountwii, (char*)"SRAM Manager");
            if (ret >= savecountwii-1)
                ret = savecount-1;
        }
        else           /* Gamecube menu */
            ret = RunMenu (savemenu, savecount, (char*)"SRAM Manager");

        switch (ret)
        {
		    case 0:
                /*** Save to SD***/
                SaveSRAMToSD (NOTSILENT);
                break;
            
            case 1:
                /*** Load from SD***/
                LoadSRAMFromSD (NOTSILENT);
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
static int emuCount = 12;
static char emulatorOptions[][20] = { "Reverse Stereo OFF",
  "Interp. Sound ON", "Transparency ON", "FPS Display OFF",
  "MultiTap 5 OFF", "C-Stick Zoom OFF",
  "Auto Load OFF", "Auto Save OFF", "Verify MC Saves OFF",
  "Video Filtering OFF", "Save Prefs Now", "Return to previous"
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
			
		sprintf (emulatorOptions[9], "Video Filtering %s",
			GCSettings.render == true ? " ON" : "OFF");
		
		ret = RunMenu (emulatorOptions, emuCount, (char*)"Emulator Options");
		
		switch (ret)
		{
			case 0:
				Settings.ReverseStereo ^= 1;
				break;
			
			case 1:
				Settings.InterpolatedSound ^= 1;
				break;
			
			case 2:
				Settings.Transparency ^= 1;
				break;
			
			case 3:
				Settings.DisplayFrameRate ^= 1;
				break;
			
			case 4:
				Settings.MultiPlayer5Master ^= 1;
				
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
				GCSettings.NGCZoom ^= 1;
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
				GCSettings.VerifySaves ^= 1;
				break;

			case 9:
				GCSettings.render ^= 1;
				break;
				
			case 10:
				quickSavePrefs(NOTSILENT);
				break;
			
			case -1: /*** Button B ***/
			case 11:
				quit = 1;
				break;
			
		}
	}
	
	menu = oldmenu;
}

/****************************************************************************
 * Controller Configuration
 *
 * Snes9x 1.50 uses a cmd system to work out which button has been pressed.
 * Here, I simply move the designated value to the gcpadmaps array, which saves
 * on updating the cmd sequences.
 ****************************************************************************/
u32
GetInput (u16 ctrlr_type)
{
	u32 exp_type, pressed;
	pressed=0;
	s8 gc_px = 0;
	
	while( PAD_ButtonsHeld(0)
#ifdef HW_RVL
	| WPAD_ButtonsHeld(0)
#endif
	) VIDEO_WaitVSync();	// button 'debounce'
	
	while (pressed == 0)
	{
		VIDEO_WaitVSync();
		// get input based on controller type
		if (ctrlr_type == CTRLR_GCPAD) 
		{
			pressed = PAD_ButtonsHeld (0);
			gc_px = PAD_SubStickX (0);
		} 
#ifdef HW_RVL
		else 
		{	
		//	if ( WPAD_Probe( 0, &exp_type) == 0)	// check wiimote and expansion status (first if wiimote is connected & no errors)
		//	{
				pressed = WPAD_ButtonsHeld (0);
				
		//		if (ctrlr_type != CTRLR_WIIMOTE && exp_type != ctrlr_type+1)	// if we need input from an expansion, and its not connected...
		//			pressed = 0;
		//	}
		}
#endif
		/*** check for exit sequence (c-stick left OR home button) ***/
		if ( (gc_px < -70) || (pressed & WPAD_BUTTON_HOME) || (pressed & WPAD_CLASSIC_BUTTON_HOME) )
			return 0;
	}	// end while
	while( pressed == (PAD_ButtonsHeld(0) 
#ifdef HW_RVL
						| WPAD_ButtonsHeld(0)
#endif
						) ) VIDEO_WaitVSync();
	
	return pressed;
}	// end GetInput()

int cfg_text_count = 7;
char cfg_text[][20] = {
"Remapping          ",
"Press Any Button",
"on the",
"       ",	// identify controller
"                   ",
"Press C-Left or",
"Home to exit"
};

u32
GetButtonMap(u16 ctrlr_type, char* btn_name)
{
	u32 pressed, previous;
	char temp[20] = "";
	int k;
	pressed = 0; previous = 1;
	
	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			strncpy (cfg_text[3], (char*)"NUNCHUK", 7);
			break;
		case CTRLR_CLASSIC:
			strncpy (cfg_text[3], (char*)"CLASSIC", 7);
			break;
		case CTRLR_GCPAD:
			strncpy (cfg_text[3], (char*)"GC PAD", 7);
			break;
		case CTRLR_WIIMOTE:
			strncpy (cfg_text[3], (char*)"WIIMOTE", 7);
			break;
	}; 
	
	/*** note which button we are remapping ***/
	sprintf (temp, (char*)"Remapping ");
	for (k=0; k<9-strlen(btn_name) ;k++) strcat(temp, " "); // add whitespace padding to align text
	strncat (temp, btn_name, 9);		// snes button we are remapping
	strncpy (cfg_text[0], temp, 19);	// copy this all back to the text we wish to display
	
	DrawMenu(&cfg_text[0], NULL, cfg_text_count, 1);	// display text
	
//	while (previous != pressed && pressed == 0);	// get two consecutive button presses (which are the same)
//	{
//		previous = pressed;
//		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads
		pressed = GetInput(ctrlr_type);
//	}
	return pressed;
}	// end getButtonMap()
 
int cfg_btns_count = 13;
char cfg_btns_menu[][20] = { 
	"A        -         ",
	"B        -         ",
	"X        -         ",
	"Y        -         ",
	"L TRIG   -         ",
	"R TRIG   -         ",
	"SELECT   -         ",
	"START    -         ",
	"UP       -         ",
	"DOWN     -         ",
	"LEFT     -         ",
	"RIGHT    -         ",
	"Return to previous"
};

extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

void
ConfigureButtons (u16 ctrlr_type)
{
	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;
	char* menu_title;
	u32 pressed;
	
	unsigned int* currentpadmap;
	char temp[20] = "";
	int i, j, k;
	
	/*** Update Menu Title (based on controller we're configuring) ***/
	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			menu_title = (char*)"SNES     -  NUNCHUK";
			currentpadmap = ncpadmap;
			break;
		case CTRLR_CLASSIC:
			menu_title = (char*)"SNES     -  CLASSIC";
			currentpadmap = ccpadmap;
			break;
		case CTRLR_GCPAD:
			menu_title = (char*)"SNES     -   GC PAD";
			currentpadmap = gcpadmap;
			break;
		case CTRLR_WIIMOTE:
			menu_title = (char*)"SNES     -  WIIMOTE";
			currentpadmap = wmpadmap;
			break;
	};
	
	while (quit == 0)
	{	
		/*** Update Menu with Current ButtonMap ***/
		for (i=0; i<12; i++) // snes pad has 12 buttons to config (go thru them)
		{
			// get current padmap button name to display
			for ( j=0; 
					j < ctrlr_def[ctrlr_type].num_btns &&
					currentpadmap[i] != ctrlr_def[ctrlr_type].map[j].btn	// match padmap button press with button names
				; j++ );
			
			memset (temp, 0, sizeof(temp));
			strncpy (temp, cfg_btns_menu[i], 12);	// copy snes button information
			if (currentpadmap[i] == ctrlr_def[ctrlr_type].map[j].btn)		// check if a match was made
			{
				for (k=0; k<7-strlen(ctrlr_def[ctrlr_type].map[j].name) ;k++) strcat(temp, " "); // add whitespace padding to align text
				strncat (temp, ctrlr_def[ctrlr_type].map[j].name, 6);		// update button map display
			}
			else
				strcat (temp, (char*)"---");								// otherwise, button is 'unmapped'
			strncpy (cfg_btns_menu[i], temp, 19);	// move back updated information

		}
	
		ret = RunMenu (cfg_btns_menu, cfg_btns_count, menu_title, 18);
		
		switch (ret)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
				/*** Change button map ***/
				// wait for input
				memset (temp, 0, sizeof(temp));
				strncpy(temp, cfg_btns_menu[ret], 6);			// get the name of the snes button we're changing
				pressed = GetButtonMap(ctrlr_type, temp);	// get a button selection from user
				// FIX: check if input is valid for this controller
				if (pressed != 0)	// check if a the button was configured, or if the user exited.
					currentpadmap[ret] = pressed;	// update mapping
				break;

			case -1: /*** Button B ***/
			case 12:
				/*** Return ***/
				quit = 1;
				break;
		}
	}
	
	menu = oldmenu;
}	// end configurebuttons()

int cfg_ctrlr_count_wii = 6;
char cfg_ctrlr_menu_wii[][20] = { "Nunchuk",
	"Classic Controller",
	"Gamecube Pad",
	"Wiimote",
	"Save Prefs Now",
	"Return to previous"
};

int cfg_ctrlr_count_gc = 3;
char cfg_ctrlr_menu_gc[][20] = { "Gamecube Pad",
	"Save Prefs Now",
	"Return to previous"
};

void
ConfigureControllers ()
{
	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;
	
	while (quit == 0)
	{	
#ifdef HW_RVL
		/*** Wii Controller Config Menu ***/
        ret = RunMenu (cfg_ctrlr_menu_wii, cfg_ctrlr_count_wii, (char*)"Configure Controllers");
		
		switch (ret)
		{
			case 0:
				/*** Configure Nunchuk ***/
				ConfigureButtons (CTRLR_NUNCHUK);
				break;
			
			case 1:
				/*** Configure Classic ***/
				ConfigureButtons (CTRLR_CLASSIC);
				break;
			
			case 2:
				/*** Configure GC Pad ***/
				ConfigureButtons (CTRLR_GCPAD);
				break;
			
			case 3:
				/*** Configure Wiimote ***/
				ConfigureButtons (CTRLR_WIIMOTE);
				break;
				
			case 4:
				/*** Save Preferences Now ***/
				quickSavePrefs(NOTSILENT);
				break;

			case -1: /*** Button B ***/
			case 5:
				/*** Return ***/
				quit = 1;
				break;
		}
#else
		/*** Gamecube Controller Config Menu ***/
        ret = RunMenu (cfg_ctrlr_menu_gc, cfg_ctrlr_count_gc, (char*)"Configure Controllers");
		
		switch (ret)
		{
			case 0:
				/*** Configure Nunchuk ***/
				ConfigureButtons (CTRLR_GCPAD);
				break;
				
			case 1:
				/*** Save Preferences Now ***/
				quickSavePrefs(NOTSILENT);
				break;

			case -1: /*** Button B ***/
			case 2:
				/*** Return ***/
				quit = 1;
				break;
		}
#endif
	}
	
	menu = oldmenu;
}

/****************************************************************************
 * Main Menu
 ****************************************************************************/
int menucount = 10;
char menuitems[][20] = { "Choose Game",
  "SRAM Manager", "Freeze Manager",
  "Config Controllers", "Emulator Options",
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
				/*** Configure Controllers ***/
				ConfigureControllers ();
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
				#ifdef HW_RVL
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
#ifdef HW_RVL
	while( PAD_ButtonsHeld(0) || WPAD_ButtonsHeld(0) )
	    VIDEO_WaitVSync();
#else
	while( PAD_ButtonsHeld(0) )
	    VIDEO_WaitVSync();
#endif
	
	ReInitGCVideo();	// update video after reading settings
}
