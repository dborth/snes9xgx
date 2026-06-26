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

// -------------------------------------------------------------------------
// HQ2X Lookup Tables
// -------------------------------------------------------------------------

static int32_t y_r_lut[32], y_g_lut[64], y_b_lut[32];
static int32_t u_r_lut[32], u_g_lut[64], u_b_lut[32];
static int32_t v_r_lut[32], v_g_lut[64], v_b_lut[32];
static bool hq2x_initialized = false;

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

static void InitHQ2X() {
	if (hq2x_initialized) return;
	for (int i = 0; i < 32; i++) {
		int32_t r = i << 3; int32_t b = i << 3;
		y_r_lut[i] = 16829 * r; u_r_lut[i] = -9714 * r; v_r_lut[i] = 28784 * r;
		y_b_lut[i] =  6416 * b; u_b_lut[i] = 28784 * b; v_b_lut[i] = -4681 * b;
	}
	for (int i = 0; i < 64; i++) {
		int32_t g = i << 2;
		y_g_lut[i] = 33039 * g; u_g_lut[i] = -19071 * g; v_g_lut[i] = -24103 * g;
	}
	hq2x_initialized = true;
}

void SelectFilterMethod (int filterID)
{
	renderFilter = (RenderFilter)filterID;
	FilterMethod = FilterToMethod(renderFilter);

	if(renderFilter == FILTER_HQ2X || renderFilter == FILTER_HQ2XS || renderFilter == FILTER_HQ2XBOLD) {
		InitHQ2X();
	}
}

static bool isValidDimensions (int width, int height) {
	if (height > MAX_HEIGHT || width > MAX_WIDTH || height <= 0 || width <= 0)
		return false;
	return true;
}

// Fixed Point Inlined Replacements for RGB Lookup Tables
// Optimized Brightness using shifts instead of multiplication
static inline uint16_t inlineRGBtoBright(uint16_t c) {
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    return (r << 1) + r + (g << 1) + g + b;
}

