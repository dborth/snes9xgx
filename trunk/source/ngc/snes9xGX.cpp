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
#include <fat.h>
#include <wiiuse/wpad.h>
#include <sdcard/card_cmn.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/card_io.h>

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
#include "button_mapping.h"

unsigned long ARAM_ROMSIZE = 0;
int ConfigRequested = 0;
extern int FrameTimer;

extern long long prev;
extern unsigned int timediffallowed;

int scope_x = 0; int scope_y = 0;	// superscope cursor position

extern void fat_enable_readahead_all();

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
/*** Gamecube controller Padmap ***/
unsigned int gcpadmap[] = { PAD_BUTTON_A, PAD_BUTTON_B,
  PAD_BUTTON_X, PAD_BUTTON_Y,
  PAD_TRIGGER_L, PAD_TRIGGER_R,
  PAD_TRIGGER_Z, PAD_BUTTON_START,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT
};
/*** Wiimote Padmap ***/
unsigned int wmpadmap[] = { WPAD_BUTTON_B, WPAD_BUTTON_2,
  WPAD_BUTTON_1, WPAD_BUTTON_A,
  0x0000, 0x0000,
  WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
  WPAD_BUTTON_RIGHT, WPAD_BUTTON_LEFT,
  WPAD_BUTTON_UP, WPAD_BUTTON_DOWN
};
/*** Classic Controller Padmap ***/
unsigned int ccpadmap[] = { WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B,
  WPAD_CLASSIC_BUTTON_X, WPAD_CLASSIC_BUTTON_Y,
  WPAD_CLASSIC_BUTTON_FULL_L, WPAD_CLASSIC_BUTTON_FULL_R,
  WPAD_CLASSIC_BUTTON_MINUS, WPAD_CLASSIC_BUTTON_PLUS,
  WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
  WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT
};
/*** Nunchuk + wiimote Padmap ***/
unsigned int ncpadmap[] = { WPAD_BUTTON_A, WPAD_BUTTON_B,
  WPAD_NUNCHUK_BUTTON_C, WPAD_NUNCHUK_BUTTON_Z,
  WPAD_BUTTON_MINUS, WPAD_BUTTON_PLUS,
  WPAD_BUTTON_2, WPAD_BUTTON_1,
  WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
  WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT
};
/*** Superscope : GC controller button mapping ***/
unsigned int gcscopemap[] = { PAD_TRIGGER_Z, PAD_BUTTON_B,
  PAD_BUTTON_A, PAD_BUTTON_Y,
  PAD_BUTTON_START
};
/*** Superscope : wiimote button mapping ***/
unsigned int wmscopemap[] = { WPAD_BUTTON_MINUS, WPAD_BUTTON_B,
  WPAD_BUTTON_A, WPAD_BUTTON_DOWN,
  WPAD_BUTTON_PLUS
};

void UpdateCursorPosition (int pad, int &pos_x, int &pos_y)
{	
	// gc left joystick
	signed char pad_x = PAD_StickX (pad);	
	signed char pad_y = PAD_StickY (pad);
	int SCOPEPADCAL = 30;
#ifdef HW_RVL
	// wiimote ir
	struct ir_t ir;
#endif

	if (pad_x > SCOPEPADCAL){
		pos_x +=4;
		if (pos_x > 256) pos_x = 256;
	}
	if (pad_x < -SCOPEPADCAL){
		pos_x -=4;
		if (pos_x < 0) pos_x = 0;
	}
   
	if (pad_y < -SCOPEPADCAL){
		pos_y +=4;
		if (pos_y > 224) pos_y = 224;
	}
	if (pad_y > SCOPEPADCAL){
		pos_y -=4;
		if (pos_y < 0) pos_y = 0;            
	}
	
#ifdef HW_RVL
	// read wiimote IR
	WPAD_IR(pad, &ir);
	if (ir.valid)
	{
		scope_x = (ir.x * 256) / 640;
		scope_y = (ir.y * 224) / 480;
	}
#endif

}

