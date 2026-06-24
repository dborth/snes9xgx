/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2026
 *
 * filter.cpp
 * HQ2x, Scale2X, 2xBR, DDT filters
 * Original code from Michniewski, adapted from Snes9x Win32/MacOSX ports
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

// Data Cache Block Touch for Gekko/Broadway (32-byte cache lines)
#define DCBT(ptr) __builtin_prefetch((void*)(ptr), 0, 0)

TFilterMethod FilterMethod;

template<int GuiScale> void RenderHQ2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
template<int GuiScale> void RenderScale2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
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
        case FILTER_SCANLINES: return "TV Mode";
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
        case FILTER_2XBR:       return Render2xBR<FILTER_2XBR>;
        case FILTER_2XBRLV1:    return Render2xBRlv1<FILTER_2XBRLV1>;
        case FILTER_DDT:        return RenderDDT<FILTER_DDT>;
        default: return 0;
    }
}

int GetFilterScale(RenderFilter filterID)
{
    switch(filterID)
    {
        case FILTER_NONE:
        case FILTER_SCANLINES:
        return 1;
        default:
        case FILTER_HQ2X:
        case FILTER_HQ2XS:
        case FILTER_HQ2XBOLD:
        case FILTER_SCALE2X:
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

// PowerPC rlwinm masks for 16-bit uints in 32-bit GPRs
static inline int RGB565_to_Lum(uint16 c) {
    uint32 r, g, b;
    __asm__ volatile ("rlwinm %0, %1, 21, 27, 31" : "=r"(r) : "r"(c)); // Red: shift right 11 -> rotate 21
    __asm__ volatile ("rlwinm %0, %1, 27, 26, 31" : "=r"(g) : "r"(c)); // Green: shift right 5 -> rotate 27
    __asm__ volatile ("rlwinm %0, %1, 0,  27, 31" : "=r"(b) : "r"(c)); // Blue: shift right 0 -> rotate 0
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

// -------------------------------------------------------------------------
// Optimized Gekko/Broadway Inline Assembly Interpolation
// -------------------------------------------------------------------------

static inline uint16 Interp01(uint16 c1, uint16 c2) {
    if (c1 == c2) return c1;
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"    // Extract Green (0x07E0)
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"   // Extract Red/Blue using wrap mask (0xFFFFF81F)
        "mulli   %[g1], %[g1], 3         \n\t"    // G1 * 3
        "mulli   %[rb1], %[rb1], 3       \n\t"    // RB1 * 3
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t" // Interleaved extract to hide mulli latency
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"    // G1*3 + G2
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"   // RB1*3 + RB2
        "rlwinm  %[out], %[g1], 30, 21, 26 \n\t"  // Shift right 2 (left 30) & mask G
        "rlwimi  %[out], %[rb1], 30, 27, 20 \n\t" // Shift right 2 & mask Insert RB into output
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2)
    );
    return out;
}

static inline uint16 Interp02(uint16 c1, uint16 c2, uint16 c3) {
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g1]     \n\t"    // c1 * 2
        "add     %[rb1], %[rb1], %[rb1]  \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"    // + c2
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[g_tmp], %[c3], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c3], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"    // + c3
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[out], %[g1], 30, 21, 26 \n\t"  // >> 2
        "rlwimi  %[out], %[rb1], 30, 27, 20 \n\t"
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2), [c3] "r" (c3)
    );
    return out;
}

static inline uint16 Interp06(uint16 c1, uint16 c2, uint16 c3) {
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"
        "mulli   %[g1], %[g1], 5         \n\t"
        "mulli   %[rb1], %[rb1], 5       \n\t"
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "add     %[g_tmp], %[g_tmp], %[g_tmp] \n\t" // c2 * 2
        "add     %[rb_tmp], %[rb_tmp], %[rb_tmp] \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[g_tmp], %[c3], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c3], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[out], %[g1], 29, 21, 26 \n\t"  // >> 3 (left 29)
        "rlwimi  %[out], %[rb1], 29, 27, 20 \n\t"
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2), [c3] "r" (c3)
    );
    return out;
}

static inline uint16 Interp07(uint16 c1, uint16 c2, uint16 c3) {
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"
        "mulli   %[g1], %[g1], 6         \n\t"
        "mulli   %[rb1], %[rb1], 6       \n\t"
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[g_tmp], %[c3], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c3], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[out], %[g1], 29, 21, 26 \n\t"  // >> 3
        "rlwimi  %[out], %[rb1], 29, 27, 20 \n\t"
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2), [c3] "r" (c3)
    );
    return out;
}

static inline uint16 Interp09(uint16 c1, uint16 c2, uint16 c3) {
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g1]     \n\t"    // c1 * 2
        "add     %[rb1], %[rb1], %[rb1]  \n\t"
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "mulli   %[g_tmp], %[g_tmp], 3   \n\t"    // c2 * 3
        "mulli   %[rb_tmp], %[rb_tmp], 3 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[g_tmp], %[c3], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c3], 0, 27, 20 \n\t"
        "mulli   %[g_tmp], %[g_tmp], 3   \n\t"    // c3 * 3
        "mulli   %[rb_tmp], %[rb_tmp], 3 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[out], %[g1], 29, 21, 26 \n\t"  // >> 3
        "rlwimi  %[out], %[rb1], 29, 27, 20 \n\t"
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2), [c3] "r" (c3)
    );
    return out;
}

