/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#ifdef WII_DVD
extern "C" {
#include <di/di.h>
}
#endif

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
#include "cheats.h"

#include "snes9xGX.h"
#include "video.h"
#include "filesel.h"
#include "unzip.h"
#include "smbop.h"
#include "memcardop.h"
#include "fileop.h"
#include "freeze.h"
#include "dvd.h"
#include "s9xconfig.h"
#include "sram.h"
#include "preferences.h"
#include "button_mapping.h"
#include "menudraw.h"
#include "cheatmgr.h"
#include "input.h"
#include "patch.h"

/****************************************************************************
 * Load Manager
 ***************************************************************************/

int
LoadManager ()
{
	int loadROM = OpenROM(GCSettings.LoadMethod);

	if (loadROM)
	{
		// load UPS/IPS/PPF patch
		LoadPatch(GCSettings.LoadMethod);

		Memory.LoadROM ("BLANK.SMC");
		Memory.LoadSRAM ("BLANK");

		// load SRAM or snapshot
		if ( GCSettings.AutoLoad == 1 )
			LoadSRAM(GCSettings.SaveMethod, SILENT);
		else if ( GCSettings.AutoLoad == 2 )
			NGCUnfreezeGame (GCSettings.SaveMethod, SILENT);

		// setup cheats
		SetupCheats();
	}

	return loadROM;
}

/****************************************************************************
 * Cheat Menu
 ***************************************************************************/
static int cheatmenuCount = 0;
static char cheatmenu[MAX_CHEATS][50];
static char cheatmenuvalue[MAX_CHEATS][50];

void CheatMenu()
{
	int ret = -1;
	int oldmenu = menu;
	menu = 0;

	int selection = 0;
	int offset = 0;
	int redraw = 1;
	int selectit = 0;

    u32 p = 0;
	u32 wp = 0;
	u32 ph = 0;
	u32 wh = 0;
    signed char gc_ay = 0;
	signed char gc_sx = 0;
	signed char wm_ay = 0;
	signed char wm_sx = 0;

	int scroll_delay = 0;
	bool move_selection = 0;
	#define SCROLL_INITIAL_DELAY	15
	#define SCROLL_LOOP_DELAY		2

	if(Cheat.num_cheats > 0)
	{
		cheatmenuCount = Cheat.num_cheats + 1;

		for(uint16 i=0; i < Cheat.num_cheats; i++)
			sprintf (cheatmenu[i], "%s", Cheat.c[i].name);

		sprintf (cheatmenu[cheatmenuCount-1], "Back to Game Menu");

		while(ret != cheatmenuCount-1)
		{
			if(ret >= 0)
			{
				if(Cheat.c[ret].enabled)
					S9xDisableCheat(ret);
				else
					S9xEnableCheat(ret);

				ret = -1;
			}

			for(uint16 i=0; i < Cheat.num_cheats; i++)
				sprintf (cheatmenuvalue[i], "%s", Cheat.c[i].enabled == true ? "ON" : "OFF");

			if (redraw)
			    ShowCheats (cheatmenu, cheatmenuvalue, cheatmenuCount, offset, selection);

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
				break;

			if ( (p & PAD_BUTTON_B) || (wp & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)) )
				break;

			/*** Check buttons, perform actions ***/
			if ( (p & PAD_BUTTON_A) || selectit || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) )
			{
				if ( selectit )
					selectit = 0;

				redraw = 1;
				ret = selection;
			}	// End of A

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
					if (selection == cheatmenuCount)
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
						selection = cheatmenuCount - 1;
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
					selection = cheatmenuCount - 1;
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
				if (selection > cheatmenuCount - 1)
					selection = offset = 0;
				if ((selection - offset) >= PAGESIZE)
					offset += PAGESIZE;
				redraw = 1;
			}
		}
	}
	else
	{
		WaitPrompt((char*)"No cheats found!");
	}
	menu = oldmenu;
}

/****************************************************************************
 * Game Options Menu
 ***************************************************************************/

