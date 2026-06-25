/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2026
 *
 * videofilters.cpp
 * HQ2x, Scale2X, 2xBR, DDT filters
 * Original code from Michniewski, adapted from Snes9x Win32/MacOSX ports
****************************************************************************/
#include "videofilters.h"

#include <gccore.h>


#define MAX_HEIGHT 512
#define MAX_WIDTH 512

// Data Cache Block Touch for Gekko/Broadway (32-byte cache lines)
#define DCBT(ptr) __builtin_prefetch((void*)(ptr), 0, 0)

static RenderFilter renderFilter = FILTER_NONE;
TFilterMethod FilterMethod;

template<int GuiScale> void RenderHQ2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);
template<int GuiScale> void RenderScale2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);
template<int GuiScale> void Render2xBR (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);
template<int GuiScale> void Render2xBRlv1 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);
template<int GuiScale> void RenderDDT (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);

const char* GetFilterName (int filterID)
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

int GetFilterScale()
{
	switch(renderFilter)
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

void SelectFilterMethod (int filterID)
{
	renderFilter = (RenderFilter)filterID;
	FilterMethod = FilterToMethod(renderFilter);
}

static bool isValidDimensions (int width, int height) {
	if (height > MAX_HEIGHT || width > MAX_WIDTH || height <= 0 || width <= 0)
		return false;
	return true;
}

// Fixed Point Inlined Replacements for RGB Lookup Tables
static inline uint16_t inlineRGBtoBright(uint16_t c) {
	uint16_t b = (c & 0x001F) << 3;
	uint16_t g = (c & 0x07E0) >> 3;
	uint16_t r = (c & 0xF800) >> 8;
	return (r * 3) + (g * 3) + (b << 1);
}

static inline int inlineRGBtoYUV(uint16_t c) {
	int32_t b = (c & 0x001F) << 3;
	int32_t g = (c & 0x07E0) >> 3;
	int32_t r = (c & 0xF800) >> 8;

	// Fixed point arithmetic (multiplying floats by 65536, +32768 equals +0.5f rounding)
	int32_t y = ( 16829 * r + 33039 * g +  6416 * b + 32768) >> 16;
	int32_t u = (- 9714 * r - 19071 * g + 28784 * b + 32768) >> 16;
	int32_t v = ( 28784 * r - 24103 * g -  4681 * b + 32768) >> 16;

	y += 16;
	u += 128;
	v += 128;

	return ((y & 0xFF) << 16) | ((u & 0xFF) << 8) | (v & 0xFF);
}

// PowerPC rlwinm masks for 16-bit uints in 32-bit GPRs
static inline int RGB565_to_Lum(uint16_t c) {
	uint32_t r, g, b;
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

static inline uint16_t Interp01(uint16_t c1, uint16_t c2) {
	if (c1 == c2) return c1;
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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

static inline uint16_t Interp02(uint16_t c1, uint16_t c2, uint16_t c3) {
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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

static inline uint16_t Interp06(uint16_t c1, uint16_t c2, uint16_t c3) {
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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

static inline uint16_t Interp07(uint16_t c1, uint16_t c2, uint16_t c3) {
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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

static inline uint16_t Interp09(uint16_t c1, uint16_t c2, uint16_t c3) {
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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

static inline uint16_t Interp10(uint16_t c1, uint16_t c2, uint16_t c3) {
	uint32_t out, g1, rb1, g_tmp, rb_tmp;
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
static inline uint32_t BranchlessDiff(uint32_t c1, uint32_t c2) {
	uint32_t mask, tmp1, tmp2;
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
// Optimized HQ2X Render Loop
// -------------------------------------------------------------------------
template<int GuiScale>
void RenderHQ2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	uint32_t src1line = srcPitch >> 1;
	uint32_t dst1line = dstPitch >> 1;
	uint16_t *sp = (uint16_t *) srcPtr;
	uint16_t *dp = (uint16_t *) dstPtr;

	int w1, w2, w3, w4, w5, w6, w7, w8, w9;
	uint32_t pattern;

	// ---------------------------------------------------------
	// FILTER: HQ2X (Standard YUV Differencing)
	// ---------------------------------------------------------
	if (GuiScale == FILTER_HQ2X)
	{
		uint32_t y5;

		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				y5 = inlineRGBtoYUV(w5);
				pattern = 0;

				if (w1 != w5 && BranchlessDiff(inlineRGBtoYUV(w1), y5)) pattern |= (1 << 0);
				if (w2 != w5 && BranchlessDiff(inlineRGBtoYUV(w2), y5)) pattern |= (1 << 1);
				if (w3 != w5 && BranchlessDiff(inlineRGBtoYUV(w3), y5)) pattern |= (1 << 2);
				if (w4 != w5 && BranchlessDiff(inlineRGBtoYUV(w4), y5)) pattern |= (1 << 3);
				if (w6 != w5 && BranchlessDiff(inlineRGBtoYUV(w6), y5)) pattern |= (1 << 4);
				if (w7 != w5 && BranchlessDiff(inlineRGBtoYUV(w7), y5)) pattern |= (1 << 5);
				if (w8 != w5 && BranchlessDiff(inlineRGBtoYUV(w8), y5)) pattern |= (1 << 6);
				if (w9 != w5 && BranchlessDiff(inlineRGBtoYUV(w9), y5)) pattern |= (1 << 7);

				switch (pattern) { HQ2XCASES }

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;
			bool bot_aligned = (((uint32_t)(dp + dst1line) & 0x1F) == 0);

			while (chunks--) {
				// "b" constraint prevents r0 allocation. "memory" clobber prevents instruction reordering.
				__asm__ volatile ("dcbz 0, %0" :: "b" (dp) : "memory");
				if (bot_aligned) {
					__asm__ volatile ("dcbz 0, %0" :: "b" (dp + dst1line) : "memory");
				}
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				// Process 8 pixels. We avoid a macro here to prevent GCC from easily
				// unrolling the massive switch statement and thrashing the I-Cache.
				for(int i = 0; i < 8; i++) {
					sp++;
					w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
					y5 = inlineRGBtoYUV(w5);
					pattern = 0;

					if (w1 != w5 && BranchlessDiff(inlineRGBtoYUV(w1), y5)) pattern |= (1 << 0);
					if (w2 != w5 && BranchlessDiff(inlineRGBtoYUV(w2), y5)) pattern |= (1 << 1);
					if (w3 != w5 && BranchlessDiff(inlineRGBtoYUV(w3), y5)) pattern |= (1 << 2);
					if (w4 != w5 && BranchlessDiff(inlineRGBtoYUV(w4), y5)) pattern |= (1 << 3);
					if (w6 != w5 && BranchlessDiff(inlineRGBtoYUV(w6), y5)) pattern |= (1 << 4);
					if (w7 != w5 && BranchlessDiff(inlineRGBtoYUV(w7), y5)) pattern |= (1 << 5);
					if (w8 != w5 && BranchlessDiff(inlineRGBtoYUV(w8), y5)) pattern |= (1 << 6);
					if (w9 != w5 && BranchlessDiff(inlineRGBtoYUV(w9), y5)) pattern |= (1 << 7);

					switch (pattern) { HQ2XCASES }

					w1 = w2; w4 = w5; w7 = w8;
					w2 = w3; w5 = w6; w8 = w9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				y5 = inlineRGBtoYUV(w5);
				pattern = 0;

				if (w1 != w5 && BranchlessDiff(inlineRGBtoYUV(w1), y5)) pattern |= (1 << 0);
				if (w2 != w5 && BranchlessDiff(inlineRGBtoYUV(w2), y5)) pattern |= (1 << 1);
				if (w3 != w5 && BranchlessDiff(inlineRGBtoYUV(w3), y5)) pattern |= (1 << 2);
				if (w4 != w5 && BranchlessDiff(inlineRGBtoYUV(w4), y5)) pattern |= (1 << 3);
				if (w6 != w5 && BranchlessDiff(inlineRGBtoYUV(w6), y5)) pattern |= (1 << 4);
				if (w7 != w5 && BranchlessDiff(inlineRGBtoYUV(w7), y5)) pattern |= (1 << 5);
				if (w8 != w5 && BranchlessDiff(inlineRGBtoYUV(w8), y5)) pattern |= (1 << 6);
				if (w9 != w5 && BranchlessDiff(inlineRGBtoYUV(w9), y5)) pattern |= (1 << 7);

				switch (pattern) { HQ2XCASES }

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
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
		uint16_t b1, b2, b3, b4, b5, b6, b7, b8, b9;

		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			b1 = inlineRGBtoBright(w1); b4 = inlineRGBtoBright(w4); b7 = inlineRGBtoBright(w7);
			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
			b2 = inlineRGBtoBright(w2); b5 = inlineRGBtoBright(w5); b8 = inlineRGBtoBright(w8);

			int w = width;

			while (w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				b3 = inlineRGBtoBright(w3); b6 = inlineRGBtoBright(w6); b9 = inlineRGBtoBright(w9);

				uint16_t avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
				bool diff5 = (b5 > avg);
				pattern = 0;

				if (w1 != w5 && (b1 > avg) != diff5) pattern |= (1 << 0);
				if (w2 != w5 && (b2 > avg) != diff5) pattern |= (1 << 1);
				if (w3 != w5 && (b3 > avg) != diff5) pattern |= (1 << 2);
				if (w4 != w5 && (b4 > avg) != diff5) pattern |= (1 << 3);
				if (w6 != w5 && (b6 > avg) != diff5) pattern |= (1 << 4);
				if (w7 != w5 && (b7 > avg) != diff5) pattern |= (1 << 5);
				if (w8 != w5 && (b8 > avg) != diff5) pattern |= (1 << 6);
				if (w9 != w5 && (b9 > avg) != diff5) pattern |= (1 << 7);

				switch (pattern) { HQ2XCASES }

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				b1 = b2; b4 = b5; b7 = b8;
				b2 = b3; b5 = b6; b8 = b9;
				dp += 2; w--;
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
		uint16_t b1, b2, b3, b4, b5, b6, b7, b8, b9;
		uint32_t y5;

		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			b1 = inlineRGBtoBright(w1); b4 = inlineRGBtoBright(w4); b7 = inlineRGBtoBright(w7);
			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
			b2 = inlineRGBtoBright(w2); b5 = inlineRGBtoBright(w5); b8 = inlineRGBtoBright(w8);

			int w = width;

			while (w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				b3 = inlineRGBtoBright(w3); b6 = inlineRGBtoBright(w6); b9 = inlineRGBtoBright(w9);

				pattern = 0;
				if(w1 == w5 || w3 == w5 || w7 == w5 || w9 == w5) {
					y5 = inlineRGBtoYUV(w5);
					if (w1 != w5 && BranchlessDiff(inlineRGBtoYUV(w1), y5)) pattern |= (1 << 0);
					if (w2 != w5 && BranchlessDiff(inlineRGBtoYUV(w2), y5)) pattern |= (1 << 1);
					if (w3 != w5 && BranchlessDiff(inlineRGBtoYUV(w3), y5)) pattern |= (1 << 2);
					if (w4 != w5 && BranchlessDiff(inlineRGBtoYUV(w4), y5)) pattern |= (1 << 3);
					if (w6 != w5 && BranchlessDiff(inlineRGBtoYUV(w6), y5)) pattern |= (1 << 4);
					if (w7 != w5 && BranchlessDiff(inlineRGBtoYUV(w7), y5)) pattern |= (1 << 5);
					if (w8 != w5 && BranchlessDiff(inlineRGBtoYUV(w8), y5)) pattern |= (1 << 6);
					if (w9 != w5 && BranchlessDiff(inlineRGBtoYUV(w9), y5)) pattern |= (1 << 7);
				} else {
					uint16_t avg = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9) / 9;
					bool diff5 = (b5 > avg);
					if ((b1 > avg) != diff5) pattern |= (1 << 0);
					if ((b2 > avg) != diff5) pattern |= (1 << 1);
					if ((b3 > avg) != diff5) pattern |= (1 << 2);
					if ((b4 > avg) != diff5) pattern |= (1 << 3);
					if ((b6 > avg) != diff5) pattern |= (1 << 4);
					if ((b7 > avg) != diff5) pattern |= (1 << 5);
					if ((b8 > avg) != diff5) pattern |= (1 << 6);
					if ((b9 > avg) != diff5) pattern |= (1 << 7);
				}

				switch (pattern) { HQ2XCASES }

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				b1 = b2; b4 = b5; b7 = b8;
				b2 = b3; b5 = b6; b8 = b9;
				dp += 2; w--;
			}
			dp += ((dst1line - width) << 1);
			sp +=  (src1line - width);
		}
	}
}

// Fast Branchless Equality Mask for Gekko (PowerPC)
// Returns 0xFFFFFFFF if a == b, and 0x00000000 if a != b
// This replaces Gekko's missing 'isel' instruction using 4 fast integer ops.
static inline uint32_t branchless_eq_mask_s2x(uint32_t a, uint32_t b) {
	uint32_t mask;
	// If a == b, (a ^ b) is 0, cntlzw is 32. 32 >> 5 is 1. -1 is 0xFFFFFFFF.
	// If a != b, leading zeros are 16-31. >> 5 is 0. -0 is 0x00000000.
	__asm__ (
		"xor    %[mask], %[a], %[b] \n\t"
		"cntlzw %[mask], %[mask] \n\t"
		"srwi   %[mask], %[mask], 5 \n\t"
		"neg    %[mask], %[mask] \n\t"
		: [mask] "=r" (mask)
		: [a] "r" (a), [b] "r" (b)
	);
	return mask;
}

template<int GuiScale>
void RenderScale2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if (height <= 0 || width <= 0) return;

	// Convert pitches to array indices for 16-bit pointer math
	uint32_t nextlineSrc = srcPitch / sizeof(uint16_t);
	uint32_t nextlineDst = dstPitch / sizeof(uint16_t);

	uint16_t *src_base = (uint16_t *)srcPtr;
	uint16_t *dst_base = (uint16_t *)dstPtr;

	for (int y = 0; y < height; ++y) {
		uint16_t *p_mid = src_base;
		uint16_t *p_top = src_base - nextlineSrc;
		uint16_t *p_bot = src_base + nextlineSrc;

		// Output mapped as 32-bit to halve the number of store operations (mimics rlwimi/stw)
		uint32_t *dp_top = (uint32_t *)dst_base;
		uint32_t *dp_bot = (uint32_t *)(dst_base + nextlineDst);

		// Pre-load sliding window (D, E) matching your original setup
		uint32_t D = p_mid[-1];
		uint32_t E = p_mid[0];

		// Prime the L1 cache for the beginning of the row
		__builtin_prefetch(p_mid + 16, 0, 0);
		__builtin_prefetch(p_top + 16, 0, 0);
		__builtin_prefetch(p_bot + 16, 0, 0);

		for (int x = 0; x < width; ++x) {
			// Software prefetch ~32 bytes (1 cache line) ahead every 16 pixels
			if ((x & 15) == 0) {
				__builtin_prefetch(p_mid + x + 16, 0, 0);
				__builtin_prefetch(p_top + x + 16, 0, 0);
				__builtin_prefetch(p_bot + x + 16, 0, 0);
			}

			uint32_t B = p_top[x];
			uint32_t F = p_mid[x + 1];
			uint32_t H = p_bot[x];

			// Superscalar Equality Evaluation
			uint32_t M_DB = branchless_eq_mask_s2x(D, B);
			uint32_t M_BF = branchless_eq_mask_s2x(B, F);
			uint32_t M_DH = branchless_eq_mask_s2x(D, H);
			uint32_t M_HF = branchless_eq_mask_s2x(H, F);

			// Compound Logic Synthesis (GCC maps bitwise NOT `~` to the `andc` instruction naturally)
			uint32_t C0 = M_DB & ~M_BF & ~M_DH; // D==B && B!=F && D!=H
			uint32_t C1 = M_BF & ~M_DB & ~M_HF; // B==F && B!=D && F!=H
			uint32_t C2 = M_DH & ~M_DB & ~M_HF; // D==H && D!=B && H!=F
			uint32_t C3 = M_HF & ~M_DH & ~M_BF; // H==F && D!=H && B!=F

			// Pixel Resolution
			uint32_t E0 = (D & C0) | (E & ~C0); // Top-Left
			uint32_t E1 = (F & C1) | (E & ~C1); // Top-Right
			uint32_t E2 = (D & C2) | (E & ~C2); // Bottom-Left
			uint32_t E3 = (F & C3) | (E & ~C3); // Bottom-Right

			// Pack and store. Gekko is Big Endian, so shifting the left pixel up by 16
			// perfectly places the 16-bit colors sequentially in memory via a single 'stw'.
			dp_top[x] = (E0 << 16) | E1;
			dp_bot[x] = (E2 << 16) | E3;

			// Shift Sliding Window
			D = E;
			E = F;
		}

		src_base += nextlineSrc;
		dst_base += (nextlineDst << 1);
	}
}

//---------------------------------------------------------------------------------------------------------------------------

// Inline Assembly Helpers for RGB565 Extraction

static inline uint32_t ExtractR(uint16_t c) {
	uint32_t res;
	__asm__ volatile ("rlwinm %0, %1, 21, 27, 31" : "=r"(res) : "r"(c));
	return res;
}

static inline uint32_t ExtractG(uint16_t c) {
	uint32_t res;
	__asm__ volatile ("rlwinm %0, %1, 27, 26, 31" : "=r"(res) : "r"(c));
	return res;
}

static inline uint32_t ExtractB(uint16_t c) {
	uint32_t res;
	__asm__ volatile ("rlwinm %0, %1, 0, 27, 31" : "=r"(res) : "r"(c));
	return res;
}

#define RB_MASK565 0xF81F
#define R_MASK565  0xF800
#define G_MASK565  0x07E0
#define B_MASK565  0x001F
#define LB_MASK565 0xF7DE

static const uint16_t rb_mask = RB_MASK565;
static const uint16_t  g_mask =  G_MASK565;
static const uint16_t lb_mask = LB_MASK565;

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
		uint32_t sR = ExtractR(src); uint32_t sG = ExtractG(src); uint32_t sB = ExtractB(src); \
		uint32_t dR = ExtractR(dst); uint32_t dG = ExtractG(dst); uint32_t dB = ExtractB(dst); \
		uint32_t r = ((sR * weight) + (dR * (32 - weight))) >> 5; \
		uint32_t g = ((sG * weight) + (dG * (32 - weight))) >> 5; \
		uint32_t b = ((sB * weight) + (dB * (32 - weight))) >> 5; \
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
static inline uint16_t branchless_select(uint32_t a, uint32_t b, uint16_t x, uint16_t y) {
	uint32_t mask;
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
static inline uint32_t branchless_abs_diff(uint32_t a, uint32_t b) {
	uint32_t diff, mask;
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
// Optimized Render2xBR (Middle Ground: Lazy Luma + Memory-Safe DCBZ)
// -------------------------------------------------------------------------
template<int GuiScale>
void Render2xBR (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	uint32_t wd1, wd2, irlv1, irlv2u, irlv2l, dFG, dHC;
	uint16_t px;

	uint32_t nextlineSrc = srcPitch >> 1; // Convert pitch in bytes to pitch in uint16_t
	uint32_t nextlineDst = dstPitch >> 1;

	uint16_t *p_base = (uint16_t *)srcPtr;
	uint16_t *Ep_base = (uint16_t *)dstPtr;

	uint16_t A, B, C, D, E, F, G, H, I;

	while (height--) {
		// Set up pointers for the 3 rows. Offset by -1 because we pre-prime A, D, G.
		uint16_t *p_top = p_base - nextlineSrc;
		uint16_t *p_mid = p_base;
		uint16_t *p_bot = p_base + nextlineSrc;
		uint16_t *Ep = Ep_base;

		// Prime the left side of the 3x3 sliding window
		A = p_top[0]; B = p_top[1];
		D = p_mid[0]; E = p_mid[1];
		G = p_bot[0]; H = p_bot[1];

		// Advance pointers so index 2 always points to the next new column
		p_top++; p_mid++; p_bot++;

		int w = width;

		// Phase 1: Unaligned leading pixels
		while (((uint32_t)Ep & 0x1F) != 0 && w > 0) {
			C = p_top[1]; F = p_mid[1]; I = p_bot[1];

			uint32_t E32 = ((uint32_t)E << 16) | E;
			*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

			if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
				// LAZY EVALUATION: Only calculate Luma if an edge is detected
				uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
				uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
				uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

				XBR( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
				XBR( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
				XBR( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
				XBR( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
			}

			A=B; B=C; D=E; E=F; G=H; H=I;
			p_top++; p_mid++; p_bot++;
			Ep += 2; w--;
		}

		// Phase 2: Cache-Aligned Chunking
		int chunks = w >> 3;
		int tail = w & 7;
		bool bot_aligned = (((uint32_t)(Ep + nextlineDst) & 0x1F) == 0);

		while (chunks--) {
			// Memory clobbers prevent GCC from writing pixels before the cache block is zeroed
			__asm__ volatile ("dcbz 0, %0" :: "b" (Ep) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (Ep + nextlineDst) : "memory");
			}
			DCBT(p_top + 16); DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				C = p_top[1]; F = p_mid[1]; I = p_bot[1];

				uint32_t E32 = ((uint32_t)E << 16) | E;
				*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

				if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
					uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
					uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
					uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

					XBR( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
					XBR( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
					XBR( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
					XBR( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
				}

				A=B; B=C; D=E; E=F; G=H; H=I;
				p_top++; p_mid++; p_bot++;
				Ep += 2;
			}
		}

		// Phase 3: Trailing pixels
		while (tail--) {
			C = p_top[1]; F = p_mid[1]; I = p_bot[1];

			uint32_t E32 = ((uint32_t)E << 16) | E;
			*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

			if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
				uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
				uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
				uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

				XBR( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
				XBR( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
				XBR( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
				XBR( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
			}

			A=B; B=C; D=E; E=F; G=H; H=I;
			p_top++; p_mid++; p_bot++;
			Ep += 2;
		}

		p_base += nextlineSrc;
		Ep_base += nextlineDst << 1;
	}
}

// -------------------------------------------------------------------------
// Optimized Render2xBRlv1
// -------------------------------------------------------------------------
template<int GuiScale>
void Render2xBRlv1 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	uint32_t wd1, wd2, irlv1;
	uint16_t px;

	uint32_t nextlineSrc = srcPitch >> 1;
	uint32_t nextlineDst = dstPitch >> 1;

	uint16_t *p_base = (uint16_t *)srcPtr;
	uint16_t *Ep_base = (uint16_t *)dstPtr;

	uint16_t A, B, C, D, E, F, G, H, I;

	while (height--) {
		uint16_t *p_top = p_base - nextlineSrc;
		uint16_t *p_mid = p_base;
		uint16_t *p_bot = p_base + nextlineSrc;
		uint16_t *Ep = Ep_base;

		A = p_top[0]; B = p_top[1];
		D = p_mid[0]; E = p_mid[1];
		G = p_bot[0]; H = p_bot[1];

		p_top++; p_mid++; p_bot++;

		int w = width;

		while (((uint32_t)Ep & 0x1F) != 0 && w > 0) {
			C = p_top[1]; F = p_mid[1]; I = p_bot[1];

			uint32_t E32 = ((uint32_t)E << 16) | E;
			*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

			if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
				uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
				uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
				uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

				XBRLV1( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
				XBRLV1( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
				XBRLV1( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
				XBRLV1( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
			}

			A=B; B=C; D=E; E=F; G=H; H=I;
			p_top++; p_mid++; p_bot++;
			Ep += 2; w--;
		}

		int chunks = w >> 3;
		int tail = w & 7;
		bool bot_aligned = (((uint32_t)(Ep + nextlineDst) & 0x1F) == 0);

		while (chunks--) {
			__asm__ volatile ("dcbz 0, %0" :: "b" (Ep) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (Ep + nextlineDst) : "memory");
			}
			DCBT(p_top + 16); DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				C = p_top[1]; F = p_mid[1]; I = p_bot[1];

				uint32_t E32 = ((uint32_t)E << 16) | E;
				*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

				if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
					uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
					uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
					uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

					XBRLV1( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
					XBRLV1( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
					XBRLV1( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
					XBRLV1( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
				}

				A=B; B=C; D=E; E=F; G=H; H=I;
				p_top++; p_mid++; p_bot++;
				Ep += 2;
			}
		}

		while (tail--) {
			C = p_top[1]; F = p_mid[1]; I = p_bot[1];

			uint32_t E32 = ((uint32_t)E << 16) | E;
			*(uint32_t*)&Ep[0] = E32; *(uint32_t*)&Ep[nextlineDst] = E32;

			if ( (E!=F || E!=D) && (E!=H || E!=B) ) {
				uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B); uint32_t lC = RGB565_to_Lum(C);
				uint32_t lD = RGB565_to_Lum(D); uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
				uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H); uint32_t lI = RGB565_to_Lum(I);

				XBRLV1( E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, 0, 1, nextlineDst, nextlineDst + 1);
				XBRLV1( E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, nextlineDst, 0, nextlineDst + 1, 1);
				XBRLV1( E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, nextlineDst + 1, nextlineDst, 1, 0);
				XBRLV1( E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, 1, nextlineDst + 1, 0, nextlineDst);
			}

			A=B; B=C; D=E; E=F; G=H; H=I;
			p_top++; p_mid++; p_bot++;
			Ep += 2;
		}

		p_base += nextlineSrc;
		Ep_base += nextlineDst << 1;
	}
}

// -------------------------------------------------------------------------
// Micro-LUTs for fast Luma Calculation
// Total Memory Footprint: 512 Bytes (fits comfortably in L1 Cache)
// -------------------------------------------------------------------------

// 5-to-8 bit expansion pre-calculated with: 17 * R
static const uint32_t luma_r_lut[32] = {
    0, 136, 272, 425, 561, 697, 833, 986, 1122, 1258, 1394, 1530, 1683,
    1819, 1955, 2091, 2244, 2380, 2516, 2652, 2805, 2941, 3077, 3213, 3349,
    3502, 3638, 3774, 3910, 4063, 4199, 4335
};

// 6-to-8 bit expansion pre-calculated with: 28 * G
static const uint32_t luma_g_lut[64] = {
    0, 112, 224, 336, 448, 560, 672, 784, 896, 1008, 1120, 1260, 1372,
    1484, 1596, 1708, 1820, 1932, 2044, 2156, 2268, 2380, 2492, 2604, 2716,
    2828, 2940, 3052, 3164, 3276, 3388, 3500, 3640, 3752, 3864, 3976, 4088,
    4200, 4312, 4424, 4536, 4648, 4760, 4872, 4984, 5096, 5208, 5320, 5432,
    5544, 5656, 5768, 5880, 6020, 6132, 6244, 6356, 6468, 6580, 6692, 6804,
    6916, 7028, 7140
};

// 5-to-8 bit expansion pre-calculated with: 8 * B - floor(B / 2)
static const uint32_t luma_b_lut[32] = {
    0, 60, 120, 188, 248, 308, 368, 435, 495, 555, 615, 675, 743, 803,
    863, 923, 990, 1050, 1110, 1170, 1238, 1298, 1358, 1418, 1478, 1545,
    1605, 1665, 1725, 1793, 1853, 1913
};

// GCC translates bitwise masking logic into fused 1-cycle rlwinm instructions
static inline uint32_t ddt_fast_luma(uint16_t c) {
    return luma_r_lut[(c >> 11) & 0x1F] +
           luma_g_lut[(c >>  5) & 0x3F] +
           luma_b_lut[ c        & 0x1F];
}

// -------------------------------------------------------------------------
// Optimized RenderDDT
// -------------------------------------------------------------------------
template<int GuiScale>
void RenderDDT (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    // If Snes9x is rendering anything in HiRes, then just copy, don't interpolate
    if (height > 512 || width == 512 || height <= 0 || width <= 0)
    {
        return;
    }

    uint32_t nextlineSrc = srcPitch / sizeof(uint16_t);
    uint32_t nextlineDst = dstPitch / sizeof(uint16_t);

    uint16_t *p_base  = (uint16_t *)srcPtr;
    uint16_t *Ep_base = (uint16_t *)dstPtr;

    for (int y = 0; y < height; ++y) {
        uint16_t *p  = p_base;
        uint16_t *Ep = Ep_base;

        // Prime the L1 cache for the beginning of the row
        __builtin_prefetch(p + 16, 0, 0);
        __builtin_prefetch(p + nextlineSrc + 16, 0, 0);

        uint16_t E = p[0];
        uint16_t H = p[nextlineSrc];

        uint32_t lE = ddt_fast_luma(E);
        uint32_t lH = ddt_fast_luma(H);

        for (int i = 0; i < width; ++i) {
            uint16_t F = p[i + 1];
            uint16_t I = p[i + 1 + nextlineSrc];

            uint32_t lF = ddt_fast_luma(F);
            uint32_t lI = ddt_fast_luma(I);

            uint32_t E0 = (i << 1);
            uint32_t E1 = E0 + 1;
            uint32_t E2 = E0 + nextlineDst;
            uint32_t E3 = E2 + 1;

            Ep[E0] = E; Ep[E1] = E;
            Ep[E2] = E; Ep[E3] = E;

            if (E != F || E != H || F != I || H != I)
            {
                // Relying on __builtin_abs allows GCC to emit optimal branchless subfc/subfe chains
                uint32_t wd1 = __builtin_abs((int)lH - (int)lF);
                uint32_t wd2 = __builtin_abs((int)lE - (int)lI);
                uint16_t aux;

                if (wd1 > wd2)
                {
                    DDT2XBC_ODD(F, H, I, E1, E2, E3);
                }
                else if (wd1 < wd2)
                {
                    DDT2XD_ODD(F, H, E1, E2, E3);
                }
                else
                {
                    BIL2X_ODD(F, H, I, E1, E2, E3);
                }
            }

            E = F;
            H = I;
            lE = lF;
            lH = lI;
        }

        p_base  += nextlineSrc;
        Ep_base += (nextlineDst << 1);
    }
}
