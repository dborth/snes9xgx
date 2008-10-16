/**********************************************************************************
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
**********************************************************************************/


#ifndef _apumemory_h_
#define _apumemory_h_

START_EXTERN_C
extern uint8 W4;
extern uint8 APUROM[64];
END_EXTERN_C

INLINE uint8 S9xAPUGetByteZ (uint8 Address)
{
    if (Address >= 0xf0 && IAPU.DirectPage == IAPU.RAM)
    {
	if (Address >= 0xf4 && Address <= 0xf7)
	{
#ifdef SPC700_SHUTDOWN
	    IAPU.WaitAddress2 = IAPU.WaitAddress1;
	    IAPU.WaitAddress1 = IAPU.PC;
#endif	    
	    return (IAPU.RAM [Address]);
	}
	if (Address >= 0xfd)
	{
#ifdef SPC700_SHUTDOWN
	    IAPU.WaitAddress2 = IAPU.WaitAddress1;
	    IAPU.WaitAddress1 = IAPU.PC;
#endif	    
	    uint8 t = IAPU.RAM [Address];
	    IAPU.RAM [Address] = 0;
	    return (t);
	}
	else
	if (Address == 0xf3)
	    return (S9xGetAPUDSP ());

	return (IAPU.RAM [Address]);
    }
    else
	return (IAPU.DirectPage [Address]);
}

INLINE void S9xAPUSetByteZ (uint8 byte, uint8 Address)
{
    if (Address >= 0xf0 && IAPU.DirectPage == IAPU.RAM)
    {
	if (Address == 0xf3)
	    S9xSetAPUDSP (byte);
	else
	if (Address >= 0xf4 && Address <= 0xf7)
	    APU.OutPorts [Address - 0xf4] = byte;
	else
	if (Address == 0xf1)
	    S9xSetAPUControl (byte);
	else
	if (Address < 0xfd)
	{
	    IAPU.RAM [Address] = byte;
	    if (Address >= 0xfa)
	    {
		if (byte == 0)
		    APU.TimerTarget [Address - 0xfa] = 0x100;
		else
		    APU.TimerTarget [Address - 0xfa] = byte;
	    }
	}
    }
    else
	IAPU.DirectPage [Address] = byte;
}

INLINE uint8 S9xAPUGetByte (uint32 Address)
{
    Address &= 0xffff;
    
    if (Address <= 0xff && Address >= 0xf0)
    {
	if (Address >= 0xf4 && Address <= 0xf7)
	{
#ifdef SPC700_SHUTDOWN
	    IAPU.WaitAddress2 = IAPU.WaitAddress1;
	    IAPU.WaitAddress1 = IAPU.PC;
#endif	    
	    return (IAPU.RAM [Address]);
	}
	else
	if (Address == 0xf3)
	    return (S9xGetAPUDSP ());
	if (Address >= 0xfd)
	{
#ifdef SPC700_SHUTDOWN
	    IAPU.WaitAddress2 = IAPU.WaitAddress1;
	    IAPU.WaitAddress1 = IAPU.PC;
#endif
	    uint8 t = IAPU.RAM [Address];
	    IAPU.RAM [Address] = 0;
	    return (t);
	}
	return (IAPU.RAM [Address]);
    }
    else
	return (IAPU.RAM [Address]);
}

INLINE void S9xAPUSetByte (uint8 byte, uint32 Address)
{
    Address &= 0xffff;
    
    if (Address <= 0xff && Address >= 0xf0)
    {
	if (Address == 0xf3)
	    S9xSetAPUDSP (byte);
	else
	if (Address >= 0xf4 && Address <= 0xf7)
	    APU.OutPorts [Address - 0xf4] = byte;
	else
	if (Address == 0xf1)
	    S9xSetAPUControl (byte);
	else
	if (Address < 0xfd)
	{
	    IAPU.RAM [Address] = byte;
	    if (Address >= 0xfa)
	    {
		if (byte == 0)
		    APU.TimerTarget [Address - 0xfa] = 0x100;
		else
		    APU.TimerTarget [Address - 0xfa] = byte;
	    }
	}
    }
    else
    {
#if 0
if (Address >= 0x2500 && Address <= 0x2504)
printf ("%06d %04x <- %02x\n", ICPU.Scanline, Address, byte);
if (Address == 0x26c6)
{
    extern FILE *apu_trace;
    extern FILE *trace;
    APU.Flags |= TRACE_FLAG;
    CPU.Flags |= TRACE_FLAG;
    if (apu_trace == NULL)
	apu_trace = fopen ("aputrace.log", "wb");
    if (trace == NULL)
	trace = fopen ("trace.log", "wb");
    printf ("TRACING SWITCHED ON\n");
}
#endif
	if (Address < 0xffc0)
	    IAPU.RAM [Address] = byte;
	else
	{
	    APU.ExtraRAM [Address - 0xffc0] = byte;
	    if (!APU.ShowROM)
		IAPU.RAM [Address] = byte;
	}
    }
}
#endif

