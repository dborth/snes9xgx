/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


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

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


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




#ifdef __DJGPP
#include <allegro.h>
#endif

#include "snes9x.h"
#include "port.h"
#include "gfx.h"

#if (defined(USE_X86_ASM) && (defined (__i386__) || defined (__i486__) || \
               defined (__i586__) || defined (__WIN32__) || defined (__DJGPP)))
#  ifndef MMX
#    define MMX
#  endif
#endif

extern "C"
{

#ifdef MMX
    void _2xSaILine (uint8 *srcPtr, uint8 *deltaPtr, uint32 srcPitch,
		     uint32 width, uint8 *dstPtr, uint32 dstPitch);
    void _2xSaISuperEagleLine (uint8 *srcPtr, uint8 *deltaPtr,
			       uint32 srcPitch, uint32 width,
			       uint8 *dstPtr, uint32 dstPitch);
    void _2xSaISuper2xSaILine (uint8 *srcPtr, uint8 *deltaPtr,
			       uint32 srcPitch, uint32 width,
			       uint8 *dstPtr, uint32 dstPitch);
    void Init_2xSaIMMX (uint32 BitFormat);
    void BilinearMMX (uint16 * A, uint16 * B, uint16 * C, uint16 * D,
		      uint16 * dx, uint16 * dy, uint8 *dP);
    void BilinearMMXGrid0 (uint16 * A, uint16 * B, uint16 * C, uint16 * D,
			   uint16 * dx, uint16 * dy, uint8 *dP);
    void BilinearMMXGrid1 (uint16 * A, uint16 * B, uint16 * C, uint16 * D,
			   uint16 * dx, uint16 * dy, uint8 *dP);
    void EndMMX ();

#endif
}

bool8 cpu_mmx = 1;

static uint32 colorMask = 0xF7DEF7DE;
static uint32 lowPixelMask = 0x08210821;
static uint32 qcolorMask = 0xE79CE79C;
static uint32 qlowpixelMask = 0x18631863;
static uint32 redblueMask = 0xF81F;
static uint32 greenMask = 0x7E0;

int Init_2xSaI (uint32 BitFormat)
{
    if (BitFormat == 565)
    {
	colorMask = 0xF7DEF7DE;
	lowPixelMask = 0x08210821;
	qcolorMask = 0xE79CE79C;
	qlowpixelMask = 0x18631863;
	redblueMask = 0xF81F;
	greenMask = 0x7E0;
    }
    else if (BitFormat == 555)
    {
	colorMask = 0x7BDE7BDE;
	lowPixelMask = 0x04210421;
	qcolorMask = 0x739C739C;
	qlowpixelMask = 0x0C630C63;
	redblueMask = 0x7C1F;
	greenMask = 0x3E0;
    }
    else
    {
	return 0;
    }

#ifdef MMX
    Init_2xSaIMMX (BitFormat);
#endif

    return 1;
}

static inline int GetResult1 (uint32 A, uint32 B, uint32 C, uint32 D,
			      uint32 /* E */)
{
    int x = 0;
    int y = 0;
    int r = 0;

    if (A == C)
	x += 1;
    else if (B == C)
	y += 1;
    if (A == D)
	x += 1;
    else if (B == D)
	y += 1;
    if (x <= 1)
	r += 1;
    if (y <= 1)
	r -= 1;
    return r;
}

static inline int GetResult2 (uint32 A, uint32 B, uint32 C, uint32 D,
			      uint32 /* E */)
{
    int x = 0;
    int y = 0;
    int r = 0;

    if (A == C)
	x += 1;
    else if (B == C)
	y += 1;
    if (A == D)
	x += 1;
    else if (B == D)
	y += 1;
    if (x <= 1)
	r -= 1;
    if (y <= 1)
	r += 1;
    return r;
}

