/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Michniewski 2008
 *
 * HQ2x, HQ3x, HQ4x filters
 * (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)
 *
 * filter.cpp
 *
 * Adapted from Snes9x Win32/MacOSX ports
 * Video Filter Code (hq2x)
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <malloc.h>

#include "filter.h"
#include "video.h"
#include "snes9xgx.h"
#include "snes9x/memmap.h"

#include "menu.h"

#define NUMBITS (16)

// DCBT: Data Cache Block Touch for Gekko/Broadway (32-byte cache lines)
#define DCBT(ptr) __builtin_prefetch((void*)(ptr), 0, 0)

TFilterMethod FilterMethod;

template<int GuiScale> void RenderHQ2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void RenderScale2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void RenderTVMode (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void Render2xBR (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void Render2xBRlv1 (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void RenderDDT (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);

const char* GetFilterName (RenderFilter filterID)
{
    switch(filterID)
    {
        default: return "Unknown";
        case FILTER_NONE: return "None";
        case FILTER_HQ2X: return "hq2x";
        case FILTER_HQ2XS: return "hq2x Soft";
        case FILTER_HQ2XBOLD: return "hq2x Bold";
        case FILTER_SCALE2X: return "Scale2x";
        case FILTER_TVMODE: return "TV Mode";
        case FILTER_2XBR: return "2xBR";
        case FILTER_2XBRLV1: return "2xBR-lv1";
        case FILTER_DDT: return "DDT";
    }
}

// Return pointer to appropriate function
static TFilterMethod FilterToMethod (RenderFilter filterID)
{
    switch(filterID)
    {
        case FILTER_HQ2X:       return RenderHQ2X<FILTER_HQ2X>;
        case FILTER_HQ2XS:      return RenderHQ2X<FILTER_HQ2XS>;
        case FILTER_HQ2XBOLD:   return RenderHQ2X<FILTER_HQ2XBOLD>;
        case FILTER_SCALE2X:    return RenderScale2X<FILTER_SCALE2X>;
        case FILTER_TVMODE:     return RenderTVMode<FILTER_TVMODE>;
        case FILTER_2XBR:     return Render2xBR<FILTER_2XBR>;
        case FILTER_2XBRLV1:     return Render2xBRlv1<FILTER_2XBRLV1>;
        case FILTER_DDT:     return RenderDDT<FILTER_DDT>;
        default: return 0;
    }
}

int GetFilterScale(RenderFilter filterID)
{
    switch(filterID)
    {
        case FILTER_NONE:
        return 1;
        default:
        case FILTER_HQ2X:
        case FILTER_HQ2XS:
        case FILTER_HQ2XBOLD:
        case FILTER_SCALE2X:
        case FILTER_TVMODE:
        case FILTER_2XBR:
        case FILTER_2XBRLV1:
        case FILTER_DDT:
        return 2;
    }
}

void SelectFilterMethod ()
{
    FilterMethod = FilterToMethod((RenderFilter)GCSettings.FilterMethod);
}

// Fixed Point Inlined Replacements for RGB Lookup Tables
static inline uint16 inlineRGBtoBright(uint16 c) {
	uint16 b = (c & 0x001F) << 3;
	uint16 g = (c & 0x07E0) >> 3;
	uint16 r = (c & 0xF800) >> 8;
	return (r * 3) + (g * 3) + (b << 1);
}

static inline int inlineRGBtoYUV(uint16 c) {
	int32 b = (c & 0x001F) << 3;
	int32 g = (c & 0x07E0) >> 3;
	int32 r = (c & 0xF800) >> 8;

	// Fixed point arithmetic (multiplying floats by 65536, +32768 equals +0.5f rounding)
	int32 y = ( 16829 * r + 33039 * g +  6416 * b + 32768) >> 16;
	int32 u = (- 9714 * r - 19071 * g + 28784 * b + 32768) >> 16;
	int32 v = ( 28784 * r - 24103 * g -  4681 * b + 32768) >> 16;

	y += 16;
	u += 128;
	v += 128;

	return ((y & 0xFF) << 16) | ((u & 0xFF) << 8) | (v & 0xFF);
}

// Condenses 8-bit expansion and luminance weighting into pure inline constants
// to completely eliminate 256KB L2 cache thrashing on Gekko processors.
static inline int RGB565_to_Lum(uint16 c) {
    int r = (c >> 11);
    int g = (c >> 5) & 0x3F;
    int b = (c & 0x1F);
    return (r * 140) + (g * 113) + (b * 62);
}

//
// HQ2X Filter Code:
//

#define	Mask_2	0x07E0	// 00000 111111 00000
#define	Mask13	0xF81F	// 11111 000000 11111

#define	Ymask	0xFF0000
#define	Umask	0x00FF00
#define	Vmask	0x0000FF
#define	trY		0x300000
#define	trU		0x000700
#define	trV		0x000006

#define Interp01(c1, c2) \
((((c1) == (c2)) ? (c1) : \
(((((((c1) & Mask_2) *  3) + ((c2) & Mask_2)) >> 2) & Mask_2) + \
((((((c1) & Mask13) *  3) + ((c2) & Mask13)) >> 2) & Mask13))))

#define Interp02(c1, c2, c3) \
((((((c1) & Mask_2) *  2 + ((c2) & Mask_2)     + ((c3) & Mask_2)    ) >> 2) & Mask_2) + \
(((((c1) & Mask13) *  2 + ((c2) & Mask13)     + ((c3) & Mask13)    ) >> 2) & Mask13))

#define Interp06(c1, c2, c3) \
((((((c1) & Mask_2) *  5 + ((c2) & Mask_2) * 2 + ((c3) & Mask_2)    ) >> 3) & Mask_2) + \
(((((c1) & Mask13) *  5 + ((c2) & Mask13) * 2 + ((c3) & Mask13)    ) >> 3) & Mask13))

#define Interp07(c1, c2, c3) \
((((((c1) & Mask_2) *  6 + ((c2) & Mask_2)     + ((c3) & Mask_2)    ) >> 3) & Mask_2) + \
(((((c1) & Mask13) *  6 + ((c2) & Mask13)     + ((c3) & Mask13)    ) >> 3) & Mask13))

#define Interp09(c1, c2, c3) \
((((((c1) & Mask_2) *  2 + ((c2) & Mask_2) * 3 + ((c3) & Mask_2) * 3) >> 3) & Mask_2) + \
(((((c1) & Mask13) *  2 + ((c2) & Mask13) * 3 + ((c3) & Mask13) * 3) >> 3) & Mask13))

#define Interp10(c1, c2, c3) \
((((((c1) & Mask_2) * 14 + ((c2) & Mask_2)     + ((c3) & Mask_2)    ) >> 4) & Mask_2) + \
(((((c1) & Mask13) * 14 + ((c2) & Mask13)     + ((c3) & Mask13)    ) >> 4) & Mask13))

#define PIXEL00_0		*(dp) = w5
#define PIXEL00_10		*(dp) = Interp01(w5, w1)
#define PIXEL00_11		*(dp) = Interp01(w5, w4)
#define PIXEL00_12		*(dp) = Interp01(w5, w2)
#define PIXEL00_20		*(dp) = Interp02(w5, w4, w2)
#define PIXEL00_21		*(dp) = Interp02(w5, w1, w2)
#define PIXEL00_22		*(dp) = Interp02(w5, w1, w4)
#define PIXEL00_60		*(dp) = Interp06(w5, w2, w4)
#define PIXEL00_61		*(dp) = Interp06(w5, w4, w2)
#define PIXEL00_70		*(dp) = Interp07(w5, w4, w2)
#define PIXEL00_90		*(dp) = Interp09(w5, w4, w2)
#define PIXEL00_100		*(dp) = Interp10(w5, w4, w2)

#define PIXEL01_0		*(dp + 1) = w5
#define PIXEL01_10		*(dp + 1) = Interp01(w5, w3)
#define PIXEL01_11		*(dp + 1) = Interp01(w5, w2)
#define PIXEL01_12		*(dp + 1) = Interp01(w5, w6)
#define PIXEL01_20		*(dp + 1) = Interp02(w5, w2, w6)
#define PIXEL01_21		*(dp + 1) = Interp02(w5, w3, w6)
#define PIXEL01_22		*(dp + 1) = Interp02(w5, w3, w2)
#define PIXEL01_60		*(dp + 1) = Interp06(w5, w6, w2)
#define PIXEL01_61		*(dp + 1) = Interp06(w5, w2, w6)
#define PIXEL01_70		*(dp + 1) = Interp07(w5, w2, w6)
#define PIXEL01_90		*(dp + 1) = Interp09(w5, w2, w6)
#define PIXEL01_100		*(dp + 1) = Interp10(w5, w2, w6)

#define PIXEL10_0		*(dp + dst1line) = w5
#define PIXEL10_10		*(dp + dst1line) = Interp01(w5, w7)
#define PIXEL10_11		*(dp + dst1line) = Interp01(w5, w8)
#define PIXEL10_12		*(dp + dst1line) = Interp01(w5, w4)
#define PIXEL10_20		*(dp + dst1line) = Interp02(w5, w8, w4)
#define PIXEL10_21		*(dp + dst1line) = Interp02(w5, w7, w4)
#define PIXEL10_22		*(dp + dst1line) = Interp02(w5, w7, w8)
#define PIXEL10_60		*(dp + dst1line) = Interp06(w5, w4, w8)
#define PIXEL10_61		*(dp + dst1line) = Interp06(w5, w8, w4)
#define PIXEL10_70		*(dp + dst1line) = Interp07(w5, w8, w4)
#define PIXEL10_90		*(dp + dst1line) = Interp09(w5, w8, w4)
#define PIXEL10_100		*(dp + dst1line) = Interp10(w5, w8, w4)

#define PIXEL11_0		*(dp + dst1line + 1) = w5
#define PIXEL11_10		*(dp + dst1line + 1) = Interp01(w5, w9)
#define PIXEL11_11		*(dp + dst1line + 1) = Interp01(w5, w6)
#define PIXEL11_12		*(dp + dst1line + 1) = Interp01(w5, w8)
#define PIXEL11_20		*(dp + dst1line + 1) = Interp02(w5, w6, w8)
#define PIXEL11_21		*(dp + dst1line + 1) = Interp02(w5, w9, w8)
#define PIXEL11_22		*(dp + dst1line + 1) = Interp02(w5, w9, w6)
#define PIXEL11_60		*(dp + dst1line + 1) = Interp06(w5, w8, w6)
#define PIXEL11_61		*(dp + dst1line + 1) = Interp06(w5, w6, w8)
#define PIXEL11_70		*(dp + dst1line + 1) = Interp07(w5, w6, w8)
#define PIXEL11_90		*(dp + dst1line + 1) = Interp09(w5, w6, w8)
#define PIXEL11_100		*(dp + dst1line + 1) = Interp10(w5, w6, w8)

#define Absolute(c) \
(!(c & (1 << 31)) ? c : (~c + 1))

static inline bool Diff(int c1, int c2)
{
    int c1y = (c1 & Ymask) - (c2 & Ymask);
    if (Absolute(c1y) > trY) return true;
    int c1u = (c1 & Umask) - (c2 & Umask);
    if (Absolute(c1u) > trU) return true;
    int c1v = (c1 & Vmask) - (c2 & Vmask);
    if (Absolute(c1v) > trV) return true;
    
    return false;
}

#define HQ2XCASES \
case 0: case 1: case 4: case 32: case 128: case 5: case 132: case 160: case 33: case 129: case 36: case 133: case 164: case 161: case 37: case 165: PIXEL00_20; PIXEL01_20; PIXEL10_20; PIXEL11_20; break; \
case 2: case 34: case 130: case 162: PIXEL00_22; PIXEL01_21; PIXEL10_20; PIXEL11_20; break; \
case 16: case 17: case 48: case 49: PIXEL00_20; PIXEL01_22; PIXEL10_20; PIXEL11_21; break; \
case 64: case 65: case 68: case 69: PIXEL00_20; PIXEL01_20; PIXEL10_21; PIXEL11_22; break; \
case 8: case 12: case 136: case 140: PIXEL00_21; PIXEL01_20; PIXEL10_22; PIXEL11_20; break; \
case 3: case 35: case 131: case 163: PIXEL00_11; PIXEL01_21; PIXEL10_20; PIXEL11_20; break; \
case 6: case 38: case 134: case 166: PIXEL00_22; PIXEL01_12; PIXEL10_20; PIXEL11_20; break; \
case 20: case 21: case 52: case 53: PIXEL00_20; PIXEL01_11; PIXEL10_20; PIXEL11_21; break; \
case 144: case 145: case 176: case 177: PIXEL00_20; PIXEL01_22; PIXEL10_20; PIXEL11_12; break; \
case 192: case 193: case 196: case 197: PIXEL00_20; PIXEL01_20; PIXEL10_21; PIXEL11_11; break; \
case 96: case 97: case 100: case 101: PIXEL00_20; PIXEL01_20; PIXEL10_12; PIXEL11_22; break; \
case 40: case 44: case 168: case 172: PIXEL00_21; PIXEL01_20; PIXEL10_11; PIXEL11_20; break; \
case 9: case 13: case 137: case 141: PIXEL00_12; PIXEL01_20; PIXEL10_22; PIXEL11_20; break; \
case 18: case 50: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_20; PIXEL10_20; PIXEL11_21; break; \
case 80: case 81: PIXEL00_20; PIXEL01_22; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_20; break; \
case 72: case 76: PIXEL00_21; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_20; PIXEL11_22; break; \
case 10: case 138: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_20; PIXEL01_21; PIXEL10_22; PIXEL11_20; break; \
case 66: PIXEL00_22; PIXEL01_21; PIXEL10_21; PIXEL11_22; break; \
case 24: PIXEL00_21; PIXEL01_22; PIXEL10_22; PIXEL11_21; break; \
case 7: case 39: case 135: PIXEL00_11; PIXEL01_12; PIXEL10_20; PIXEL11_20; break; \
case 148: case 149: case 180: PIXEL00_20; PIXEL01_11; PIXEL10_20; PIXEL11_12; break; \
case 224: case 228: case 225: PIXEL00_20; PIXEL01_20; PIXEL10_12; PIXEL11_11; break; \
case 41: case 169: case 45: PIXEL00_12; PIXEL01_20; PIXEL10_11; PIXEL11_20; break; \
case 22: case 54: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_20; PIXEL11_21; break; \
case 208: case 209: PIXEL00_20; PIXEL01_22; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 104: case 108: PIXEL00_21; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 11: case 139: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_22; PIXEL11_20; break; \
case 19: case 51: if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL00_11, PIXEL01_10; else PIXEL00_60, PIXEL01_90; PIXEL10_20; PIXEL11_21; break; \
case 146: case 178: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_20; break; \
case 84: case 85: PIXEL00_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL01_11, PIXEL11_10; else PIXEL01_60, PIXEL11_90; PIXEL10_21; break; \
case 112: case 113: PIXEL00_20; PIXEL01_22; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL10_12, PIXEL11_10; else PIXEL10_61, PIXEL11_90; break; \
case 200: case 204: PIXEL00_21; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 73: case 77: if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL00_12, PIXEL10_10; else PIXEL00_61, PIXEL10_90; PIXEL01_20; PIXEL11_22; break; \
case 42: case 170: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_21; PIXEL11_20; break; \
case 14: case 142: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_22; PIXEL11_20; break; \
case 67: PIXEL00_11; PIXEL01_21; PIXEL10_21; PIXEL11_22; break; \
case 70: PIXEL00_22; PIXEL01_12; PIXEL10_21; PIXEL11_22; break; \
case 28: PIXEL00_21; PIXEL01_11; PIXEL10_22; PIXEL11_21; break; \
case 152: PIXEL00_21; PIXEL01_22; PIXEL10_22; PIXEL11_12; break; \
case 194: PIXEL00_22; PIXEL01_21; PIXEL10_21; PIXEL11_11; break; \
case 98: PIXEL00_22; PIXEL01_21; PIXEL10_12; PIXEL11_22; break; \
case 56: PIXEL00_21; PIXEL01_22; PIXEL10_11; PIXEL11_21; break; \
case 25: PIXEL00_12; PIXEL01_22; PIXEL10_22; PIXEL11_21; break; \
case 26: case 31: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_21; break; \
case 82: case 214: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 88: case 248: PIXEL00_21; PIXEL01_22; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 74: case 107: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_21; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 27: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_22; PIXEL11_21; break; \
case 86: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_21; PIXEL11_10; break; \
case 216: PIXEL00_21; PIXEL01_22; PIXEL10_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 106: PIXEL00_10; PIXEL01_21; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 30: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_21; break; \
case 210: PIXEL00_22; PIXEL01_10; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 120: PIXEL00_21; PIXEL01_22; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 75: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_10; PIXEL11_22; break; \
case 29: PIXEL00_12; PIXEL01_11; PIXEL10_22; PIXEL11_21; break; \
case 198: PIXEL00_22; PIXEL01_12; PIXEL10_21; PIXEL11_11; break; \
case 184: PIXEL00_21; PIXEL01_22; PIXEL10_11; PIXEL11_12; break; \
case 99: PIXEL00_11; PIXEL01_21; PIXEL10_12; PIXEL11_22; break; \
case 57: PIXEL00_12; PIXEL01_22; PIXEL10_11; PIXEL11_21; break; \
case 71: PIXEL00_11; PIXEL01_12; PIXEL10_21; PIXEL11_22; break; \
case 156: PIXEL00_21; PIXEL01_11; PIXEL10_22; PIXEL11_12; break; \
case 226: PIXEL00_22; PIXEL01_21; PIXEL10_12; PIXEL11_11; break; \
case 60: PIXEL00_21; PIXEL01_11; PIXEL10_11; PIXEL11_21; break; \
case 195: PIXEL00_11; PIXEL01_21; PIXEL10_21; PIXEL11_11; break; \
case 102: PIXEL00_22; PIXEL01_12; PIXEL10_12; PIXEL11_22; break; \
case 153: PIXEL00_12; PIXEL01_22; PIXEL10_22; PIXEL11_12; break; \
case 58: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_21; break; \
case 83: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 92: PIXEL00_21; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 202: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; PIXEL01_21; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 78: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; PIXEL11_22; break; \
case 154: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_22; PIXEL11_12; break; \
case 114: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 89: PIXEL00_12; PIXEL01_22; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 90: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 55: case 23: if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL00_11, PIXEL01_0; else PIXEL00_60, PIXEL01_90; PIXEL10_20; PIXEL11_21; break; \
case 182: case 150: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_20; break; \
case 213: case 212: PIXEL00_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL01_11, PIXEL11_0; else PIXEL01_60, PIXEL11_90; PIXEL10_21; break; \
case 241: case 240: PIXEL00_20; PIXEL01_22; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL10_12, PIXEL11_0; else PIXEL10_61, PIXEL11_90; break; \
case 236: case 232: PIXEL00_21; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 109: case 105: if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL00_12, PIXEL10_0; else PIXEL00_61, PIXEL10_90; PIXEL01_20; PIXEL11_22; break; \
case 171: case 43: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_21; PIXEL11_20; break; \
case 143: case 15: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_22; PIXEL11_20; break; \
case 124: PIXEL00_21; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 203: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_10; PIXEL11_11; break; \
case 62: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_11; PIXEL11_21; break; \
case 211: PIXEL00_11; PIXEL01_10; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 118: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_12; PIXEL11_10; break; \
case 217: PIXEL00_12; PIXEL01_22; PIXEL10_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 110: PIXEL00_10; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 155: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_22; PIXEL11_12; break; \
case 188: PIXEL00_21; PIXEL01_11; PIXEL10_11; PIXEL11_12; break; \
case 185: PIXEL00_12; PIXEL01_22; PIXEL10_11; PIXEL11_12; break; \
case 61: PIXEL00_12; PIXEL01_11; PIXEL10_11; PIXEL11_21; break; \
case 157: PIXEL00_12; PIXEL01_11; PIXEL10_22; PIXEL11_12; break; \
case 103: PIXEL00_11; PIXEL01_12; PIXEL10_12; PIXEL11_22; break; \
case 227: PIXEL00_11; PIXEL01_21; PIXEL10_12; PIXEL11_11; break; \
case 230: PIXEL00_22; PIXEL01_12; PIXEL10_12; PIXEL11_11; break; \
case 199: PIXEL00_11; PIXEL01_12; PIXEL10_21; PIXEL11_11; break; \
case 220: PIXEL00_21; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 158: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_12; break; \
case 234: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; PIXEL01_21; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_11; break; \
case 242: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 59: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_21; break; \
case 121: PIXEL00_12; PIXEL01_22; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 87: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 79: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; PIXEL11_22; break; \
case 122: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 94: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 218: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 91: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 229: PIXEL00_20; PIXEL01_20; PIXEL10_12; PIXEL11_11; break; \
case 167: PIXEL00_11; PIXEL01_12; PIXEL10_20; PIXEL11_20; break; \
case 173: PIXEL00_12; PIXEL01_20; PIXEL10_11; PIXEL11_20; break; \
case 181: PIXEL00_20; PIXEL01_11; PIXEL10_20; PIXEL11_12; break; \
case 186: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_12; break; \
case 115: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 93: PIXEL00_12; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 206: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 205: case 201: PIXEL00_12; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 174: case 46: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_10; else PIXEL00_70; PIXEL01_12; PIXEL10_11; PIXEL11_20; break; \
case 179: case 147: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_10; else PIXEL01_70; PIXEL10_20; PIXEL11_12; break; \
case 117: case 116: PIXEL00_20; PIXEL01_11; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_10; else PIXEL11_70; break; \
case 189: PIXEL00_12; PIXEL01_11; PIXEL10_11; PIXEL11_12; break; \
case 231: PIXEL00_11; PIXEL01_12; PIXEL10_12; PIXEL11_11; break; \
case 126: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 219: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 125: if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL00_12, PIXEL10_0; else PIXEL00_61, PIXEL10_90; PIXEL01_11; PIXEL11_10; break; \
case 221: PIXEL00_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL01_11, PIXEL11_0; else PIXEL01_60, PIXEL11_90; PIXEL10_10; break; \
case 207: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_10; PIXEL11_11; break; \
case 238: PIXEL00_10; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 190: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_11; break; \
case 187: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_10; PIXEL11_12; break; \
case 243: PIXEL00_11; PIXEL01_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL10_12, PIXEL11_0; else PIXEL10_61, PIXEL11_90; break; \
case 119: if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL00_11, PIXEL01_0; else PIXEL00_60, PIXEL01_90; PIXEL10_12; PIXEL11_10; break; \
case 237: case 233: PIXEL00_12; PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 175: case 47: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; PIXEL01_12; PIXEL10_11; PIXEL11_20; break; \
case 183: case 151: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_20; PIXEL11_12; break; \
case 245: case 244: PIXEL00_20; PIXEL01_11; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 250: PIXEL00_10; PIXEL01_10; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 123: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_10; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 95: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_10; PIXEL11_10; break; \
case 222: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 252: PIXEL00_21; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 249: PIXEL00_12; PIXEL01_22; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 235: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_21; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 111: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 63: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_11; PIXEL11_21; break; \
case 159: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_22; PIXEL11_12; break; \
case 215: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_21; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 246: PIXEL00_22; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 254: PIXEL00_10; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 253: PIXEL00_12; PIXEL01_11; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 251: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; PIXEL01_10; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 239: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; PIXEL01_12; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 127: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_20; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 191: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_11; PIXEL11_12; break; \
case 223: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_20; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_10; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_20; break; \
case 247: PIXEL00_11; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; PIXEL10_12; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break; \
case 255: if (Diff(inlineRGBtoYUV(w4), inlineRGBtoYUV(w2))) PIXEL00_0; else PIXEL00_100; if (Diff(inlineRGBtoYUV(w2), inlineRGBtoYUV(w6))) PIXEL01_0; else PIXEL01_100; if (Diff(inlineRGBtoYUV(w8), inlineRGBtoYUV(w4))) PIXEL10_0; else PIXEL10_100; if (Diff(inlineRGBtoYUV(w6), inlineRGBtoYUV(w8))) PIXEL11_0; else PIXEL11_100; break;

template<int GuiScale>
void RenderHQ2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

	int	w1, w2, w3, w4, w5, w6, w7, w8, w9;

	uint32	src1line = srcPitch >> 1;
	uint32	dst1line = dstPitch >> 1;
	uint16	*sp = (uint16 *) srcPtr;
	uint16	*dp = (uint16 *) dstPtr;

	uint32  pattern;
	int		l, y;

	while (height--)
	{
		sp--;

		w1 = *(sp - src1line);
		w4 = *(sp);
		w7 = *(sp + src1line);

		sp++;

		w2 = *(sp - src1line);
		w5 = *(sp);
		w8 = *(sp + src1line);

		for (l = width; l; l--)
		{
			sp++;

			// Prefetch the next cache line (32 bytes ahead) for each active row
			DCBT(sp + 16 - src1line);
			DCBT(sp + 16);
			DCBT(sp + 16 + src1line);

			w3 = *(sp - src1line);
			w6 = *(sp);
			w9 = *(sp + src1line);

			pattern = 0;

			switch(GuiScale)
			{
				case FILTER_HQ2XBOLD: {
					const uint16 avg = (inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) + inlineRGBtoBright(w4) + inlineRGBtoBright(w5) + inlineRGBtoBright(w6) + inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) / 9;
					const bool diff5 = inlineRGBtoBright(w5) > avg;
					if ((w1 != w5) && ((inlineRGBtoBright(w1) > avg) != diff5)) pattern |= (1 << 0);
					if ((w2 != w5) && ((inlineRGBtoBright(w2) > avg) != diff5)) pattern |= (1 << 1);
					if ((w3 != w5) && ((inlineRGBtoBright(w3) > avg) != diff5)) pattern |= (1 << 2);
					if ((w4 != w5) && ((inlineRGBtoBright(w4) > avg) != diff5)) pattern |= (1 << 3);
					if ((w6 != w5) && ((inlineRGBtoBright(w6) > avg) != diff5)) pattern |= (1 << 4);
					if ((w7 != w5) && ((inlineRGBtoBright(w7) > avg) != diff5)) pattern |= (1 << 5);
					if ((w8 != w5) && ((inlineRGBtoBright(w8) > avg) != diff5)) pattern |= (1 << 6);
					if ((w9 != w5) && ((inlineRGBtoBright(w9) > avg) != diff5)) pattern |= (1 << 7);
				}  break;

				case FILTER_HQ2XS: {
					bool nosame = true;
					if(w1 == w5 || w3 == w5 || w7 == w5 || w9 == w5)
					nosame = false;

					if(nosame)
					{
						const uint16 avg = (inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) + inlineRGBtoBright(w4) + inlineRGBtoBright(w5) + inlineRGBtoBright(w6) + inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) / 9;
						const bool diff5 = inlineRGBtoBright(w5) > avg;
						if((inlineRGBtoBright(w1) > avg) != diff5) pattern |= (1 << 0);
						if((inlineRGBtoBright(w2) > avg) != diff5) pattern |= (1 << 1);
						if((inlineRGBtoBright(w3) > avg) != diff5) pattern |= (1 << 2);
						if((inlineRGBtoBright(w4) > avg) != diff5) pattern |= (1 << 3);
						if((inlineRGBtoBright(w6) > avg) != diff5) pattern |= (1 << 4);
						if((inlineRGBtoBright(w7) > avg) != diff5) pattern |= (1 << 5);
						if((inlineRGBtoBright(w8) > avg) != diff5) pattern |= (1 << 6);
						if((inlineRGBtoBright(w9) > avg) != diff5) pattern |= (1 << 7);
					}
					else
					{
						y = inlineRGBtoYUV(w5);
						if ((w1 != w5) && (Diff(y, inlineRGBtoYUV(w1)))) pattern |= (1 << 0);
						if ((w2 != w5) && (Diff(y, inlineRGBtoYUV(w2)))) pattern |= (1 << 1);
						if ((w3 != w5) && (Diff(y, inlineRGBtoYUV(w3)))) pattern |= (1 << 2);
						if ((w4 != w5) && (Diff(y, inlineRGBtoYUV(w4)))) pattern |= (1 << 3);
						if ((w6 != w5) && (Diff(y, inlineRGBtoYUV(w6)))) pattern |= (1 << 4);
						if ((w7 != w5) && (Diff(y, inlineRGBtoYUV(w7)))) pattern |= (1 << 5);
						if ((w8 != w5) && (Diff(y, inlineRGBtoYUV(w8)))) pattern |= (1 << 6);
						if ((w9 != w5) && (Diff(y, inlineRGBtoYUV(w9)))) pattern |= (1 << 7);
					}
				}  break;
				default:
				case FILTER_HQ2X:
				y = inlineRGBtoYUV(w5);
				if ((w1 != w5) && (Diff(y, inlineRGBtoYUV(w1)))) pattern |= (1 << 0);
				if ((w2 != w5) && (Diff(y, inlineRGBtoYUV(w2)))) pattern |= (1 << 1);
				if ((w3 != w5) && (Diff(y, inlineRGBtoYUV(w3)))) pattern |= (1 << 2);
				if ((w4 != w5) && (Diff(y, inlineRGBtoYUV(w4)))) pattern |= (1 << 3);
				if ((w6 != w5) && (Diff(y, inlineRGBtoYUV(w6)))) pattern |= (1 << 4);
				if ((w7 != w5) && (Diff(y, inlineRGBtoYUV(w7)))) pattern |= (1 << 5);
				if ((w8 != w5) && (Diff(y, inlineRGBtoYUV(w8)))) pattern |= (1 << 6);
				if ((w9 != w5) && (Diff(y, inlineRGBtoYUV(w9)))) pattern |= (1 << 7);
				break;
			}

			switch (pattern)
			{
				HQ2XCASES
			}

			w1 = w2; w4 = w5; w7 = w8;
			w2 = w3; w5 = w6; w8 = w9;

			dp += 2;
		}

		dp += ((dst1line - width) << 1);
		sp +=  (src1line - width);
	}
}

