/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Michniewski 2008
 *
 * menu2.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiiuse/wpad.h>

#include "new_gui.h"

// HEADER STUFF
#define PI 				3.14159265f
#define PADCAL			50
#define		STICK_LEFT		0
#define		STICK_RIGHT		1
#define		STICK_X_AXIS	0
#define		STICK_Y_AXIS	1
s8 WPAD_Stick(u8 chan, u8 right, int axis);


// FILE:

typedef struct sItems {
	char* name;		// item name
	char* desc;		// item description
}sItems;
typedef struct sSection {
	int numitems;	// number of items in the menu
	int selection;	// current item selection
	char* title;	// section title
	void* icon;		// section icon
	u32 icon_size;	// size of icon in bytes
	sItems * item;	// array of items
}sSection;
typedef struct sTopMenu {
	int menuitems;	// number of sections on the topmenu
	int selection;	// current section
	sSection section[5];	// array of sections
}sTopMenu;

sItems main_items[4] = {	// Main
				{ (char*)"Credits", (char*)"Meet all the people it took to make you this emulator" },
				{ (char*)"DVD Motor Off", (char*)"Turns off your drive if it is spinning" },
				{ (char*)"Reset Wii", (char*)"Back to the System Menu" },
				{ (char*)"Return to Loader", (char*)"Go back to the Homebrew Channel, or the loader you used" }
};

sItems video_items[11] = {	// Video
				{ (char*)"Transparency", (char*)"Enable Snes9x transparency effects (runs faster with this disabled, on GC)" },
				{ (char*)"Display Frame Rate", (char*)"Show or hide how many frames are rendered per second" },
				{ (char*)"Enable Zooming", (char*)"Enable or disable zooming. Use the C-stick on the GC Pad or Right stick on the Classic Controller" },
				{ (char*)"Video Rendering", (char*)"Original SNES video mode, Filtered video, or Unfiltered video" },
				{ (char*)"Widescreen", (char*)"Enable fullscreen stretch or disable for 16:9 compensation" },

				{ (char*)"Shift Video Up", (char*)"" },
				{ (char*)"Shift Video Down", (char*)"" },
				{ (char*)"Shift Video Left", (char*)"" },
				{ (char*)"Shift Video Right", (char*)"" },

				{ (char*)"Shift:       ", (char*)"" },
				{ (char*)"Reset Video Shift", (char*)"Reset Video Shifting to zero" }
};

sItems file_items[7] = {	// File Options
				{ (char*)"Load Method", (char*)"" },
				{ (char*)"Load Folder", (char*)"" },
				{ (char*)"Save Method", (char*)"" },
				{ (char*)"Save Folder", (char*)"" },
				{ (char*)"Auto Load", (char*)"" },
				{ (char*)"Auto Save", (char*)"" },
				{ (char*)"Verify MC Saves", (char*)"" },
};

sItems game_items[9] = {	// Game
				{ (char*)"Return to Game", (char*)"" },
				{ (char*)"Reset Game", (char*)"" },
				{ (char*)"ROM Information", (char*)"" },
				{ (char*)"Cheats", (char*)"" },
				{ (char*)"Load SRAM", (char*)"" },
				{ (char*)"Save SRAM", (char*)"" },
				{ (char*)"Load Game Snapshot", (char*)"" },
				{ (char*)"Save Game Snapshot", (char*)"" },
				{ (char*)"Reset Zoom", (char*)"" },
};

sItems controller_items[8] = {	// Controller Options 
				// toggle:
				{ (char*)"MultiTap", (char*)"" },
				{ (char*)"SuperScope", (char*)"" },
				{ (char*)"Snes Mice", (char*)"" },
				{ (char*)"Justifiers", (char*)"" },
				// config:
				{ (char*)"Nunchuk", (char*)"" },
				{ (char*)"Classic Controller", (char*)"" },
				{ (char*)"Wiimote", (char*)"" },
				{ (char*)"Gamecube Pad", (char*)"" },
};


