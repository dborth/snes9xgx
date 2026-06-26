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

// YUV fixed-point multiplication LUTs.
// Total Size: 1.5 KB (Locks permanently into 32KB L1 Data Cache)
static int32_t y_r_lut[32] __attribute__((aligned(32)));
static int32_t y_g_lut[64] __attribute__((aligned(32)));
static int32_t y_b_lut[32] __attribute__((aligned(32)));
static int32_t u_r_lut[32] __attribute__((aligned(32)));
static int32_t u_g_lut[64] __attribute__((aligned(32)));
static int32_t u_b_lut[32] __attribute__((aligned(32)));
static int32_t v_r_lut[32] __attribute__((aligned(32)));
static int32_t v_g_lut[64] __attribute__((aligned(32)));
static int32_t v_b_lut[32] __attribute__((aligned(32)));

// Byuu's Symmetry Encoding Tables (512 Bytes Total)
static uint8_t rotateTable[256] __attribute__((aligned(32)));
static uint8_t hqTable[256] __attribute__((aligned(32)));
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
		y_r_lut[i] = 134632 * i; u_r_lut[i] = -77712 * i; v_r_lut[i] = 230272 * i;
		y_b_lut[i] = 51328 * i; u_b_lut[i] = 230272 * i; v_b_lut[i] = -37448 * i;
	}
	for (int i = 0; i < 64; i++) {
		y_g_lut[i] = 132156 * i; u_g_lut[i] = -76284 * i; v_g_lut[i] = -96412 * i;
	}

	const uint8_t base_hqTable[256] = {
		4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
		4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 12, 12, 5,  3,  1, 13,
		6, 6, 6,  2, 6, 6, 6,  2, 16, 14,15, 12, 16, 14,17, 13,
		2, 2, 2,  2, 2, 2, 2,  2, 12, 12,12, 12, 12, 12, 1, 13,
		4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
		4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 12, 12, 5,  3,  1, 13,
		6, 6, 6,  2, 6, 6, 6,  2, 16, 14,15, 12, 16, 14,17, 13,
		2, 2, 2,  2, 2, 2, 2,  2, 12, 12,12, 12, 12, 12, 1, 13,
		5, 5, 15, 12, 5, 5, 15, 12, 5, 5, 15, 12, 5, 5, 15, 12,
		3, 3, 12, 12, 3, 3, 12, 12, 3, 3, 12, 12, 3, 3, 12, 12,
		16, 16, 15, 12, 16, 16, 15, 12, 16, 16, 15, 12, 16, 16, 15, 12,
		14, 14, 12, 12, 14, 14, 12, 12, 14, 14, 12, 12, 14, 14, 12, 12,
		5, 5, 15, 12, 5, 5, 15, 12, 5, 5, 15, 12, 5, 5, 15, 12,
		3, 3, 12, 12, 3, 3, 12, 12, 3, 3, 12, 12, 3, 3, 12, 12,
		19, 19, 15, 12, 19, 19, 15, 12, 19, 19, 15, 12, 19, 19, 15, 12,
		13, 13, 12, 12, 13, 13, 12, 12, 13, 13, 12, 12, 13, 13, 12, 12
	};

	for(unsigned n = 0; n < 256; n++) {
		hqTable[n] = base_hqTable[n];
		rotateTable[n] = ((n >> 2) & 0x11) | ((n << 2) & 0x88) |
						 ((n & 0x01) << 5) | ((n & 0x08) << 3) |
						 ((n & 0x10) >> 3) | ((n & 0x80) >> 5);
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
static inline uint32_t inlineRGBtoYUV(uint16_t c) {
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

// Fast YUV Diff (For edge verification)
static inline bool FastDiff(uint16_t w1, uint16_t w2) {
    if (w1 == w2) return false;
    uint32_t y1 = inlineRGBtoYUV(w1);
    uint32_t y2 = inlineRGBtoYUV(w2);
    uint32_t dy = __builtin_abs((int)(y1 >> 16) - (int)(y2 >> 16));
    uint32_t du = __builtin_abs((int)((y1 >> 8) & 0xFF) - (int)((y2 >> 8) & 0xFF));
    uint32_t dv = __builtin_abs((int)(y1 & 0xFF) - (int)(y2 & 0xFF));
    return (dy > 48) | (du > 7) | (dv > 6);
}

// Optimized Diff that accepts pre-calculated Center YUV
static inline bool DiffYUV(uint16_t wX, uint32_t yC) {
    uint32_t yX = inlineRGBtoYUV(wX);
    uint32_t dy = __builtin_abs((int)(yX >> 16) - (int)(yC >> 16));
    uint32_t du = __builtin_abs((int)((yX >> 8) & 0xFF) - (int)((yC >> 8) & 0xFF));
    uint32_t dv = __builtin_abs((int)(yX & 0xFF) - (int)(yC & 0xFF));
    return (dy > 48) | (du > 7) | (dv > 6);
}

static inline uint16_t Blend(uint8_t op, uint16_t c, uint16_t crn, uint16_t s1, uint16_t s2) {
    if (op == 0) return c; // Fast path out

    uint32_t uc = Unpack565(c);
    switch(op) {
        case 1:  return Pack565(((uc << 1) + uc + Unpack565(crn)) >> 2);
        case 2:  return Pack565(((uc << 1) + uc + Unpack565(s2)) >> 2);
        case 3:  return Pack565(((uc << 1) + uc + Unpack565(s1)) >> 2);
        case 4:  return Pack565(((uc << 1) + Unpack565(s1) + Unpack565(s2)) >> 2);
        case 5:  return Pack565(((uc << 1) + Unpack565(crn) + Unpack565(s1)) >> 2);
        case 6:  return Pack565(((uc << 1) + Unpack565(crn) + Unpack565(s2)) >> 2);
        case 7:  return Pack565(((uc << 2) + uc + (Unpack565(s1) << 1) + Unpack565(s2)) >> 3);
        case 8:  return Pack565(((uc << 2) + uc + Unpack565(s1) + (Unpack565(s2) << 1)) >> 3);
        case 9:  return Pack565(((uc << 2) + (uc << 1) + Unpack565(s1) + Unpack565(s2)) >> 3);
        case 10: return Pack565(((uc << 1) + (Unpack565(s1) << 1) + Unpack565(s1) + (Unpack565(s2) << 1) + Unpack565(s2)) >> 3);
        case 11: return Pack565(((uc << 4) - (uc << 1) + Unpack565(s1) + Unpack565(s2)) >> 4);
        default: return c;
    }
}

// -------------------------------------------------------------------------
// The Rule Router (Restores True Lazy Evaluation)
// -------------------------------------------------------------------------
static inline uint16_t EvalSubpixel(uint8_t pat, uint16_t c, uint16_t crn, uint16_t s1, uint16_t s2, uint16_t opp1, uint16_t opp2) {
    uint8_t rule = hqTable[pat];
    uint8_t op = rule;

    // Dynamic Edge Detection (Rare, only executed when necessary)
    if (rule >= 12) {
        if (rule <= 17) {
            bool diff = FastDiff(s1, s2);
            if (rule == 12) op = diff ? 0 : 4;
            else if (rule == 13) op = diff ? 0 : 10;
            else if (rule == 14) op = diff ? 0 : 11;
            else if (rule == 15) op = diff ? 1 : 4;
            else if (rule == 16) op = diff ? 1 : 9;
            else if (rule == 17) op = diff ? 1 : 10;
        } else if (rule == 18) {
            bool diff = FastDiff(s1, opp1);
            op = diff ? 2 : 7;
        } else if (rule == 19) {
            bool diff = FastDiff(s2, opp2);
            op = diff ? 3 : 8;
        }
    }
    return Blend(op, c, crn, s1, s2);
}

// Universal Macro to apply the rotation matrix mapping
#define EVALUATE_HQ2X_SUBPIXELS() do { \
    uint8_t p = pattern; \
    *(dp)                = EvalSubpixel(p, w5, w1, w2, w4, w6, w8); p = rotateTable[p]; \
    *(dp + 1)            = EvalSubpixel(p, w5, w3, w6, w2, w8, w4); p = rotateTable[p]; \
    *(dp + dst1line + 1) = EvalSubpixel(p, w5, w9, w8, w6, w4, w2); p = rotateTable[p]; \
    *(dp + dst1line)     = EvalSubpixel(p, w5, w7, w4, w8, w2, w6); \
} while(0)

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

	uint32_t pattern;

	// ---------------------------------------------------------
	// FILTER: HQ2X (Standard YUV Differencing)
	// ---------------------------------------------------------
	if (GuiScale == FILTER_HQ2X)
	{
		while (height--) {
			sp--;
			uint16_t w1 = *(sp - src1line), w4 = *(sp), w7 = *(sp + src1line);
			sp++;
			uint16_t w2 = *(sp - src1line), w5 = *(sp), w8 = *(sp + src1line);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);
				uint32_t y5 = inlineRGBtoYUV(w5);

				// Fast predictability: && bypasses YUV lookup/math entirely if pixels match
				pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
				pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
				pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
				pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
				pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
				pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
				pattern |= ((w8 != w5) && DiffYUV(w8, y5)) << 6;
				pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;

			while (chunks--) {
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				// Process 8 pixels. We avoid a macro here to prevent GCC from easily
				// unrolling the massive switch statement and thrashing the I-Cache.
				for(int i = 0; i < 8; i++) {
					sp++;
					uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);
					uint32_t y5 = inlineRGBtoYUV(w5);

					pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
					pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
					pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
					pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
					pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
					pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
					pattern |= ((w8 != w5) && DiffYUV(w8, y5)) << 6;
					pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;

					EVALUATE_HQ2X_SUBPIXELS();

					w1 = w2; w4 = w5; w7 = w8;
					w2 = w3; w5 = w6; w8 = w9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);
				uint32_t y5 = inlineRGBtoYUV(w5);

				pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
				pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
				pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
				pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
				pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
				pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
				pattern |= ((w8 != w5) && Diff(w8, y5)) << 6;
				pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
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
			uint16_t w1 = *(sp - src1line), w4 = *(sp), w7 = *(sp + src1line);
			sp++;
			uint16_t w2 = *(sp - src1line), w5 = *(sp), w8 = *(sp + src1line);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

				uint16_t b5 = inlineRGBtoBright(w5);
				uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
								 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
								 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
				bool diff5 = (b5 > avg);

				pattern  = ((w1 != w5) && ((inlineRGBtoBright(w1) > avg) != diff5)) << 0;
				pattern |= ((w2 != w5) && ((inlineRGBtoBright(w2) > avg) != diff5)) << 1;
				pattern |= ((w3 != w5) && ((inlineRGBtoBright(w3) > avg) != diff5)) << 2;
				pattern |= ((w4 != w5) && ((inlineRGBtoBright(w4) > avg) != diff5)) << 3;
				pattern |= ((w6 != w5) && ((inlineRGBtoBright(w6) > avg) != diff5)) << 4;
				pattern |= ((w7 != w5) && ((inlineRGBtoBright(w7) > avg) != diff5)) << 5;
				pattern |= ((w8 != w5) && ((inlineRGBtoBright(w8) > avg) != diff5)) << 6;
				pattern |= ((w9 != w5) && ((inlineRGBtoBright(w9) > avg) != diff5)) << 7;

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;

			while (chunks--) {
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				for(int i = 0; i < 8; i++) {
					sp++;
					uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

					uint16_t b5 = inlineRGBtoBright(w5);
					uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
									 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
									 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
					bool diff5 = (b5 > avg);

					pattern  = ((w1 != w5) && ((inlineRGBtoBright(w1) > avg) != diff5)) << 0;
					pattern |= ((w2 != w5) && ((inlineRGBtoBright(w2) > avg) != diff5)) << 1;
					pattern |= ((w3 != w5) && ((inlineRGBtoBright(w3) > avg) != diff5)) << 2;
					pattern |= ((w4 != w5) && ((inlineRGBtoBright(w4) > avg) != diff5)) << 3;
					pattern |= ((w6 != w5) && ((inlineRGBtoBright(w6) > avg) != diff5)) << 4;
					pattern |= ((w7 != w5) && ((inlineRGBtoBright(w7) > avg) != diff5)) << 5;
					pattern |= ((w8 != w5) && ((inlineRGBtoBright(w8) > avg) != diff5)) << 6;
					pattern |= ((w9 != w5) && ((inlineRGBtoBright(w9) > avg) != diff5)) << 7;

					EVALUATE_HQ2X_SUBPIXELS();

					w1 = w2; w4 = w5; w7 = w8;
					w2 = w3; w5 = w6; w8 = w9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

				uint16_t b5 = inlineRGBtoBright(w5);
				uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
								 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
								 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
				bool diff5 = (b5 > avg);

				pattern  = ((w1 != w5) && ((inlineRGBtoBright(w1) > avg) != diff5)) << 0;
				pattern |= ((w2 != w5) && ((inlineRGBtoBright(w2) > avg) != diff5)) << 1;
				pattern |= ((w3 != w5) && ((inlineRGBtoBright(w3) > avg) != diff5)) << 2;
				pattern |= ((w4 != w5) && ((inlineRGBtoBright(w4) > avg) != diff5)) << 3;
				pattern |= ((w6 != w5) && ((inlineRGBtoBright(w6) > avg) != diff5)) << 4;
				pattern |= ((w7 != w5) && ((inlineRGBtoBright(w7) > avg) != diff5)) << 5;
				pattern |= ((w8 != w5) && ((inlineRGBtoBright(w8) > avg) != diff5)) << 6;
				pattern |= ((w9 != w5) && ((inlineRGBtoBright(w9) > avg) != diff5)) << 7;

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
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
			uint16_t w1 = *(sp - src1line), w4 = *(sp), w7 = *(sp + src1line);
			sp++;
			uint16_t w2 = *(sp - src1line), w5 = *(sp), w8 = *(sp + src1line);

			int w = width;

			// Phase 1: Unaligned leading pixels
			while (((uint32_t)dp & 0x1F) != 0 && w > 0) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

				bool use_yuv = (w1==w5) | (w3==w5) | (w7==w5) | (w9==w5);
				if(use_yuv) {
					uint32_t y5 = inlineRGBtoYUV(w5);
					pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
					pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
					pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
					pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
					pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
					pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
					pattern |= ((w8 != w5) && DiffYUV(w8, y5)) << 6;
					pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;
				} else {
					uint16_t b5 = inlineRGBtoBright(w5);
					uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
									 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
									 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
					bool diff5 = (b5 > avg);

					pattern  = ((inlineRGBtoBright(w1) > avg) != diff5) << 0;
					pattern |= ((inlineRGBtoBright(w2) > avg) != diff5) << 1;
					pattern |= ((inlineRGBtoBright(w3) > avg) != diff5) << 2;
					pattern |= ((inlineRGBtoBright(w4) > avg) != diff5) << 3;
					pattern |= ((inlineRGBtoBright(w6) > avg) != diff5) << 4;
					pattern |= ((inlineRGBtoBright(w7) > avg) != diff5) << 5;
					pattern |= ((inlineRGBtoBright(w8) > avg) != diff5) << 6;
					pattern |= ((inlineRGBtoBright(w9) > avg) != diff5) << 7;
				}

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
				dp += 2; w--;
			}

			// Phase 2: Cache-Aligned Chunking
			int chunks = w >> 3;
			int tail = w & 7;

			while (chunks--) {
				DCBT(sp + 16 - src1line); DCBT(sp + 16); DCBT(sp + 16 + src1line);

				for(int i = 0; i < 8; i++) {
					sp++;
					uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

					bool use_yuv = (w1==w5) | (w3==w5) | (w7==w5) | (w9==w5);
					if(use_yuv) {
						uint32_t y5 = inlineRGBtoYUV(w5);
						pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
						pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
						pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
						pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
						pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
						pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
						pattern |= ((w8 != w5) && DiffYUV(w8, y5)) << 6;
						pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;
					} else {
						uint16_t b5 = inlineRGBtoBright(w5);
						uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
										 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
										 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
						bool diff5 = (b5 > avg);

						pattern  = ((inlineRGBtoBright(w1) > avg) != diff5) << 0;
						pattern |= ((inlineRGBtoBright(w2) > avg) != diff5) << 1;
						pattern |= ((inlineRGBtoBright(w3) > avg) != diff5) << 2;
						pattern |= ((inlineRGBtoBright(w4) > avg) != diff5) << 3;
						pattern |= ((inlineRGBtoBright(w6) > avg) != diff5) << 4;
						pattern |= ((inlineRGBtoBright(w7) > avg) != diff5) << 5;
						pattern |= ((inlineRGBtoBright(w8) > avg) != diff5) << 6;
						pattern |= ((inlineRGBtoBright(w9) > avg) != diff5) << 7;
					}

					EVALUATE_HQ2X_SUBPIXELS();

					w1 = w2; w4 = w5; w7 = w8;
					w2 = w3; w5 = w6; w8 = w9;
					dp += 2;
				}
			}

			// Phase 3: Trailing pixels
			while (tail--) {
				sp++;
				uint16_t w3 = *(sp - src1line), w6 = *(sp), w9 = *(sp + src1line);

				bool use_yuv = (w1==w5) | (w3==w5) | (w7==w5) | (w9==w5);
				if(use_yuv) {
					uint32_t y5 = inlineRGBtoYUV(w5);
					pattern  = ((w1 != w5) && DiffYUV(w1, y5)) << 0;
					pattern |= ((w2 != w5) && DiffYUV(w2, y5)) << 1;
					pattern |= ((w3 != w5) && DiffYUV(w3, y5)) << 2;
					pattern |= ((w4 != w5) && DiffYUV(w4, y5)) << 3;
					pattern |= ((w6 != w5) && DiffYUV(w6, y5)) << 4;
					pattern |= ((w7 != w5) && DiffYUV(w7, y5)) << 5;
					pattern |= ((w8 != w5) && DiffYUV(w8, y5)) << 6;
					pattern |= ((w9 != w5) && DiffYUV(w9, y5)) << 7;
				} else {
					uint16_t b5 = inlineRGBtoBright(w5);
					uint16_t avg = ((inlineRGBtoBright(w1) + inlineRGBtoBright(w2) + inlineRGBtoBright(w3) +
									 inlineRGBtoBright(w4) + b5 + inlineRGBtoBright(w6) +
									 inlineRGBtoBright(w7) + inlineRGBtoBright(w8) + inlineRGBtoBright(w9)) * 7282) >> 16;
					bool diff5 = (b5 > avg);

					pattern  = ((inlineRGBtoBright(w1) > avg) != diff5) << 0;
					pattern |= ((inlineRGBtoBright(w2) > avg) != diff5) << 1;
					pattern |= ((inlineRGBtoBright(w3) > avg) != diff5) << 2;
					pattern |= ((inlineRGBtoBright(w4) > avg) != diff5) << 3;
					pattern |= ((inlineRGBtoBright(w6) > avg) != diff5) << 4;
					pattern |= ((inlineRGBtoBright(w7) > avg) != diff5) << 5;
					pattern |= ((inlineRGBtoBright(w8) > avg) != diff5) << 6;
					pattern |= ((inlineRGBtoBright(w9) > avg) != diff5) << 7;
				}

				EVALUATE_HQ2X_SUBPIXELS();

				w1 = w2; w4 = w5; w7 = w8;
				w2 = w3; w5 = w6; w8 = w9;
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