static inline int GetResult (uint32 A, uint32 B, uint32 C, uint32 D)
{
    int x = 0;
    int y = 0;
    int r = 0;

    if (A == C)
	x += 1;
    else if (B == C)
	y += 1;
    if (A == D)
	x += 1;
    else if (B == D)
	y += 1;
    if (x <= 1)
	r += 1;
    if (y <= 1)
	r -= 1;
    return r;
}

static inline uint32 INTERPOLATE (uint32 A, uint32 B)
{
    if (A != B)
    {
	return (((A & colorMask) >> 1) + ((B & colorMask) >> 1) +
		(A & B & lowPixelMask));
    }
    else
	return A;
}

static inline uint32 Q_INTERPOLATE (uint32 A, uint32 B, uint32 C, uint32 D)
{
    register uint32 x = ((A & qcolorMask) >> 2) +
	((B & qcolorMask) >> 2) +
	((C & qcolorMask) >> 2) + ((D & qcolorMask) >> 2);
    register uint32 y = (A & qlowpixelMask) +
	(B & qlowpixelMask) + (C & qlowpixelMask) + (D & qlowpixelMask);

    y = (y >> 2) & qlowpixelMask;
    return x + y;
}

#define BLUE_MASK565 0x001F001F
#define RED_MASK565 0xF800F800
#define GREEN_MASK565 0x07E007E0

#define BLUE_MASK555 0x001F001F
#define RED_MASK555 0x7C007C00
#define GREEN_MASK555 0x03E003E0

void Super2xSaI (uint8 *srcPtr, uint32 srcPitch,
		 uint8 *deltaPtr, uint8 *dstPtr, uint32 dstPitch,
		 int width, int height)
{
    uint16 *bP;
    uint8  *dP;
    uint32 inc_bP;

#ifdef MMX
    if (cpu_mmx)
    {
	while (height--)
	{
	    _2xSaISuper2xSaILine (srcPtr, deltaPtr, srcPitch, width,
				  dstPtr, dstPitch);
	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	    deltaPtr += srcPitch;
	}
    }
    else
#endif

    {
        uint32 Nextline = srcPitch >> 1;
	inc_bP = 1;

	while (height--)
	{
	    bP = (uint16 *) srcPtr;
	    dP = (uint8 *) dstPtr;

	    for (uint32 finish = width; finish; finish -= inc_bP)
	    {
		uint32 color4, color5, color6;
		uint32 color1, color2, color3;
		uint32 colorA0, colorA1, colorA2, colorA3,
		    colorB0, colorB1, colorB2, colorB3, colorS1, colorS2;
		uint32 product1a, product1b, product2a, product2b;

//---------------------------------------    B1 B2
//                                         4  5  6 S2
//                                         1  2  3 S1
//                                           A1 A2

		colorB0 = *(bP - Nextline - 1);
		colorB1 = *(bP - Nextline);
		colorB2 = *(bP - Nextline + 1);
		colorB3 = *(bP - Nextline + 2);

		color4 = *(bP - 1);
		color5 = *(bP);
		color6 = *(bP + 1);
		colorS2 = *(bP + 2);

		color1 = *(bP + Nextline - 1);
		color2 = *(bP + Nextline);
		color3 = *(bP + Nextline + 1);
		colorS1 = *(bP + Nextline + 2);

		colorA0 = *(bP + Nextline + Nextline - 1);
		colorA1 = *(bP + Nextline + Nextline);
		colorA2 = *(bP + Nextline + Nextline + 1);
		colorA3 = *(bP + Nextline + Nextline + 2);

//--------------------------------------
		if (color2 == color6 && color5 != color3)
		{
		    product2b = product1b = color2;
		}
		else if (color5 == color3 && color2 != color6)
		{
		    product2b = product1b = color5;
		}
		else if (color5 == color3 && color2 == color6)
		{
		    register int r = 0;

		    r += GetResult (color6, color5, color1, colorA1);
		    r += GetResult (color6, color5, color4, colorB1);
		    r += GetResult (color6, color5, colorA2, colorS1);
		    r += GetResult (color6, color5, colorB2, colorS2);

		    if (r > 0)
			product2b = product1b = color6;
		    else if (r < 0)
			product2b = product1b = color5;
		    else
		    {
			product2b = product1b = INTERPOLATE (color5, color6);
		    }
		}
		else
		{
		    if (color6 == color3 && color3 == colorA1
			    && color2 != colorA2 && color3 != colorA0)
			product2b =
			    Q_INTERPOLATE (color3, color3, color3, color2);
		    else if (color5 == color2 && color2 == colorA2
			     && colorA1 != color3 && color2 != colorA3)
			product2b =
			    Q_INTERPOLATE (color2, color2, color2, color3);
		    else
			product2b = INTERPOLATE (color2, color3);

		    if (color6 == color3 && color6 == colorB1
			    && color5 != colorB2 && color6 != colorB0)
			product1b =
			    Q_INTERPOLATE (color6, color6, color6, color5);
		    else if (color5 == color2 && color5 == colorB2
			     && colorB1 != color6 && color5 != colorB3)
			product1b =
			    Q_INTERPOLATE (color6, color5, color5, color5);
		    else
			product1b = INTERPOLATE (color5, color6);
		}

		if (color5 == color3 && color2 != color6 && color4 == color5
			&& color5 != colorA2)
		    product2a = INTERPOLATE (color2, color5);
		else
		    if (color5 == color1 && color6 == color5
			&& color4 != color2 && color5 != colorA0)
		    product2a = INTERPOLATE (color2, color5);
		else
		    product2a = color2;

		if (color2 == color6 && color5 != color3 && color1 == color2
			&& color2 != colorB2)
		    product1a = INTERPOLATE (color2, color5);
		else
		    if (color4 == color2 && color3 == color2
			&& color1 != color5 && color2 != colorB0)
		    product1a = INTERPOLATE (color2, color5);
		else
		    product1a = color5;

#ifdef MSB_FIRST
		product1a = product1b | (product1a << 16);
		product2a = product2b | (product2a << 16);
#else
		product1a = product1a | (product1b << 16);
		product2a = product2a | (product2b << 16);
#endif

		*((uint32 *) dP) = product1a;
		*((uint32 *) (dP + dstPitch)) = product2a;

		bP += inc_bP;
		dP += sizeof (uint32);
	    }			// end of for ( finish= width etc..)

	    srcPtr   += srcPitch;
	    dstPtr   += dstPitch * 2;
	    deltaPtr += srcPitch;
	}			// while (height--)
    }
}