static inline uint16 Interp10(uint16 c1, uint16 c2, uint16 c3) {
    uint32 out, g1, rb1, g_tmp, rb_tmp;
    __asm__ volatile (
        "rlwinm  %[g1], %[c1], 0, 21, 26 \n\t"
        "rlwinm  %[rb1], %[c1], 0, 27, 20 \n\t"
        "mulli   %[g1], %[g1], 14        \n\t"
        "mulli   %[rb1], %[rb1], 14      \n\t"
        "rlwinm  %[g_tmp], %[c2], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c2], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[g_tmp], %[c3], 0, 21, 26 \n\t"
        "rlwinm  %[rb_tmp], %[c3], 0, 27, 20 \n\t"
        "add     %[g1], %[g1], %[g_tmp]  \n\t"
        "add     %[rb1], %[rb1], %[rb_tmp]\n\t"
        "rlwinm  %[out], %[g1], 28, 21, 26 \n\t"  // >> 4 (left 28)
        "rlwimi  %[out], %[rb1], 28, 27, 20 \n\t"
        : [out] "=r" (out), [g1] "=&r" (g1), [rb1] "=&r" (rb1), [g_tmp] "=&r" (g_tmp), [rb_tmp] "=&r" (rb_tmp)
        : [c1] "r" (c1), [c2] "r" (c2), [c3] "r" (c3)
    );
    return out;
}

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

// -------------------------------------------------------------------------
// Branchless YUV Difference Evaluator (PowerPC Assembly)
// Returns 0xFFFFFFFF if the threshold is exceeded, or 0x00000000 if not.
// -------------------------------------------------------------------------
static inline uint32 BranchlessDiff(uint32 c1, uint32 c2) {
    uint32 mask, tmp1, tmp2;
    __asm__ volatile (
        // --- Y Component ---
        "andis. %[tmp1], %[c1], 0xFF00 \n\t"
        "andis. %[tmp2], %[c2], 0xFF00 \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t" // tmp1 = c1 - c2
        "srawi  %[tmp2], %[tmp1], 31 \n\t"      // tmp2 = sign mask (tmp1 >> 31)
        "xor    %[tmp1], %[tmp1], %[tmp2] \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t" // tmp1 = abs(c1 - c2)
        "lis    %[tmp2], 0x0030 \n\t"           // threshold trY = 0x300000
        "subfc  %[tmp1], %[tmp1], %[tmp2] \n\t" // trY - abs(Y)
        "subfe  %[mask], %[tmp1], %[tmp1] \n\t" // mask = -1 if borrow (CA=0)

        // --- U Component ---
        "rlwinm %[tmp1], %[c1], 0, 16, 23 \n\t" // Extract 0x00FF00
        "rlwinm %[tmp2], %[c2], 0, 16, 23 \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t"
        "srawi  %[tmp2], %[tmp1], 31 \n\t"
        "xor    %[tmp1], %[tmp1], %[tmp2] \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t"
        "li     %[tmp2], 0x0700 \n\t"           // threshold trU = 0x0700
        "subfc  %[tmp1], %[tmp1], %[tmp2] \n\t"
        "subfe  %[tmp2], %[tmp1], %[tmp1] \n\t"
        "or     %[mask], %[mask], %[tmp2] \n\t" // Accumulate mask

        // --- V Component ---
        "rlwinm %[tmp1], %[c1], 0, 24, 31 \n\t" // Extract 0x0000FF
        "rlwinm %[tmp2], %[c2], 0, 24, 31 \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t"
        "srawi  %[tmp2], %[tmp1], 31 \n\t"
        "xor    %[tmp1], %[tmp1], %[tmp2] \n\t"
        "subf   %[tmp1], %[tmp2], %[tmp1] \n\t"
        "li     %[tmp2], 0x0006 \n\t"           // threshold trV = 0x0006
        "subfc  %[tmp1], %[tmp1], %[tmp2] \n\t"
        "subfe  %[tmp2], %[tmp1], %[tmp1] \n\t"
        "or     %[mask], %[mask], %[tmp2] \n\t" // Accumulate mask

        : [mask] "=r" (mask), [tmp1] "=&r" (tmp1), [tmp2] "=&r" (tmp2)
        : [c1] "r" (c1), [c2] "r" (c2)
        : "cc"
    );
    return mask;
}

// -------------------------------------------------------------------------
// Fast Branchless Pattern Evaluation Macros
// -------------------------------------------------------------------------
#define EVAL_PATTERN_BIT(wX, w5, yX, y5, bit) \
    ( (-((int)(wX != w5))) & BranchlessDiff(yX, y5) & (1 << bit) )

