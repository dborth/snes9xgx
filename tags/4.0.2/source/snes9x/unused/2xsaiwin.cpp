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




//#define MMX

#include "snes9x/snes9x.h"
#include "snes9x/port.h"
#include "snes9x/gfx.h"

#ifdef MMX
EXTERN_C void _2xSaILine  (uint8 *srcPtr, uint8 *deltaPtr, uint32 srcPitch, uint32 width,
                        uint8 *dstPtr, uint32 dstPitch);
EXTERN_C void _2xSaISuperEagleLine  (uint8 *srcPtr, uint8 *deltaPtr, uint32 srcPitch, uint32 width,
                        uint8 *dstPtr, uint32 dstPitch);
EXTERN_C int Init_2xSaIMMX (uint32 BitFormat);
#endif

bool mmx_cpu = false;

static uint32 colorMask = 0xF7DEF7DE;
static uint32 lowPixelMask = 0x08210821;
static uint32 qcolorMask = 0xE79CE79C;
static uint32 qlowpixelMask = 0x18631863;


int Init_2xSaI(uint32 BitFormat)
{
        if (BitFormat == 565)
        {
                colorMask = 0xF7DEF7DE;
                lowPixelMask = 0x08210821;
                qcolorMask = 0xE79CE79C;
                qlowpixelMask = 0x18631863;
        }
        else
        if (BitFormat == 555)
        {
                colorMask = 0x7BDE7BDE;
                lowPixelMask = 0x04210421;
                qcolorMask = 0x739C739C;
                qlowpixelMask = 0x0C630C63;
        }
        else
        {
                return 0;
        }
#ifdef MMX
        Init_2xSaIMMX(BitFormat);
#endif
        return 1;
}

STATIC inline int GetResult1(uint32 A, uint32 B, uint32 C, uint32 D, uint32 E)
{
 int x = 0;
 int y = 0;
 int r = 0;
 if (A == C) x+=1; else if (B == C) y+=1;
 if (A == D) x+=1; else if (B == D) y+=1;
 if (x <= 1) r+=1;
 if (y <= 1) r-=1;
 return r;
}

STATIC inline int GetResult2(uint32 A, uint32 B, uint32 C, uint32 D, uint32 E)
{
 int x = 0;
 int y = 0;
 int r = 0;
 if (A == C) x+=1; else if (B == C) y+=1;
 if (A == D) x+=1; else if (B == D) y+=1;
 if (x <= 1) r-=1;
 if (y <= 1) r+=1;
 return r;
}


STATIC inline int GetResult(uint32 A, uint32 B, uint32 C, uint32 D)
{
 int x = 0;
 int y = 0;
 int r = 0;
 if (A == C) x+=1; else if (B == C) y+=1;
 if (A == D) x+=1; else if (B == D) y+=1;
 if (x <= 1) r+=1;
 if (y <= 1) r-=1;
 return r;
}


STATIC inline uint32 INTERPOLATE(uint32 A, uint32 B)
{
    if (A !=B)
    {
       return ( ((A & colorMask) >> 1) + ((B & colorMask) >> 1) + (A & B & lowPixelMask) );
    }
    else return A;
}


STATIC inline uint32 Q_INTERPOLATE(uint32 A, uint32 B, uint32 C, uint32 D)
{
        register uint32 x = ((A & qcolorMask) >> 2) +
                            ((B & qcolorMask) >> 2) +
                            ((C & qcolorMask) >> 2) +
                            ((D & qcolorMask) >> 2);
        register uint32 y = (A & qlowpixelMask) +
                            (B & qlowpixelMask) +
                            (C & qlowpixelMask) +
                            (D & qlowpixelMask);
        y = (y>>2) & qlowpixelMask;
        return x+y;
}