template<int GuiScale>
void RenderScale2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	if (height <= 0 || width <= 0) return;

	u32 p = (u32)srcPtr;
	u32 q = (u32)dstPtr;

	// Pitch variables (Constrained to base registers to prevent r0 allocation for lhzx/stwx)
	u32 srcP = srcPitch;
	s32 negSrcP = -(s32)srcPitch;
	u32 dstP = dstPitch;
	u32 dstP2x = dstPitch << 1;

	u32 w = width >> 1; // Unrolled by 2 pixels per iteration
	u32 h = height;     // Explicitly declared for the outer loop constraint

	// Register allocation mappings
	u32 src, dst, B, D, E, F, H;
	u32 M_DB, M_BF, M_DH, M_HF;
	u32 C0, C1, C2, C3, T1, T2;

	__asm__ __volatile__ (
		"1: \n" // --- Outer Row Loop ---
		"mr     %[src], %[p] \n"
		"mr     %[dst], %[q] \n"

		// Prime the sliding window (D and E) for the start of the row
		"lhz    %[D], -2(%[src]) \n"
		"lhz    %[E],  0(%[src]) \n"
		"mtctr  %[w] \n"

		"2: \n" // --- Inner Pixel Loop (Unrolled x2) ---

		// 1. Prefetch Cache Line (32 bytes / 16 pixels ahead)
		"addi   %[T1], %[src], 32 \n"
		"dcbt   0, %[T1] \n"

		// ==========================================
		// PIXEL ITERATION 1
		// ==========================================
		"lhzx   %[B], %[negSrcP], %[src] \n"
		"lhz    %[F], 2(%[src]) \n"
		"lhzx   %[H], %[srcP], %[src] \n"

		// 2. Superscalar Equality Evaluation
		"xor    %[T1], %[D], %[B] \n"
		"xor    %[T2], %[B], %[F] \n"
		"cntlzw %[T1], %[T1] \n"
		"cntlzw %[T2], %[T2] \n"
		"srwi   %[M_DB], %[T1], 5 \n"
		"srwi   %[M_BF], %[T2], 5 \n"
		"neg    %[M_DB], %[M_DB] \n"
		"neg    %[M_BF], %[M_BF] \n"

		"xor    %[T1], %[D], %[H] \n"
		"xor    %[T2], %[H], %[F] \n"
		"cntlzw %[T1], %[T1] \n"
		"cntlzw %[T2], %[T2] \n"
		"srwi   %[M_DH], %[T1], 5 \n"
		"srwi   %[M_HF], %[T2], 5 \n"
		"neg    %[M_DH], %[M_DH] \n"
		"neg    %[M_HF], %[M_HF] \n"

		// 3. Compound Logic Synthesis
		"andc   %[C0], %[M_DB], %[M_BF] \n"
		"andc   %[C0], %[C0], %[M_DH] \n" // C0: D==B && B!=F && D!=H

		"andc   %[C1], %[M_BF], %[M_DB] \n"
		"andc   %[C1], %[C1], %[M_HF] \n" // C1: B==F && B!=D && F!=H

		"andc   %[C2], %[M_DH], %[M_DB] \n"
		"andc   %[C2], %[C2], %[M_HF] \n" // C2: D==H && D!=B && H!=F

		"andc   %[C3], %[M_HF], %[M_DH] \n"
		"andc   %[C3], %[C3], %[M_BF] \n" // C3: H==F && D!=H && B!=F

		// 4. Pixel Resolution & Packing
		"and    %[T1], %[D], %[C0] \n"
		"andc   %[T2], %[E], %[C0] \n"
		"or     %[C0], %[T1], %[T2] \n"   // Resolve Top-Left

		"and    %[T1], %[F], %[C1] \n"
		"andc   %[T2], %[E], %[C1] \n"
		"or     %[C1], %[T1], %[T2] \n"   // Resolve Top-Right

		"rlwimi %[C1], %[C0], 16, 0, 15 \n" // Pack Top-Left and Top-Right
		"stw    %[C1], 0(%[dst]) \n"

		"and    %[T1], %[D], %[C2] \n"
		"andc   %[T2], %[E], %[C2] \n"
		"or     %[C2], %[T1], %[T2] \n"   // Resolve Bottom-Left

		"and    %[T1], %[F], %[C3] \n"
		"andc   %[T2], %[E], %[C3] \n"
		"or     %[C3], %[T1], %[T2] \n"   // Resolve Bottom-Right

		"rlwimi %[C3], %[C2], 16, 0, 15 \n" // Pack Bottom-Left and Bottom-Right
		"stwx   %[C3], %[dstP], %[dst] \n"

		// 5. Shift Sliding Window
		"mr     %[D], %[E] \n"
		"mr     %[E], %[F] \n"
		"addi   %[src], %[src], 2 \n"
		"addi   %[dst], %[dst], 4 \n"

		// ==========================================
		// PIXEL ITERATION 2
		// ==========================================
		"lhzx   %[B], %[negSrcP], %[src] \n"
		"lhz    %[F], 2(%[src]) \n"
		"lhzx   %[H], %[srcP], %[src] \n"

		"xor    %[T1], %[D], %[B] \n"
		"xor    %[T2], %[B], %[F] \n"
		"cntlzw %[T1], %[T1] \n"
		"cntlzw %[T2], %[T2] \n"
		"srwi   %[M_DB], %[T1], 5 \n"
		"srwi   %[M_BF], %[T2], 5 \n"
		"neg    %[M_DB], %[M_DB] \n"
		"neg    %[M_BF], %[M_BF] \n"

		"xor    %[T1], %[D], %[H] \n"
		"xor    %[T2], %[H], %[F] \n"
		"cntlzw %[T1], %[T1] \n"
		"cntlzw %[T2], %[T2] \n"
		"srwi   %[M_DH], %[T1], 5 \n"
		"srwi   %[M_HF], %[T2], 5 \n"
		"neg    %[M_DH], %[M_DH] \n"
		"neg    %[M_HF], %[M_HF] \n"

		"andc   %[C0], %[M_DB], %[M_BF] \n"
		"andc   %[C0], %[C0], %[M_DH] \n"

		"andc   %[C1], %[M_BF], %[M_DB] \n"
		"andc   %[C1], %[C1], %[M_HF] \n"

		"andc   %[C2], %[M_DH], %[M_DB] \n"
		"andc   %[C2], %[C2], %[M_HF] \n"

		"andc   %[C3], %[M_HF], %[M_DH] \n"
		"andc   %[C3], %[C3], %[M_BF] \n"

		"and    %[T1], %[D], %[C0] \n"
		"andc   %[T2], %[E], %[C0] \n"
		"or     %[C0], %[T1], %[T2] \n"

		"and    %[T1], %[F], %[C1] \n"
		"andc   %[T2], %[E], %[C1] \n"
		"or     %[C1], %[T1], %[T2] \n"

		"rlwimi %[C1], %[C0], 16, 0, 15 \n"
		"stw    %[C1], 0(%[dst]) \n"

		"and    %[T1], %[D], %[C2] \n"
		"andc   %[T2], %[E], %[C2] \n"
		"or     %[C2], %[T1], %[T2] \n"

		"and    %[T1], %[F], %[C3] \n"
		"andc   %[T2], %[E], %[C3] \n"
		"or     %[C3], %[T1], %[T2] \n"

		"rlwimi %[C3], %[C2], 16, 0, 15 \n"
		"stwx   %[C3], %[dstP], %[dst] \n"

		"mr     %[D], %[E] \n"
		"mr     %[E], %[F] \n"
		"addi   %[src], %[src], 2 \n"
		"addi   %[dst], %[dst], 4 \n"

		"bdnz   2b \n" // Loop until row is done

		// --- Advance Row Pointers ---
		"add    %[p], %[p], %[srcP] \n"
		"add    %[q], %[q], %[dstP2x] \n"
		"subic. %[h], %[h], 1 \n"
		"bne    1b \n" // Loop until all rows are done

		// --- Constraints ---
		: [p] "+b" (p), [q] "+b" (q), [h] "+r" (h),
		  [src] "=&b" (src), [dst] "=&b" (dst),
		  [B] "=&r" (B), [D] "=&r" (D), [E] "=&r" (E), [F] "=&r" (F), [H] "=&r" (H),
		  [M_DB] "=&r" (M_DB), [M_BF] "=&r" (M_BF), [M_DH] "=&r" (M_DH), [M_HF] "=&r" (M_HF),
		  [C0] "=&r" (C0), [C1] "=&r" (C1), [C2] "=&r" (C2), [C3] "=&r" (C3),
		  [T1] "=&r" (T1), [T2] "=&r" (T2)
		: [w] "r" (w),
		  [srcP] "b" (srcP), [negSrcP] "b" (negSrcP), // "b" constraint guarantees r1-r31
		  [dstP] "b" (dstP), [dstP2x] "r" (dstP2x)
		: "cc", "memory"
	);
}

