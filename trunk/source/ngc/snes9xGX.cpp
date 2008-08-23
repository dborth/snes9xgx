/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 *
 * snes9xGX.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ****************************************************************************/

/****************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com) and
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  Brad Jorsch (anomie@users.sourceforge.net),
                             funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com),
                             Nach (n-a-c-h@users.sourceforge.net), and
                             zones (kasumitokoduck@yahoo.com)

  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x filter
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Specific ports contains the works of other authors. See headers in
  individual files.

  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
*****************************************************************************/
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
#ifdef __cplusplus
extern "C" {
#include <di/di.h>
}
#endif
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
#include "gctime.h"
#include "button_mapping.h"
#include "fileop.h"
#include "input.h"

unsigned long ARAM_ROMSIZE = 0;
int ConfigRequested = 0;
extern int FrameTimer;

extern long long prev;
extern unsigned int timediffallowed;

extern void fat_enable_readahead_all();


/****************************************************************************
 * setFrameTimerMethod()
 * change frametimer method depending on whether ROM is NTSC or PAL
 ****************************************************************************/
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
 *
 * The 'clock' timer is long gone.
 * System now only uses vbl as hardware clock, so force PAL50 if you need 50hz
 ****************************************************************************/
/* Eke-Eke: initialize frame Sync */
extern void S9xInitSync();

void
emulate ()
{
	S9xSetSoundMute (TRUE);
	AudioStart ();
	S9xInitSync();

	setFrameTimerMethod(); 	// set frametimer method every time a ROM is loaded

	while (1)
	{
		S9xMainLoop ();
		NGCReportButtons ();

		if (ConfigRequested)
		{
			VIDEO_WaitVSync ();

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

			mainmenu (3); // go to game menu

			/*** Update any emulation settings changed in the menu ***/
			ReInitGCVideo();	// update video after reading settings
			FrameTimer = 0;
			setFrameTimerMethod(); // set frametimer method every time a ROM is loaded

			Settings.SuperScopeMaster = (GCSettings.Superscope > 0 ? true : false);
			Settings.MouseMaster = (GCSettings.Mouse > 0 ? true : false);
			Settings.JustifierMaster = (GCSettings.Justifier > 0 ? true : false);
			SetControllers();
			S9xReportControllers(); // FIX

			ConfigRequested = 0;
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
 * NB: The SNES ROM is now delivered from ARAM. (AR_SNESROM)
 *        Any method of loading a ROM, RAM, DVD, SMB, SDCard,
 *	MUST place the unpacked ROM at this location.
 *	This provides a single consistent interface in memmap.cpp.
 *	Refer to that file if you need to change it.
 ****************************************************************************/
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

	/*** Initialise GC ***/
	InitGCVideo ();	/*** Get the ball rolling ***/

	/*** Initialise freetype ***/
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}

	unpackbackdrop ();

	/*** Set defaults ***/
	DefaultSettings ();

	S9xUnmapAllControls ();
	SetDefaultButtonMap ();

	//printf ("Initialise Memory\n");
	/*** Allocate SNES Memory ***/
	if (!Memory.Init ())
		while (1);

	//printf ("Initialise APU\n");
	/*** Allocate APU ***/
	if (!S9xInitAPU ())
		while (1);

	/*** Set Pixel Renderer to match 565 ***/
	S9xSetRenderPixelFormat (RGB565);

	/*** Initialise Snes Sound System ***/
	S9xInitSound (5, TRUE, 1024);

	//printf ("Initialise GFX\n");
	/*** Initialise Graphics ***/
	setGFX ();
	if (!S9xGraphicsInit ())
		while (1);

	// Initialize libFAT for SD and USB
	fatInitDefault();
	//fatInit(8192, false);
	//fat_enable_readahead_all();

	// Initialize DVD subsystem (GameCube only)
	#ifndef HW_RVL
	DVD_Init ();
	#endif

	#ifdef FORCE_WII
	isWii = TRUE;
	#else
	int drvid = dvd_driveid ();
	if ( drvid == 4 || drvid == 6 || drvid == 8 )
		isWii = FALSE;
	else
		isWii = TRUE;
	#endif

	// Load preferences
	if(!LoadPrefs(GCSettings.SaveMethod, SILENT))
	{
		WaitPrompt((char*) "Preferences reset - check settings!");
		selectedMenu = 2; // change to preferences menu
	}

	// Correct any relevant saved settings that are invalid
	Settings.FrameTimeNTSC = 16667;
	Settings.FrameTimePAL = 20000;
    if ( Settings.TurboSkipFrames <= Settings.SkipFrames )
        Settings.TurboSkipFrames = 20;

	/*** No appended ROM, so get the user to load one ***/
	if (ARAM_ROMSIZE == 0)
	{
		while (ARAM_ROMSIZE == 0)
		{
			mainmenu (selectedMenu);
		}
	}
	else
	{
		/*** Load ROM ***/
		save_flags = CPU.Flags;
		if (!Memory.LoadROM ("VIRTUAL.ROM"))
			while (1);
		CPU.Flags = save_flags;

		/*** Load SRAM ***/
		//Memory.LoadSRAM ("DVD");
	}

	/*** Emulate ***/
	emulate ();

	/*** NO! - We're never leaving here ! ***/
	while (1);
		return 0;
}