#define EVAL_BOLD_BIT(wX, w5, bX, avg, diff5_mask, bit) \
    ( (-((int)(wX != w5))) & ( (-((int)(bX > avg))) ^ diff5_mask ) & (1 << bit) )

#define EVAL_XS_BIT(bX, avg, diff5_mask, bit) \
    ( ( (-((int)(bX > avg))) ^ diff5_mask ) & (1 << bit) )


// -------------------------------------------------------------------------
// Optimized HQ2X Render Loop (Unrolled x2 + Static LUT Jump Table)
// -------------------------------------------------------------------------
template<int GuiScale>
void RenderHQ2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
    // If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
    if (height > SNES_HEIGHT_EXTENDED || width == 512)
    {
        return;
    }

    uint32 src1line = srcPitch >> 1;
    uint32 dst1line = dstPitch >> 1;
    uint16 *sp = (uint16 *) srcPtr;
    uint16 *dp = (uint16 *) dstPtr;

    int w1, w2, w3, w4, w5, w6, w7, w8, w9;
    uint32 pattern;

    // ---------------------------------------------------------
    // FILTER: HQ2X (Standard YUV Differencing)
    // ---------------------------------------------------------
    if (GuiScale == FILTER_HQ2X)
    {
        uint32 y1, y2, y3, y4, y5, y6, y7, y8, y9;

        while (height--) {
            sp--;
            w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
            y1 = inlineRGBtoYUV(w1); y4 = inlineRGBtoYUV(w4); y7 = inlineRGBtoYUV(w7);

            sp++;
            w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
            y2 = inlineRGBtoYUV(w2); y5 = inlineRGBtoYUV(w5); y8 = inlineRGBtoYUV(w8);

            // Unrolled by 2
            for (int l = width >> 1; l; l--) {
                // --- PIXEL 1 ---
                sp++;
                DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);

                if (pattern > 255) __builtin_unreachable(); // Forces raw LUT generation
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                y1 = y2; y4 = y5; y7 = y8;
                y2 = y3; y5 = y6; y8 = y9;
                dp += 2;

                // --- PIXEL 2 ---
                sp++;
                // Notice: No DCBT here! Halves memory cache interface pressure.
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                y1 = y2; y4 = y5; y7 = y8;
                y2 = y3; y5 = y6; y8 = y9;
                dp += 2;
            }

            // Trailing pixel handler (If width is odd)
            if (width & 1) {
                sp++;
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }
                dp += 2;
            }

            dp += ((dst1line - width) << 1);
            sp +=  (src1line - width);
        }
    }
    // ---------------------------------------------------------
    // FILTER: HQ2X BOLD (Brightness Averaging)
    // ---------------------------------------------------------
    else if (GuiScale == FILTER_HQ2XBOLD)
    {
        uint16 b1, b2, b3, b4, b5, b6, b7, b8, b9;

        while (height--) {
            sp--;
            w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
            b1 = inlineRGBtoBright(w1); b4 = inlineRGBtoBright(w4); b7 = inlineRGBtoBright(w7);

            sp++;
            w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
            b2 = inlineRGBtoBright(w2); b5 = inlineRGBtoBright(w5); b8 = inlineRGBtoBright(w8);

            for (int l = width >> 1; l; l--) {
                // --- PIXEL 1 ---
                sp++;
                DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                b3 = inlineRGBtoBright(w3); b6 = inlineRGBtoBright(w6); b9 = inlineRGBtoBright(w9);

                uint16 avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                uint32 diff5_mask = -((int)(b5 > avg));

                pattern = 0;
                pattern |= EVAL_BOLD_BIT(w1, w5, b1, avg, diff5_mask, 0);
                pattern |= EVAL_BOLD_BIT(w2, w5, b2, avg, diff5_mask, 1);
                pattern |= EVAL_BOLD_BIT(w3, w5, b3, avg, diff5_mask, 2);
                pattern |= EVAL_BOLD_BIT(w4, w5, b4, avg, diff5_mask, 3);
                pattern |= EVAL_BOLD_BIT(w6, w5, b6, avg, diff5_mask, 4);
                pattern |= EVAL_BOLD_BIT(w7, w5, b7, avg, diff5_mask, 5);
                pattern |= EVAL_BOLD_BIT(w8, w5, b8, avg, diff5_mask, 6);
                pattern |= EVAL_BOLD_BIT(w9, w5, b9, avg, diff5_mask, 7);

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                b1 = b2; b4 = b5; b7 = b8;
                b2 = b3; b5 = b6; b8 = b9;
                dp += 2;

                // --- PIXEL 2 ---
                sp++;
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                b3 = inlineRGBtoBright(w3); b6 = inlineRGBtoBright(w6); b9 = inlineRGBtoBright(w9);

                avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                diff5_mask = -((int)(b5 > avg));

                pattern = 0;
                pattern |= EVAL_BOLD_BIT(w1, w5, b1, avg, diff5_mask, 0);
                pattern |= EVAL_BOLD_BIT(w2, w5, b2, avg, diff5_mask, 1);
                pattern |= EVAL_BOLD_BIT(w3, w5, b3, avg, diff5_mask, 2);
                pattern |= EVAL_BOLD_BIT(w4, w5, b4, avg, diff5_mask, 3);
                pattern |= EVAL_BOLD_BIT(w6, w5, b6, avg, diff5_mask, 4);
                pattern |= EVAL_BOLD_BIT(w7, w5, b7, avg, diff5_mask, 5);
                pattern |= EVAL_BOLD_BIT(w8, w5, b8, avg, diff5_mask, 6);
                pattern |= EVAL_BOLD_BIT(w9, w5, b9, avg, diff5_mask, 7);

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                b1 = b2; b4 = b5; b7 = b8;
                b2 = b3; b5 = b6; b8 = b9;
                dp += 2;
            }

            if (width & 1) {
                sp++;
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                b3 = inlineRGBtoBright(w3); b6 = inlineRGBtoBright(w6); b9 = inlineRGBtoBright(w9);

                uint16 avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                uint32 diff5_mask = -((int)(b5 > avg));

                pattern = 0;
                pattern |= EVAL_BOLD_BIT(w1, w5, b1, avg, diff5_mask, 0);
                pattern |= EVAL_BOLD_BIT(w2, w5, b2, avg, diff5_mask, 1);
                pattern |= EVAL_BOLD_BIT(w3, w5, b3, avg, diff5_mask, 2);
                pattern |= EVAL_BOLD_BIT(w4, w5, b4, avg, diff5_mask, 3);
                pattern |= EVAL_BOLD_BIT(w6, w5, b6, avg, diff5_mask, 4);
                pattern |= EVAL_BOLD_BIT(w7, w5, b7, avg, diff5_mask, 5);
                pattern |= EVAL_BOLD_BIT(w8, w5, b8, avg, diff5_mask, 6);
                pattern |= EVAL_BOLD_BIT(w9, w5, b9, avg, diff5_mask, 7);

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }
                dp += 2;
            }

            dp += ((dst1line - width) << 1);
            sp +=  (src1line - width);
        }
    }
    // ---------------------------------------------------------
    // FILTER: HQ2X SOFT (Hybrid YUV / Brightness)
    // ---------------------------------------------------------
    else if (GuiScale == FILTER_HQ2XS)
    {
        uint32 y1, y2, y3, y4, y5, y6, y7, y8, y9;

        while (height--) {
            sp--;
            w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
            y1 = inlineRGBtoYUV(w1); y4 = inlineRGBtoYUV(w4); y7 = inlineRGBtoYUV(w7);

            sp++;
            w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
            y2 = inlineRGBtoYUV(w2); y5 = inlineRGBtoYUV(w5); y8 = inlineRGBtoYUV(w8);

            for (int l = width >> 1; l; l--) {
                // --- PIXEL 1 ---
                sp++;
                DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                uint32 match = (w1 == w5) | (w3 == w5) | (w7 == w5) | (w9 == w5);

                if (!match) {
                    uint16 b1 = inlineRGBtoBright(w1); uint16 b2 = inlineRGBtoBright(w2); uint16 b3 = inlineRGBtoBright(w3);
                    uint16 b4 = inlineRGBtoBright(w4); uint16 b5 = inlineRGBtoBright(w5); uint16 b6 = inlineRGBtoBright(w6);
                    uint16 b7 = inlineRGBtoBright(w7); uint16 b8 = inlineRGBtoBright(w8); uint16 b9 = inlineRGBtoBright(w9);

                    uint16 avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                    uint32 diff5_mask = -((int)(b5 > avg));

                    pattern |= EVAL_XS_BIT(b1, avg, diff5_mask, 0);
                    pattern |= EVAL_XS_BIT(b2, avg, diff5_mask, 1);
                    pattern |= EVAL_XS_BIT(b3, avg, diff5_mask, 2);
                    pattern |= EVAL_XS_BIT(b4, avg, diff5_mask, 3);
                    pattern |= EVAL_XS_BIT(b6, avg, diff5_mask, 4);
                    pattern |= EVAL_XS_BIT(b7, avg, diff5_mask, 5);
                    pattern |= EVAL_XS_BIT(b8, avg, diff5_mask, 6);
                    pattern |= EVAL_XS_BIT(b9, avg, diff5_mask, 7);
                } else {
                    pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                    pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                    pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                    pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                    pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                    pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                    pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                    pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);
                }

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                y1 = y2; y4 = y5; y7 = y8;
                y2 = y3; y5 = y6; y8 = y9;
                dp += 2;

                // --- PIXEL 2 ---
                sp++;
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                match = (w1 == w5) | (w3 == w5) | (w7 == w5) | (w9 == w5);

                if (!match) {
                    uint16 b1 = inlineRGBtoBright(w1); uint16 b2 = inlineRGBtoBright(w2); uint16 b3 = inlineRGBtoBright(w3);
                    uint16 b4 = inlineRGBtoBright(w4); uint16 b5 = inlineRGBtoBright(w5); uint16 b6 = inlineRGBtoBright(w6);
                    uint16 b7 = inlineRGBtoBright(w7); uint16 b8 = inlineRGBtoBright(w8); uint16 b9 = inlineRGBtoBright(w9);

                    uint16 avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                    uint32 diff5_mask = -((int)(b5 > avg));

                    pattern |= EVAL_XS_BIT(b1, avg, diff5_mask, 0);
                    pattern |= EVAL_XS_BIT(b2, avg, diff5_mask, 1);
                    pattern |= EVAL_XS_BIT(b3, avg, diff5_mask, 2);
                    pattern |= EVAL_XS_BIT(b4, avg, diff5_mask, 3);
                    pattern |= EVAL_XS_BIT(b6, avg, diff5_mask, 4);
                    pattern |= EVAL_XS_BIT(b7, avg, diff5_mask, 5);
                    pattern |= EVAL_XS_BIT(b8, avg, diff5_mask, 6);
                    pattern |= EVAL_XS_BIT(b9, avg, diff5_mask, 7);
                } else {
                    pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                    pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                    pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                    pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                    pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                    pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                    pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                    pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);
                }

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }

                w1 = w2; w4 = w5; w7 = w8;
                w2 = w3; w5 = w6; w8 = w9;
                y1 = y2; y4 = y5; y7 = y8;
                y2 = y3; y5 = y6; y8 = y9;
                dp += 2;
            }

            if (width & 1) {
                sp++;
                w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
                y3 = inlineRGBtoYUV(w3); y6 = inlineRGBtoYUV(w6); y9 = inlineRGBtoYUV(w9);

                pattern = 0;
                uint32 match = (w1 == w5) | (w3 == w5) | (w7 == w5) | (w9 == w5);

                if (!match) {
                    uint16 b1 = inlineRGBtoBright(w1); uint16 b2 = inlineRGBtoBright(w2); uint16 b3 = inlineRGBtoBright(w3);
                    uint16 b4 = inlineRGBtoBright(w4); uint16 b5 = inlineRGBtoBright(w5); uint16 b6 = inlineRGBtoBright(w6);
                    uint16 b7 = inlineRGBtoBright(w7); uint16 b8 = inlineRGBtoBright(w8); uint16 b9 = inlineRGBtoBright(w9);

                    uint16 avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
                    uint32 diff5_mask = -((int)(b5 > avg));

                    pattern |= EVAL_XS_BIT(b1, avg, diff5_mask, 0);
                    pattern |= EVAL_XS_BIT(b2, avg, diff5_mask, 1);
                    pattern |= EVAL_XS_BIT(b3, avg, diff5_mask, 2);
                    pattern |= EVAL_XS_BIT(b4, avg, diff5_mask, 3);
                    pattern |= EVAL_XS_BIT(b6, avg, diff5_mask, 4);
                    pattern |= EVAL_XS_BIT(b7, avg, diff5_mask, 5);
                    pattern |= EVAL_XS_BIT(b8, avg, diff5_mask, 6);
                    pattern |= EVAL_XS_BIT(b9, avg, diff5_mask, 7);
                } else {
                    pattern |= EVAL_PATTERN_BIT(w1, w5, y1, y5, 0);
                    pattern |= EVAL_PATTERN_BIT(w2, w5, y2, y5, 1);
                    pattern |= EVAL_PATTERN_BIT(w3, w5, y3, y5, 2);
                    pattern |= EVAL_PATTERN_BIT(w4, w5, y4, y5, 3);
                    pattern |= EVAL_PATTERN_BIT(w6, w5, y6, y5, 4);
                    pattern |= EVAL_PATTERN_BIT(w7, w5, y7, y5, 5);
                    pattern |= EVAL_PATTERN_BIT(w8, w5, y8, y5, 6);
                    pattern |= EVAL_PATTERN_BIT(w9, w5, y9, y5, 7);
                }

                if (pattern > 255) __builtin_unreachable();
                switch (pattern) { HQ2XCASES }
                dp += 2;
            }

            dp += ((dst1line - width) << 1);
            sp +=  (src1line - width);
        }
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