template<int GuiScale>
void RenderTVMode (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    unsigned int nextlineSrc = srcPitch / sizeof(uint16);
    uint16 *p = (uint16 *)srcPtr;
    
    unsigned int nextlineDst = dstPitch / sizeof(uint16);
    uint16 *q = (uint16 *)dstPtr;
    
    while(height--) {
        for (int i = 0, j = 0; i < width; ++i, j += 2) {
            uint16 p1 = *(p + i);
            uint32 pi;
            
            pi = (((p1 & Mask_2) * 6) >> 3) & Mask_2;
            pi |= (((p1 & Mask13) * 6) >> 3) & Mask13;
            
            *(q + j) = p1;
            *(q + j + 1) = p1;
            *(q + j + nextlineDst) = (uint16)pi;
            *(q + j + nextlineDst + 1) = (uint16)pi;
        }
        p += nextlineSrc;
        q += nextlineDst << 1;
    }
}

//---------------------------------------------------------------------------------------------------------------------------

#define RB_MASK565 0xF81F
#define R_MASK565  0xF800
#define G_MASK565  0x07E0
#define B_MASK565  0x001F
#define LB_MASK565 0xF7DE

static const uint16 rb_mask = RB_MASK565;
static const uint16  r_mask =  R_MASK565;
static const uint16  g_mask =  G_MASK565;
static const uint16  b_mask =  B_MASK565;
static const uint16 lb_mask = LB_MASK565;