int
GameMenu ()
{
	int gamemenuCount = 10;
	char gamemenu[][50] = {
	  "Return to Game",
	  "Reset Game",
	  "ROM Information",
	  "Cheats",
	  "Load SRAM", "Save SRAM",
	  "Load Game Snapshot", "Save Game Snapshot",
	  "Reset Zoom",
	  "Back to Main Menu"
	};

	int ret, retval = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;

	while (quit == 0)
	{
		// disable SRAM/SNAPSHOT saving/loading if AUTO is on

		if (GCSettings.AutoLoad == 1) // Auto Load SRAM
			gamemenu[4][0] = '\0';
		else if (GCSettings.AutoLoad == 2) // Auto Load SNAPSHOT
			gamemenu[6][0] = '\0';

		if (GCSettings.AutoSave == 1) // Auto Save SRAM
			gamemenu[5][0] = '\0';
		else if (GCSettings.AutoSave == 2) // Auto Save SNAPSHOT
			gamemenu[7][0] = '\0';
		else if (GCSettings.AutoSave == 3) // Auto Save BOTH
		{
			gamemenu[5][0] = '\0';
			gamemenu[7][0] = '\0';
		}

		// hide cheats menu if cheats file not present
		if(Cheat.num_cheats == 0)
			gamemenu[3][0] = '\0';

		ret = RunMenu (gamemenu, gamemenuCount, (char*)"Game Menu");

		switch (ret)
		{
			case 0: // Return to Game
				quit = retval = 1;
				break;

			case 1: // Reset Game
				S9xSoftReset ();
				quit = retval = 1;
				break;

			case 2: // ROM Information
				RomInfo();
				WaitButtonA ();
				break;

			case 3: // load cheats
				CheatMenu();
				break;

			case 4: // Load SRAM
				quit = retval = LoadSRAM(GCSettings.SaveMethod, NOTSILENT);
				break;

			case 5: // Save SRAM
				SaveSRAM(GCSettings.SaveMethod, NOTSILENT);
				break;

			case 6: // Load Freeze
				quit = retval = NGCUnfreezeGame (GCSettings.SaveMethod, NOTSILENT);
				break;

			case 7: // Save Freeze
				NGCFreezeGame (GCSettings.SaveMethod, NOTSILENT);
				break;

			case 8:	// Reset Zoom
				zoom_reset ();
				quit = retval = 1;
				break;

			case -1: // Button B
			case 9: // Return to previous menu
				retval = 0;
				quit = 1;
				break;
		}
	}

	menu = oldmenu;

	return retval;
}

/****************************************************************************
 * File Options Menu
 ***************************************************************************/
static int filemenuCount = 8;
static char filemenu[][50] = {
	"Load Method",
	"Load Folder",
	"Save Method",
	"Save Folder",

	"Auto Load",
	"Auto Save",
	"Verify MC Saves",

	"Back to Preferences Menu"
};