#define HOR
#define VER
void Super2xSaI(uint8 *srcPtr, uint32 srcPitch,
	     uint8 *deltaPtr,
	     uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    uint32 *dP;
    uint16 *bP;

#ifdef MMX_BLA  //no MMX version yet
    if (cpu_mmx && width != 512)
    {
	for (height; height; height-=1)
	{
	    	bP = (uint16 *) srcPtr;
        	xP = (uint16 *) deltaPtr;
	    	dP = (uint32 *) dstPtr;
        	_2xSaISuperEagleLine  ((uint8 *) bP, (uint8 *) xP, srcPitch, width, (uint8 *) dP, dstPitch);
		dstPtr += dstPitch << 1;
	    	srcPtr += srcPitch;
        	deltaPtr += srcPitch;
        }
    }
    else
    {
#endif
        uint32 Nextline = srcPitch >> 1;

		for (height; height; height-=1)
	{
	    bP = (uint16 *) srcPtr;
	    dP = (uint32 *) dstPtr;
            for (uint32 finish = width; finish; finish -= 1 )
            {
           	uint32 color4, color5, color6;
           	uint32 color1, color2, color3;
           	uint32 colorA0, colorA1, colorA2, colorA3,
                  	colorB0, colorB1, colorB2, colorB3,
                  	colorS1, colorS2;
           	uint32 product1a, product1b,
                  	product2a, product2b;

//---------------------------------------    B1 B2
//                                         4  5  6 S2
//                                         1  2  3 S1
//                                           A1 A2

            	colorB0 = *(bP- Nextline - 1);
            	colorB1 = *(bP- Nextline);
            	colorB2 = *(bP- Nextline + 1);
            	colorB3 = *(bP- Nextline + 2);

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
                else
                if (color5 == color3 && color2 != color6)
                {
                   product2b = product1b = color5;
                }
                else
                if (color5 == color3 && color2 == color6 && color5 != color6)
                {
                   register int r = 0;

                   r += GetResult (color6, color5, color1, colorA1);
                   r += GetResult (color6, color5, color4, colorB1);
                   r += GetResult (color6, color5, colorA2, colorS1);
                   r += GetResult (color6, color5, colorB2, colorS2);

                   if (r > 0)
                      product2b = product1b = color6;
                   else
                   if (r < 0)
                      product2b = product1b = color5;
                   else
                   {
                      product2b = product1b = INTERPOLATE (color5, color6);
                   }

                }
                else
                {

#ifdef VER
                   if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
                      product2b = Q_INTERPOLATE (color3, color3, color3, color2);
                   else
                   if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
                      product2b = Q_INTERPOLATE (color2, color2, color2, color3);
                   else
#endif
                      product2b = INTERPOLATE (color2, color3);

#ifdef VER
                   if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
                      product1b = Q_INTERPOLATE (color6, color6, color6, color5);
                   else
                   if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
                      product1b = Q_INTERPOLATE (color6, color5, color5, color5);
                   else
#endif
                      product1b = INTERPOLATE (color5, color6);
                }

#ifdef HOR
                if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
                   product2a = INTERPOLATE (color2, color5);
                else
                if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
                   product2a = INTERPOLATE(color2, color5);
                else
#endif
                   product2a = color2;

#ifdef HOR
                if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
                   product1a = INTERPOLATE (color2, color5);
                else
                if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
                   product1a = INTERPOLATE(color2, color5);
                else
#endif
                   product1a = color5;


            	product1a = product1a | (product1b << 16);
            	product2a = product2a | (product2b << 16);

		*(dP) = product1a;
		*(dP+(dstPitch>>2)) = product2a;

                bP += 1;
                dP += 1;
            }//end of for ( finish= width etc..)

	    dstPtr += dstPitch << 1;
            srcPtr += srcPitch;
            deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
#ifdef MMX_BLA
    }
#endif
}






/*ONLY use with 640x480x16 or higher resolutions*/
/*Only use this if 2*width * 2*height fits on the current screen*/
void SuperEagle(uint8 *srcPtr, uint32 srcPitch,
	     uint8 *deltaPtr,
		 uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    uint32 *dP;
    uint16 *bP;
    uint16 *xP;

#ifdef MMX
    if (mmx_cpu && width != 512)
    {
	for (height; height; height-=1)
	{
		bP = (uint16 *) srcPtr;
		xP = (uint16 *) deltaPtr;
		dP = (uint32 *) dstPtr;
        	_2xSaISuperEagleLine  ((uint8 *) bP, (uint8 *) xP, srcPitch, width, (uint8 *)dP, dstPitch);
		dstPtr += dstPitch << 1;
	    	srcPtr += srcPitch;
        	deltaPtr += srcPitch;
        }
    }
    else
    {
#endif
        uint32 Nextline = srcPitch >> 1;

        for (height; height; height-=1)
	{
	    bP = (uint16 *) srcPtr;
	    dP = (uint32 *) dstPtr;
            for (uint32 finish = width; finish; finish -= 1 )
            {

           	uint32 color4, color5, color6;
           	uint32 color1, color2, color3;
           	uint32 colorA0, colorA1, colorA2, colorA3,
                  colorB0, colorB1, colorB2, colorB3,
                  colorS1, colorS2;
           	uint32 product1a, product1b,
                  product2a, product2b;

            	colorB0 = *(bP- Nextline - 1);
            	colorB1 = *(bP- Nextline);
            	colorB2 = *(bP- Nextline + 1);
            	colorB3 = *(bP- Nextline + 2);

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
                   product1b = product2a = color2;
                   if ((color1 == color2 && color6 == colorS2) ||
                       (color2 == colorA1 && color6 == colorB2))
                   {
                       product1a = INTERPOLATE (color2, color5);
                       product1a = INTERPOLATE (color2, product1a);
                       product2b = INTERPOLATE (color2, color3);
                       product2b = INTERPOLATE (color2, product2b);
//                       product1a = color2;
//                       product2b = color2;
                   }
                   else
                   {
                      product1a = INTERPOLATE (color5, color6);
                      product2b = INTERPOLATE (color2, color3);
                   }
                }
                else
                if (color5 == color3 && color2 != color6)
                {
                   product2b = product1a = color5;
                   if ((colorB1 == color5 && color3 == colorA2) ||
                       (color4 == color5 && color3 == colorS1))
                   {
                       product1b = INTERPOLATE (color5, color6);
                       product1b = INTERPOLATE (color5, product1b);
                       product2a = INTERPOLATE (color5, color2);
                       product2a = INTERPOLATE (color5, product2a);
//                       product1b = color5;
//                       product2a = color5;
                   }
                   else
                   {
                      product1b = INTERPOLATE (color5, color6);
                      product2a = INTERPOLATE (color2, color3);
                   }
                }
                else
                if (color5 == color3 && color2 == color6 && color5 != color6)
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
                   else
                   if (r < 0)
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

                   if ((color2 == color5) || (color3 == color6))
                   {
                      product1a = color5;
                      product2a = color2;
                      product1b = color6;
                      product2b = color3;

                   }
                   else
                   {
                      product1b = product1a = INTERPOLATE (color5, color6);
                      product1a = INTERPOLATE (color5, product1a);
                      product1b = INTERPOLATE (color6, product1b);

                      product2a = product2b = INTERPOLATE (color2, color3);
                      product2a = INTERPOLATE (color2, product2a);
                      product2b = INTERPOLATE (color3, product2b);
                   }
                }


            	product1a = product1a | (product1b << 16);
            	product2a = product2a | (product2b << 16);

		*(dP) = product1a;
		*(dP+(dstPitch>>2)) = product2a;

                bP += 1;
                dP += 1;
            }//end of for ( finish= width etc..)

	    dstPtr += dstPitch << 1;
            srcPtr += srcPitch;
            deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
#ifdef MMX
    }
#endif
}