// fast L1-cached LUT approach - avoids heavy multiplication stalls
static inline int inlineRGBtoYUV(uint16_t c) {
    uint32_t r_idx = c >> 11;
    uint32_t g_idx = (c >> 5) & 0x3F;
    uint32_t b_idx = c & 0x1F;

    int32_t y = (y_r_lut[r_idx] + y_g_lut[g_idx] + y_b_lut[b_idx] + 32768) >> 16;
    int32_t u = (u_r_lut[r_idx] + u_g_lut[g_idx] + u_b_lut[b_idx] + 32768) >> 16;
    int32_t v = (v_r_lut[r_idx] + v_g_lut[g_idx] + v_b_lut[b_idx] + 32768) >> 16;

    return (((y + 16) & 0xFF) << 16) | (((u + 128) & 0xFF) << 8) | ((v + 128) & 0xFF);
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

// -------------------------------------------------------------------------
// Highly Optimized C++ SWAR Interpolation (SIMD Within A Register)
// -------------------------------------------------------------------------

// Unpacks a 16-bit RGB565 pixel into a 32-bit integer.
// Moves Green to bits 21-26, leaves Red at 11-15, and Blue at 0-4.
// This creates empty "headroom" between the channels to safely absorb multiplication.
static inline uint32_t Unpack565(uint16_t p) {
    return ((uint32_t)p | ((uint32_t)p << 16)) & 0x07E0F81F;
}

// Packs the 32-bit SWAR integer back into a 16-bit RGB565 pixel.
// Because the interpolation weights always sum to exactly 1<<shift,
// the right-shift division perfectly zeroes out the expanded headroom bits.
// & 0x07E0 and & 0xF81F masks to truncate downward channel bleed
static inline uint16_t Pack565(uint32_t up) {
    return (uint16_t)((up >> 16) & 0x07E0) | (uint16_t)(up & 0xF81F);
}

// -------------------------------------------------------------------------
// Fast YUV Difference Evaluator (Short-Circuiting C++)
// Uses __builtin_abs for 3-cycle absolute diff.
// -------------------------------------------------------------------------
static inline bool Diff(uint32_t c1, uint32_t c2) {
    uint32_t dy = __builtin_abs((int)(c1 >> 16) - (int)(c2 >> 16));
    uint32_t du = __builtin_abs((int)((c1 >> 8) & 0xFF) - (int)((c2 >> 8) & 0xFF));
    uint32_t dv = __builtin_abs((int)(c1 & 0xFF) - (int)(c2 & 0xFF));

    // Bitwise OR forces straight-line evaluation, eliminating branch prediction penalties
    return (dy > 48) | (du > 7) | (dv > 6);
}

// -------------------------------------------------------------------------
// Arithmetic Multiplexer Weights
// Format: (Shift << 16) | (W_Center << 12) | (W_Corner << 8) | (W_Side1 << 4) | (W_Side2)
// -------------------------------------------------------------------------
#define W_0   0x00000 // c
#define W_10  0x23100 // (3c + crn) >> 2
#define W_11  0x23010 // (3c + s1) >> 2
#define W_12  0x23001 // (3c + s2) >> 2
#define W_20  0x22011 // (2c + s1 + s2) >> 2
#define W_21  0x22101 // (2c + crn + s2) >> 2
#define W_22  0x22110 // (2c + crn + s1) >> 2
#define W_60  0x35012 // (5c + s1 + 2s2) >> 3
#define W_61  0x35021 // (5c + 2s1 + s2) >> 3
#define W_70  0x36011 // (6c + s1 + s2) >> 3
#define W_90  0x32033 // (2c + 3s1 + 3s2) >> 3
#define W_100 0x4E011 // (14c + s1 + s2) >> 4  (0xE = 14)

#define PIXEL00_0    wt00 = W_0
#define PIXEL00_10   wt00 = W_10
#define PIXEL00_11   wt00 = W_11
#define PIXEL00_12   wt00 = W_12
#define PIXEL00_20   wt00 = W_20
#define PIXEL00_21   wt00 = W_21
#define PIXEL00_22   wt00 = W_22
#define PIXEL00_60   wt00 = W_60
#define PIXEL00_61   wt00 = W_61
#define PIXEL00_70   wt00 = W_70
#define PIXEL00_90   wt00 = W_90
#define PIXEL00_100  wt00 = W_100

#define PIXEL01_0    wt01 = W_0
#define PIXEL01_10   wt01 = W_10
#define PIXEL01_11   wt01 = W_11
#define PIXEL01_12   wt01 = W_12
#define PIXEL01_20   wt01 = W_20
#define PIXEL01_21   wt01 = W_21
#define PIXEL01_22   wt01 = W_22
#define PIXEL01_60   wt01 = W_60
#define PIXEL01_61   wt01 = W_61
#define PIXEL01_70   wt01 = W_70
#define PIXEL01_90   wt01 = W_90
#define PIXEL01_100  wt01 = W_100

#define PIXEL10_0    wt10 = W_0
#define PIXEL10_10   wt10 = W_10
#define PIXEL10_11   wt10 = W_11
#define PIXEL10_12   wt10 = W_12
#define PIXEL10_20   wt10 = W_20
#define PIXEL10_21   wt10 = W_21
#define PIXEL10_22   wt10 = W_22
#define PIXEL10_60   wt10 = W_60
#define PIXEL10_61   wt10 = W_61
#define PIXEL10_70   wt10 = W_70
#define PIXEL10_90   wt10 = W_90
#define PIXEL10_100  wt10 = W_100

#define PIXEL11_0    wt11 = W_0
#define PIXEL11_10   wt11 = W_10
#define PIXEL11_11   wt11 = W_11
#define PIXEL11_12   wt11 = W_12
#define PIXEL11_20   wt11 = W_20
#define PIXEL11_21   wt11 = W_21
#define PIXEL11_22   wt11 = W_22
#define PIXEL11_60   wt11 = W_60
#define PIXEL11_61   wt11 = W_61
#define PIXEL11_70   wt11 = W_70
#define PIXEL11_90   wt11 = W_90
#define PIXEL11_100  wt11 = W_100

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
case 18: case 50: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_20; PIXEL10_20; PIXEL11_21; break; \
case 80: case 81: PIXEL00_20; PIXEL01_22; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_20; break; \
case 72: case 76: PIXEL00_21; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_20; PIXEL11_22; break; \
case 10: case 138: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_20; PIXEL01_21; PIXEL10_22; PIXEL11_20; break; \
case 66: PIXEL00_22; PIXEL01_21; PIXEL10_21; PIXEL11_22; break; \
case 24: PIXEL00_21; PIXEL01_22; PIXEL10_22; PIXEL11_21; break; \
case 7: case 39: case 135: PIXEL00_11; PIXEL01_12; PIXEL10_20; PIXEL11_20; break; \
case 148: case 149: case 180: PIXEL00_20; PIXEL01_11; PIXEL10_20; PIXEL11_12; break; \
case 224: case 228: case 225: PIXEL00_20; PIXEL01_20; PIXEL10_12; PIXEL11_11; break; \
case 41: case 169: case 45: PIXEL00_12; PIXEL01_20; PIXEL10_11; PIXEL11_20; break; \
case 22: case 54: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_20; PIXEL11_21; break; \
case 208: case 209: PIXEL00_20; PIXEL01_22; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 104: case 108: PIXEL00_21; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 11: case 139: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_22; PIXEL11_20; break; \
case 19: case 51: if (Diff(y2, y6)) PIXEL00_11, PIXEL01_10; else PIXEL00_60, PIXEL01_90; PIXEL10_20; PIXEL11_21; break; \
case 146: case 178: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_10, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_20; break; \
case 84: case 85: PIXEL00_20; if (Diff(y6, y8)) PIXEL01_11, PIXEL11_10; else PIXEL01_60, PIXEL11_90; PIXEL10_21; break; \
case 112: case 113: PIXEL00_20; PIXEL01_22; if (Diff(y6, y8)) PIXEL10_12, PIXEL11_10; else PIXEL10_61, PIXEL11_90; break; \
case 200: case 204: PIXEL00_21; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_10, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 73: case 77: if (Diff(y8, y4)) PIXEL00_12, PIXEL10_10; else PIXEL00_61, PIXEL10_90; PIXEL01_20; PIXEL11_22; break; \
case 42: case 170: if (Diff(y4, y2)) PIXEL00_10, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_21; PIXEL11_20; break; \
case 14: case 142: if (Diff(y4, y2)) PIXEL00_10, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_22; PIXEL11_20; break; \
case 67: PIXEL00_11; PIXEL01_21; PIXEL10_21; PIXEL11_22; break; \
case 70: PIXEL00_22; PIXEL01_12; PIXEL10_21; PIXEL11_22; break; \
case 28: PIXEL00_21; PIXEL01_11; PIXEL10_22; PIXEL11_21; break; \
case 152: PIXEL00_21; PIXEL01_22; PIXEL10_22; PIXEL11_12; break; \
case 194: PIXEL00_22; PIXEL01_21; PIXEL10_21; PIXEL11_11; break; \
case 98: PIXEL00_22; PIXEL01_21; PIXEL10_12; PIXEL11_22; break; \
case 56: PIXEL00_21; PIXEL01_22; PIXEL10_11; PIXEL11_21; break; \
case 25: PIXEL00_12; PIXEL01_22; PIXEL10_22; PIXEL11_21; break; \
case 26: case 31: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_21; break; \
case 82: case 214: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 88: case 248: PIXEL00_21; PIXEL01_22; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 74: case 107: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_21; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 27: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_22; PIXEL11_21; break; \
case 86: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_21; PIXEL11_10; break; \
case 216: PIXEL00_21; PIXEL01_22; PIXEL10_10; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 106: PIXEL00_10; PIXEL01_21; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 30: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_21; break; \
case 210: PIXEL00_22; PIXEL01_10; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 120: PIXEL00_21; PIXEL01_22; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 75: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_10; PIXEL11_22; break; \
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
case 58: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_21; break; \
case 83: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 92: PIXEL00_21; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 202: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; PIXEL01_21; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 78: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; PIXEL11_22; break; \
case 154: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_22; PIXEL11_12; break; \
case 114: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 89: PIXEL00_12; PIXEL01_22; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 90: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 55: case 23: if (Diff(y2, y6)) PIXEL00_11, PIXEL01_0; else PIXEL00_60, PIXEL01_90; PIXEL10_20; PIXEL11_21; break; \
case 182: case 150: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_20; break; \
case 213: case 212: PIXEL00_20; if (Diff(y6, y8)) PIXEL01_11, PIXEL11_0; else PIXEL01_60, PIXEL11_90; PIXEL10_21; break; \
case 241: case 240: PIXEL00_20; PIXEL01_22; if (Diff(y6, y8)) PIXEL10_12, PIXEL11_0; else PIXEL10_61, PIXEL11_90; break; \
case 236: case 232: PIXEL00_21; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 109: case 105: if (Diff(y8, y4)) PIXEL00_12, PIXEL10_0; else PIXEL00_61, PIXEL10_90; PIXEL01_20; PIXEL11_22; break; \
case 171: case 43: if (Diff(y4, y2)) PIXEL00_0, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_21; PIXEL11_20; break; \
case 143: case 15: if (Diff(y4, y2)) PIXEL00_0, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_22; PIXEL11_20; break; \
case 124: PIXEL00_21; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 203: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_21; PIXEL10_10; PIXEL11_11; break; \
case 62: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_11; PIXEL11_21; break; \
case 211: PIXEL00_11; PIXEL01_10; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 118: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_12; PIXEL11_10; break; \
case 217: PIXEL00_12; PIXEL01_22; PIXEL10_10; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 110: PIXEL00_10; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 155: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_22; PIXEL11_12; break; \
case 188: PIXEL00_21; PIXEL01_11; PIXEL10_11; PIXEL11_12; break; \
case 185: PIXEL00_12; PIXEL01_22; PIXEL10_11; PIXEL11_12; break; \
case 61: PIXEL00_12; PIXEL01_11; PIXEL10_11; PIXEL11_21; break; \
case 157: PIXEL00_12; PIXEL01_11; PIXEL10_22; PIXEL11_12; break; \
case 103: PIXEL00_11; PIXEL01_12; PIXEL10_12; PIXEL11_22; break; \
case 227: PIXEL00_11; PIXEL01_21; PIXEL10_12; PIXEL11_11; break; \
case 230: PIXEL00_22; PIXEL01_12; PIXEL10_12; PIXEL11_11; break; \
case 199: PIXEL00_11; PIXEL01_12; PIXEL10_21; PIXEL11_11; break; \
case 220: PIXEL00_21; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 158: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_22; PIXEL11_12; break; \
case 234: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; PIXEL01_21; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_11; break; \
case 242: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 59: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_21; break; \
case 121: PIXEL00_12; PIXEL01_22; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 87: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 79: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; PIXEL11_22; break; \
case 122: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 94: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 218: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 91: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 229: PIXEL00_20; PIXEL01_20; PIXEL10_12; PIXEL11_11; break; \
case 167: PIXEL00_11; PIXEL01_12; PIXEL10_20; PIXEL11_20; break; \
case 173: PIXEL00_12; PIXEL01_20; PIXEL10_11; PIXEL11_20; break; \
case 181: PIXEL00_20; PIXEL01_11; PIXEL10_20; PIXEL11_12; break; \
case 186: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_11; PIXEL11_12; break; \
case 115: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 93: PIXEL00_12; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 206: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 205: case 201: PIXEL00_12; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_10; else PIXEL10_70; PIXEL11_11; break; \
case 174: case 46: if (Diff(y4, y2)) PIXEL00_10; else PIXEL00_70; PIXEL01_12; PIXEL10_11; PIXEL11_20; break; \
case 179: case 147: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_10; else PIXEL01_70; PIXEL10_20; PIXEL11_12; break; \
case 117: case 116: PIXEL00_20; PIXEL01_11; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_10; else PIXEL11_70; break; \
case 189: PIXEL00_12; PIXEL01_11; PIXEL10_11; PIXEL11_12; break; \
case 231: PIXEL00_11; PIXEL01_12; PIXEL10_12; PIXEL11_11; break; \
case 126: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 219: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_10; PIXEL10_10; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 125: if (Diff(y8, y4)) PIXEL00_12, PIXEL10_0; else PIXEL00_61, PIXEL10_90; PIXEL01_11; PIXEL11_10; break; \
case 221: PIXEL00_12; if (Diff(y6, y8)) PIXEL01_11, PIXEL11_0; else PIXEL01_60, PIXEL11_90; PIXEL10_10; break; \
case 207: if (Diff(y4, y2)) PIXEL00_0, PIXEL01_12; else PIXEL00_90, PIXEL01_61; PIXEL10_10; PIXEL11_11; break; \
case 238: PIXEL00_10; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_0, PIXEL11_11; else PIXEL10_90, PIXEL11_60; break; \
case 190: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0, PIXEL11_12; else PIXEL01_90, PIXEL11_61; PIXEL10_11; break; \
case 187: if (Diff(y4, y2)) PIXEL00_0, PIXEL10_11; else PIXEL00_90, PIXEL10_60; PIXEL01_10; PIXEL11_12; break; \
case 243: PIXEL00_11; PIXEL01_10; if (Diff(y6, y8)) PIXEL10_12, PIXEL11_0; else PIXEL10_61, PIXEL11_90; break; \
case 119: if (Diff(y2, y6)) PIXEL00_11, PIXEL01_0; else PIXEL00_60, PIXEL01_90; PIXEL10_12; PIXEL11_10; break; \
case 237: case 233: PIXEL00_12; PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 175: case 47: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; PIXEL01_12; PIXEL10_11; PIXEL11_20; break; \
case 183: case 151: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_20; PIXEL11_12; break; \
case 245: case 244: PIXEL00_20; PIXEL01_11; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 250: PIXEL00_10; PIXEL01_10; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 123: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_10; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 95: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_10; PIXEL11_10; break; \
case 222: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_10; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 252: PIXEL00_21; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 249: PIXEL00_12; PIXEL01_22; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 235: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_21; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 111: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_22; break; \
case 63: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_11; PIXEL11_21; break; \
case 159: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_22; PIXEL11_12; break; \
case 215: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_21; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 246: PIXEL00_22; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 254: PIXEL00_10; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 253: PIXEL00_12; PIXEL01_11; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 251: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; PIXEL01_10; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 239: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; PIXEL01_12; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; PIXEL11_11; break; \
case 127: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_20; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_20; PIXEL11_10; break; \
case 191: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_11; PIXEL11_12; break; \
case 223: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_20; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_10; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_20; break; \
case 247: PIXEL00_11; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; PIXEL10_12; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break; \
case 255: if (Diff(y4, y2)) PIXEL00_0; else PIXEL00_100; if (Diff(y2, y6)) PIXEL01_0; else PIXEL01_100; if (Diff(y8, y4)) PIXEL10_0; else PIXEL10_100; if (Diff(y6, y8)) PIXEL11_0; else PIXEL11_100; break;

// The single, centralized math engine. Executed 4 times per pixel. No branches.
static inline uint16_t UniversalBlend(uint16_t c, uint32_t u_c, uint32_t u_crn, uint32_t u_s1, uint32_t u_s2, uint32_t wt) {
    if (wt == W_0) return c; // Fast return for flat colors

    uint32_t shift = wt >> 16;
    uint32_t wc    = (wt >> 12) & 0xF;
    uint32_t wcrn  = (wt >> 8) & 0xF;
    uint32_t ws1   = (wt >> 4) & 0xF;
    uint32_t ws2   = wt & 0xF;

    return Pack565((u_c * wc + u_crn * wcrn + u_s1 * ws1 + u_s2 * ws2) >> shift);
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
	uint32_t u1, u2, u3, u4, u5, u6, u7, u8, u9;
	uint32_t pattern;
	uint32_t wt00, wt01, wt10, wt11;

	// ---------------------------------------------------------
	// FILTER: HQ2X (Standard YUV Differencing)
	// ---------------------------------------------------------
	if (GuiScale == FILTER_HQ2X)
	{
		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			u1 = Unpack565(w1); u4 = Unpack565(w4); u7 = Unpack565(w7);
			uint32_t y1 = inlineRGBtoYUV(w1);
			uint32_t y4 = inlineRGBtoYUV(w4);
			uint32_t y7 = inlineRGBtoYUV(w7);

			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
			u2 = Unpack565(w2); u5 = Unpack565(w5); u8 = Unpack565(w8);
			uint32_t y2 = inlineRGBtoYUV(w2);
			uint32_t y5 = inlineRGBtoYUV(w5);
			uint32_t y8 = inlineRGBtoYUV(w8);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint32_t y3 = inlineRGBtoYUV(w3);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y9 = inlineRGBtoYUV(w9);

				pattern = 0;
				if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
				if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
				if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
				if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
				if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
				if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
				if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
				if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);

				switch (pattern) { HQ2XCASES }
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
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
					u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
					uint32_t y3 = inlineRGBtoYUV(w3);
					uint32_t y6 = inlineRGBtoYUV(w6);
					uint32_t y9 = inlineRGBtoYUV(w9);

					pattern = 0;
					if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
					if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
					if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
					if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
					if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
					if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
					if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
					if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);

					switch (pattern) { HQ2XCASES }
					*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
					*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
					*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
					*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

					w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
					w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint32_t y3 = inlineRGBtoYUV(w3);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y9 = inlineRGBtoYUV(w9);

				pattern = 0;
				if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
				if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
				if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
				if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
				if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
				if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
				if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
				if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);

				switch (pattern) { HQ2XCASES }
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
				dp += 2;
			}
			dp += ((dst1line - width) << 1);
			sp +=  (src1line - width);
		}
	}
	// ---------------------------------------------------------
	// FILTER: HQ2X BOLD (Brightness Averaging with Sliding Window)
	// ---------------------------------------------------------
	else if (GuiScale == FILTER_HQ2XBOLD)
	{
		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			u1 = Unpack565(w1); u4 = Unpack565(w4); u7 = Unpack565(w7);
			uint16_t b1 = inlineRGBtoBright(w1);
			uint16_t b4 = inlineRGBtoBright(w4);
			uint16_t b7 = inlineRGBtoBright(w7);

			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
			u2 = Unpack565(w2); u5 = Unpack565(w5); u8 = Unpack565(w8);
			uint16_t b2 = inlineRGBtoBright(w2);
			uint16_t b5 = inlineRGBtoBright(w5);
			uint16_t b8 = inlineRGBtoBright(w8);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint16_t b3 = inlineRGBtoBright(w3);
				uint16_t b6 = inlineRGBtoBright(w6);
				uint16_t b9 = inlineRGBtoBright(w9);

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

				// Re-eval YUV solely for edge-rule verification inside the rule map
				uint32_t y2 = inlineRGBtoYUV(w2);
				uint32_t y4 = inlineRGBtoYUV(w4);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y8 = inlineRGBtoYUV(w8);

				switch (pattern) { HQ2XCASES }
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; b1 = b2; b4 = b5; b7 = b8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; b2 = b3; b5 = b6; b8 = b9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;
			bool bot_aligned = (((uint32_t)(dp + dst1line) & 0x1F) == 0);

			while (chunks--) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (dp) : "memory");
				if (bot_aligned) {
					__asm__ volatile ("dcbz 0, %0" :: "b" (dp + dst1line) : "memory");
				}
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				for(int i = 0; i < 8; i++) {
					sp++;
					w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
					u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
					uint16_t b3 = inlineRGBtoBright(w3);
					uint16_t b6 = inlineRGBtoBright(w6);
					uint16_t b9 = inlineRGBtoBright(w9);

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

					uint32_t y2 = inlineRGBtoYUV(w2);
					uint32_t y4 = inlineRGBtoYUV(w4);
					uint32_t y6 = inlineRGBtoYUV(w6);
					uint32_t y8 = inlineRGBtoYUV(w8);

					switch (pattern) { HQ2XCASES }
					*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
					*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
					*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
					*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

					w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; b1 = b2; b4 = b5; b7 = b8;
					w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; b2 = b3; b5 = b6; b8 = b9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint16_t b3 = inlineRGBtoBright(w3);
				uint16_t b6 = inlineRGBtoBright(w6);
				uint16_t b9 = inlineRGBtoBright(w9);

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

				uint32_t y2 = inlineRGBtoYUV(w2);
				uint32_t y4 = inlineRGBtoYUV(w4);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y8 = inlineRGBtoYUV(w8);

				switch (pattern) { HQ2XCASES }
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; b1 = b2; b4 = b5; b7 = b8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; b2 = b3; b5 = b6; b8 = b9;
				dp += 2;
			}
			dp += ((dst1line - width) << 1);
			sp +=  (src1line - width);
		}
	}
	// ---------------------------------------------------------
	// FILTER: HQ2X SOFT (Hybrid YUV / Brightness with Sliding Window)
	// ---------------------------------------------------------
	else if (GuiScale == FILTER_HQ2XS)
	{
		while (height--) {
			sp--;
			w1 = *(sp - src1line); w4 = *(sp); w7 = *(sp + src1line);
			u1 = Unpack565(w1); u4 = Unpack565(w4); u7 = Unpack565(w7);
			uint32_t y1 = inlineRGBtoYUV(w1);
			uint32_t y4 = inlineRGBtoYUV(w4);
			uint32_t y7 = inlineRGBtoYUV(w7);

			sp++;
			w2 = *(sp - src1line); w5 = *(sp); w8 = *(sp + src1line);
			u2 = Unpack565(w2); u5 = Unpack565(w5); u8 = Unpack565(w8);
			uint32_t y2 = inlineRGBtoYUV(w2);
			uint32_t y5 = inlineRGBtoYUV(w5);
			uint32_t y8 = inlineRGBtoYUV(w8);

			int w = width;

			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint32_t y3 = inlineRGBtoYUV(w3);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y9 = inlineRGBtoYUV(w9);

				pattern = 0;
				if(w1 == w5 || w3 == w5 || w7 == w5 || w9 == w5) {
					if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
					if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
					if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
					if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
					if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
					if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
					if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
					if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);
				} else {
					// Lazy Brightness: Only calculated when YUV is bypassed, saving 9 registers in the sliding window.
					uint16_t b1 = inlineRGBtoBright(w1); uint16_t b2 = inlineRGBtoBright(w2); uint16_t b3 = inlineRGBtoBright(w3);
					uint16_t b4 = inlineRGBtoBright(w4); uint16_t b5 = inlineRGBtoBright(w5); uint16_t b6 = inlineRGBtoBright(w6);
					uint16_t b7 = inlineRGBtoBright(w7); uint16_t b8 = inlineRGBtoBright(w8); uint16_t b9 = inlineRGBtoBright(w9);

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
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;
			bool bot_aligned = (((uint32_t)(dp + dst1line) & 0x1F) == 0);

			while (chunks--) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (dp) : "memory");
				if (bot_aligned) {
					__asm__ volatile ("dcbz 0, %0" :: "b" (dp + dst1line) : "memory");
				}
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				for(int i = 0; i < 8; i++) {
					sp++;
					w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
					u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
					uint32_t y3 = inlineRGBtoYUV(w3);
					uint32_t y6 = inlineRGBtoYUV(w6);
					uint32_t y9 = inlineRGBtoYUV(w9);

					pattern = 0;
					if(w1 == w5 || w3 == w5 || w7 == w5 || w9 == w5) {
						if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
						if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
						if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
						if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
						if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
						if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
						if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
						if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);
					} else {
						// Lazy Brightness
						uint16_t b1 = inlineRGBtoBright(w1); uint16_t b2 = inlineRGBtoBright(w2); uint16_t b3 = inlineRGBtoBright(w3);
						uint16_t b4 = inlineRGBtoBright(w4); uint16_t b5 = inlineRGBtoBright(w5); uint16_t b6 = inlineRGBtoBright(w6);
						uint16_t b7 = inlineRGBtoBright(w7); uint16_t b8 = inlineRGBtoBright(w8); uint16_t b9 = inlineRGBtoBright(w9);

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
					*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
					*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
					*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
					*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

					w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
					w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				w3 = *(sp - src1line); w6 = *(sp); w9 = *(sp + src1line);
				u3 = Unpack565(w3); u6 = Unpack565(w6); u9 = Unpack565(w9);
				uint32_t y3 = inlineRGBtoYUV(w3);
				uint32_t y6 = inlineRGBtoYUV(w6);
				uint32_t y9 = inlineRGBtoYUV(w9);

				pattern = 0;
				if(w1 == w5 || w3 == w5 || w7 == w5 || w9 == w5) {
					if (w1 != w5 && Diff(y1, y5)) pattern |= (1 << 0);
					if (w2 != w5 && Diff(y2, y5)) pattern |= (1 << 1);
					if (w3 != w5 && Diff(y3, y5)) pattern |= (1 << 2);
					if (w4 != w5 && Diff(y4, y5)) pattern |= (1 << 3);
					if (w6 != w5 && Diff(y6, y5)) pattern |= (1 << 4);
					if (w7 != w5 && Diff(y7, y5)) pattern |= (1 << 5);
					if (w8 != w5 && Diff(y8, y5)) pattern |= (1 << 6);
					if (w9 != w5 && Diff(y9, y5)) pattern |= (1 << 7);
				} else {
					// Lazy Brightness
					uint16_t b1 = inlineRGBtoBright(w1); uint16_t b2 = inlineRGBtoBright(w2); uint16_t b3 = inlineRGBtoBright(w3);
					uint16_t b4 = inlineRGBtoBright(w4); uint16_t b5 = inlineRGBtoBright(w5); uint16_t b6 = inlineRGBtoBright(w6);
					uint16_t b7 = inlineRGBtoBright(w7); uint16_t b8 = inlineRGBtoBright(w8); uint16_t b9 = inlineRGBtoBright(w9);

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
				*(dp)                = UniversalBlend(w5, u5, (w1==w5)?u5:u1, (w4==w5)?u5:u4, (w2==w5)?u5:u2, wt00);
				*(dp + 1)            = UniversalBlend(w5, u5, (w3==w5)?u5:u3, (w2==w5)?u5:u2, (w6==w5)?u5:u6, wt01);
				*(dp + dst1line)     = UniversalBlend(w5, u5, (w7==w5)?u5:u7, (w8==w5)?u5:u8, (w4==w5)?u5:u4, wt10);
				*(dp + dst1line + 1) = UniversalBlend(w5, u5, (w9==w5)?u5:u9, (w6==w5)?u5:u6, (w8==w5)?u5:u8, wt11);

				w1 = w2; w4 = w5; w7 = w8; u1 = u2; u4 = u5; u7 = u8; y1 = y2; y4 = y5; y7 = y8;
				w2 = w3; w5 = w6; w8 = w9; u2 = u3; u5 = u6; u8 = u9; y2 = y3; y5 = y6; y8 = y9;
				dp += 2;
			}
			dp += ((dst1line - width) << 1);
			sp +=  (src1line - width);
		}
	}
}

