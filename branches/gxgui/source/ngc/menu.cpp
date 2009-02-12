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

#ifdef HW_RVL
extern "C" {
#include <di/di.h>
}
#endif

#include "snes9x.h"
#include "memmap.h"
#include "s9xdebug.h"
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
#include "gcunzip.h"
#include "networkop.h"
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
#include "filter.h"

#include "filelist.h"
#include "GRRLIB.h"
#include "gui/gui.h"
#include "menu.h"

extern int menu;
static GuiImage * gameScreenImg = NULL;

int rumbleCount[4] = {0,0,0,0};
int rumbleRequest[4] = {0,0,0,0};
int ExitRequested = 0;
static GuiTrigger userInput[4];
static GuiImageData * pointer[4];
static GuiWindow * mainWindow = NULL;

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
		WaitPrompt("No cheats found!");
	}
	menu = oldmenu;
}

/****************************************************************************
 * Game Options Menu
 ***************************************************************************/
/*
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

		ret = RunMenu (gamemenu, gamemenuCount, "Game Menu");

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
*/
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
		filemenu[6][0] = 0;
		#else
		sprintf (filemenu[6], "Verify MC Saves %s", GCSettings.VerifySaves == true ? " ON" : "OFF");
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

		ret = RunMenu (filemenu, filemenuCount, "Save/Load Options");

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
static int videomenuCount = 11;
static char videomenu[][50] = {

	"Enable Zooming",
	"Video Rendering",
	"Video Scaling",
	"Video Filtering",

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

		sprintf (videomenu[3], "Video Filtering %s", GetFilterName((RenderFilter)GCSettings.FilterMethod));

		sprintf (videomenu[8], "Video Shift: %d, %d", GCSettings.xshift, GCSettings.yshift);

		ret = RunMenu (videomenu, videomenuCount, "Video Options");

		switch (ret)
		{
			case 0:
				GCSettings.Zoom ^= 1;
				break;

			case 1:
				GCSettings.render++;
				if (GCSettings.render > 2 || GCSettings.FilterMethod != FILTER_NONE)
					GCSettings.render = 0;
				// reset zoom
				zoom_reset ();
				break;

			case 2:
				GCSettings.widescreen ^= 1;
				break;

			case 3:
				GCSettings.FilterMethod++;
				if (GCSettings.FilterMethod >= NUM_FILTERS)
					GCSettings.FilterMethod = 0;
				SelectFilterMethod();
				break;

			case 4:
				// Move up
				GCSettings.yshift--;
				break;
			case 5:
				// Move down
				GCSettings.yshift++;
				break;
			case 6:
				// Move left
				GCSettings.xshift--;
				break;
			case 7:
				// Move right
				GCSettings.xshift++;
				break;

			case 8:
				break;

			case 9:
				// reset video shifts
				GCSettings.xshift = GCSettings.yshift = 0;
				WaitPrompt("Video Shift Reset");
				break;

			case -1: // Button B
			case 10:
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

static int cfg_text_count = 7;
static char cfg_text[][50] = {
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
			strncpy (cfg_text[3], "NUNCHUK", 7);
			break;
		case CTRLR_CLASSIC:
			strncpy (cfg_text[3], "CLASSIC", 7);
			break;
		case CTRLR_GCPAD:
			strncpy (cfg_text[3], "GC PAD", 7);
			break;
		case CTRLR_WIIMOTE:
			strncpy (cfg_text[3], "WIIMOTE", 7);
			break;
	};

	/*** note which button we are remapping ***/
	sprintf (temp, "Remapping ");
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

static int cfg_btns_count = 13;
static char cfg_btns_menu[][50] = {
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

void
ConfigureButtons (u16 ctrlr_type)
{
	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;
	char menu_title[50];
	u32 pressed;

	unsigned int* currentpadmap = 0;
	char temp[50] = "";
	int i, j;
	uint k;

	/*** Update Menu Title (based on controller we're configuring) ***/
	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			sprintf(menu_title, "SNES     -  NUNCHUK");
			currentpadmap = ncpadmap;
			break;
		case CTRLR_CLASSIC:
			sprintf(menu_title, "SNES     -  CLASSIC");
			currentpadmap = ccpadmap;
			break;
		case CTRLR_GCPAD:
			sprintf(menu_title, "SNES     -   GC PAD");
			currentpadmap = gcpadmap;
			break;
		case CTRLR_WIIMOTE:
			sprintf(menu_title, "SNES     -  WIIMOTE");
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
				strcat (temp, "---");								// otherwise, button is 'unmapped'
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
        ret = RunMenu (ctlrmenu, ctlrmenucount, "Configure Controllers");

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
		ret = RunMenu (prefmenu, prefmenuCount, "Preferences");

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
				WaitPrompt("Preferences Reset");
				break;

			case -1: /*** Button B ***/
			case 4:
				SavePrefs(SILENT);
				quit = 1;
				break;

		}
	}
	menu = oldmenu;
}

/****************************************************************************
 * Main Menu
 ***************************************************************************/

void ShutoffRumble()
{
	for(int i=0;i<4;i++)
	{
		WPAD_Rumble(i, 0);
		rumbleCount[i] = 0;
	}
}

void UpdateMenu()
{
	mainWindow->Draw();

	for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
	{
		#ifdef HW_RVL
		memcpy(&userInput[i].wpad, WPAD_Data(i), sizeof(WPADData));

		if(userInput[i].wpad.ir.valid)
		{
			GRRLIB_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48, 96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
		}
		#endif

		userInput[i].chan = i;
		userInput[i].pad.button = PAD_ButtonsDown(i);
		userInput[i].pad.stickX = PAD_StickX(i);
		userInput[i].pad.stickY = PAD_StickY(i);
		userInput[i].pad.substickX = PAD_SubStickX(i);
		userInput[i].pad.substickY = PAD_SubStickY(i);
		userInput[i].pad.triggerL = PAD_TriggerL(i);
		userInput[i].pad.triggerR = PAD_TriggerR(i);

		mainWindow->Update(&userInput[i]);

		#ifdef HW_RVL
		if(rumbleRequest[i] && rumbleCount[i] < 3)
		{
			WPAD_Rumble(i, 1); // rumble on
			rumbleCount[i]++;
		}
		else if(rumbleRequest[i])
		{
			rumbleCount[i] = 12;
			rumbleRequest[i] = 0;
		}
		else
		{
			if(rumbleCount[i])
				rumbleCount[i]--;
			WPAD_Rumble(i, 0); // rumble off
		}
		#endif
	}

	GRRLIB_Render();

	if(ExitRequested)
		ExitToLoader();

	#ifdef HW_RVL
	if(updateFound)
	{
		updateFound = WaitPromptChoice("An update is available!", "Update later", "Update now");
		if(updateFound)
			if(DownloadUpdate())
				ExitToLoader();
	}

	if(ShutdownRequested)
		ShutdownWii();
	#endif
}

/****************************************************************************
 * GUI Thread
 ***************************************************************************/
lwp_t guithread = LWP_THREAD_NULL;

/****************************************************************************
 * guicallback
 ***************************************************************************/
static bool guiReady = false;
static void *
guicallback (void *arg)
{
	while(1)
	{
		if(!guiReady)
		{
			usleep(500000); // just a test
		}
		else
		{
			//UpdateMenu();
		}
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 ***************************************************************************/
void
InitGUIThread()
{
	LWP_CreateThread (&guithread, guicallback, NULL, NULL, 0, 70);
	//LWP_SuspendThread (guithread);
}

void
ProgressWindow(const char *title, const char *msg)
{
	int choice = -1;

	GuiWindow promptWindow(448,256);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	throbberImg.SetPosition(0, 40);

	GuiText titleTxt(title, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,10);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 0xff});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&throbberImg);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	guiReady = true;

	float angle = 0;

	while(choice < 1000) // simulates a delay - should actually check a flag
	{
		UpdateMenu();
		choice++;

		if(choice % 5 == 0)
		{
			angle+=45;
			if(angle >= 360)
				angle = 0;
			throbberImg.SetAngle(angle);
		}
	}

	guiReady = false;
	mainWindow->Remove(&promptWindow);
	mainWindow->ResetState();
	guiReady = true;
}