/*ONLY use with 640x480x16 or higher resolutions*/
/*Only use this if 2*width * 2*height fits on the current screen*/
void _2xSaI(uint8 *srcPtr, uint32 srcPitch,
	     uint8 *deltaPtr,
	     uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    uint32 *dP;
    uint16 *bP;
    uint16 *xP;

#ifdef MMX
    if (mmx_cpu && width != 512)
    {
	for (height; height; height-=1)
	{

	    bP = (uint16 *) srcPtr;
	    xP = (uint16 *) deltaPtr;
	    dP = (uint32 *) dstPtr;
            _2xSaILine  ((uint8 *) bP, (uint8 *) xP, srcPitch, width, (uint8 *)dP, dstPitch);
	    dstPtr += dstPitch << 1;
	    srcPtr += srcPitch;
            deltaPtr += srcPitch;
        }
    }
    else
    {
#endif
        uint32 Nextline = srcPitch >> 1;

        for (height; height; height-=1)
	{
	    bP = (uint16 *) srcPtr;
	    dP = (uint32 *) dstPtr;
            for (uint32 finish = width; finish; finish -= 1 )
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
                colorI = *(bP- Nextline - 1);
                colorE = *(bP- Nextline);
                colorF = *(bP- Nextline + 1);
                colorJ = *(bP- Nextline + 2);

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
                           if ( ((colorA == colorE) && (colorB == colorL)) ||
                                ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ)) )
                           {
                              product = colorA;
                           }
                           else
                           {
                              product = INTERPOLATE(colorA, colorB);
                           }

                           if (((colorA == colorG) && (colorC == colorO)) ||
                               ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM)) )
                           {
                              product1 = colorA;
                           }
                           else
                           {
                              product1 = INTERPOLATE(colorA, colorC);
                           }
                           product2 = colorA;
                        }
                        else
                        if ((colorB == colorC) && (colorA != colorD))
                        {
                           if (((colorB == colorF) && (colorA == colorH)) ||
                               ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI)) )
                           {
                              product = colorB;
                           }
                           else
                           {
                              product = INTERPOLATE(colorA, colorB);
                           }

                           if (((colorC == colorH) && (colorA == colorF)) ||
                               ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI)) )
                           {
                              product1 = colorC;
                           }
                           else
                           {
                              product1 = INTERPOLATE(colorA, colorC);
                           }
                           product2 = colorB;
                        }
                        else
                        if ((colorA == colorD) && (colorB == colorC))
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
                              product1 = INTERPOLATE(colorA, colorC);
                              product = INTERPOLATE(colorA, colorB);

                              r += GetResult1 (colorA, colorB, colorG, colorE, colorI);
                              r += GetResult2 (colorB, colorA, colorK, colorF, colorJ);
                              r += GetResult2 (colorB, colorA, colorH, colorN, colorM);
                              r += GetResult1 (colorA, colorB, colorL, colorO, colorP);

                              if (r > 0)
                                  product2 = colorA;
                              else
                              if (r < 0)
                                  product2 = colorB;
                              else
                              {
                                  product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);
                              }
                           }
                        }
                        else
                        {
                           product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);

                           if ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ))
                           {
                              product = colorA;
                           }
                           else
                           if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
                           {
                              product = colorB;
                           }
                           else
                           {
                              product = INTERPOLATE(colorA, colorB);
                           }

                           if ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM))
                           {
                              product1 = colorA;
                           }
                           else
                           if ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI))
                           {
                              product1 = colorC;
                           }
                           else
                           {
                              product1 = INTERPOLATE(colorA, colorC);
                           }
                        }
                        product = colorA | (product << 16);
                        product1 = product1 | (product2 << 16);
			*(dP) = product;
			*(dP+(dstPitch>>2)) = product1;

                    bP += 1;
                    dP += 1;
                }//end of for ( finish= width etc..)

	    dstPtr += dstPitch << 1;
            srcPtr += srcPitch;
            deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
#ifdef MMX
    }
#endif
}