sTopMenu Menu = { 
	5,	// menuitems
	0,	// selection
	{	// array of sections
		// main
		{	
			4,							// num of items
			0,							// selection
			(char*)"Main",				// title
			0,							// pointer to icon
			0,							// size of icon in bytes
			(sItems*) main_items		// ptr to array of items
		},
		
		// video
		{	
			11,							// num of items
			0,							// selection
			(char*)"Video",				// title
			0,							// pointer to icon
			0,							// size of icon in bytes
			(sItems*) video_items		// ptr to array of items
		},
		
		// file
		{	
			7,							// num of items
			0,							// selection
			(char*)"File",			// title
			0,							// pointer to icon
			0,							// size of icon in bytes
			(sItems*) file_items	// ptr to array of items
		},
		// game
		{	
			9,							// num of items
			0,							// selection
			(char*)"Game",			// title
			0,							// pointer to icon
			0,							// size of icon in bytes
			(sItems*) game_items	// ptr to array of items
		},
		// controller
		{	
			8,							// num of items
			0,							// selection
			(char*)"Controller",			// title
			0,							// pointer to icon
			0,							// size of icon in bytes
			(sItems*) controller_items	// ptr to array of items
		}
	}	// end section array

};

int findmenuitem(int direction)
{
	int* section = &(Menu.selection);
	int* item = &(Menu.section[*section].selection);
	int* numitems = &(Menu.section[*section].numitems);

	int nextItem = *item + direction;

	if(nextItem < 0)
		nextItem = *numitems-1;
	else if(nextItem >= *numitems)
		nextItem = 0;

	if(strlen(Menu.section[*section].item[*item].name) > 0)
		return nextItem;
	else
		return findmenuitem(direction);
}

void
gui_DrawMenu()
{
	#define LINES_PER_PAGE	5
	
	int section;
	int item;
	int numitems;
	
	int fontsize;
	int ypos;
	int line_height;
	
	int i, line;
	
	// set up
	gui_clearscreen();
	gui_draw();	// show the backdrop
	
	// render the section we are in

	section = Menu.selection;
	item = Menu.section[section].selection;
	numitems = Menu.section[section].numitems;
	
	// top bar text
	gui_setfontcolour (0,0,0,255);	// black
	fontsize = 32;
	line_height = fontsize + 8;
	ypos = 55;
	setfontsize (fontsize);
	gui_DrawText (-1, ypos, Menu.section[section].title);	// print the title of the section
	
	// draw list
	gui_setfontcolour (10,10,10,255);
	fontsize = 24;
	line_height = fontsize + 8;
	setfontsize (fontsize);	
	ypos = 113;
	if (item == 0)	// first item
	{
		for (i=0, line=0; i<numitems && line<LINES_PER_PAGE ;i++, line++)
		{
			if (i == item)
			{	gui_drawbox (30, line * line_height + (ypos-line_height+6), 610, line_height, 255, 0, 0, 150);	}
				
			gui_DrawText (75, ypos + line * line_height, Menu.section[section].item[i].name);
		}
		// select item
	} 
	else if (numitems>LINES_PER_PAGE && numitems - item > LINES_PER_PAGE-1 )	// if we aren't on the last page yet
	{
		for (i=item-1, line=0; i<numitems && line<LINES_PER_PAGE ;i++, line++)
		{
			if (i == item)
			{	gui_drawbox (30, line * line_height + (ypos-line_height+6), 610, line_height, 255, 0, 0, 150);	}
				
			gui_DrawText (75, ypos + line * line_height, Menu.section[section].item[i].name);
			//cout << Menu.section[section].item[i].name << endl;
		}
		// select item
	}
	else	// we're on the last page
	{
		for (i=((numitems>LINES_PER_PAGE) ? numitems-LINES_PER_PAGE : 0), line=0; i<numitems ;i++, line++)
		{
			if (i == item)
			{	gui_drawbox (30, line * line_height + (ypos-line_height+6), 610, line_height, 255, 0, 0, 150);	}
				
			gui_DrawText (75, ypos + line * line_height, Menu.section[section].item[i].name);
			//cout << Menu.section[section].item[i].name << endl;
		}
		// select item
	}
	
	// show the description
	
	// bottom bar text
	gui_setfontcolour (50,50,50,255);
	ypos = 400;
	setfontsize (24);	
	gui_DrawText (75, ypos,  Menu.section[section].item[item].desc);
	
	// voila
	gui_showscreen ();

}