// -------------------------------------------------------------------------
// Optimized RenderScale2X (dcbz Chunking + Relative Pointer Sliding)
// -------------------------------------------------------------------------

#define PROCESS_SCALE2X_PIXEL() do { \
	uint32_t B = p_top[0]; \
	uint32_t F = p_mid[1]; \
	uint32_t H = p_bot[0]; \
	uint32_t E0 = (D == B && B != F && D != H) ? D : E; \
	uint32_t E1 = (B == F && B != D && F != H) ? F : E; \
	uint32_t E2 = (D == H && D != B && H != F) ? D : E; \
	uint32_t E3 = (H == F && D != H && B != F) ? F : E; \
	*(uint32_t*)&dp[0] = (E0 << 16) | E1; \
	*(uint32_t*)&dp[nextlineDst] = (E2 << 16) | E3; \
	D = E; E = F; \
	p_top++; p_mid++; p_bot++; dp += 2; \
} while(0)

template<int GuiScale>
void RenderScale2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	uint32_t nextlineSrc = srcPitch / sizeof(uint16_t);
	uint32_t nextlineDst = dstPitch / sizeof(uint16_t);

	uint16_t *src_base = (uint16_t *)srcPtr;
	uint16_t *dst_base = (uint16_t *)dstPtr;

	for (int y = 0; y < height; ++y) {
		uint16_t *p_mid = src_base;
		uint16_t *p_top = src_base - nextlineSrc;
		uint16_t *p_bot = src_base + nextlineSrc;
		uint16_t *dp = dst_base;

		uint32_t D = p_mid[-1];
		uint32_t E = p_mid[0];

		int w = width;

		// Phase 1: Unaligned leading pixels
		while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
			PROCESS_SCALE2X_PIXEL();
			w--;
		}

		// Phase 2: Cache-Aligned Chunking
		int chunks = w >> 3;
		int tail = w & 7;
		bool bot_aligned = (((uint32_t)(dp + nextlineDst) & 0x1F) == 0);

		while (chunks--) {
			__asm__ volatile ("dcbz 0, %0" :: "b" (dp) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (dp + nextlineDst) : "memory");
			}
			DCBT(p_top + 16); DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				PROCESS_SCALE2X_PIXEL();
			}
		}

		// Phase 3: Trailing pixels
		while (tail--) {
			PROCESS_SCALE2X_PIXEL();
		}

		src_base += nextlineSrc;
		dst_base += (nextlineDst << 1);
	}
}

