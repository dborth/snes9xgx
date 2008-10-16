/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * s9xconfig.cpp
 *
 * Configuration parameters have been moved here for easier maintenance.
 * Refer to Snes9x.h for all combinations.
 * The defaults used here are taken directly from porting.html
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
#include "snes9x.h"
#include "snes9xGX.h"
#include "smbload.h"

void
DefaultSettings ()
{
    /*** Default ALL to false ***/
  memset (&Settings, 0, sizeof (Settings));

	/*** General ***/
  Settings.MouseMaster = true;
  Settings.SuperScopeMaster = true;
  Settings.MultiPlayer5Master = false;
  Settings.JustifierMaster = true;
  Settings.ShutdownMaster = false;
  Settings.CyclesPercentage = 100; // snes9x 1.50 and earlier
  
    /* Eke-Eke: specific to snes9x 1.51 */
//  Settings.BlockInvalidVRAMAccess = true;
//  Settings.HDMATimingHack = 100;

	/*** Sound defaults. On GC this is 32Khz/16bit/Stereo/InterpolatedSound ***/
  Settings.APUEnabled = true;
  Settings.NextAPUEnabled = true;
  Settings.SoundPlaybackRate = 32000;
  Settings.Stereo = true;
  Settings.SixteenBitSound = true;
  Settings.SoundEnvelopeHeightReading = true;
  Settings.DisableSampleCaching = true;
  Settings.InterpolatedSound = true;
  Settings.ReverseStereo = false;

	/*** Graphics ***/
  Settings.Transparency = true;
  Settings.SupportHiRes = true;
  Settings.SkipFrames = 10;
  Settings.TurboSkipFrames = 19;
  Settings.DisplayFrameRate = false;
//  Settings.AutoDisplayMessages = 1;    /*** eke-eke snes9x 1.51 ***/
  
    /* Eke-Eke: frame timings in 50hz and 60hz cpu mode */
  Settings.FrameTimePAL = 20000;
  Settings.FrameTimeNTSC = 16667;

	/*** SDD1 - Star Ocean Returns -;) ***/
  Settings.SDD1Pack = true;
  
  GCSettings.AutoLoad = 1;
  GCSettings.AutoSave = 1;
  
  strncpy (GCSettings.gcip, GC_IP, 15);
  strncpy (GCSettings.gwip, GW_IP, 15);
  strncpy (GCSettings.mask, MASK, 15);
  strncpy (GCSettings.smbip, SMB_IP, 15);
  strncpy (GCSettings.smbuser, SMB_USER, 19);
  strncpy (GCSettings.smbpwd, SMB_PWD, 19);
  strncpy (GCSettings.smbgcid, SMB_GCID, 19);
  strncpy (GCSettings.smbsvid, SMB_SVID, 19);
  strncpy (GCSettings.smbshare, SMB_SHARE, 19);
  
  GCSettings.NGCZoom = 0;
  GCSettings.VerifySaves = 0;
  GCSettings.render = 0;
  
  Settings.ForceNTSC = 0;
  Settings.ForcePAL = 0;
  Settings.ForceHiROM = 0;
  Settings.ForceLoROM = 0;
  Settings.ForceHeader = 0;
  Settings.ForceNoHeader = 0;
  Settings.ForceTransparency = 0;
  Settings.ForceInterleaved = 0;
  Settings.ForceInterleaved2 = 0;
  Settings.ForceInterleaveGD24 = 0;
  Settings.ForceNotInterleaved = 0;
  Settings.ForceNoSuperFX = 0;
  Settings.ForceSuperFX = 0;
  Settings.ForceDSP1 = 0;
  Settings.ForceNoDSP1 = 0;
  
}