//---------------------------------------------------------------------------------------------------------------------------

// Inline Assembly Helpers for RGB565 Extraction

static inline uint32 ExtractR(uint16 c) {
    uint32 res;
    __asm__ volatile ("rlwinm %0, %1, 21, 27, 31" : "=r"(res) : "r"(c));
    return res;
}

static inline uint32 ExtractG(uint16 c) {
    uint32 res;
    __asm__ volatile ("rlwinm %0, %1, 27, 26, 31" : "=r"(res) : "r"(c));
    return res;
}

static inline uint32 ExtractB(uint16 c) {
    uint32 res;
    __asm__ volatile ("rlwinm %0, %1, 0, 27, 31" : "=r"(res) : "r"(c));
    return res;
}

#define RB_MASK565 0xF81F
#define R_MASK565  0xF800
#define G_MASK565  0x07E0
#define B_MASK565  0x001F
#define LB_MASK565 0xF7DE

static const uint16 rb_mask = RB_MASK565;
static const uint16  g_mask =  G_MASK565;
static const uint16 lb_mask = LB_MASK565;

#define ALPHA_BLEND_128_W(dst, src) dst = ((src & lb_mask) >> 1) + ((dst & lb_mask) >> 1)
#define ALPHA_BLEND_64_W(dst, src) \
    dst = ( \
          (rb_mask & ((dst & rb_mask) + ((((src & rb_mask) - (dst & rb_mask))) >>2))) | \
          ( g_mask & ((dst &  g_mask) + ((((src &  g_mask) - (dst &  g_mask))) >>2))) )

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