//---------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------
// SWAR (SIMD Within A Register) Alpha Blending Macros
// -------------------------------------------------------------------------
// Replaces the old ExtractR/G/B inline assembly and masking methods.
// By utilizing the Unpack565/Pack565 technique, we compute all three
// color channels in parallel without overflowing boundaries.

#define ALPHA_BLEND_128_W(dst, src) \
	(dst) = Pack565((Unpack565(dst) + Unpack565(src)) >> 1)

// dst = (3*dst + src) / 4
#define ALPHA_BLEND_64_W(dst, src) \
	(dst) = Pack565((Unpack565(dst) * 3 + Unpack565(src)) >> 2)

// dst = (dst + 3*src) / 4
#define ALPHA_BLEND_192_W(dst, src) \
	(dst) = Pack565((Unpack565(dst) + Unpack565(src) * 3) >> 2)

// dst = (dst + 7*src) / 8
#define ALPHA_BLEND_224_W(dst, src) \
	(dst) = Pack565((Unpack565(dst) + Unpack565(src) * 7) >> 3)

// Custom blend weight (out of 32)
#define ALPHA_BLEND_X_W(dst, src, weight) \
	(dst) = Pack565((Unpack565(dst) * (32 - (weight)) + Unpack565(src) * (weight)) >> 5)