/****************************************************************************
 * This is the joypad algorithm submitted by Krullo.
 ****************************************************************************/
void
decodepad (int pad)
{
  int i, offset;
  signed char pad_x, pad_y;
  //unsigned short jp, wp;	//
  u32 jp, wp;
  float t;
  float mag = 0;
  float mag2 = 0;
  u16 ang = 0;
  u16 ang2 = 0;
  u32 exp_type;

  /*** Do analogue updates ***/
  pad_x = PAD_StickX (pad);
  pad_y = PAD_StickY (pad);
  jp = PAD_ButtonsHeld (pad);
#ifdef HW_RVL
  exp_type = wpad_get_analogues(pad, &mag, &ang, &mag2, &ang2);	// get joystick info from wii expansions
  wp = WPAD_ButtonsHeld (pad);	// wiimote
#else
  wp = 0;
#endif

	/***
	Gamecube Joystick input
	***/
	// Is XY inside the "zone"?
	if (pad_x * pad_x + pad_y * pad_y > padcal * padcal)
	{

		/*** we don't want division by ZERO ***/
	      if (pad_x > 0 && pad_y == 0)
		jp |= PAD_BUTTON_RIGHT;
	      if (pad_x < 0 && pad_y == 0)
		jp |= PAD_BUTTON_LEFT;
	      if (pad_x == 0 && pad_y > 0)
		jp |= PAD_BUTTON_UP;
	      if (pad_x == 0 && pad_y < 0)
		jp |= PAD_BUTTON_DOWN;

	      if (pad_x != 0 && pad_y != 0)
		{

		/*** Recalc left / right ***/
		  t = (float) pad_y / pad_x;
		  if (t >= -2.41421356237 && t < 2.41421356237)
		    {
		      if (pad_x >= 0)
			jp |= PAD_BUTTON_RIGHT;
		      else
			jp |= PAD_BUTTON_LEFT;
		    }

		/*** Recalc up / down ***/
		  t = (float) pad_x / pad_y;
		  if (t >= -2.41421356237 && t < 2.41421356237)
		    {
		      if (pad_y >= 0)
			jp |= PAD_BUTTON_UP;
		      else
			jp |= PAD_BUTTON_DOWN;
		    }
		}
	}
#ifdef HW_RVL
	/***
	Wii Joystick (classic, nunchuk) input
	***/
	if (exp_type == WPAD_EXP_NUNCHUK)
	{
		if ( mag>JOY_THRESHOLD && (ang>300 || ang<50) )
			wp |= WPAD_BUTTON_UP;
		if ( mag>JOY_THRESHOLD && (ang>130 && ang<230) )
			wp |= WPAD_BUTTON_DOWN;
		if ( mag>JOY_THRESHOLD && (ang>220 && ang<320) )
			wp |= WPAD_BUTTON_LEFT;
		if ( mag>JOY_THRESHOLD && (ang>40 && ang<140) )
			wp |= WPAD_BUTTON_RIGHT;
	} else if (exp_type == WPAD_EXP_CLASSIC)
	{
		if ( mag>JOY_THRESHOLD && (ang>300 || ang<50) )
			wp |= WPAD_CLASSIC_BUTTON_UP;
		if ( mag>JOY_THRESHOLD && (ang>130 && ang<230) )
			wp |= WPAD_CLASSIC_BUTTON_DOWN;
		if ( mag>JOY_THRESHOLD && (ang>220 && ang<320) )
			wp |= WPAD_CLASSIC_BUTTON_LEFT;
		if ( mag>JOY_THRESHOLD && (ang>40 && ang<140) )
			wp |= WPAD_CLASSIC_BUTTON_RIGHT;	
	}
#endif
	
	/*** Fix offset to pad ***/
	offset = ((pad + 1) << 4);

	/*** Report pressed buttons (gamepads) ***/
	for (i = 0; i < MAXJP; i++)
    {
		if ( (jp & gcpadmap[i])											// gamecube controller
#ifdef HW_RVL
		|| ( (exp_type == WPAD_EXP_NONE) && (wp & wmpadmap[i]) )	// wiimote
		|| ( (exp_type == WPAD_EXP_CLASSIC) && (wp & ccpadmap[i]) )	// classic controller
		|| ( (exp_type == WPAD_EXP_NUNCHUK) && (wp & ncpadmap[i]) )	// nunchuk + wiimote
#endif
		)
			S9xReportButton (offset + i, true);
		else
			S9xReportButton (offset + i, false);
    }
	
	/*** Superscope ***/
	if (pad == GCSettings.Superscope-1)	// report only once
	{
		// buttons
		offset = 0x50;
		for (i = 0; i < 6; i++)
		{
		  if ( jp & gcscopemap[i] 
#ifdef HW_RVL
				|| wp & wmscopemap[i]
#endif
		  )
			S9xReportButton (offset + i, true);
		  else
			S9xReportButton (offset + i, false);
		}
		// pointer
		offset = 0x60;
		UpdateCursorPosition (pad, scope_x, scope_y);
		S9xReportPointer(offset, (u16)scope_x, (u16)scope_y);
	}
}
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
 * NGCReportButtons
 * Called on each rendered frame					     
 ****************************************************************************/