// Fixed Extract macros applied properly
#define ALPHA_BLEND_X_W(dst, src, weight) \
    dst = ({ \
        uint32 sR = ExtractR(src); uint32 sG = ExtractG(src); uint32 sB = ExtractB(src); \
        uint32 dR = ExtractR(dst); uint32 dG = ExtractG(dst); uint32 dB = ExtractB(dst); \
        uint32 r = ((sR * weight) + (dR * (32 - weight))) >> 5; \
        uint32 g = ((sG * weight) + (dG * (32 - weight))) >> 5; \
        uint32 b = ((sB * weight) + (dB * (32 - weight))) >> 5; \
        ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F); \
    })

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

// -------------------------------------------------------------------------
// Branchless execution helpers for Gekko/Broadway pipeline optimization
// -------------------------------------------------------------------------
static inline uint16 branchless_select(uint32 a, uint32 b, uint16 x, uint16 y) {
    uint32 mask;
    // subfc calculates: b + ~a + 1 (which resolves to b - a)
    // If b >= a (i.e. a <= b), the subtraction sets the Carry flag (CA) to 1.
    // subfe calculates: mask + ~mask + CA = -1 + CA.
    // If CA = 1 (a <= b), mask = 0x00000000.
    // If CA = 0 (a > b), mask = 0xFFFFFFFF.
    __asm__ volatile (
        "subfc %0, %1, %2 \n\t"
        "subfe %0, %0, %0 \n\t"
        : "=r" (mask)
        : "r" (a), "r" (b)
        : "xer"
    );
    // Returns x when mask == 0, returns y when mask == 0xFFFFFFFF
    return x ^ ((x ^ y) & mask);
}
#define BRANCHLESS_SELECT(a, b, x, y) branchless_select((a), (b), (x), (y))