void SuperEagle (uint8 *srcPtr, uint32 srcPitch, uint8 *deltaPtr,
		 uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    uint8  *dP;
    uint16 *bP;
    uint16 *xP;
    uint32 inc_bP;

#ifdef MMX
    if (cpu_mmx)
    {
	while (height--)
	{
	    _2xSaISuperEagleLine (srcPtr, deltaPtr, srcPitch, width,
				  dstPtr, dstPitch);
	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	    deltaPtr += srcPitch;
	}
    }
    else
#endif

    {
	inc_bP = 1;

	uint32 Nextline = srcPitch >> 1;

	while (height--)
	{
	    bP = (uint16 *) srcPtr;
	    xP = (uint16 *) deltaPtr;
	    dP = dstPtr;
	    for (uint32 finish = width; finish; finish -= inc_bP)
	    {
		uint32 color4, color5, color6;
		uint32 color1, color2, color3;
		uint32 colorA1, colorA2, colorB1, colorB2, colorS1, colorS2;
		uint32 product1a, product1b, product2a, product2b;

		colorB1 = *(bP - Nextline);
		colorB2 = *(bP - Nextline + 1);

		color4 = *(bP - 1);
		color5 = *(bP);
		color6 = *(bP + 1);
		colorS2 = *(bP + 2);

		color1 = *(bP + Nextline - 1);
		color2 = *(bP + Nextline);
		color3 = *(bP + Nextline + 1);
		colorS1 = *(bP + Nextline + 2);

		colorA1 = *(bP + Nextline + Nextline);
		colorA2 = *(bP + Nextline + Nextline + 1);

		// --------------------------------------
		if (color2 == color6 && color5 != color3)
		{
		    product1b = product2a = color2;
		    if ((color1 == color2) || (color6 == colorB2))
		    {
			product1a = INTERPOLATE (color2, color5);
			product1a = INTERPOLATE (color2, product1a);
//                       product1a = color2;
		    }
		    else
		    {
			product1a = INTERPOLATE (color5, color6);
		    }

		    if ((color6 == colorS2) || (color2 == colorA1))
		    {
			product2b = INTERPOLATE (color2, color3);
			product2b = INTERPOLATE (color2, product2b);
//                       product2b = color2;
		    }
		    else
		    {
			product2b = INTERPOLATE (color2, color3);
		    }
		}
		else if (color5 == color3 && color2 != color6)
		{
		    product2b = product1a = color5;

		    if ((colorB1 == color5) || (color3 == colorS1))
		    {
			product1b = INTERPOLATE (color5, color6);
			product1b = INTERPOLATE (color5, product1b);
//                       product1b = color5;
		    }
		    else
		    {
			product1b = INTERPOLATE (color5, color6);
		    }

		    if ((color3 == colorA2) || (color4 == color5))
		    {
			product2a = INTERPOLATE (color5, color2);
			product2a = INTERPOLATE (color5, product2a);
//                       product2a = color5;
		    }
		    else
		    {
			product2a = INTERPOLATE (color2, color3);
		    }

		}
		else if (color5 == color3 && color2 == color6)
		{
		    register int r = 0;

		    r += GetResult (color6, color5, color1, colorA1);
		    r += GetResult (color6, color5, color4, colorB1);
		    r += GetResult (color6, color5, colorA2, colorS1);
		    r += GetResult (color6, color5, colorB2, colorS2);

		    if (r > 0)
		    {
			product1b = product2a = color2;
			product1a = product2b = INTERPOLATE (color5, color6);
		    }
		    else if (r < 0)
		    {
			product2b = product1a = color5;
			product1b = product2a = INTERPOLATE (color5, color6);
		    }
		    else
		    {
			product2b = product1a = color5;
			product1b = product2a = color2;
		    }
		}
		else
		{
		    product2b = product1a = INTERPOLATE (color2, color6);
		    product2b =
			Q_INTERPOLATE (color3, color3, color3, product2b);
		    product1a =
			Q_INTERPOLATE (color5, color5, color5, product1a);

		    product2a = product1b = INTERPOLATE (color5, color3);
		    product2a =
			Q_INTERPOLATE (color2, color2, color2, product2a);
		    product1b =
			Q_INTERPOLATE (color6, color6, color6, product1b);

//                    product1a = color5;
//                    product1b = color6;
//                    product2a = color2;
//                    product2b = color3;
		}
#ifdef MSB_FIRST
		product1a = product1b | (product1a << 16);
		product2a = product2b | (product2a << 16);
#else
		product1a = product1a | (product1b << 16);
		product2a = product2a | (product2b << 16);
#endif

		*((uint32 *) dP) = product1a;
		*((uint32 *) (dP + dstPitch)) = product2a;
		*xP = color5;

		bP += inc_bP;
		xP += inc_bP;
		dP += sizeof (uint32);
	    }			// end of for ( finish= width etc..)

	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	    deltaPtr += srcPitch;
	}			// endof: while (height--)
    }
}