#define ALPHA_BLEND_128_W(dst, src) dst = ((src & lb_mask) >> 1) + ((dst & lb_mask) >> 1)

#define ALPHA_BLEND_16_W(dst, src) \
    dst = ( (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask))) >>4))) | \
            ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask))) >>4))) )

#define ALPHA_BLEND_32_W(dst, src) \
    dst = ( \
          (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask))) >>3))) | \
          ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask))) >>3))) )

#define ALPHA_BLEND_64_W(dst, src) \
    dst = ( \
          (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask))) >>2))) | \
          ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask))) >>2))) )

#define ALPHA_BLEND_96_W(dst, src) \
    dst = ( \
          (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask)) * 96) >>8))) | \
          ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask)) * 96) >>8))) )

#define ALPHA_BLEND_192_W(dst, src) \
    dst = ( \
          (rb_mask & ((src & rb_mask) + ((((dst & rb_mask) - (src & rb_mask))) >>2))) | \
          ( g_mask & ((src &  g_mask) + ((((dst &  g_mask) - (src &  g_mask))) >>2))) )

#define ALPHA_BLEND_224_W(dst, src) \
    dst = ( \
          (rb_mask & ((src & rb_mask) + ((((dst & rb_mask) - (src & rb_mask))) >>3))) | \
          ( g_mask & ((src &  g_mask) + ((((dst &  g_mask) - (src &  g_mask))) >>3))) )