// -------------------------------------------------------------------------
// Branchless Absolute Difference (PowerPC subfc/subfe implementation)
// -------------------------------------------------------------------------
static inline uint32 branchless_abs_diff(uint32 a, uint32 b) {
    uint32 diff, mask;
    __asm__ volatile (
        "subfc %[diff], %[a], %[b] \n\t"       // diff = b - a (Sets CA=1 if b>=a, CA=0 if b<a)
        "subfe %[mask], %[diff], %[diff] \n\t" // mask = -1 + CA (0x0 if pos, 0xFFFFFFFF if neg)
        "xor   %[diff], %[diff], %[mask] \n\t" // Invert bits if negative
        "subf  %[diff], %[mask], %[diff] \n\t" // Add 1 if negative (completing two's complement absolute)
        : [diff] "=&r" (diff), [mask] "=&r" (mask)
        : [a] "r" (a), [b] "r" (b)
        : "cc" // Notifies the compiler that XER (carry flags) are altered
    );
    return diff;
}

#define df(A, B) branchless_abs_diff((A), (B))

#define eq(lA, lB)\
    (df(lA, lB) < 155)\

// Pass pre-computed Luma values directly to avoid redundant RGB565_to_Lum calls
#define XBR(PE, PI, PH, PF, PG, PC, PD, PB, PA, lPE, lPI, lPH, lPF, lPG, lPC, lPD, lPB, lPA, N0, N1, N2, N3) \
    if ( PE!=PH && PE!=PF )\
    {\
        wd1 = df(lPH,lPF); \
        wd2 = df(lPE,lPI); \
        if ((wd1<<1)<wd2)\
        {\
            if ( !eq(lPF,lPB) && !eq(lPF,lPC) || !eq(lPH,lPD) && !eq(lPH,lPG) || eq(lPE,lPG) || eq(lPE,lPC) )\
                {\
                dFG=df(lPF,lPG); dHC=df(lPH,lPC); \
                irlv2u = (PE!=PC && PB!=PC); irlv2l = (PE!=PG && PD!=PG); px = BRANCHLESS_SELECT(df(lPE,lPF), df(lPE,lPH), PF, PH); \
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
            px = BRANCHLESS_SELECT(df(lPE,lPF), df(lPE,lPH), PF, PH);\
            ALPHA_BLEND_64_W( Ep[N3], px); \
        }\
    }\

#define XBRLV1(PE, PI, PH, PF, PG, PC, PD, PB, PA, lPE, lPI, lPH, lPF, lPG, lPC, lPD, lPB, lPA, N0, N1, N2, N3) \
    irlv1   = (PE!=PH && PE!=PF); \
    if ( irlv1 )\
    {\
        wd1 = df(lPH,lPF); \
        wd2 = df(lPE,lPI); \
        if (((wd1<<1)<wd2) && eq(lPB,lPD) && PB!=PF && PD!=PH)\
        {\
                px = BRANCHLESS_SELECT(df(lPE,lPF), df(lPE,lPH), PF, PH); \
                DIA_2X(N3, px);\
        }\
	else if (wd1<=wd2)\
        {\
            px = BRANCHLESS_SELECT(df(lPE,lPF), df(lPE,lPH), PF, PH);\
            ALPHA_BLEND_64_W( Ep[N3], px); \
        }\
    }\

#define DDT(PE, PI, PH, PF, lPE, lPI, lPH, lPF, N0, N1, N2, N3) \
        wd1 = (df(lPH,lPF)); \
        wd2 = (df(lPE,lPI)); \
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

// -------------------------------------------------------------------------
// Optimized Render2xBR (Explicit Register Pinning & lhzu Window Sliding)
// -------------------------------------------------------------------------
template<int GuiScale>
void Render2xBR (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

    uint32 wd1, wd2, irlv1, irlv2u, irlv2l, dFG, dHC;
    uint16 px;

    uint32 nextlineSrc = srcPitch;
    uint32 nextlineDst = dstPitch >> 1;

    // Pin sliding window to non-volatile GPRs
    register uint32 A asm("r14"), B asm("r15"), C asm("r16");
    register uint32 D asm("r17"), E asm("r18"), F asm("r19");
    register uint32 G asm("r20"), H asm("r21"), I asm("r22");

    register uint32 lA asm("r23"), lB asm("r24"), lC asm("r25");
    register uint32 lD asm("r26"), lE asm("r27"), lF asm("r28");
    register uint32 lG asm("r29"), lH asm("r30"), lI asm("r31");

    uint8 *p_top_base = srcPtr - nextlineSrc;
    uint8 *p_mid_base = srcPtr;
    uint8 *p_bot_base = srcPtr + nextlineSrc;
    uint16 *Ep_base = (uint16 *)dstPtr;

    while (height--) {
        // Offset by -1 for lhzu pre-increment
        uint16 *p_top = (uint16 *)p_top_base - 1;
        uint16 *p_mid = (uint16 *)p_mid_base - 1;
        uint16 *p_bot = (uint16 *)p_bot_base - 1;
        uint16 *Ep = Ep_base;

        // Prime the left side of the window
        A = p_top[0]; B = p_top[1];
        D = p_mid[0]; E = p_mid[1];
        G = p_bot[0]; H = p_bot[1];

        lA = RGB565_to_Lum(A); lB = RGB565_to_Lum(B);
        lD = RGB565_to_Lum(D); lE = RGB565_to_Lum(E);
        lG = RGB565_to_Lum(G); lH = RGB565_to_Lum(H);

        // Advance pointers so lhzu fetches column index 2
        p_top++; p_mid++; p_bot++;

        int w = width;
        while (w--) {
            // Pre-fetch 16 bytes ahead
            DCBT(p_mid + 8);

            __asm__ volatile (
                "lhzu %[C], 2(%[ptop]) \n\t"
                "lhzu %[F], 2(%[pmid]) \n\t"
                "lhzu %[I], 2(%[pbot]) \n\t"
                : [C] "=r" (C), [F] "=r" (F), [I] "=r" (I),
                  [ptop] "+b" (p_top), [pmid] "+b" (p_mid), [pbot] "+b" (p_bot)
            );

            lC = RGB565_to_Lum(C);
            lF = RGB565_to_Lum(F);
            lI = RGB565_to_Lum(I);

            uint32 E32 = ((uint32)E << 16) | E;
            *(uint32*)&Ep[0] = E32;
            *(uint32*)&Ep[nextlineDst] = E32;

            if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
                XBR( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
                XBR( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
                XBR( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
                XBR( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
            }

            // Shift register window
            A=B; B=C;
            D=E; E=F;
            G=H; H=I;
            lA=lB; lB=lC;
            lD=lE; lE=lF;
            lG=lH; lH=lI;

            Ep += 2;
        }

        p_top_base += nextlineSrc;
        p_mid_base += nextlineSrc;
        p_bot_base += nextlineSrc;
        Ep_base += nextlineDst << 1;
    }
}

// -------------------------------------------------------------------------
// Optimized Render2xBRlv1
// -------------------------------------------------------------------------
template<int GuiScale>
void Render2xBRlv1 (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

    uint32 wd1, wd2, irlv1;
    uint16 px;

    uint32 nextlineSrc = srcPitch;
    uint32 nextlineDst = dstPitch >> 1;

    register uint32 A asm("r14"), B asm("r15"), C asm("r16");
    register uint32 D asm("r17"), E asm("r18"), F asm("r19");
    register uint32 G asm("r20"), H asm("r21"), I asm("r22");

    register uint32 lA asm("r23"), lB asm("r24"), lC asm("r25");
    register uint32 lD asm("r26"), lE asm("r27"), lF asm("r28");
    register uint32 lG asm("r29"), lH asm("r30"), lI asm("r31");

    uint8 *p_top_base = srcPtr - nextlineSrc;
    uint8 *p_mid_base = srcPtr;
    uint8 *p_bot_base = srcPtr + nextlineSrc;
    uint16 *Ep_base = (uint16 *)dstPtr;

    while (height--) {
        uint16 *p_top = (uint16 *)p_top_base - 1;
        uint16 *p_mid = (uint16 *)p_mid_base - 1;
        uint16 *p_bot = (uint16 *)p_bot_base - 1;
        uint16 *Ep = Ep_base;

        A = p_top[0]; B = p_top[1];
        D = p_mid[0]; E = p_mid[1];
        G = p_bot[0]; H = p_bot[1];

        lA = RGB565_to_Lum(A); lB = RGB565_to_Lum(B);
        lD = RGB565_to_Lum(D); lE = RGB565_to_Lum(E);
        lG = RGB565_to_Lum(G); lH = RGB565_to_Lum(H);

        p_top++; p_mid++; p_bot++;

        int w = width;
        while (w--) {
            DCBT(p_mid + 8);

            __asm__ volatile (
                "lhzu %[C], 2(%[ptop]) \n\t"
                "lhzu %[F], 2(%[pmid]) \n\t"
                "lhzu %[I], 2(%[pbot]) \n\t"
                : [C] "=r" (C), [F] "=r" (F), [I] "=r" (I),
                  [ptop] "+b" (p_top), [pmid] "+b" (p_mid), [pbot] "+b" (p_bot)
            );

            lC = RGB565_to_Lum(C);
            lF = RGB565_to_Lum(F);
            lI = RGB565_to_Lum(I);

            uint32 E32 = ((uint32)E << 16) | E;
            *(uint32*)&Ep[0] = E32;
            *(uint32*)&Ep[nextlineDst] = E32;

            if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
                XBRLV1( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
                XBRLV1( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
                XBRLV1( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
                XBRLV1( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
            }

            A=B; B=C;
            D=E; E=F;
            G=H; H=I;
            lA=lB; lB=lC;
            lD=lE; lE=lF;
            lG=lH; lH=lI;

            Ep += 2;
        }

        p_top_base += nextlineSrc;
        p_mid_base += nextlineSrc;
        p_bot_base += nextlineSrc;
        Ep_base += nextlineDst << 1;
    }
}

// -------------------------------------------------------------------------
// Optimized RenderDDT
// -------------------------------------------------------------------------
template<int GuiScale>
void RenderDDT (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	// If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
	if (height > SNES_HEIGHT_EXTENDED || width == 512)
	{
		return;
	}

    uint32 wd1, wd2;
    uint16 aux; // Required internally by the DDT/BIL2X macros

    uint32 nextlineSrc = srcPitch;
    uint32 nextlineDst = dstPitch >> 1;

    // Smaller sliding window for DDT (no top row)
    register uint32 E asm("r18"), F asm("r19");
    register uint32 H asm("r21"), I asm("r22");

    register uint32 lE asm("r27"), lF asm("r28");
    register uint32 lH asm("r30"), lI asm("r31");

    uint8 *p_mid_base = srcPtr;
    uint8 *p_bot_base = srcPtr + nextlineSrc;
    uint16 *Ep_base = (uint16 *)dstPtr;

    while (height--) {
        uint16 *p_mid = (uint16 *)p_mid_base - 1;
        uint16 *p_bot = (uint16 *)p_bot_base - 1;
        uint16 *Ep = Ep_base;

        // Prime the 2x2 forward window
        E = p_mid[1];
        H = p_bot[1];

        lE = RGB565_to_Lum(E);
        lH = RGB565_to_Lum(H);

        p_mid++; p_bot++;

        int w = width;
        while (w--) {
            DCBT(p_mid + 8);

            __asm__ volatile (
                "lhzu %[F], 2(%[pmid]) \n\t"
                "lhzu %[I], 2(%[pbot]) \n\t"
                : [F] "=r" (F), [I] "=r" (I),
                  [pmid] "+b" (p_mid), [pbot] "+b" (p_bot)
            );

            lF = RGB565_to_Lum(F);
            lI = RGB565_to_Lum(I);

            uint32 E32 = ((uint32)E << 16) | E;
            *(uint32*)&Ep[0] = E32;
            *(uint32*)&Ep[nextlineDst] = E32;

            if (E!=F || E!=H || F!=I || H!=I) {
                DDT( E, I, H, F, lE, lI, lH, lF, 0, 1, nextlineDst, nextlineDst + 1);
            }

            E=F; H=I;
            lE=lF; lH=lI;

            Ep += 2;
        }

        p_mid_base += nextlineSrc;
        p_bot_base += nextlineSrc;
        Ep_base += nextlineDst << 1;
    }
}