void
NGCReportButtons ()
{
	s8 gc_px = PAD_SubStickX (0);
	s8 gc_py = PAD_SubStickY (0);
	u16 gc_pb = PAD_ButtonsHeld (0);
#ifdef HW_RVL
	float mag1 = 0;
	float mag2 = 0;
	u16 ang1 = 0;
	u16 ang2 = 0;
	u32 wm_pb = WPAD_ButtonsHeld (0);	// wiimote / expansion button info
	wpad_get_analogues(0, &mag1, &ang1, &mag2, &ang2);		// get joystick info from wii expansions
#endif
	
	/*** Check for video zoom ***/
	if (GCSettings.NGCZoom)
	{
		if (gc_py < -18 || gc_py > 18)
			zoom ((float) gc_py / -18);
#ifdef HW_RVL
		if ( mag2>0.2 && (ang2>340 || ang2<20) )	// classic rjs up
			zoom ((float) mag2 / -0.2);
		if ( mag2>0.2 && (ang2>160 && ang2<200) )	// classic rjs down
			zoom ((float) mag2 / 0.2);
#endif
	}

    Settings.TurboMode = ( (gc_px > 70) 
#ifdef HW_RVL
							|| (mag2>JOY_THRESHOLD && ang2>75 && ang2<115) 
#endif
							);	// RIGHT on c-stick and on classic ctrlr right joystick

	/*** Check for menu:
	       CStick left
	       OR "L+R+X+Y" (eg. Hombrew/Adapted SNES controllers) 
	       OR "Home" on the wiimote or classic controller	***/

    if ((gc_px < -70) ||
        ((gc_pb & PAD_TRIGGER_L) &&
         (gc_pb & PAD_TRIGGER_R ) &&
         (gc_pb & PAD_BUTTON_X) &&
         (gc_pb & PAD_BUTTON_Y ))
#ifdef HW_RVL
		 || (wm_pb & WPAD_BUTTON_HOME)
		 || (wm_pb & WPAD_CLASSIC_BUTTON_HOME)
#endif
       )
    {
        ConfigRequested = 1;
        
        VIDEO_WaitVSync ();
        
        if ( GCSettings.AutoSave == 1 )
        {
            //if ( WaitPromptChoice ((char*)"Save SRAM?", (char*)"Don't Save", (char*)"Save") )
                quickSaveSRAM ( SILENT );
        }
        else if ( GCSettings.AutoSave == 2 )
        {
            if ( WaitPromptChoice ((char*)"Save Freeze State?", (char*)"Don't Save", (char*)"Save") )
                quickSaveFreeze ( SILENT );
        }
        else if ( GCSettings.AutoSave == 3 )
        {
            if ( WaitPromptChoice ((char*)"Save SRAM and Freeze State?", (char*)"Don't Save", (char*)"Save") )
            {
                quickSaveSRAM ( SILENT );
                quickSaveFreeze ( SILENT );
            }
        }
        
        mainmenu ();
        FrameTimer = 0;
        ConfigRequested = 0;
		
		setFrameTimerMethod(); 	// set frametimer method every time a ROM is loaded
		
		S9xReportControllers();	// FIX
		
		
    }
    else
    {
        int j = (Settings.MultiPlayer5Master == true ? 4 : 2);
        for (int i = 0; i < j; i++)
            decodepad (i);
    }
}