#define LEFT_UP_2_2X(N3, N2, N1, PIXEL)\
    ALPHA_BLEND_224_W(Ep[N3], PIXEL); \
    ALPHA_BLEND_64_W( Ep[N2], PIXEL); \
    Ep[N1] = Ep[N2]; \


#define LEFT_2_2X(N3, N2, PIXEL)\
    ALPHA_BLEND_192_W(Ep[N3], PIXEL); \
    ALPHA_BLEND_64_W( Ep[N2], PIXEL); \


#define UP_2_2X(N3, N1, PIXEL)\
    ALPHA_BLEND_192_W(Ep[N3], PIXEL); \
    ALPHA_BLEND_64_W( Ep[N1], PIXEL); \


#define DIA_2X(N3, PIXEL)\
    ALPHA_BLEND_128_W(Ep[N3], PIXEL); \


#define ALPHA_BLEND_X_W(dst, src, VAL) \
    dst = ( \
          (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask)) * VAL) >>5))) | \
          ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask)) * VAL) >>5))))

#define BIL2X_ODD(PF, PH, PI, N1, N2, N3) \
    ALPHA_BLEND_128_W(Ep[N1], PF); \
    ALPHA_BLEND_128_W(Ep[N2], PH); \
    Ep[N3] = Ep[N1]; \
    aux = PH; \
    ALPHA_BLEND_128_W(aux, PI); \
    ALPHA_BLEND_128_W(Ep[N3], aux); \