int
WaitPromptChoiceNew(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,256);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,10);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 0xff});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 0xff});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	btn1.SetPosition(25, -25);
	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 0xff});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-25, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);
	promptWindow.Append(&btn2);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	guiReady = true;

	while(choice == -1)
	{
		UpdateMenu();

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}
	guiReady = false;
	mainWindow->Remove(&promptWindow);
	mainWindow->ResetState();
	guiReady = true;
	return choice;
}

int GameMenu()
{
	int menu = MENU_NONE;

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText saveBtnTxt("Save", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage saveBtnImg(&btnLargeOutline);
	GuiImage saveBtnImgOver(&btnLargeOutlineOver);
	GuiButton saveBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	saveBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	saveBtn.SetPosition(50, 120);
	saveBtn.SetLabel(&saveBtnTxt);
	saveBtn.SetImage(&saveBtnImg);
	saveBtn.SetImageOver(&saveBtnImgOver);
	saveBtn.SetSoundOver(&btnSoundOver);
	saveBtn.SetTrigger(&trigA);

	GuiText loadBtnTxt("Load", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage loadBtnImg(&btnLargeOutline);
	GuiImage loadBtnImgOver(&btnLargeOutlineOver);
	GuiButton loadBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	loadBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	loadBtn.SetPosition(0, 120);
	loadBtn.SetLabel(&loadBtnTxt);
	loadBtn.SetImage(&loadBtnImg);
	loadBtn.SetImageOver(&loadBtnImgOver);
	loadBtn.SetSoundOver(&btnSoundOver);
	loadBtn.SetTrigger(&trigA);

	GuiText resetBtnTxt("Reset", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage resetBtnImg(&btnLargeOutline);
	GuiImage resetBtnImgOver(&btnLargeOutlineOver);
	GuiButton resetBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	resetBtn.SetPosition(-50, 120);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);

	GuiText controllerBtnTxt("Controller", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage controllerBtnImg(&btnLargeOutline);
	GuiImage controllerBtnImgOver(&btnLargeOutlineOver);
	GuiButton controllerBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	controllerBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	controllerBtn.SetPosition(-125, 250);
	controllerBtn.SetLabel(&controllerBtnTxt);
	controllerBtn.SetImage(&controllerBtnImg);
	controllerBtn.SetImageOver(&controllerBtnImgOver);
	controllerBtn.SetSoundOver(&btnSoundOver);
	controllerBtn.SetTrigger(&trigA);

	GuiText cheatsBtnTxt("Cheats", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage cheatsBtnImg(&btnLargeOutline);
	GuiImage cheatsBtnImgOver(&btnLargeOutlineOver);
	GuiButton cheatsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	cheatsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	cheatsBtn.SetPosition(125, 250);
	cheatsBtn.SetLabel(&cheatsBtnTxt);
	cheatsBtn.SetImage(&cheatsBtnImg);
	cheatsBtn.SetImageOver(&cheatsBtnImgOver);
	cheatsBtn.SetSoundOver(&btnSoundOver);
	cheatsBtn.SetTrigger(&trigA);

	GuiText backBtnTxt("Main Menu", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	backBtn.SetPosition(0, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage closeBtnImg(&btnOutline);
	GuiImage closeBtnImgOver(&btnOutlineOver);
	GuiButton closeBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-30, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&saveBtn);
	w.Append(&loadBtn);
	w.Append(&resetBtn);
	w.Append(&controllerBtn);
	w.Append(&cheatsBtn);

	w.Append(&backBtn);
	w.Append(&closeBtn);

	mainWindow->Append(&w);

	guiReady = true;

	while(menu == MENU_NONE)
	{
		UpdateMenu();

		if(backBtn.GetState() == STATE_CLICKED)
		{
			if(gameScreenImg)
			{
				mainWindow->Remove(gameScreenImg);
				delete gameScreenImg;
				gameScreenImg = NULL;
				free(gameScreenTex);
				gameScreenTex = NULL;
			}

			menu = MENU_GAMESELECTION;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			S9xSoftReset ();
			menu = MENU_EXIT;
		}
	}
	guiReady = false;
	mainWindow->Remove(&w);
	return menu;
}

int SettingsMenu()
{
	int menu = MENU_NONE;

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText mappingBtnTxt("Button Mapping", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage mappingBtnImg(&btnLargeOutline);
	GuiImage mappingBtnImgOver(&btnLargeOutlineOver);
	GuiButton mappingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	mappingBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	mappingBtn.SetPosition(50, 120);
	mappingBtn.SetLabel(&mappingBtnTxt);
	mappingBtn.SetImage(&mappingBtnImg);
	mappingBtn.SetImageOver(&mappingBtnImgOver);
	mappingBtn.SetSoundOver(&btnSoundOver);
	mappingBtn.SetTrigger(&trigA);

	GuiText videoBtnTxt("Video", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage videoBtnImg(&btnLargeOutline);
	GuiImage videoBtnImgOver(&btnLargeOutlineOver);
	GuiButton videoBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	videoBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	videoBtn.SetPosition(0, 120);
	videoBtn.SetLabel(&videoBtnTxt);
	videoBtn.SetImage(&videoBtnImg);
	videoBtn.SetImageOver(&videoBtnImgOver);
	videoBtn.SetSoundOver(&btnSoundOver);
	videoBtn.SetTrigger(&trigA);

	GuiText savingBtnTxt("Saving / Loading", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage savingBtnImg(&btnLargeOutline);
	GuiImage savingBtnImgOver(&btnLargeOutlineOver);
	GuiButton savingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	savingBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	savingBtn.SetPosition(-50, 120);
	savingBtn.SetLabel(&savingBtnTxt);
	savingBtn.SetImage(&savingBtnImg);
	savingBtn.SetImageOver(&savingBtnImgOver);
	savingBtn.SetSoundOver(&btnSoundOver);
	savingBtn.SetTrigger(&trigA);

	GuiText menuBtnTxt("Menu", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage menuBtnImg(&btnLargeOutline);
	GuiImage menuBtnImgOver(&btnLargeOutlineOver);
	GuiButton menuBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	menuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	menuBtn.SetPosition(-125, 250);
	menuBtn.SetLabel(&menuBtnTxt);
	menuBtn.SetImage(&menuBtnImg);
	menuBtn.SetImageOver(&menuBtnImgOver);
	menuBtn.SetSoundOver(&btnSoundOver);
	menuBtn.SetTrigger(&trigA);

	GuiText networkBtnTxt("Network", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage networkBtnImg(&btnLargeOutline);
	GuiImage networkBtnImgOver(&btnLargeOutlineOver);
	GuiButton networkBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	networkBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	networkBtn.SetPosition(125, 250);
	networkBtn.SetLabel(&networkBtnTxt);
	networkBtn.SetImage(&networkBtnImg);
	networkBtn.SetImageOver(&networkBtnImgOver);
	networkBtn.SetSoundOver(&btnSoundOver);
	networkBtn.SetTrigger(&trigA);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiText resetBtnTxt("Reset Settings", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	resetBtn.SetPosition(-100, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&mappingBtn);
	w.Append(&videoBtn);
	w.Append(&savingBtn);
	w.Append(&menuBtn);
	w.Append(&networkBtn);

	w.Append(&backBtn);
	w.Append(&resetBtn);

	mainWindow->Append(&w);

	guiReady = true;

	while(menu == MENU_NONE)
	{
		UpdateMenu();

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESELECTION;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();
			int choice = WaitPromptChoiceNew(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");

			if(choice == 1)
				DefaultSettings ();
		}
		else if(videoBtn.GetState() == STATE_CLICKED)
		{
			ProgressWindow("Please Wait","Loading...");
		}
	}
	guiReady = false;
	mainWindow->Remove(&w);
	return menu;
}

int GameSelectionMenu()
{
	int menu = MENU_NONE;

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText settingsBtnTxt("Settings", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage settingsBtnImg(&btnOutline);
	GuiImage settingsBtnImgOver(&btnOutlineOver);
	GuiButton settingsBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	settingsBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	settingsBtn.SetPosition(100, -35);
	settingsBtn.SetLabel(&settingsBtnTxt);
	settingsBtn.SetImage(&settingsBtnImg);
	settingsBtn.SetImageOver(&settingsBtnImgOver);
	settingsBtn.SetSoundOver(&btnSoundOver);
	settingsBtn.SetTrigger(&trigA);

	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	exitBtn.SetPosition(-100, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);

	GuiText loadBtnTxt("Load", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage loadBtnImg(&btnOutline);
	GuiImage loadBtnImgOver(&btnOutlineOver);
	GuiButton loadBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	loadBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	loadBtn.SetLabel(&loadBtnTxt);
	loadBtn.SetImage(&loadBtnImg);
	loadBtn.SetImageOver(&loadBtnImgOver);
	loadBtn.SetSoundOver(&btnSoundOver);
	loadBtn.SetTrigger(&trigA);

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.Append(&settingsBtn);
	buttonWindow.Append(&exitBtn);
	buttonWindow.Append(&loadBtn);

	GuiWindow gameWindow(424, 256);
	gameWindow.SetPosition(50, 103);

	GuiImageData bgGameSelection(bg_game_selection_png);
	GuiImage bgGameSelectionImg(&bgGameSelection);
	bgGameSelectionImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	GuiImageData bgGameSelectionEntry(bg_game_selection_entry_png);

	GuiImageData scrollbar(scrollbar_png);
	GuiImage scrollbarImg(&scrollbar);
	scrollbarImg.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarImg.SetPosition(0, 30);

	GuiImageData arrowDown(scrollbar_arrowdown_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(scrollbar_arrowdown_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiImageData arrowUp(scrollbar_arrowup_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(scrollbar_arrowup_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiImageData scrollbarBox(scrollbar_box_png);
	GuiImage scrollbarBoxImg(&scrollbarBox);
	GuiImageData scrollbarBoxOver(scrollbar_box_over_png);
	GuiImage scrollbarBoxOverImg(&scrollbarBoxOver);

	GuiButton arrowUpBtn(arrowUpImg.GetWidth(), arrowUpImg.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	arrowUpBtn.SetSelectable(false);

	GuiButton arrowDownBtn(arrowDownImg.GetWidth(), arrowDownImg.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	arrowDownBtn.SetSelectable(false);

	GuiButton scrollbarBoxBtn(scrollbarBoxImg.GetWidth(), scrollbarBoxImg.GetHeight());
	scrollbarBoxBtn.SetImage(&scrollbarBoxImg);
	scrollbarBoxBtn.SetImageOver(&scrollbarBoxOverImg);
	scrollbarBoxBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn.SetPosition(0,100);
	scrollbarBoxBtn.SetSelectable(false);

	gameWindow.Append(&bgGameSelectionImg);

	GuiText * gameListText[7];
	GuiButton * gameList[7];
	GuiImage * gameListBg[7];

	for(int i=0; i<7; i++)
	{
		gameListText[i] = new GuiText("Game",22, (GXColor){0, 0, 0, 0xff});
		gameListText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		gameListText[i]->SetPosition(5,0);

		gameListBg[i] = new GuiImage(&bgGameSelectionEntry);

		gameList[i] = new GuiButton(400,32);
		gameList[i]->SetLabel(gameListText[i]);
		gameList[i]->SetImageOver(gameListBg[i]);
		gameList[i]->SetPosition(2,32*i+2);
		gameWindow.Append(gameList[i]);
	}

	gameWindow.Append(&scrollbarImg);
	gameWindow.Append(&arrowUpBtn);
	gameWindow.Append(&arrowDownBtn);
	gameWindow.Append(&scrollbarBoxBtn);

	// populate initial directory listing

	guiReady = false;
	mainWindow->Append(&buttonWindow);
	mainWindow->Append(&gameWindow);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		UpdateMenu();

		// update gameWindow based on arrow buttons
		// set MENU_EXIT if A button pressed on a game

		if(settingsBtn.GetState() == STATE_CLICKED)
			menu = MENU_SETTINGS;
		else if(exitBtn.GetState() == STATE_CLICKED)
			ExitRequested = 1;
		else if(loadBtn.GetState() == STATE_CLICKED)
			menu = MENU_EXIT;
	}
	guiReady = false;
	mainWindow->Remove(&buttonWindow);
	mainWindow->Remove(&gameWindow);
	return menu;
}

void
MainMenu (int menu)
{
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	if(gameScreenTex)
	{
		gameScreenImg = new GuiImage(gameScreenTex, screenwidth, screenheight);
		mainWindow->Append(gameScreenImg);
	}

	GuiImageData bgTop(bg_top_png);
	GuiImage bgTopImg(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	GuiImage bgBottomImg(&bgBottom);
	bgBottomImg.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	mainWindow->Append(&bgTopImg);
	mainWindow->Append(&bgBottomImg);

	// memory usage - for debugging
	volatile u32 sysmem = 0;
	char mem[150];
	GuiText memTxt(mem, 22, (GXColor){255, 255, 255, 0xff});
	memTxt.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	memTxt.SetPosition(-20, 40);
	mainWindow->Append(&memTxt);

	LWP_ResumeThread (guithread);

	while(menu != MENU_EXIT)
	{
		sysmem = (u32)SYS_GetArena1Hi() - (u32)SYS_GetArena1Lo();
		sprintf(mem, "%u", sysmem);
		memTxt.SetText(mem);

		switch (menu)
		{
			case MENU_GAMESELECTION:
				menu = GameSelectionMenu();
				break;
			case MENU_GAME:
				menu = GameMenu();
				break;
			case MENU_SETTINGS:
				menu = SettingsMenu();
				break;
			default: // unrecognized menu
				menu = MENU_EXIT;
				break;
		}
	}

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	guiReady = false;
	LWP_SuspendThread (guithread);
	delete mainWindow;
	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];
	mainWindow = NULL;

	if(gameScreenImg)
	{
		delete gameScreenImg;
		gameScreenImg = NULL;
		free(gameScreenTex);
		gameScreenTex = NULL;
	}
}