/****************************************************************************
 * wpad_get_analogues()
 *
 * gets the analogue stick magnitude and angle values (
 * from classic or nunchuk expansions)					     
 ****************************************************************************/
u32 wpad_get_analogues(int pad, float* mag1, u16* ang1, float* mag2, u16* ang2)
{
	*mag1 = *ang1 = *mag2 = *ang2 = 0;
	u32 exp_type = 0;
#ifdef HW_RVL
	struct expansion_t exp;
	memset( &exp, 0, sizeof(exp) );
	
	if ( WPAD_Probe( pad, &exp_type) == 0)	// check wiimote and expansion status (first if wiimote is connected & no errors)
	{
		WPAD_Expansion(pad, &exp);	// expansion connected. get info
		if (exp_type == WPAD_EXP_CLASSIC)
		{
			*ang1 = exp.classic.ljs.ang;	// left cc joystick
			*mag1 = exp.classic.ljs.mag;
			*ang2 = exp.classic.rjs.ang;	// right cc joystick
			*mag2 = exp.classic.rjs.mag;
		}
		else if (exp_type == WPAD_EXP_NUNCHUK)
		{
			*ang1 = exp.nunchuk.js.ang;	// nunchuk joystick
			*mag1 = exp.nunchuk.js.mag;
		}
	}
#endif
	return exp_type;	// return expansion type
}

void SetControllers ()
{
  if (Settings.MultiPlayer5Master == true)
    {
	
      S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
      S9xSetController (1, CTL_MP5, 1, 2, 3, -1);
    }
  else if (Settings.SuperScopeMaster == true)
    {
      S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
      S9xSetController (1, CTL_SUPERSCOPE, 1, 0, 0, 0);	
	}
  else
    {
	/*** Plugin 2 Joypads by default ***/
      S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
      S9xSetController (1, CTL_JOYPAD, 1, 0, 0, 0);
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
  
  maxcode = 0x50;
	/*** Superscope ***/
  ASSIGN_BUTTON_FALSE (maxcode++, "Superscope AimOffscreen");
  ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Fire");
  ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Cursor");
  ASSIGN_BUTTON_FALSE (maxcode++, "Superscope ToggleTurbo");
  ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Pause");
  
  maxcode = 0x60;
  S9xMapPointer( maxcode++, S9xGetCommandT("Pointer Superscope"), false);
  // add mouses here
	
  SetControllers ();

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

	setFrameTimerMethod();	// also called in NGCReportButtons() 

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
	
	/*** Initialize libFAT and SD cards ***/
	fatInitDefault();
	//fatInit(8192, false);
	//fat_enable_readahead_all();
	

	/*** Initialize DVD subsystem ***/
	DVD_Init ();
	
#ifdef FORCE_WII
	isWii = TRUE;
#else
    int drvid = dvd_driveid ();
    if ( drvid == 4 || drvid == 6 || drvid == 8 )
        isWii = FALSE;
    else
        isWii = TRUE;
#endif

#ifdef HW_RVL
	WPAD_Init();
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
#endif

	
	/*** Initialise freetype ***/
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}
	setfontsize (16);/***sets the font size.***/
	
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
	}
	
	/*** Emulate ***/
	emulate ();
	
	/*** NO! - We're never leaving here ! ***/
	while (1);
	return 0;

}