// -------------------------------------------------------------------------
// Scaling Kernel Matrix Routing Macros
// -------------------------------------------------------------------------

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
// Optimized RenderDDT (dcbz Chunking + Relative Pointer Sliding)
// -------------------------------------------------------------------------

#define PROCESS_DDT_PIXEL() do { \
	uint16_t F = p_mid[1]; \
	uint16_t I = p_bot[1]; \
	if (E != F || E != H || F != I || H != I) { \
		uint32_t lE = ddt_fast_luma(E); \
		uint32_t lH = ddt_fast_luma(H); \
		uint32_t lF = ddt_fast_luma(F); \
		uint32_t lI = ddt_fast_luma(I); \
		Ep[0] = E; Ep[1] = E; \
		Ep[nextlineDst] = E; Ep[nextlineDst + 1] = E; \
		uint32_t wd1 = __builtin_abs((int)lH - (int)lF); \
		uint32_t wd2 = __builtin_abs((int)lE - (int)lI); \
		uint16_t aux; \
		if (wd1 > wd2) { \
			DDT2XBC_ODD(F, H, I, 1, nextlineDst, nextlineDst + 1); \
		} else if (wd1 < wd2) { \
			DDT2XD_ODD(F, H, 1, nextlineDst, nextlineDst + 1); \
		} else { \
			BIL2X_ODD(F, H, I, 1, nextlineDst, nextlineDst + 1); \
		} \
	} else { \
		uint32_t E32 = ((uint32_t)E << 16) | E; \
		*(uint32_t*)&Ep[0] = E32; \
		*(uint32_t*)&Ep[nextlineDst] = E32; \
	} \
	E = F; H = I; \
	p_mid++; p_bot++; Ep += 2; \
} while(0)


