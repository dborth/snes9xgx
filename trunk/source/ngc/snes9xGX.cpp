/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Tantric September 2008
 *
 * snes9xGX.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <sdcard/card_cmn.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/card_io.h>
#include <fat.h>

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

#include "snes9xGX.h"
#include "dvd.h"
#include "smbop.h"
#include "video.h"
#include "menudraw.h"
#include "s9xconfig.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "fileop.h"
#include "input.h"

#include "gui.h"

unsigned long ARAM_ROMSIZE = 0;
int ConfigRequested = 0;
extern int FrameTimer;

extern long long prev;
extern unsigned int timediffallowed;

extern void fat_enable_readahead_all();


/****************************************************************************
 * setFrameTimerMethod()
 * change frametimer method depending on whether ROM is NTSC or PAL
 ***************************************************************************/
extern u8 vmode_60hz;
int timerstyle;

void setFrameTimerMethod()
{
	/*
	Set frametimer method
	(timerstyle: 0=NTSC vblank, 1=PAL int timer)
	*/
	if ( Settings.PAL ) {
		if(vmode_60hz == 1)
			timerstyle = 1;
		else
			timerstyle = 0;
		//timediffallowed = Settings.FrameTimePAL;
	} else {
		if(vmode_60hz == 1)
			timerstyle = 0;
		else
			timerstyle = 1;
		//timediffallowed = Settings.FrameTimeNTSC;
	}
	return;
}

/****************************************************************************
 * Emulation loop
 ***************************************************************************/
extern void S9xInitSync();
bool CheckVideo = 0; // for forcing video reset in video.cpp
extern uint32 prevRenderedFrameCount;

void
emulate ()
{
	S9xSetSoundMute (TRUE);
	AudioStart ();
	S9xInitSync(); // Eke-Eke: initialize frame Sync

	setFrameTimerMethod(); // set frametimer method every time a ROM is loaded

	while (1)
	{
		S9xMainLoop ();
		NGCReportButtons ();

		if (ConfigRequested)
		{
			// change to menu video mode
			ResetVideo_Menu ();

			if ( GCSettings.AutoSave == 1 )
			{
				SaveSRAM ( GCSettings.SaveMethod, SILENT );
			}
			else if ( GCSettings.AutoSave == 2 )
			{
				if ( WaitPromptChoice ((char*)"Save Freeze State?", (char*)"Don't Save", (char*)"Save") )
					NGCFreezeGame ( GCSettings.SaveMethod, SILENT );
			}
			else if ( GCSettings.AutoSave == 3 )
			{
				if ( WaitPromptChoice ((char*)"Save SRAM and Freeze State?", (char*)"Don't Save", (char*)"Save") )
				{
					SaveSRAM(GCSettings.SaveMethod, SILENT );
					NGCFreezeGame ( GCSettings.SaveMethod, SILENT );
				}
			}

			// GUI Stuff
			/*
			gui_alloc();
			gui_makebg ();
			gui_clearscreen();
			gui_draw ();
			gui_showscreen ();
			//gui_savescreen ();
			*/

			mainmenu (3); // go to game menu

			FrameTimer = 0;
			setFrameTimerMethod (); // set frametimer method every time a ROM is loaded

			Settings.SuperScopeMaster = (GCSettings.Superscope > 0 ? true : false);
			Settings.MouseMaster = (GCSettings.Mouse > 0 ? true : false);
			Settings.JustifierMaster = (GCSettings.Justifier > 0 ? true : false);
			SetControllers ();
			//S9xReportControllers ();

			ConfigRequested = 0;

			CheckVideo = 1;	// force video update
			prevRenderedFrameCount = IPPU.RenderedFramesCount;

		}//if ConfigRequested

	}//while
}

/****************************************************************************
 * MAIN
 *
 * Steps to Snes9x Emulation :
 *	1. Initialise GC Video
 *	2. Initialise libfreetype (Nice to read something)
 *	3. Set S9xSettings to standard defaults (s9xconfig.h)
 *	4. Allocate Snes9x Memory
 *	5. Allocate APU Memory
 *	6. Set Pixel format to RGB565 for GL Rendering
 *	7. Initialise Snes9x/GC Sound System
 *	8. Initialise Snes9x Graphics subsystem
 *	9. Let's Party!
 *
 * The SNES ROM is delivered from ARAM. (AR_SNESROM)
 * Any method of loading a ROM - RAM, DVD, SMB, SDCard, etc
 * MUST place the unpacked ROM at this location.
 * This provides a single consistent interface in memmap.cpp.
 * Refer to that file if you need to change it.
 ***************************************************************************/
int
main ()
{
#ifdef WII_DVD
	DI_Init();	// first
#endif

	unsigned int save_flags;
	int selectedMenu = -1;

#ifdef HW_RVL
	WPAD_Init();
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#endif

	// Initialise video
	InitGCVideo ();

	// Initialise freetype font system
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}

	unpackbackdrop ();

	// Set defaults
	DefaultSettings ();

	S9xUnmapAllControls ();
	SetDefaultButtonMap ();

	// Allocate SNES Memory
	if (!Memory.Init ())
		while (1);

	// Allocate APU
	if (!S9xInitAPU ())
		while (1);

	// Set Pixel Renderer to match 565
	S9xSetRenderPixelFormat (RGB565);

	// Initialise Snes Sound System
	S9xInitSound (5, TRUE, 1024);

	// Initialise Graphics
	setGFX ();
	if (!S9xGraphicsInit ())
		while (1);

	// Initialize libFAT for SD and USB
	fatInitDefault();

	// Initialize DVD subsystem (GameCube only)
	#ifndef HW_RVL
	DVD_Init ();
	#endif

	// Check if DVD drive belongs to a Wii
	SetDVDDriveType();

	// Load preferences
	if(!LoadPrefs())
	{
		WaitPrompt((char*) "Preferences reset - check settings!");
		selectedMenu = 2; // change to preferences menu
	}

	// No appended ROM, so get the user to load one
	if (ARAM_ROMSIZE == 0)
	{
		while (ARAM_ROMSIZE == 0)
		{
			mainmenu (selectedMenu);
		}
	}
	else
	{
		// Load ROM
		save_flags = CPU.Flags;
		if (!Memory.LoadROM ("VIRTUAL.ROM"))
			while (1);
		CPU.Flags = save_flags;
	}

	// Emulate
	emulate ();

	// NO! - We're never leaving here!
	while (1);
		return 0;
}
