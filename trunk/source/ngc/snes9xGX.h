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

#ifndef _SNES9XGX_H_
#define _SNES9XGX_H_

#include <gccore.h>
#include "snes9x.h"

// FIX: these are unused, but could be... must also change "freezecomment", PREFSVERSTRING, and "sramcomment"
//#define GCVERSION "002"
//#define GCVERSIONSTRING "Snes9x 1.5 v002"

#define NOTSILENT 0
#define SILENT 1

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB
};

struct SGCSettings{
    uint8  AutoLoad;
    uint8  AutoSave;
    uint8  LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	char   LoadFolder[200]; // Path to game files
	uint8  SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, SMB
	char   SaveFolder[200]; // Path to save files
    char   gcip[16];
    char   gwip[16];
    char   mask[16];
    char   smbip[16];
    char   smbuser[20];
    char   smbpwd[20];
    char   smbgcid[20];
    char   smbsvid[20];
    char   smbshare[20];
    bool8  NGCZoom;
    uint8  VerifySaves;
	u16		render;		// 0 - original, 1 - no AA
	u16 Superscope;
	u16 Mouse;
	u16 Justifier; 
};

START_EXTERN_C
extern struct SGCSettings GCSettings;
extern unsigned short saveicon[1024];
extern bool8 isWii;

extern u32 wpad_get_analogues(int pad, float* mag1, u16* ang1, float* mag2, u16* ang2);
extern void SetControllers ();
END_EXTERN_C

#define	JOY_THRESHOLD	0.70	// for wii (expansion) analogues

/*** default SMB settings ***/
#ifndef GC_IP
#define GC_IP "192.168.1.32"		/*** IP to assign the GameCube ***/
#endif
#ifndef GW_IP
#define GW_IP "192.168.1.100"	/*** Your gateway IP ***/
#endif
#ifndef MASK
#define MASK "255.255.255.0"	/*** Your subnet mask ***/
#endif
#ifndef SMB_USER
#define SMB_USER "Guest"		/*** Your share user ***/
#endif
#ifndef SMB_PWD
#define SMB_PWD "password"		/*** Your share user password ***/
#endif
#ifndef SMB_GCID
#define SMB_GCID "gamecube"	/*** Machine Name of GameCube ***/
#endif
#ifndef SMB_SVID
#define SMB_SVID "mypc"	/*** Machine Name of Server(Share) ***/
#endif
#ifndef SMB_SHARE
#define SMB_SHARE "gcshare"	/*** Share name on server ***/
#endif
#ifndef SMB_IP
#define SMB_IP "192.168.1.100"	/*** IP Address of share server ***/
#endif

#endif