void
FileOptions ()
{
	int ret = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;
	while (quit == 0)
	{
		// some load/save methods are not implemented - here's where we skip them
		// they need to be skipped in the order they were enumerated in snes9xGX.h

		// no USB ports on GameCube
		#ifndef HW_RVL
		if(GCSettings.LoadMethod == METHOD_USB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_USB)
			GCSettings.SaveMethod++;
		#endif

		// saving to DVD is impossible
		if(GCSettings.SaveMethod == METHOD_DVD)
			GCSettings.SaveMethod++;

		// disable SMB in GC mode (stalls out)
		#ifndef HW_RVL
		if(GCSettings.LoadMethod == METHOD_SMB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_SMB)
			GCSettings.SaveMethod++;
		#endif

		// disable MC saving in Wii mode - does not work for some reason!
		#ifdef HW_RVL
		if(GCSettings.SaveMethod == METHOD_MC_SLOTA)
			GCSettings.SaveMethod++;
		if(GCSettings.SaveMethod == METHOD_MC_SLOTB)
			GCSettings.SaveMethod++;
		#endif

		// correct load/save methods out of bounds
		if(GCSettings.LoadMethod > 4)
			GCSettings.LoadMethod = 0;
		if(GCSettings.SaveMethod > 6)
			GCSettings.SaveMethod = 0;

		if (GCSettings.LoadMethod == METHOD_AUTO) sprintf (filemenu[0],"Load Method AUTO");
		else if (GCSettings.LoadMethod == METHOD_SD) sprintf (filemenu[0],"Load Method SD");
		else if (GCSettings.LoadMethod == METHOD_USB) sprintf (filemenu[0],"Load Method USB");
		else if (GCSettings.LoadMethod == METHOD_DVD) sprintf (filemenu[0],"Load Method DVD");
		else if (GCSettings.LoadMethod == METHOD_SMB) sprintf (filemenu[0],"Load Method Network");

		sprintf (filemenu[1], "Load Folder %s",	GCSettings.LoadFolder);

		if (GCSettings.SaveMethod == METHOD_AUTO) sprintf (filemenu[2],"Save Method AUTO");
		else if (GCSettings.SaveMethod == METHOD_SD) sprintf (filemenu[2],"Save Method SD");
		else if (GCSettings.SaveMethod == METHOD_USB) sprintf (filemenu[2],"Save Method USB");
		else if (GCSettings.SaveMethod == METHOD_SMB) sprintf (filemenu[2],"Save Method Network");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTA) sprintf (filemenu[2],"Save Method MC Slot A");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTB) sprintf (filemenu[2],"Save Method MC Slot B");

		sprintf (filemenu[3], "Save Folder %s",	GCSettings.SaveFolder);

		// disable changing load/save directories for now
		filemenu[1][0] = '\0';
		filemenu[3][0] = '\0';

		if (GCSettings.AutoLoad == 0) sprintf (filemenu[4],"Auto Load OFF");
		else if (GCSettings.AutoLoad == 1) sprintf (filemenu[4],"Auto Load SRAM");
		else if (GCSettings.AutoLoad == 2) sprintf (filemenu[4],"Auto Load SNAPSHOT");

		if (GCSettings.AutoSave == 0) sprintf (filemenu[5],"Auto Save OFF");
		else if (GCSettings.AutoSave == 1) sprintf (filemenu[5],"Auto Save SRAM");
		else if (GCSettings.AutoSave == 2) sprintf (filemenu[5],"Auto Save SNAPSHOT");
		else if (GCSettings.AutoSave == 3) sprintf (filemenu[5],"Auto Save BOTH");

		sprintf (filemenu[6], "Verify MC Saves %s",
			GCSettings.VerifySaves == true ? " ON" : "OFF");

		ret = RunMenu (filemenu, filemenuCount, (char*)"Save/Load Options");

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod ++;
				break;

			case 1:
				break;

			case 2:
				GCSettings.SaveMethod ++;
				break;

			case 3:
				break;

			case 4:
				GCSettings.AutoLoad ++;
				if (GCSettings.AutoLoad > 2)
					GCSettings.AutoLoad = 0;
				break;

			case 5:
				GCSettings.AutoSave ++;
				if (GCSettings.AutoSave > 3)
					GCSettings.AutoSave = 0;
				break;

			case 6:
				GCSettings.VerifySaves ^= 1;
				break;
			case -1: /*** Button B ***/
			case 7:
				quit = 1;
				break;

		}
	}
	menu = oldmenu;
}

/****************************************************************************
 * Video Options
 ***************************************************************************/
static int videomenuCount = 10;
static char videomenu[][50] = {

	"Enable Zooming",
	"Video Rendering",
	"Video Scaling",

	"Shift Video Up",
	"Shift Video Down",
	"Shift Video Left",
	"Shift Video Right",

	"Video Shift:       ",
	"Reset Video Shift",

	"Back to Preferences Menu"
};

