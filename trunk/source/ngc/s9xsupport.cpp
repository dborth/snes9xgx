/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * s9xsupport.cpp
 *
 * This file contains the supporting functions defined in porting.html, with
 * others taken from unix/x11.cpp and unix/unix.cpp
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
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#include "gctime.h"
#include "snes9xGX.h"
#include "video.h"
#include "audio.h"

extern u32 FrameTimer;

long long prev;
long long now;


/*** Miscellaneous Functions ***/
void
S9xMessage (int /*type */ , int /*number */ , const char *message)
{
#define MAX_MESSAGE_LEN (36 * 3)

  static char buffer[MAX_MESSAGE_LEN + 1];
  strncpy (buffer, message, MAX_MESSAGE_LEN);
  buffer[MAX_MESSAGE_LEN] = 0;
  S9xSetInfoString (buffer);
}

void
S9xExit ()
{
	/*** Nintendo Gamecube will NEVER get here ... unless
	      something major went wrong !!

	      In which case, I'll settle for a reboot first -;)
	***/
}

/*** File based functions ***/
const char *
S9xChooseFilename (bool8 read_only)
{
  return NULL;
}

const char *
S9xChooseMovieFilename (bool8 read_only)
{
  return NULL;
}

const char *
S9xGetDirectory (enum s9x_getdirtype dirtype)
{
  return NULL;
}

const char *
S9xGetFilename (const char *ex, enum s9x_getdirtype dirtype)
{
  return NULL;
}

const char *
S9xGetFilenameInc (const char *e, enum s9x_getdirtype dirtype)
{
  return NULL;
}

/*** Memory based functions ***/
void
S9xAutoSaveSRAM ()
{

}

/*** Sound based functions ***/
void
S9xToggleSoundChannel (int c)
{
  if (c == 8)
    so.sound_switch = 255;
  else
    so.sound_switch ^= 1 << c;
  S9xSetSoundControl (so.sound_switch);
}

/****************************************************************************
 * OpenSoundDevice
 *
 * Main initialisation for NGC sound system
 ****************************************************************************/
bool8
S9xOpenSoundDevice (int mode, bool8 stereo, int buffer_size)
{
  so.stereo = TRUE;
  so.playback_rate = 32000;
  so.sixteen_bit = TRUE;
  so.encoded = 0;
  so.buffer_size = 4096;
  so.sound_switch = 255;
  S9xSetPlaybackRate (so.playback_rate);

  InitGCAudio ();
  return TRUE;
}

/*** Deprecated function. NGC uses threaded sound ***/
void
S9xGenerateSound ()
{
}

/* eke-eke */
void S9xInitSync()
{
  FrameTimer = 0;
  prev = gettime();
}

/*** Synchronisation ***/
extern int timerstyle;

void S9xSyncSpeed ()
{
    uint32 skipFrms = Settings.SkipFrames;

    if ( Settings.TurboMode )
        skipFrms = Settings.TurboSkipFrames;

    if ( timerstyle == 0 ) /* use NGC vertical sync (VSYNC) with NTSC roms */
    {
        while (FrameTimer == 0)
        {
            usleep (50);
        }

        if (FrameTimer > skipFrms)
            FrameTimer = skipFrms;

        if ((FrameTimer > 1) && (IPPU.SkippedFrames < skipFrms))
        {
            IPPU.SkippedFrames++;
            IPPU.RenderThisFrame = FALSE;
        }
        else
        {
            IPPU.SkippedFrames = 0;
            IPPU.RenderThisFrame = TRUE;
        }
	}
    else  /* use internal timer for PAL roms */
    {
        unsigned int timediffallowed = Settings.TurboMode ? 0 : Settings.FrameTime;
        now = gettime();

        if (diff_usec(prev, now) > timediffallowed)
        {
            /*while ( diff_usec((prev, now) <  timediffallowed * 2) {
                now = gettime();
            }*/

            /* Timer has already expired */
            if (IPPU.SkippedFrames < skipFrms)
            {
                IPPU.SkippedFrames++;
                IPPU.RenderThisFrame = FALSE;
            }
            else
            {
                IPPU.SkippedFrames = 0;
                IPPU.RenderThisFrame = TRUE;
            }
        }
        else
        {
            /*** Ahead - so hold up ***/
            while (diff_usec(prev, now) <  timediffallowed) now=gettime();
            IPPU.RenderThisFrame = TRUE;
            IPPU.SkippedFrames = 0;
        }

        prev = now;
    }

    if ( !Settings.TurboMode )
        FrameTimer--;
    return;

}

/*** Video / Display related functions ***/
bool8
S9xInitUpdate ()
{
  /***
    * Is this necessary in 1.50 ?
    * memset (GFX.Screen, 0, IMAGE_WIDTH * IMAGE_HEIGHT * 2);
    */
  return (TRUE);
}

bool8
S9xDeinitUpdate (int Width, int Height)
{
  update_video (Width, Height);
  return (TRUE);
}

bool8
S9xContinueUpdate (int Width, int Height)
{
  return (TRUE);
}

void
S9xSetPalette ()
{
  return;
}

/*** Input functions ***/
void
S9xHandlePortCommand (s9xcommand_t cmd, int16 data1, int16 data2)
{
  return;
}

bool
S9xPollButton (uint32 id, bool * pressed)
{
  return 0;
}

bool
S9xPollAxis (uint32 id, int16 * value)
{
  return 0;
}

bool
S9xPollPointer (uint32 id, int16 * x, int16 * y)
{
  return 0;
}

void
S9xLoadSDD1Data ()
{

  Memory.FreeSDD1Data ();

  Settings.SDD1Pack = FALSE;

  if (strncmp (Memory.ROMName, "Star Ocean", 10) == 0)
    Settings.SDD1Pack = TRUE;

  if (strncmp (Memory.ROMName, "STREET FIGHTER ALPHA2", 21) == 0)
    Settings.SDD1Pack = TRUE;

  return;

}
