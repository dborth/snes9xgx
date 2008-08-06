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
#include "video.h"
#include "ftfont.h"
#include "s9xconfig.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "memfile.h"
#include "preferences.h"
#include "gctime.h"

unsigned long ARAM_ROMSIZE = 0;
int ConfigRequested = 0;
extern int FrameTimer;

extern tb_t prev;
extern unsigned int timediffallowed;

/****************************************************************************
 * Controller Functions
 *
 * The following map the NGC Pads to the *NEW* controller system.
 ****************************************************************************/
#define ASSIGN_BUTTON_TRUE( keycode, snescmd ) \
	  S9xMapButton( keycode, cmd = S9xGetCommandT(snescmd), true)

#define ASSIGN_BUTTON_FALSE( keycode, snescmd ) \
	  S9xMapButton( keycode, cmd = S9xGetCommandT(snescmd), false)

#define MAXJP 12
int padcal = 50;
unsigned short gcpadmap[] = { PAD_BUTTON_A, PAD_BUTTON_B,
  PAD_BUTTON_X, PAD_BUTTON_Y,
  PAD_TRIGGER_L, PAD_TRIGGER_R,
  PAD_TRIGGER_Z, PAD_BUTTON_START,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};

#if 0
/****************************************************************************
 * decodepad
 * Called per pad with offset
 ****************************************************************************/
void
decodepadold (int pad)
{
  int i, offset;
  signed char x, y;
  unsigned short jp;

	/*** Do analogue updates ***/
  x = PAD_StickX (pad);
  y = PAD_StickY (pad);
  jp = PAD_ButtonsHeld (pad);

	/*** Recalc left / right ***/
  if (x < -padcal)
    jp |= PAD_BUTTON_LEFT;
  if (x > padcal)
    jp |= PAD_BUTTON_RIGHT;

	/*** Recalc up / down ***/
  if (y > padcal)
    jp |= PAD_BUTTON_UP;
  if (y < -padcal)
    jp |= PAD_BUTTON_DOWN;

	/*** Fix offset to pad ***/
  offset = ((pad + 1) << 4);

  for (i = 0; i < MAXJP; i++)
    {
      if (jp & gcpadmap[i])
	S9xReportButton (offset + i, true);
      else
	S9xReportButton (offset + i, false);
    }
}
#endif

/****************************************************************************
 * This is the joypad algorithm submitted by Krullo.
 ****************************************************************************/
void
decodepad (int pad)
{
  int i, offset;
  signed char x, y;
  unsigned short jp;
  float t;

 /*** Do analogue updates ***/
  x = PAD_StickX (pad);
  y = PAD_StickY (pad);
  jp = PAD_ButtonsHeld (pad);

/*** Is XY inside the "zone"? ***/
  if (x * x + y * y > padcal * padcal)
    {

/*** we don't want division by ZERO ***/
      if (x > 0 && y == 0)
	jp |= PAD_BUTTON_RIGHT;
      if (x < 0 && y == 0)
	jp |= PAD_BUTTON_LEFT;
      if (x == 0 && y > 0)
	jp |= PAD_BUTTON_UP;
      if (x == 0 && y < 0)
	jp |= PAD_BUTTON_DOWN;

      if (x != 0 && y != 0)
	{

/*** Recalc left / right ***/
	  t = (float) y / x;
	  if (t >= -2.41421356237 && t < 2.41421356237)
	    {
	      if (x >= 0)
		jp |= PAD_BUTTON_RIGHT;
	      else
		jp |= PAD_BUTTON_LEFT;
	    }

/*** Recalc up / down ***/
	  t = (float) x / y;
	  if (t >= -2.41421356237 && t < 2.41421356237)
	    {
	      if (y >= 0)
		jp |= PAD_BUTTON_UP;
	      else
		jp |= PAD_BUTTON_DOWN;
	    }
	}
    }

	/*** Fix offset to pad ***/
  offset = ((pad + 1) << 4);

  for (i = 0; i < MAXJP; i++)
    {
      if (jp & gcpadmap[i])
	S9xReportButton (offset + i, true);
      else
	S9xReportButton (offset + i, false);
    }

}

/****************************************************************************
 * NGCReportButtons
 * Called on each rendered frame					     
 ****************************************************************************/