int
gui_RunMenu ()
{
    int redraw = 1;
    int quit = 0;
    int ret = 0;
	
	int* section = &(Menu.selection);
	int* item = &(Menu.section[*section].selection);
	//int* numitems = &(Menu.section[*section].numitems);

    u32 p = 0;
	u32 wp = 0;
    signed char gc_ay = 0;
	signed char gc_ax = 0;
	signed char wm_ay = 0;
	signed char wm_ax = 0;

    while (quit == 0)
    {
        if (redraw)
        {
            gui_DrawMenu ();
            redraw = 0;
        }

		gc_ay = PAD_StickY (0);
		gc_ax = PAD_StickX (0);
		p = PAD_ButtonsDown (0);
		#ifdef HW_RVL
		wm_ay = WPAD_Stick (0, STICK_LEFT, STICK_Y_AXIS);
		wm_ax = WPAD_Stick (0, STICK_LEFT, STICK_X_AXIS);
		wp = WPAD_ButtonsDown (0);
		#endif


		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads

        /*** Look for up ***/
        if ( (p & PAD_BUTTON_UP) || (wp & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (gc_ay > PADCAL) || (wm_ay > PADCAL) )
        {
            redraw = 1;
            //menu = FindMenuItem(&items[0], maxitems, menu, -1);
			*item = findmenuitem(-1);
        }

        /*** Look for down ***/
        else if ( (p & PAD_BUTTON_DOWN) || (wp & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (gc_ay < -PADCAL) || (wm_ay < -PADCAL) )
        {
            redraw = 1;
            //menu = FindMenuItem(&items[0], maxitems, menu, +1);
			*item = findmenuitem(+1);
        }
		
		/*** Look for left ***/
        else if ( (p & PAD_BUTTON_LEFT) || (wp & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)) || (gc_ax < -PADCAL) || (wm_ax < -PADCAL) )
        {
            redraw = 1;
			if (*section > 0)
				*section -= 1;
			else
				*section = Menu.menuitems-1;
        }
		
		/*** Look for right ***/
        else if ( (p & PAD_BUTTON_RIGHT) || (wp & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)) || (gc_ax > PADCAL) || (wm_ax > PADCAL) )
        {
            redraw = 1;
			if (*section < Menu.menuitems-1)
				*section += 1;
			else
				*section = 0;
        }

        else if ((p & PAD_BUTTON_A) || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)))
        {
            quit = 1;
            //ret = menu;
			ret = *item;
        }

        else if ((p & PAD_BUTTON_B) || (wp & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)))
        {
            quit = -1;
            ret = -1;
        }
		
		else if (wp & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME))
        {
            exit(0);
        }
    }

	/*** Wait for B button to be released before proceeding ***/
	while ( (PAD_ButtonsDown(0) & PAD_BUTTON_B)
#ifdef HW_RVL
			|| (WPAD_ButtonsDown(0) & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B))
#endif
			)
	{
		ret = -1;
		VIDEO_WaitVSync();
	}

    return ret;
}

/****************************************************************************
 * WPAD_Stick
 *
 * Get X/Y value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 WPAD_Stick(u8 chan, u8 right, int axis)
{
	float mag = 0.0;
	float ang = 0.0;
	WPADData *data = WPAD_Data(chan);

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate x/y value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val;

	if(axis == 0) // x-axis
		val = mag * sin((PI * ang)/180.0f);
	else // y-axis
		val = mag * cos((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

// eof