void
VideoOptions ()
{
	int ret = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;
	while (quit == 0)
	{
		sprintf (videomenu[0], "Enable Zooming %s",
			GCSettings.Zoom == true ? " ON" : "OFF");

		// don't allow original render mode if progressive video mode detected
		if (GCSettings.render==0 && progressive)
			GCSettings.render++;

		if ( GCSettings.render == 0 )
			sprintf (videomenu[1], "Video Rendering Original");
		if ( GCSettings.render == 1 )
			sprintf (videomenu[1], "Video Rendering Filtered");
		if ( GCSettings.render == 2 )
			sprintf (videomenu[1], "Video Rendering Unfiltered");

		sprintf (videomenu[2], "Video Scaling %s",
			GCSettings.widescreen == true ? "16:9 Correction" : "Default");

		sprintf (videomenu[7], "Video Shift: %d, %d", GCSettings.xshift, GCSettings.yshift);

		ret = RunMenu (videomenu, videomenuCount, (char*)"Video Options");

		switch (ret)
		{
			case 0:
				GCSettings.Zoom ^= 1;
				break;

			case 1:
				GCSettings.render++;
				if (GCSettings.render > 2 )
					GCSettings.render = 0;
				// reset zoom
				zoom_reset ();
				break;

			case 2:
				GCSettings.widescreen ^= 1;
				break;

			case 3:
				// Move up
				GCSettings.yshift--;
				break;
			case 4:
				// Move down
				GCSettings.yshift++;
				break;
			case 5:
				// Move left
				GCSettings.xshift--;
				break;
			case 6:
				// Move right
				GCSettings.xshift++;
				break;

			case 7:
				break;

			case 8:
				// reset video shifts
				GCSettings.xshift = GCSettings.yshift = 0;
				WaitPrompt((char *)"Video Shift Reset");
				break;

			case -1: // Button B
			case 9:
				quit = 1;
				break;

		}
	}
	menu = oldmenu;
}

/****************************************************************************
 * Controller Configuration
 *
 * Snes9x 1.51 uses a cmd system to work out which button has been pressed.
 * Here, I simply move the designated value to the gcpadmaps array, which
 * saves on updating the cmd sequences.
 ***************************************************************************/