#define DDT2XBC_ODD(PF, PH, PI, N1, N2, N3) \
    ALPHA_BLEND_128_W(Ep[N1], PF); \
    ALPHA_BLEND_128_W(Ep[N2], PH); \
    ALPHA_BLEND_128_W(Ep[N3], PI); \

#define DDT2XD_ODD(PF, PH, N1, N2, N3) \
    ALPHA_BLEND_128_W(Ep[N1], PF); \
    ALPHA_BLEND_128_W(Ep[N2], PH); \
    Ep[N3] = PF; \
    ALPHA_BLEND_128_W(Ep[N3], PH); \

#define df(A, B)\
    abs(RGB565_to_Lum(A) - RGB565_to_Lum(B))\

#define eq(A, B)\
    (df(A, B) < 155)\


#define XBR(PE, PI, PH, PF, PG, PC, PD, PB, PA, N0, N1, N2, N3) \
    if ( PE!=PH && PE!=PF )\
    {\
        wd1 = df(PH,PF); \
        wd2 = df(PE,PI); \
        if ((wd1<<1)<wd2)\
        {\
            if ( !eq(PF,PB) && !eq(PF,PC) || !eq(PH,PD) && !eq(PH,PG) || eq(PE,PG) || eq(PE,PC) )\
                {\
                dFG=df(PF,PG); dHC=df(PH,PC); \
                irlv2u = (PE!=PC && PB!=PC); irlv2l = (PE!=PG && PD!=PG); px = (df(PE,PF) <= df(PE,PH)) ? PF : PH; \
                if ( irlv2l && irlv2u && ((dFG<<1)<=dHC) && (dFG>=(dHC<<1)) ) \
                {\
                    LEFT_UP_2_2X(N3, N2, N1, px)\
                }\
                else if ( irlv2l && ((dFG<<1)<=dHC) ) \
                {\
                    LEFT_2_2X(N3, N2, px);\
                }\
                else if ( irlv2u && (dFG>=(dHC<<1)) ) \
                {\
                    UP_2_2X(N3, N1, px);\
                }\
                else \
                {\
                    DIA_2X(N3, px);\
                }\
            }\
        }\
        else if (wd1<=wd2)\
        {\
            px = (df(PE,PF) <= df(PE,PH)) ? PF : PH;\
            ALPHA_BLEND_64_W( Ep[N3], px); \
        }\
    }\