void _2xSaI (uint8 *srcPtr, uint32 srcPitch, uint8 *deltaPtr,
	     uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    uint8  *dP;
    uint16 *bP;
    uint32 inc_bP;

#ifdef MMX
    if (cpu_mmx)
    {
	while (height--)
	{
	    _2xSaILine (srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch);
	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	    deltaPtr += srcPitch;
	}
    }
    else
#endif

    {
	inc_bP = 1;

	uint32 Nextline = srcPitch >> 1;

	while (height--)
	{
	    bP = (uint16 *) srcPtr;
	    dP = dstPtr;

	    for (uint32 finish = width; finish; finish -= inc_bP)
	    {

		register uint32 colorA, colorB;
		uint32 colorC, colorD,
		    colorE, colorF, colorG, colorH,
		    colorI, colorJ, colorK, colorL,

		    colorM, colorN, colorO, colorP;
		uint32 product, product1, product2;

//---------------------------------------
// Map of the pixels:                    I|E F|J
//                                       G|A B|K
//                                       H|C D|L
//                                       M|N O|P
		colorI = *(bP - Nextline - 1);
		colorE = *(bP - Nextline);
		colorF = *(bP - Nextline + 1);
		colorJ = *(bP - Nextline + 2);

		colorG = *(bP - 1);
		colorA = *(bP);
		colorB = *(bP + 1);
		colorK = *(bP + 2);

		colorH = *(bP + Nextline - 1);
		colorC = *(bP + Nextline);
		colorD = *(bP + Nextline + 1);
		colorL = *(bP + Nextline + 2);

		colorM = *(bP + Nextline + Nextline - 1);
		colorN = *(bP + Nextline + Nextline);
		colorO = *(bP + Nextline + Nextline + 1);
		colorP = *(bP + Nextline + Nextline + 2);

		if ((colorA == colorD) && (colorB != colorC))
		{
		    if (((colorA == colorE) && (colorB == colorL)) ||
			    ((colorA == colorC) && (colorA == colorF)
			     && (colorB != colorE) && (colorB == colorJ)))
		    {
			product = colorA;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if (((colorA == colorG) && (colorC == colorO)) ||
			    ((colorA == colorB) && (colorA == colorH)
			     && (colorG != colorC) && (colorC == colorM)))
		    {
			product1 = colorA;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		    product2 = colorA;
		}
		else if ((colorB == colorC) && (colorA != colorD))
		{
		    if (((colorB == colorF) && (colorA == colorH)) ||
			    ((colorB == colorE) && (colorB == colorD)
			     && (colorA != colorF) && (colorA == colorI)))
		    {
			product = colorB;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if (((colorC == colorH) && (colorA == colorF)) ||
			    ((colorC == colorG) && (colorC == colorD)
			     && (colorA != colorH) && (colorA == colorI)))
		    {
			product1 = colorC;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		    product2 = colorB;
		}
		else if ((colorA == colorD) && (colorB == colorC))
		{
		    if (colorA == colorB)
		    {
			product = colorA;
			product1 = colorA;
			product2 = colorA;
		    }
		    else
		    {
			register int r = 0;

			product1 = INTERPOLATE (colorA, colorC);
			product = INTERPOLATE (colorA, colorB);

			r +=
                            GetResult1 (colorA, colorB, colorG, colorE,
					colorI);
			r +=
			    GetResult2 (colorB, colorA, colorK, colorF,
					colorJ);
			r +=
			    GetResult2 (colorB, colorA, colorH, colorN,
					colorM);
			r +=
			    GetResult1 (colorA, colorB, colorL, colorO,
					colorP);

			if (r > 0)
			    product2 = colorA;
			else if (r < 0)
			    product2 = colorB;
			else
			{
			    product2 =
				Q_INTERPOLATE (colorA, colorB, colorC,
					       colorD);
			}
		    }
		}
		else
		{
		    product2 = Q_INTERPOLATE (colorA, colorB, colorC, colorD);

		    if ((colorA == colorC) && (colorA == colorF)
			    && (colorB != colorE) && (colorB == colorJ))
		    {
			product = colorA;
		    }
		    else
			if ((colorB == colorE) && (colorB == colorD)
			    && (colorA != colorF) && (colorA == colorI))
		    {
			product = colorB;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if ((colorA == colorB) && (colorA == colorH)
			    && (colorG != colorC) && (colorC == colorM))
		    {
			product1 = colorA;
		    }
		    else
			if ((colorC == colorG) && (colorC == colorD)
			    && (colorA != colorH) && (colorA == colorI))
		    {
			product1 = colorC;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		}

#ifdef MSB_FIRST
		product = product | (colorA << 16);
		product1 = product2 | (product1 << 16);
#else
		product = colorA | (product << 16);
		product1 = product1 | (product2 << 16);
#endif
		*((int32 *) dP) = product;
		*((uint32 *) (dP + dstPitch)) = product1;

		bP += inc_bP;
		dP += sizeof (uint32);
	    }			// end of for ( finish= width etc..)

	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	    deltaPtr += srcPitch;
	}			// endof: while (height--)
    }
}

#ifdef MMX
void Scale_2xSaI (uint8 *srcPtr, uint32 srcPitch, uint8 * /* deltaPtr */,
		  uint8 *dstPtr, uint32 dstPitch,
		  uint32 dstWidth, uint32 dstHeight, int width, int height)
{
    uint8  *dP;
    uint16 *bP;
    uint32 w;
    uint32 h;
    uint32 dw;
    uint32 dh;
    uint32 hfinish;
    uint32 wfinish;
    uint32 Nextline;
    uint16 colorA[4];
    uint16 colorB[4];
    uint16 colorC[4];
    uint16 colorD[4];
    uint16 dx[4];
    uint16 dy[4];

    Nextline = srcPitch >> 1;

    wfinish = (width - 1) << 16;	// convert to fixed point
    hfinish = (height - 1) << 16;	// convert to fixed point
    dw = wfinish / (dstWidth - 1);
    dh = hfinish / (dstHeight - 1);

    for (h = 0; h < hfinish; h += dh)
    {
	uint32 y1, y2;

	y1 = h & 0xffff;	// fraction part of fixed point
	y2 = 0x10000 - y1;
	bP = (uint16 *) (srcPtr + ((h >> 16) * srcPitch));
	dP = dstPtr;

	for (w = 0; w < wfinish;)
	{
	    uint32 A, B, C, D;
	    uint32 E, F, G, H;
	    uint32 I, J, K, L;
	    uint32 x1, x2, a1, f1, f2;
	    uint32 position;

	    for (int c = 0; c < 4; c++)
	    {
		position = w >> 16;
		A = bP[position];	// current pixel
		B = bP[position + 1];	// next pixel
		C = bP[position + Nextline];
		D = bP[position + Nextline + 1];
		E = bP[position - Nextline];
		F = bP[position - Nextline + 1];
		G = bP[position - 1];
		H = bP[position + Nextline - 1];
		I = bP[position + 2];
		J = bP[position + Nextline + 2];
		K = bP[position + Nextline + Nextline];
		L = bP[position + Nextline + Nextline + 1];

		x1 = w & 0xffff;	// fraction part of fixed point
		x2 = 0x10000 - x1;

		/*1*/
		if (A == D && B != C)
		{
		    f1 = (x1 >> 1) + (0x10000 >> 2);
		    f2 = (y1 >> 1) + (0x10000 >> 2);
		    if (y1 <= f1 && A == J && A != E)	// close to B
		    {
			a1 = f1 - y1;
			colorA[c] = A;
			colorB[c] = B;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y1 >= f1 && A == G && A != L)	// close to C
		    {
			a1 = y1 - f1;
			colorA[c] = A;
			colorB[c] = C;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (x1 >= f2 && A == E && A != J)	// close to B
		    {
			a1 = x1 - f2;
			colorA[c] = A;
			colorB[c] = B;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (x1 <= f2 && A == L && A != G)	// close to C
		    {
			a1 = f2 - x1;
			colorA[c] = A;
			colorB[c] = C;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y1 >= x1)	// close to C
		    {
			a1 = y1 - x1;
			colorA[c] = A;
			colorB[c] = C;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y1 <= x1)	// close to B
		    {
			a1 = x1 - y1;
			colorA[c] = A;
			colorB[c] = B;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		}
		else
		/*2*/
		if (B == C && A != D)
		{
		    f1 = (x1 >> 1) + (0x10000 >> 2);
		    f2 = (y1 >> 1) + (0x10000 >> 2);
		    if (y2 >= f1 && B == H && B != F)	// close to A
		    {
			a1 = y2 - f1;
			colorA[c] = B;
			colorB[c] = A;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y2 <= f1 && B == I && B != K)	// close to D
		    {
			a1 = f1 - y2;
			colorA[c] = B;
			colorB[c] = D;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (x2 >= f2 && B == F && B != H)	// close to A
		    {
			a1 = x2 - f2;
			colorA[c] = B;
			colorB[c] = A;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (x2 <= f2 && B == K && B != I)	// close to D
		    {
			a1 = f2 - x2;
			colorA[c] = B;
			colorB[c] = D;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y2 >= x1)	// close to A
		    {
			a1 = y2 - x1;
			colorA[c] = B;
			colorB[c] = A;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		    else if (y2 <= x1)	// close to D
		    {
			a1 = x1 - y2;
			colorA[c] = B;
			colorB[c] = D;
			colorC[c] = 0;
			colorD[c] = 0;
			dx[c] = a1;
			dy[c] = 0;
		    }
		}
		/*3*/
		else
		{
		    colorA[c] = A;
		    colorB[c] = B;
		    colorC[c] = C;
		    colorD[c] = D;
		    dx[c] = x1;
		    dy[c] = y1;
		}
		w += dw;
	    }
	    BilinearMMX (colorA, colorB, colorC, colorD, dx, dy, dP);
	    dP += 8;
	}
	dstPtr += dstPitch;
    };
    EndMMX ();

}

#else
static uint32 Bilinear (uint32 A, uint32 B, uint32 x)
{
    unsigned long areaA, areaB;
    unsigned long result;

    if (A == B)
	return A;

    areaB = (x >> 11) & 0x1f;	// reduce 16 bit fraction to 5 bits
    areaA = 0x20 - areaB;

    A = (A & redblueMask) | ((A & greenMask) << 16);
    B = (B & redblueMask) | ((B & greenMask) << 16);

    result = ((areaA * A) + (areaB * B)) >> 5;

    return (result & redblueMask) | ((result >> 16) & greenMask);

}

static uint32 Bilinear4 (uint32 A, uint32 B, uint32 C, uint32 D, uint32 x,
			 uint32 y)
{
    unsigned long areaA, areaB, areaC, areaD;
    unsigned long result, xy;

    x = (x >> 11) & 0x1f;
    y = (y >> 11) & 0x1f;
    xy = (x * y) >> 5;

    A = (A & redblueMask) | ((A & greenMask) << 16);
    B = (B & redblueMask) | ((B & greenMask) << 16);
    C = (C & redblueMask) | ((C & greenMask) << 16);
    D = (D & redblueMask) | ((D & greenMask) << 16);

    areaA = 0x20 + xy - x - y;
    areaB = x - xy;
    areaC = y - xy;
    areaD = xy;

    result = ((areaA * A) + (areaB * B) + (areaC * C) + (areaD * D)) >> 5;

    return (result & redblueMask) | ((result >> 16) & greenMask);
}

void Scale_2xSaI (uint8 *srcPtr, uint32 srcPitch, uint8 * /* deltaPtr */,
		  uint8 *dstPtr, uint32 dstPitch,
		  uint32 dstWidth, uint32 dstHeight, int width, int height)
{
    uint8  *dP;
    uint16 *bP;

    uint32 w;
    uint32 h;
    uint32 dw;
    uint32 dh;
    uint32 hfinish;
    uint32 wfinish;

    uint32 Nextline = srcPitch >> 1;

    wfinish = (width - 1) << 16;	// convert to fixed point
    dw = wfinish / (dstWidth - 1);
    hfinish = (height - 1) << 16;	// convert to fixed point
    dh = hfinish / (dstHeight - 1);

    for (h = 0; h < hfinish; h += dh)
    {
	uint32 y1, y2;

	y1 = h & 0xffff;	// fraction part of fixed point
	bP = (uint16 *) (srcPtr + ((h >> 16) * srcPitch));
	dP = dstPtr;
	y2 = 0x10000 - y1;

	w = 0;

	for (; w < wfinish;)
	{
	    uint32 A, B, C, D;
	    uint32 E, F, G, H;
	    uint32 I, J, K, L;
	    uint32 x1, x2, a1, f1, f2;
	    uint32 position, product1;

	    position = w >> 16;
	    A = bP[position];	// current pixel
	    B = bP[position + 1];	// next pixel
	    C = bP[position + Nextline];
	    D = bP[position + Nextline + 1];
	    E = bP[position - Nextline];
	    F = bP[position - Nextline + 1];
	    G = bP[position - 1];
	    H = bP[position + Nextline - 1];
	    I = bP[position + 2];
	    J = bP[position + Nextline + 2];
	    K = bP[position + Nextline + Nextline];
	    L = bP[position + Nextline + Nextline + 1];

	    x1 = w & 0xffff;	// fraction part of fixed point
	    x2 = 0x10000 - x1;

	    /*0*/
	    if (A == B && C == D && A == C)
		product1 = A;
	    else
	    /*1*/
	    if (A == D && B != C)
	    {
		f1 = (x1 >> 1) + (0x10000 >> 2);
		f2 = (y1 >> 1) + (0x10000 >> 2);
		if (y1 <= f1 && A == J && A != E)	// close to B
		{
		    a1 = f1 - y1;
		    product1 = Bilinear (A, B, a1);
		}
		else if (y1 >= f1 && A == G && A != L)	// close to C
		{
		    a1 = y1 - f1;
		    product1 = Bilinear (A, C, a1);
		}
		else if (x1 >= f2 && A == E && A != J)	// close to B
		{
		    a1 = x1 - f2;
		    product1 = Bilinear (A, B, a1);
		}
		else if (x1 <= f2 && A == L && A != G)	// close to C
		{
		    a1 = f2 - x1;
		    product1 = Bilinear (A, C, a1);
		}
		else if (y1 >= x1)	// close to C
		{
		    a1 = y1 - x1;
		    product1 = Bilinear (A, C, a1);
		}
		else if (y1 <= x1)	// close to B
		{
		    a1 = x1 - y1;
		    product1 = Bilinear (A, B, a1);
		}
	    }
	    else
	    /*2*/
	    if (B == C && A != D)
	    {
		f1 = (x1 >> 1) + (0x10000 >> 2);
		f2 = (y1 >> 1) + (0x10000 >> 2);
		if (y2 >= f1 && B == H && B != F)	// close to A
		{
		    a1 = y2 - f1;
		    product1 = Bilinear (B, A, a1);
		}
		else if (y2 <= f1 && B == I && B != K)	// close to D
		{
		    a1 = f1 - y2;
		    product1 = Bilinear (B, D, a1);
		}
		else if (x2 >= f2 && B == F && B != H)	// close to A
		{
		    a1 = x2 - f2;
		    product1 = Bilinear (B, A, a1);
		}
		else if (x2 <= f2 && B == K && B != I)	// close to D
		{
		    a1 = f2 - x2;
		    product1 = Bilinear (B, D, a1);
		}
		else if (y2 >= x1)	// close to A
		{
		    a1 = y2 - x1;
		    product1 = Bilinear (B, A, a1);
		}
		else if (y2 <= x1)	// close to D
		{
		    a1 = x1 - y2;
		    product1 = Bilinear (B, D, a1);
		}
	    }
	    /*3*/
	    else
	    {
		product1 = Bilinear4 (A, B, C, D, x1, y1);
	    }

//end First Pixel
	    *(uint32 *) dP = product1;
	    dP += 2;
	    w += dw;
	}
	dstPtr += dstPitch;
    }
}

#endif