u32
GetInput (u16 ctrlr_type)
{
	//u32 exp_type;
	u32 pressed;
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
char cfg_text[][50] = {
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
	char temp[50] = "";
	uint k;
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
	for (k=0; k<9-strlen(btn_name); k++) strcat(temp, " "); // add whitespace padding to align text
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
char cfg_btns_menu[][50] = {
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
	char* menu_title = NULL;
	u32 pressed;

	unsigned int* currentpadmap = 0;
	char temp[50] = "";
	int i, j;
	uint k;

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

		ret = RunMenu (cfg_btns_menu, cfg_btns_count, menu_title, 16);

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

int ctlrmenucount = 9;
char ctlrmenu[][50] = {
	// toggle:
	"MultiTap",
	"SuperScope",
	"Snes Mice",
	"Justifiers",
	// config:
	"Nunchuk",
	"Classic Controller",
	"Wiimote",
	"Gamecube Pad",
	"Back to Preferences Menu"
};

void
ConfigureControllers ()
{
	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;

	// disable unavailable controller options if in GC mode
	#ifndef HW_RVL
		ctlrmenu[4][0] = '\0';
		ctlrmenu[5][0] = '\0';
		ctlrmenu[6][0] = '\0';
	#endif

	while (quit == 0)
	{
		sprintf (ctlrmenu[0], "MultiTap %s", Settings.MultiPlayer5Master == true ? " ON" : "OFF");

		if (GCSettings.Superscope > 0) sprintf (ctlrmenu[1], "Superscope: Pad %d", GCSettings.Superscope);
		else sprintf (ctlrmenu[1], "Superscope     OFF");

		if (GCSettings.Mouse > 0) sprintf (ctlrmenu[2], "Mice:   %d", GCSettings.Mouse);
		else sprintf (ctlrmenu[2], "Mice: OFF");

		if (GCSettings.Justifier > 0) sprintf (ctlrmenu[3], "Justifiers:   %d", GCSettings.Justifier);
		else sprintf (ctlrmenu[3], "Justifiers: OFF");

		/*** Controller Config Menu ***/
        ret = RunMenu (ctlrmenu, ctlrmenucount, (char*)"Configure Controllers");

		switch (ret)
		{
			case 0:
				Settings.MultiPlayer5Master ^= 1;
				break;
			case 1:
				GCSettings.Superscope ++;
				if (GCSettings.Superscope > 4)
					GCSettings.Superscope = 0;
				break;
			case 2:
				GCSettings.Mouse ++;
				if (GCSettings.Mouse > 2)
					GCSettings.Mouse = 0;
				break;
			case 3:
				GCSettings.Justifier ++;
				if (GCSettings.Justifier > 2)
					GCSettings.Justifier = 0;
				break;

			case 4:
				/*** Configure Nunchuk ***/
				ConfigureButtons (CTRLR_NUNCHUK);
				break;

			case 5:
				/*** Configure Classic ***/
				ConfigureButtons (CTRLR_CLASSIC);
				break;

			case 6:
				/*** Configure Wiimote ***/
				ConfigureButtons (CTRLR_WIIMOTE);
				break;

			case 7:
				/*** Configure GC Pad ***/
				ConfigureButtons (CTRLR_GCPAD);
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
 * Preferences Menu
 ***************************************************************************/
static int prefmenuCount = 5;
static char prefmenu[][50] = {
	"Controllers",
	"Video",
	"Saving / Loading",
	"Reset Preferences",
	"Back to Main Menu"
};

void
PreferencesMenu ()
{
	int ret = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;
	while (quit == 0)
	{
		ret = RunMenu (prefmenu, prefmenuCount, (char*)"Preferences");

		switch (ret)
		{
			case 0:
				ConfigureControllers ();
				break;

			case 1:
				VideoOptions ();
				break;

			case 2:
				FileOptions ();
				break;

			case 3:
				DefaultSettings ();
				WaitPrompt((char *)"Preferences Reset");
				break;

			case -1: /*** Button B ***/
			case 4:
				SavePrefs(GCSettings.SaveMethod, SILENT);
				quit = 1;
				break;

		}
	}
	menu = oldmenu;
}

/****************************************************************************
 * Main Menu
 ***************************************************************************/
int menucount = 7;
char menuitems[][50] = {
  "Choose Game",
  "Preferences",
  "Game Menu",
  "Credits",
  "DVD Motor Off",
  "Reset System",
  "Return to Loader"
};

void
MainMenu (int selectedMenu)
{
	int quit = 0;
	int ret;

	// disable game-specific menu items if a ROM isn't loaded
	if (SNESROMSize == 0)
    	menuitems[2][0] = '\0';
	else
		sprintf (menuitems[2], "Game Menu");

	#ifdef HW_RVL
	// don't show dvd motor off on the wii
	menuitems[4][0] = 0;
	// rename reset/exit items
	sprintf (menuitems[5], "Return to Wii Menu");
	sprintf (menuitems[6], "Return to Homebrew Channel");
	#endif

	VIDEO_WaitVSync ();

	while (quit == 0)
	{
		if(selectedMenu >= 0)
		{
			ret = selectedMenu;
			selectedMenu = -1; // default back to main menu
		}
		else
		{
			ret = RunMenu (menuitems, menucount, (char*)"Main Menu");
		}

		switch (ret)
		{
			case 0:
				// Load ROM Menu
				quit = LoadManager ();
				break;

			case 1:
				// Preferences
				PreferencesMenu ();
				break;

			case 2:
				// Game Options
				quit = GameMenu ();
				break;

			case 3:
				// Credits
				Credits ();
				WaitButtonA ();
                break;

			case 4:
				// turn the dvd motor off (GC only)
				#ifdef HW_DOL
				dvd_motor_off ();
				#endif
				break;

			case 5:
				// Reset the Gamecube/Wii
			    Reboot();
                break;

			case 6:
				ExitToLoader();
				break;

			case -1: // Button B
				// Return to Game
				quit = 1;
				break;
		}
	}

	// Wait for buttons to be released
	int count = 0; // how long we've been waiting for the user to release the button
	while(count < 50 && (
		PAD_ButtonsHeld(0)
		#ifdef HW_RVL
		|| WPAD_ButtonsHeld(0)
		#endif
	))
	{
		VIDEO_WaitVSync();
		count++;
	}
}