#define XBRLV1(PE, PI, PH, PF, PG, PC, PD, PB, PA, N0, N1, N2, N3) \
    irlv1   = (PE!=PH && PE!=PF); \
    if ( irlv1 )\
    {\
        wd1 = df(PH,PF); \
        wd2 = df(PE,PI); \
        if (((wd1<<1)<wd2) && eq(PB,PD) && PB!=PF && PD!=PH)\
        {\
                px = (df(PE,PF) <= df(PE,PH)) ? PF : PH; \
                DIA_2X(N3, px);\
        }\
	else if (wd1<=wd2)\
        {\
            px = (df(PE,PF) <= df(PE,PH)) ? PF : PH;\
            ALPHA_BLEND_64_W( Ep[N3], px); \
        }\
    }\


#define DDT(PE, PI, PH, PF, N0, N1, N2, N3) \
        wd1 = (df(PH,PF)); \
        wd2 = (df(PE,PI)); \
	if (wd1>wd2)\
	{\
            DDT2XBC_ODD(PF, PH, PI, N1, N2, N3);\
	}\
	else if (wd1<wd2)\
	{\
            DDT2XD_ODD(PF, PH, N1, N2, N3);\
	}\
	else\
	{\
            BIL2X_ODD(PF, PH, PI, N1, N2, N3);\
        }

template<int GuiScale>
void Render2xBR (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

	uint32 wd1, wd2;
	uint32 irlv1, irlv2u, irlv2l;
	uint32 dFG, dHC;
	uint32 E0, E1, E2, E3;
	uint16 px;

	uint32 nextlineSrc = srcPitch / sizeof(uint16);
	uint16 *p  = (uint16 *)srcPtr;
	uint32 nextlineDst = dstPitch / sizeof(uint16);
	uint16 *Ep = (uint16 *)dstPtr;

	// Macro for pixel processing to maintain logic consistency across unrolled iterations
	#define PROCESS_XBR_PIXEL(idx) \
		E0 = (idx << 1); \
		E1 = (idx << 1) + 1; \
		E2 = (idx << 1) + nextlineDst; \
		E3 = (idx << 1) + nextlineDst + 1; \
		Ep[E0] = Ep[E1] = Ep[E2] = Ep[E3] = *(p + idx); \
		if ( (*(p + idx) != *(p + idx + 1) || *(p + idx) != *(p + idx - 1)) && \
		     (*(p + idx) != *(p + idx + nextlineSrc) || *(p + idx) != *(p + idx - nextlineSrc)) ) { \
			XBR(*(p + idx), *(p + idx + 1 + nextlineSrc), *(p + idx + nextlineSrc), *(p + idx + 1), *(p + idx - 1 + nextlineSrc), *(p + idx + 1 - nextlineSrc), *(p + idx - 1), *(p + idx - nextlineSrc), *(p + idx - 1 - nextlineSrc), E0, E1, E2, E3); \
			XBR(*(p + idx), *(p + idx + 1 - nextlineSrc), *(p + idx + 1), *(p + idx - nextlineSrc), *(p + idx + 1 + nextlineSrc), *(p + idx - 1 - nextlineSrc), *(p + idx + nextlineSrc), *(p + idx - 1), *(p + idx - 1 + nextlineSrc), E2, E0, E3, E1); \
			XBR(*(p + idx), *(p + idx - 1 - nextlineSrc), *(p + idx - nextlineSrc), *(p + idx - 1), *(p + idx + 1 - nextlineSrc), *(p + idx - 1 + nextlineSrc), *(p + idx + 1), *(p + idx + nextlineSrc), *(p + idx + 1 + nextlineSrc), E3, E2, E1, E0); \
			XBR(*(p + idx), *(p + idx - 1 + nextlineSrc), *(p + idx - 1), *(p + idx + nextlineSrc), *(p + idx - 1 - nextlineSrc), *(p + idx + 1 + nextlineSrc), *(p + idx - nextlineSrc), *(p + idx + 1), *(p + idx + 1 - nextlineSrc), E1, E3, E0, E2); \
		}

	while (height--) {
		int i = 0;
		// Unrolled loop: 8 pixels per chunk
		for (; i <= width - 8; i += 8) {
			DCBT(p + i + 16 - nextlineSrc);
			DCBT(p + i + 16);
			DCBT(p + i + 16 + nextlineSrc);

			PROCESS_XBR_PIXEL(i);
			PROCESS_XBR_PIXEL(i + 1);
			PROCESS_XBR_PIXEL(i + 2);
			PROCESS_XBR_PIXEL(i + 3);
			PROCESS_XBR_PIXEL(i + 4);
			PROCESS_XBR_PIXEL(i + 5);
			PROCESS_XBR_PIXEL(i + 6);
			PROCESS_XBR_PIXEL(i + 7);
		}
		// Remainder loop
		for (; i < width; i++) {
			PROCESS_XBR_PIXEL(i);
		}
		p += nextlineSrc;
		Ep += nextlineDst << 1;
	}
	#undef PROCESS_XBR_PIXEL
}