template<int GuiScale>
void RenderDDT (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	uint32_t nextlineSrc = srcPitch / sizeof(uint16_t);
	uint32_t nextlineDst = dstPitch / sizeof(uint16_t);

	uint16_t *p_base  = (uint16_t *)srcPtr;
	uint16_t *Ep_base = (uint16_t *)dstPtr;

	for (int y = 0; y < height; ++y) {
		uint16_t *p_mid = p_base;
		uint16_t *p_bot = p_base + nextlineSrc;
		uint16_t *Ep    = Ep_base;

		uint16_t E = p_mid[0];
		uint16_t H = p_bot[0];

		int w = width;

		// Phase 1: Unaligned leading pixels
		while (((uint32_t)Ep & 0x1F) != 0 && w > 0) {
			PROCESS_DDT_PIXEL();
			w--;
		}

		// Phase 2: Cache-Aligned Chunking
		int chunks = w >> 3;
		int tail = w & 7;
		bool bot_aligned = (((uint32_t)(Ep + nextlineDst) & 0x1F) == 0);

		while (chunks--) {
			__asm__ volatile ("dcbz 0, %0" :: "b" (Ep) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (Ep + nextlineDst) : "memory");
			}
			DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				PROCESS_DDT_PIXEL();
			}
		}

		// Phase 3: Trailing pixels
		while (tail--) {
			PROCESS_DDT_PIXEL();
		}

		p_base  += nextlineSrc;
		Ep_base += (nextlineDst << 1);
	}
}