void
NGCReportButtons ()
{
	s8 px = PAD_SubStickX (0);
	s8 py = PAD_SubStickY (0);
	u16 pb = PAD_ButtonsHeld (0);
	
	/*** Check for video zoom ***/
	if (GCSettings.NGCZoom)
	{
		if (py < -18 || py > 18)
			zoom ((float) py / -18);
	}

    Settings.TurboMode = (px > 70);

	/*** Check for menu:
	       CStick left
	       OR "L+R+X+Y" (eg. Hombrew/Adapted SNES controllers) ***/

    if ((px < -70) ||
        ((pb & PAD_TRIGGER_L) &&
         (pb & PAD_TRIGGER_R ) &&
         (pb & PAD_BUTTON_X) &&
         (pb & PAD_BUTTON_Y ))
       )
    {
        ConfigRequested = 1;
        
        VIDEO_WaitVSync ();
        
        if ( GCSettings.AutoSave == 1 )
        {
            if ( WaitPromptChoice ("Save SRAM?", "Don't Save", "Save") )
                quickSaveSRAM ( SILENT );
        }
        else if ( GCSettings.AutoSave == 2 )
        {
            if ( WaitPromptChoice ("Save Freeze State?", "Don't Save", "Save") )
                quickSaveFreeze ( SILENT );
        }
        else if ( GCSettings.AutoSave == 3 )
        {
            if ( WaitPromptChoice ("Save SRAM and Freeze State?", "Don't Save", "Save") )
            {
                quickSaveSRAM ( SILENT );
                quickSaveFreeze ( SILENT );
            }
        }
        
        mainmenu ();
        FrameTimer = 0;
        ConfigRequested = 0;
    }
    else
    {
        int j = (Settings.MultiPlayer5Master == true ? 4 : 2);
        for (int i = 0; i < j; i++)
            decodepad (i);
    }
}

/****************************************************************************
 * Set the default mapping for NGC
 ****************************************************************************/
void
SetDefaultButtonMap ()
{
  int maxcode = 0x10;
  s9xcommand_t cmd;

	/*** Joypad 1 ***/
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 A");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 B");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 X");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Y");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 L");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 R");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Select");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Start");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Up");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Down");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Left");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Right");

  maxcode = 0x20;
	/*** Joypad 2 ***/
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 A");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 B");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 X");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Y");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 L");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 R");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Select");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Start");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Up");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Down");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Left");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Right");

  maxcode = 0x30;
	/*** Joypad 3 ***/
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 A");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 B");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 X");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Y");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 L");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 R");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Select");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Start");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Up");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Down");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Left");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Right");

  maxcode = 0x40;
	/*** Joypad 4 ***/
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 A");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 B");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 X");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Y");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 L");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 R");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Select");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Start");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Up");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Down");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Left");
  ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Right");

  if (Settings.MultiPlayer5Master == false)
    {
	/*** Plugin 2 Joypads by default ***/
      S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
      S9xSetController (1, CTL_JOYPAD, 1, 0, 0, 0);
    }
  else
    {
      S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
      S9xSetController (1, CTL_MP5, 1, 2, 3, -1);
    }

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
//  FrameTimer = 0;
  AudioStart ();
  S9xInitSync();

  while (1)
    {
      S9xMainLoop ();
      NGCReportButtons ();
    }
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
	unsigned int save_flags;
	
	/*** Initialise GC ***/
	InitGCVideo ();	/*** Get the ball rolling ***/
	
#ifdef FORCE_WII
	isWii = TRUE;
#else
    int drvid = dvd_driveid ();
    if ( drvid == 4 || drvid == 6 || drvid == 8 )
        isWii = FALSE;
    else
        isWii = TRUE;
#endif
	
	/*** Initialise freetype ***/
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}
	
	/*** Set defaults ***/
	DefaultSettings ();
	
	S9xUnmapAllControls ();
	SetDefaultButtonMap ();
	
	printf ("Initialise Memory\n");
	/*** Allocate SNES Memory ***/
	if (!Memory.Init ())
		while (1);
	
	printf ("Initialise APU\n");
	/*** Allocate APU ***/
	if (!S9xInitAPU ())
		while (1);
	
	/*** Set Pixel Renderer to match 565 ***/
	S9xSetRenderPixelFormat (RGB565);
	
	/*** Initialise Snes Sound System ***/
	S9xInitSound (5, TRUE, 1024);
	
	printf ("Initialise GFX\n");
	/*** Initialise Graphics ***/
	setGFX ();
	if (!S9xGraphicsInit ())
		while (1);
	
	legal ();
	WaitButtonA ();
	
	// Load preferences
	quickLoadPrefs(SILENT);
	
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
			mainmenu ();
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
		Memory.LoadSRAM ("DVD");
		
        if ( GCSettings.AutoLoad == 1 )
            quickLoadSRAM ( SILENT );
        else if ( GCSettings.AutoLoad == 2 )
            quickLoadFreeze ( SILENT );
	}
	/*** Emulate ***/
	emulate ();
	
	/*** NO! - We're never leaving here ! ***/
	while (1);
	return 0;

}