template<int GuiScale>
void Render2xBRlv1 (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

	uint32 wd1, wd2;
	uint32 irlv1;
	uint32 E0, E1, E2, E3;
	uint16 px;

	uint32 nextlineSrc = srcPitch / sizeof(uint16);
	uint16 *p  = (uint16 *)srcPtr;
	uint32 nextlineDst = dstPitch / sizeof(uint16);
	uint16 *Ep = (uint16 *)dstPtr;

	#define PROCESS_XBRLV1_PIXEL(idx) \
		E0 = (idx << 1); \
		E1 = (idx << 1) + 1; \
		E2 = (idx << 1) + nextlineDst; \
		E3 = (idx << 1) + nextlineDst + 1; \
		Ep[E0] = Ep[E1] = Ep[E2] = Ep[E3] = *(p + idx); \
		if ( (*(p + idx) != *(p + idx + 1) || *(p + idx) != *(p + idx - 1)) && \
		     (*(p + idx) != *(p + idx + nextlineSrc) || *(p + idx) != *(p + idx - nextlineSrc)) ) { \
			XBRLV1(*(p + idx), *(p + idx + 1 + nextlineSrc), *(p + idx + nextlineSrc), *(p + idx + 1), *(p + idx - 1 + nextlineSrc), *(p + idx + 1 - nextlineSrc), *(p + idx - 1), *(p + idx - nextlineSrc), *(p + idx - 1 - nextlineSrc), E0, E1, E2, E3); \
			XBRLV1(*(p + idx), *(p + idx + 1 - nextlineSrc), *(p + idx + 1), *(p + idx - nextlineSrc), *(p + idx + 1 + nextlineSrc), *(p + idx - 1 - nextlineSrc), *(p + idx + nextlineSrc), *(p + idx - 1), *(p + idx - 1 + nextlineSrc), E2, E0, E3, E1); \
			XBRLV1(*(p + idx), *(p + idx - 1 - nextlineSrc), *(p + idx - nextlineSrc), *(p + idx - 1), *(p + idx + 1 - nextlineSrc), *(p + idx - 1 + nextlineSrc), *(p + idx + 1), *(p + idx + nextlineSrc), *(p + idx + 1 + nextlineSrc), E3, E2, E1, E0); \
			XBRLV1(*(p + idx), *(p + idx - 1 + nextlineSrc), *(p + idx - 1), *(p + idx + nextlineSrc), *(p + idx - 1 - nextlineSrc), *(p + idx + 1 + nextlineSrc), *(p + idx - nextlineSrc), *(p + idx + 1), *(p + idx + 1 - nextlineSrc), E1, E3, E0, E2); \
		}

	while (height--) {
		int i = 0;
		for (; i <= width - 8; i += 8) {
			DCBT(p + i + 16 - nextlineSrc);
			DCBT(p + i + 16);
			DCBT(p + i + 16 + nextlineSrc);

			PROCESS_XBRLV1_PIXEL(i);
			PROCESS_XBRLV1_PIXEL(i + 1);
			PROCESS_XBRLV1_PIXEL(i + 2);
			PROCESS_XBRLV1_PIXEL(i + 3);
			PROCESS_XBRLV1_PIXEL(i + 4);
			PROCESS_XBRLV1_PIXEL(i + 5);
			PROCESS_XBRLV1_PIXEL(i + 6);
			PROCESS_XBRLV1_PIXEL(i + 7);
		}
		for (; i < width; i++) {
			PROCESS_XBRLV1_PIXEL(i);
		}
		p += nextlineSrc;
		Ep += nextlineDst << 1;
	}
	#undef PROCESS_XBRLV1_PIXEL
}

template<int GuiScale>
void RenderDDT (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

	uint32 wd1, wd2;
	uint32 E0, E1, E2, E3;
	uint16 aux;

	uint32 nextlineSrc = srcPitch / sizeof(uint16);
	uint16 *p  = (uint16 *)srcPtr;
	uint32 nextlineDst = dstPitch / sizeof(uint16);
	uint16 *Ep = (uint16 *)dstPtr;

	#define PROCESS_DDT_PIXEL(idx) \
		E0 = (idx << 1); \
		E1 = (idx << 1) + 1; \
		E2 = (idx << 1) + nextlineDst; \
		E3 = (idx << 1) + nextlineDst + 1; \
		Ep[E0] = Ep[E1] = Ep[E2] = Ep[E3] = *(p + idx); \
		if (*(p + idx) != *(p + idx + 1) || *(p + idx) != *(p + idx + nextlineSrc) || *(p + idx + 1) != *(p + idx + 1 + nextlineSrc) || *(p + idx + nextlineSrc) != *(p + idx + 1 + nextlineSrc)) { \
			DDT(*(p + idx), *(p + idx + 1 + nextlineSrc), *(p + idx + nextlineSrc), *(p + idx + 1), E0, E1, E2, E3); \
		}

	while (height--) {
		int i = 0;
		for (; i <= width - 8; i += 8) {
			DCBT(p + i + 16);
			DCBT(p + i + 16 + nextlineSrc);

			PROCESS_DDT_PIXEL(i);
			PROCESS_DDT_PIXEL(i + 1);
			PROCESS_DDT_PIXEL(i + 2);
			PROCESS_DDT_PIXEL(i + 3);
			PROCESS_DDT_PIXEL(i + 4);
			PROCESS_DDT_PIXEL(i + 5);
			PROCESS_DDT_PIXEL(i + 6);
			PROCESS_DDT_PIXEL(i + 7);
		}
		for (; i < width; i++) {
			PROCESS_DDT_PIXEL(i);
		}
		p += nextlineSrc;
		Ep += nextlineDst << 1;
	}
	#undef PROCESS_DDT_PIXEL
}
