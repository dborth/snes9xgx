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

// SNES filtered-framebuffer limits. Hi-res (512-wide) modes are NOT routed
// through these 2x filters, so 256 is the true maximum in both axes (the
// tall 256x239 PAL/interlace-deflicker case fits). Sizing to the real maximum
// keeps the signature ring buffers (sized by MAX_WIDTH) small in the 32KB L1 D-cache.
#define MAX_HEIGHT 256
#define MAX_WIDTH 256

// Data Cache Block Touch for Gekko/Broadway (32-byte cache lines)
#define DCBT(ptr) __builtin_prefetch((void*)(ptr), 0, 0)

static RenderFilter renderFilter = FILTER_NONE;
TFilterMethod FilterMethod;

// -------------------------------------------------------------------------
// Micro-LUTs for fast Luma Calculation
// Small memory footprint (fits comfortably in L1 Cache)
// -------------------------------------------------------------------------

// YUV fixed-point multiplication LUTs
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

// 2xBR Fast Luma LUTs
static uint32_t xbr_luma_r_lut[32] __attribute__((aligned(32)));
static uint32_t xbr_luma_g_lut[64] __attribute__((aligned(32)));
static uint32_t xbr_luma_b_lut[32] __attribute__((aligned(32)));


// DDT Fast Luma LUTs
// 5-to-8 bit expansion pre-calculated with: 17 * R
static const uint32_t ddt_luma_r_lut[32] = {
	0, 136, 272, 425, 561, 697, 833, 986, 1122, 1258, 1394, 1530, 1683,
	1819, 1955, 2091, 2244, 2380, 2516, 2652, 2805, 2941, 3077, 3213, 3349,
	3502, 3638, 3774, 3910, 4063, 4199, 4335
};

// 6-to-8 bit expansion pre-calculated with: 28 * G
static const uint32_t ddt_luma_g_lut[64] = {
	0, 112, 224, 336, 448, 560, 672, 784, 896, 1008, 1120, 1260, 1372,
	1484, 1596, 1708, 1820, 1932, 2044, 2156, 2268, 2380, 2492, 2604, 2716,
	2828, 2940, 3052, 3164, 3276, 3388, 3500, 3640, 3752, 3864, 3976, 4088,
	4200, 4312, 4424, 4536, 4648, 4760, 4872, 4984, 5096, 5208, 5320, 5432,
	5544, 5656, 5768, 5880, 6020, 6132, 6244, 6356, 6468, 6580, 6692, 6804,
	6916, 7028, 7140
};

// 5-to-8 bit expansion pre-calculated with: 8 * B - floor(B / 2)
static const uint32_t ddt_luma_b_lut[32] = {
	0, 60, 120, 188, 248, 308, 368, 435, 495, 555, 615, 675, 743, 803,
	863, 923, 990, 1050, 1110, 1170, 1238, 1298, 1358, 1418, 1478, 1545,
	1605, 1665, 1725, 1793, 1853, 1913
};

// Byuu's Symmetry Encoding Tables (512 Bytes Total)
static uint8_t rotateTable[256] __attribute__((aligned(32)));
static uint8_t hqTable[256] __attribute__((aligned(32)));

static bool tables_initialized = false;

// ------------------------------------------------------------------------------
// HQ2X PER-PIXEL SIGNATURE RING
// ------------------------------------------------------------------------------
// These are WORKING BUFFERS, not LUTs. These rings are sized by the source WIDTH and
// only the touched bytes (width+2 entries per row) ever enter the cache.
// The whole-capacity footprint is small and fully bounded:
//
// Per row entry: YUV = 4 bytes, brightness = 2 bytes (313 max => needs >8 bits)
// Footprint at the 256-wide maximum (width+2 = 258 entries/row):
//
// HQ2X : 3 YUV rows  = 3 * 258 * 4 = ~3.0 KB
// BOLD : 3 brt rows  = 3 * 258 * 2 = ~1.5 KB
// SOFT : YUV + brt   = ~4.5 KB (only variant needing both rings)
//
// Plus resident tables: 1.5 KB YUV LUTs + 512 B symmetry = ~2 KB.
// Worst case (SOFT) total ~6.5 KB of the 32 KB L1 D-cache (~20%), leaving
// ample room for the source pixel stream. (The destination is the uncached
// GX framebuffer and does NOT consume L1.)
//
// +2 per row = one guard column at each end so the seeded sliding window
// (which starts at column -1) reads defined values without per-pixel clamping
static uint32_t yuvRowA[MAX_WIDTH + 2] __attribute__((aligned(32)));
static uint32_t yuvRowB[MAX_WIDTH + 2] __attribute__((aligned(32)));
static uint32_t yuvRowC[MAX_WIDTH + 2] __attribute__((aligned(32)));
static uint16_t brtRowA[MAX_WIDTH + 2] __attribute__((aligned(32)));
static uint16_t brtRowB[MAX_WIDTH + 2] __attribute__((aligned(32)));
static uint16_t brtRowC[MAX_WIDTH + 2] __attribute__((aligned(32)));

// ------------------------------------------------------------------------------
// MACRO-BLOCK DEDUPLICATION STATE
// ------------------------------------------------------------------------------
// Padded to 34x34 to allow branchless dirty dilation (no bounds checking)
static bool render_mask[34][34] __attribute__((aligned(32)));
static bool invalidate_hashes = true; // Forces a full render on first pass or res change

// 128KB static buffer to hold the raw frame. Aligned to 32 bytes for cache.
static uint16_t previousEmuScreen[MAX_HEIGHT * MAX_WIDTH] __attribute__((aligned(32)));

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

static void InitFilterTables() {
	if (tables_initialized) return;

	for (int i = 0; i < 32; i++) {
		y_r_lut[i] = 134632 * i; u_r_lut[i] = -77712 * i; v_r_lut[i] = 230272 * i;
		y_b_lut[i] = 51328 * i; u_b_lut[i] = 230272 * i; v_b_lut[i] = -37448 * i;

		// Populate xBR Luma LUTs
		xbr_luma_r_lut[i] = i * 140;
		xbr_luma_b_lut[i] = i * 62;
	}
	for (int i = 0; i < 64; i++) {
		y_g_lut[i] = 132156 * i; u_g_lut[i] = -76284 * i; v_g_lut[i] = -96412 * i;
		xbr_luma_g_lut[i] = i * 113;
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
	tables_initialized = true;
}

void SelectFilterMethod (int filterID)
{
	renderFilter = (RenderFilter)filterID;
	FilterMethod = FilterToMethod(renderFilter);

	// Handle menu transition. Next frame will fully render and resync filtermem.
	invalidate_hashes = true;
	InitFilterTables();
}

static bool isValidDimensions (int width, int height) {
	if (height > MAX_HEIGHT || width > MAX_WIDTH || height <= 0 || width <= 0)
		return false;
	return true;
}

// Fixed Point Inlined Replacements for RGB Lookup Tables
// Optimized Brightness using shifts instead of multiplication
// 3R + 3G + B, range 0..186.
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
// Optimized with L1 Micro-LUTs to eliminate mullw cycle stalls
static inline int RGB565_to_Lum(uint16_t c) {
	uint32_t r, g, b;
	__asm__ volatile ("rlwinm %0, %1, 21, 27, 31" : "=r"(r) : "r"(c)); // Red: shift right 11 -> rotate 21
	__asm__ volatile ("rlwinm %0, %1, 27, 26, 31" : "=r"(g) : "r"(c)); // Green: shift right 5 -> rotate 27
	__asm__ volatile ("rlwinm %0, %1, 0,  27, 31" : "=r"(b) : "r"(c)); // Blue: shift right 0 -> rotate 0
	return xbr_luma_r_lut[r] + xbr_luma_g_lut[g] + xbr_luma_b_lut[b];
}

// YUV threshold compare on two ALREADY-CACHED packed YUV values
// Thresholds - 48/7/6
static inline bool DiffYUVCached(uint32_t a, uint32_t b) {
	if (a == b) return false;
	uint32_t dy = __builtin_abs((int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF));
	uint32_t du = __builtin_abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF));
	uint32_t dv = __builtin_abs((int)(a & 0xFF) - (int)(b & 0xFF));
	return (dy > 48) | (du > 7) | (dv > 6);
}

//
// HQ2X Filter Code:
//

// -------------------------------------------------------------------------
// DEDUPLICATION PRE-PASS: Combined Compare & Copy
// -------------------------------------------------------------------------
static inline void ComputeBlockDifferences(const uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
	static int prev_width = 0;
	static int prev_height = 0;

	// If the emulator changes resolution, clear the previous buffer
	if (width != prev_width || height != prev_height || invalidate_hashes) {
		__builtin_memset(previousEmuScreen, 0, sizeof(previousEmuScreen));
		invalidate_hashes = true;
		prev_width = width;
		prev_height = height;
	}

	int blocks_w = (width + 7) >> 3;
	int blocks_h = (height + 7) >> 3;

	// Clear mask. Padded to 34x34 to allow branchless assignments.
	__builtin_memset(render_mask, 0, sizeof(render_mask));

	bool force = invalidate_hashes;
	invalidate_hashes = false;

	bool dirty[34][34] = {false};
	const uint32_t prevPitch = MAX_WIDTH * 2; // Prev buffer is fixed width

	// Pass 1: Compare Blocks and Copy if Dirty
	for (int by = 0; by < blocks_h; by++) {
		const uint8_t* row_base = srcPtr + (by << 3) * srcPitch;
		uint8_t* prev_row_base  = (uint8_t*)previousEmuScreen + (by << 3) * prevPitch;

		for (int bx = 0; bx < blocks_w; bx++) {
			const uint32_t* block_ptr = (const uint32_t*)row_base;
			uint32_t* prev_ptr        = (uint32_t*)prev_row_base;
			bool is_dirty = force;

			// Exact memory comparison reading 16 bytes (four 32-bit ints) per row
			if (!is_dirty) {
				for (int y = 0; y < 8 && ((by << 3) + y) < height; y++) {
					if (block_ptr[0] != prev_ptr[0] || block_ptr[1] != prev_ptr[1] ||
					    block_ptr[2] != prev_ptr[2] || block_ptr[3] != prev_ptr[3]) {
						is_dirty = true;
						break;
					}
					block_ptr = (const uint32_t*)((const uint8_t*)block_ptr + srcPitch);
					prev_ptr  = (uint32_t*)((uint8_t*)prev_ptr + prevPitch);
				}
			}

			// If block changed, flag dirty AND update previous buffer immediately
			if (is_dirty) {
				dirty[by + 1][bx + 1] = true;

				block_ptr = (const uint32_t*)row_base;
				prev_ptr  = (uint32_t*)prev_row_base;

				for (int y = 0; y < 8 && ((by << 3) + y) < height; y++) {
					prev_ptr[0] = block_ptr[0];
					prev_ptr[1] = block_ptr[1];
					prev_ptr[2] = block_ptr[2];
					prev_ptr[3] = block_ptr[3];
					block_ptr = (const uint32_t*)((const uint8_t*)block_ptr + srcPitch);
					prev_ptr  = (uint32_t*)((uint8_t*)prev_ptr + prevPitch);
				}
			}

			row_base += 16;
			prev_row_base += 16;
		}
	}

	// Pass 2: Branchless Dirty Dilation
	// Unconditionally set the 3x3 footprint for any dirty block to solve HQ2X seams
	for (int by = 1; by <= blocks_h; by++) {
		for (int bx = 1; bx <= blocks_w; bx++) {
			if (dirty[by][bx]) {
				render_mask[by-1][bx-1] = true; render_mask[by-1][bx] = true; render_mask[by-1][bx+1] = true;
				render_mask[by  ][bx-1] = true; render_mask[by  ][bx] = true; render_mask[by  ][bx+1] = true;
				render_mask[by+1][bx-1] = true; render_mask[by+1][bx] = true; render_mask[by+1][bx+1] = true;
			}
		}
	}
}

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

// HQ2X blend operator, UNPACKED SWAR domain (single source of truth).
// uc/ucrn/us1/us2 are Unpack565() of the center / corner / side1 / side2 pixels.
// The caller unpacks each of the nine window pixels ONCE per 2x2 block and
// reuses them across all four subpixels, so no pixel is unpacked more than once.
static inline uint16_t BlendU(uint8_t op, uint32_t uc, uint32_t ucrn, uint32_t us1, uint32_t us2) {
	switch(op) {
		case 0:  return Pack565(uc);              // center unchanged
		case 1:  return Pack565(((uc << 1) + uc + ucrn) >> 2);
		case 2:  return Pack565(((uc << 1) + uc + us2) >> 2);
		case 3:  return Pack565(((uc << 1) + uc + us1) >> 2);
		case 4:  return Pack565(((uc << 1) + us1 + us2) >> 2);
		case 5:  return Pack565(((uc << 1) + ucrn + us1) >> 2);
		case 6:  return Pack565(((uc << 1) + ucrn + us2) >> 2);
		case 7:  return Pack565(((uc << 2) + uc + (us1 << 1) + us2) >> 3);
		case 8:  return Pack565(((uc << 2) + uc + us1 + (us2 << 1)) >> 3);
		case 9:  return Pack565(((uc << 2) + (uc << 1) + us1 + us2) >> 3);
		case 10: return Pack565(((uc << 1) + (us1 << 1) + us1 + (us2 << 1) + us2) >> 3);
		case 11: return Pack565(((uc << 4) - (uc << 1) + us1 + us2) >> 4);
		default: return Pack565(uc);
	}
}

// Resolve only the OP for a pattern+rule, given cached YUV signatures for the
// dynamic edge rules.
static inline uint8_t ResolveOp(uint8_t pat, uint32_t ds1, uint32_t ds2, uint32_t do1, uint32_t do2) {
	uint8_t rule = hqTable[pat];
	if (rule < 12) return rule;

	if (rule <= 17) {
		bool diff = DiffYUVCached(ds1, ds2);
		switch(rule) {
			case 12: return diff ? 0 : 4;
			case 13: return diff ? 0 : 10;
			case 14: return diff ? 0 : 11;
			case 15: return diff ? 1 : 4;
			case 16: return diff ? 1 : 9;
			default: return diff ? 1 : 10;  // rule 17
		}
	} else if (rule == 18) {
		return DiffYUVCached(ds1, do1) ? 2 : 7;
	} else { // rule == 19
		return DiffYUVCached(ds2, do2) ? 3 : 8;
	}
}

// -------------------------------------------------------------------------
// EMIT 2x2 BLOCK (Write Gather Pipe Optimized)
// -------------------------------------------------------------------------
// Passing the four signatures as macro args avoids any variable shadowing
// between the YUV-ring path and the BOLD on-demand path.
// On Big-Endian (PowerPC), a 32-bit store maps perfectly to two contiguous
// 16-bit pixel writes, halving the write transactions across the bus.
#define EVALUATE_HQ2X_SUBPIXELS(sig2, sig4, sig6, sig8) do { \
	uint8_t p = pattern; \
	uint16_t p0 = BlendU(ResolveOp(p, sig2, sig4, sig6, sig8), U5, U1, U2, U4); p = rotateTable[p]; \
	uint16_t p1 = BlendU(ResolveOp(p, sig6, sig2, sig8, sig4), U5, U3, U6, U2); p = rotateTable[p]; \
	uint16_t p3 = BlendU(ResolveOp(p, sig8, sig6, sig4, sig2), U5, U9, U8, U6); p = rotateTable[p]; \
	uint16_t p2 = BlendU(ResolveOp(p, sig4, sig8, sig2, sig6), U5, U7, U4, U8); \
	*(uint32_t*)(dp) = (p0 << 16) | p1; \
	*(uint32_t*)(dp + dst1line) = (p2 << 16) | p3; \
} while(0)

// SIGNATURE ROW BUILDERS
//
// Convert one full source scanline into cached signatures, ONCE. Guard cells at
// index 0 and width+1 replicate the edge columns so the seeded sliding window
// (which starts at x=-1) reads defined values without per-pixel clamping.

// DEDUP - Returns true iff two source rows are bit-identical (early-out).
static inline bool RowsEqual(const uint16_t *a, const uint16_t *b, int width) {
	for (int x = 0; x < width; x++)
		if (a[x] != b[x])
			return false;
	return true;
}

// YUV row builder with run-length + row-repeat
static inline void BuildYuvRow(uint32_t *dst, const uint16_t *src, const uint16_t *prevSrc, const uint32_t *prevSig, int width) {
	// whole-row repeat. If this source row equals the previous one, its
	// signatures equal the previous signature row verbatim (incl. guard cells).
	if (prevSrc && RowsEqual(src, prevSrc, width)) {
		__builtin_memcpy(dst, prevSig, (size_t)(width + 2) * sizeof(uint32_t));
		return;
	}

	uint16_t prevPix = src[0];
	uint32_t prevVal = inlineRGBtoYUV(prevPix);
	dst[0] = prevVal;         // left guard mirrors column 0
	dst[1] = prevVal;         // column 0 itself
	for (int x = 1; x < width; x++) {
		uint16_t p = src[x];
		if (p != prevPix) { prevVal = inlineRGBtoYUV(p); prevPix = p; }
		dst[x + 1] = prevVal;
	}
	dst[width + 1] = dst[width]; // right guard mirrors last column
}

// Brightness row builder
static inline void BuildBrtRow(uint16_t *dst, const uint16_t *src, const uint16_t *prevSrc, const uint16_t *prevSig, int width) {
	if (prevSrc && RowsEqual(src, prevSrc, width)) {
		__builtin_memcpy(dst, prevSig, (size_t)(width + 2) * sizeof(uint16_t));
		return;
	}

	uint16_t prevPix = src[0];
	uint16_t prevVal = inlineRGBtoBright(prevPix);
	dst[0] = prevVal;
	dst[1] = prevVal;
	for (int x = 1; x < width; x++) {
		uint16_t p = src[x];
		if (p != prevPix) { prevVal = inlineRGBtoBright(p); prevPix = p; }
		dst[x + 1] = prevVal;
	}
	dst[width + 1] = dst[width];
}

// =========================================================================
// OPTIMIZED HQ2X RENDER LOOP
// =========================================================================
// One templated body; the GuiScale parameter selects, at compile time, which
// cached signature(s) are built and how the 8-bit edge pattern is derived
//
// FILTER_HQ2X    : pattern bit = DiffYUVCached(neighbourYUV, centerYUV)
// FILTER_HQ2XBOLD: 3x3 brightness mean, bit = (brightN>mean) != (brightC>mean)
// FILTER_HQ2XS   : if any diagonal neighbour equals center -> YUV path,
//                  else brightness path (same hybrid rule as original)
//
// Pixels AND signatures slide through registers (U1=U2; y1=y2; ...). No reloads
// and no per-pixel branch clamps in the inner loop - only the row builders touch
// memory linearly (and are prefetched one row ahead).
//
// CACHE NOTE: The signature rings touch (width+2)*bytes*rows of L1 (see the
// buffer declaration block). With MAX_WIDTH capped at 256, the worst case (SOFT)
// is ~4.5 KB of rings + ~2 KB tables =~ 6.5 KB of 32 KB L1 - comfortably bounded.
// isValidDimensions() rejects anything larger, so the footprint can never grow
// beyond this.
template<int GuiScale>
void RenderHQ2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
	if(!isValidDimensions(width, height)) return;

	// Execute Pre-Pass Deduplication (Combined Exact Compare & Copy)
	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	const uint32_t src1line = srcPitch >> 1;
	const uint32_t dst1line = dstPitch >> 1;

	uint16_t *srcBase = (uint16_t *)srcPtr;
	uint16_t *dp      = (uint16_t *)dstPtr;

	// Rolling YUV signature rows (used by HQ2X and HQ2XS).
	uint32_t *yTop = yuvRowA, *yMid = yuvRowB, *yBot = yuvRowC;
	// Rolling brightness signature rows (used by HQ2XBOLD and HQ2XS).
	uint16_t *bTop = brtRowA, *bMid = brtRowB, *bBot = brtRowC;

	// Compile-time selectors so the loop body has zero variant branches.
	const bool USE_YUV   = (GuiScale == FILTER_HQ2X) || (GuiScale == FILTER_HQ2XS);
	const bool USE_BRIGHT = (GuiScale == FILTER_HQ2XBOLD) || (GuiScale == FILTER_HQ2XS);

	// Prime the top & middle window rows from row 0. The window's row -1 clamps
	// to row 0 (edge replication).
	//
	// DEDUP - yTop is built fresh (no previous row -> nullptr disables A3,
	// run-length A2 still applies). yMid is built from the SAME row 0, so we
	// hand it (srcBase, yTop) and A3 collapses it to a single memcpy.

	if (USE_YUV) { BuildYuvRow(yTop, srcBase, nullptr, nullptr, width);
				   BuildYuvRow(yMid, srcBase, srcBase, yTop, width); }

	if (USE_BRIGHT) { BuildBrtRow(bTop, srcBase, nullptr, nullptr, width);
					  BuildBrtRow(bMid, srcBase, srcBase, bTop, width); }

	for (int y = 0; y < height; y++) {
		int by = y >> 3;

		const uint16_t *rowMid = srcBase + (uint32_t)y * src1line;
		const uint16_t *rowTop = (y > 0) ? rowMid - src1line : rowMid;
		const uint16_t *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

		// Build ONLY the new bottom signature row(s) this iteration.
		// DEDUP - The row above rowBot is rowMid, whose signatures are in
		// yMid/bMid; passing them enables the whole-row-repeat skip (A3) when
		// rowBot == rowMid (common in vertical flat regions / letterboxing).
		if (USE_YUV) BuildYuvRow(yBot, rowBot, rowMid, yMid, width);
		if (USE_BRIGHT) BuildBrtRow(bBot, rowBot, rowMid, bMid, width);

		// Prefetch the scanline we'll convert next iteration (~1 row ahead).
		if (y + 2 < height)
			DCBT(rowBot + src1line);

		// ---- Seed the horizontal sliding window at column x = 0 ----
        // Directly into the 32-bit unpacked SWAR state (no 16-bit variables kept)
		// Initialize only the unpacked values in registers (avoids stack spilling)
		uint32_t U1 = Unpack565(rowTop[0]), U2 = U1;
		uint32_t U4 = Unpack565(rowMid[0]), U5 = U4;
		uint32_t U7 = Unpack565(rowBot[0]), U8 = U7;

		for (int x = 0; x < width; x++) {
			int bx = x >> 3;

			// Macro-Block Frame Skip Check (+1 offset due to padded grid)
			if (!render_mask[by + 1][bx + 1]) {
				int skip_to = (bx + 1) << 3;
				if (skip_to > width) skip_to = width;

				int diff = skip_to - x;
				dp += diff * 2;
				x = skip_to - 1;

				int left = x;
				int center = (skip_to < width) ? skip_to : width - 1;

				U1 = Unpack565(rowTop[left]); U4 = Unpack565(rowMid[left]); U7 = Unpack565(rowBot[left]);
				U2 = Unpack565(rowTop[center]); U5 = Unpack565(rowMid[center]); U8 = Unpack565(rowBot[center]);
				continue;
			}

			int rx = (x + 1 < width) ? x + 1 : x;

			uint32_t U3 = Unpack565(rowTop[rx]);
			uint32_t U6 = Unpack565(rowMid[rx]);
			uint32_t U9 = Unpack565(rowBot[rx]);

			// ---- FLAT-BLOCK FAST PATH (bit-exact, all variants) --------
			// If all nine window pixels are BIT-identical to the centre, the
			// 3x3 neighbourhood is uniform and canonical HQ2X emits the centre
			// to all four subpixels - regardless of which rule hqTable[pattern]
			// selects.
			//
			// This skips the per-pixel YUV pattern build AND the four blend
			// evaluations on flat content (the dominant case in SNES frames).
			// The check is a branchless XOR/OR reduction (1-cycle ops, dual-
			// issued) followed by ONE highly-predictable branch - exactly the
			// trade the Broadway rules favour (predictable branch >> arithmetic).
			// Cost on NON-flat content: ~8 cycles of XOR/OR that don't pay off;
			// this is the deliberate, bounded regression for detailed scenes.
			//
			// NOTE: we must test BIT-equality (not YUV/brightness equality).
			// pattern==0 does not imply op 0 (hqTable[8]==4), so YUV-equal-but-
			// bit-different pixels would blend (2c+s1+s2)/4 != c. Bit-equality
			// is the only predicate under which every op collapses to the centre.
			uint32_t flatBits = (U1 ^ U5) | (U2 ^ U5) | (U3 ^ U5) |
                                (U4 ^ U5) | (U6 ^ U5) | (U7 ^ U5) |
                                (U8 ^ U5) | (U9 ^ U5);

			if (flatBits == 0) {
				uint16_t w5 = Pack565(U5);
				uint32_t w32 = (w5 << 16) | w5; // WGP 32-bit optimization
				*(uint32_t*)(dp) = w32;
				*(uint32_t*)(dp + dst1line) = w32;

				// Slide the window/signatures so the next column is correct
				U1 = U2; U4 = U5; U7 = U8;
				U2 = U3; U5 = U6; U8 = U9;
				dp += 2;
				continue;
			}

			// Complex path: Fetch signatures from cache on-demand to save GPRs
            // Guard cells at index 0 guarantee x, x+1, x+2 align to Left, Center, Right
			uint32_t pattern = 0;
			uint32_t c_y2 = 0, c_y4 = 0, c_y6 = 0, c_y8 = 0;

			if (GuiScale == FILTER_HQ2X) {
				// Standard: each neighbour's cached YUV vs center cached YUV
				uint32_t y5 = yMid[x+1];
				pattern  = DiffYUVCached(yTop[x], y5) << 0;
				pattern |= DiffYUVCached(yTop[x+1], y5) << 1;
				pattern |= DiffYUVCached(yTop[x+2], y5) << 2;
				pattern |= DiffYUVCached(yMid[x], y5) << 3;
				pattern |= DiffYUVCached(yMid[x+2], y5) << 4;
				pattern |= DiffYUVCached(yBot[x], y5) << 5;
				pattern |= DiffYUVCached(yBot[x+1], y5) << 6;
				pattern |= DiffYUVCached(yBot[x+2], y5) << 7;
				c_y2 = yTop[x+1]; c_y4 = yMid[x]; c_y6 = yMid[x+2]; c_y8 = yBot[x+1];
			}
			else if (GuiScale == FILTER_HQ2XBOLD) {
				// Brightness vs 3x3 mean. sum<=9*186=1674; (sum*7282)>>16 ~= sum/9
				uint32_t sum = bTop[x] + bTop[x+1] + bTop[x+2] +
				               bMid[x] + bMid[x+1] + bMid[x+2] +
				               bBot[x] + bBot[x+1] + bBot[x+2];
				uint32_t mean = (sum * 7282) >> 16;
				bool c5 = (bMid[x+1] > mean);
				pattern  = ((U1 != U5) && ((bTop[x] > mean) != c5)) << 0;
				pattern |= ((U2 != U5) && ((bTop[x+1] > mean) != c5)) << 1;
				pattern |= ((U3 != U5) && ((bTop[x+2] > mean) != c5)) << 2;
				pattern |= ((U4 != U5) && ((bMid[x] > mean) != c5)) << 3;
				pattern |= ((U6 != U5) && ((bMid[x+2] > mean) != c5)) << 4;
				pattern |= ((U7 != U5) && ((bBot[x] > mean) != c5)) << 5;
				pattern |= ((U8 != U5) && ((bBot[x+1] > mean) != c5)) << 6;
				pattern |= ((U9 != U5) && ((bBot[x+2] > mean) != c5)) << 7;

				c_y2 = inlineRGBtoYUV(Pack565(U2));
				c_y4 = inlineRGBtoYUV(Pack565(U4));
				c_y6 = inlineRGBtoYUV(Pack565(U6));
				c_y8 = inlineRGBtoYUV(Pack565(U8));
			}
			else { // FILTER_HQ2XS - hybrid
				bool use_yuv = (U1 == U5) | (U3 == U5) | (U7 == U5) | (U9 == U5);
				if (use_yuv) {
					uint32_t y5 = yMid[x+1];
					pattern  = DiffYUVCached(yTop[x], y5) << 0;
					pattern |= DiffYUVCached(yTop[x+1], y5) << 1;
					pattern |= DiffYUVCached(yTop[x+2], y5) << 2;
					pattern |= DiffYUVCached(yMid[x], y5) << 3;
					pattern |= DiffYUVCached(yMid[x+2], y5) << 4;
					pattern |= DiffYUVCached(yBot[x], y5) << 5;
					pattern |= DiffYUVCached(yBot[x+1], y5) << 6;
					pattern |= DiffYUVCached(yBot[x+2], y5) << 7;
				} else {
					uint32_t sum = bTop[x] + bTop[x+1] + bTop[x+2] +
					               bMid[x] + bMid[x+1] + bMid[x+2] +
					               bBot[x] + bBot[x+1] + bBot[x+2];
	                uint32_t mean = (sum * 7282) >> 16;
	                bool c5 = (bMid[x+1] > mean);
	                pattern  = ((bTop[x] > mean) != c5) << 0;
	                pattern |= ((bTop[x+1] > mean) != c5) << 1;
	                pattern |= ((bTop[x+2] > mean) != c5) << 2;
	                pattern |= ((bMid[x] > mean) != c5) << 3;
	                pattern |= ((bMid[x+2] > mean) != c5) << 4;
	                pattern |= ((bBot[x] > mean) != c5) << 5;
	                pattern |= ((bBot[x+1] > mean) != c5) << 6;
	                pattern |= ((bBot[x+2] > mean) != c5) << 7;
	            }
	            c_y2 = yTop[x+1]; c_y4 = yMid[x]; c_y6 = yMid[x+2]; c_y8 = yBot[x+1];
			}

			EVALUATE_HQ2X_SUBPIXELS(c_y2, c_y4, c_y6, c_y8);

			// Slide the window one column to the right (register shuffle)
			U1 = U2; U4 = U5; U7 = U8;
			U2 = U3; U5 = U6; U8 = U9;

			dp += 2;
		}

		// Advance destination by one output scanline (we wrote two rows).
		dp += ((dst1line - (uint32_t)width) << 1);

		// Rotate the signature rings: mid->top, bot->mid, reuse old top as bot.
		if (USE_YUV) { uint32_t *t = yTop; yTop = yMid; yMid = yBot; yBot = t; }
		if (USE_BRIGHT) { uint16_t *t = bTop; bTop = bMid; bMid = bBot; bBot = t; }
	}
}

// -------------------------------------------------------------------------
// Optimized RenderScale2X (dcbz Chunking + Relative Pointer Sliding)
// -------------------------------------------------------------------------

#define PROCESS_SCALE2X_PIXEL() do { \
	uint32_t B = p_top[0]; \
	uint32_t F = p_mid[1]; \
	uint32_t H = p_bot[0]; \
	uint32_t E0 = E, E1 = E, E2 = E, E3 = E; \
	if (B != H && D != F) { \
		if (D == B) E0 = D; \
		if (B == F) E1 = F; \
		if (D == H) E2 = D; \
		if (H == F) E3 = F; \
	} \
	*(uint32_t*)&dp[0] = (E0 << 16) | E1; \
	*(uint32_t*)&dp[nextlineDst] = (E2 << 16) | E3; \
	D = E; E = F; \
	p_top++; p_mid++; p_bot++; dp += 2; \
} while(0)

template<int GuiScale>
void RenderScale2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

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

		// Only allow chunk skipping if Phase 1 didn't offset us from the 8x8 grid
		bool aligned_chunking = (((width - w) % 8) == 0);
		int bx = (width - w) >> 3;
		int by = y >> 3;

		while (chunks--) {
			if (aligned_chunking && !render_mask[by + 1][bx + 1]) {
				p_top += 8; p_mid += 8; p_bot += 8; dp += 16;
				D = p_mid[-1]; E = p_mid[0]; // Scale2X history is left and center
				bx++;
				continue;
			}

			__asm__ volatile ("dcbz 0, %0" :: "b" (dp) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (dp + nextlineDst) : "memory");
			}
			DCBT(p_top + 16); DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				PROCESS_SCALE2X_PIXEL();
			}
			bx++;
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

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	uint32_t wd1, wd2, irlv1, irlv2u, irlv2l, dFG, dHC;
	uint16_t px;

	uint32_t nextlineSrc = srcPitch >> 1; // Convert pitch in bytes to pitch in uint16_t
	uint32_t nextlineDst = dstPitch >> 1;

	uint16_t *p_base = (uint16_t *)srcPtr;
	uint16_t *Ep_base = (uint16_t *)dstPtr;

	uint16_t A, B, C, D, E, F, G, H, I;

	for (int y = 0; y < height; ++y) {
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

		// Guard against misalignment
		bool aligned_chunking = (((width - w) % 8) == 0);
		int bx = (width - w) >> 3;
		int by = y >> 3;

		while (chunks--) {
			if (aligned_chunking && !render_mask[by + 1][bx + 1]) {
				p_top += 8; p_mid += 8; p_bot += 8; Ep += 16;

				// To maintain history, A/D/G are offset -1, B/E/H are offset 0
				A = p_top[-1]; B = p_top[0];
				D = p_mid[-1]; E = p_mid[0];
				G = p_bot[-1]; H = p_bot[0];

				bx++;
				continue;
			}

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
			bx++;
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

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	uint32_t wd1, wd2, irlv1;
	uint16_t px;

	uint32_t nextlineSrc = srcPitch >> 1;
	uint32_t nextlineDst = dstPitch >> 1;

	uint16_t *p_base = (uint16_t *)srcPtr;
	uint16_t *Ep_base = (uint16_t *)dstPtr;

	uint16_t A, B, C, D, E, F, G, H, I;

	for (int y = 0; y < height; ++y) {
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

		// Guard against misalignment
		bool aligned_chunking = (((width - w) % 8) == 0);
		int bx = (width - w) >> 3;
		int by = y >> 3;

		while (chunks--) {
			if (aligned_chunking && !render_mask[by + 1][bx + 1]) {
				p_top += 8; p_mid += 8; p_bot += 8; Ep += 16;

				A = p_top[-1]; B = p_top[0];
				D = p_mid[-1]; E = p_mid[0];
				G = p_bot[-1]; H = p_bot[0];

				bx++;
				continue;
			}

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
			bx++;
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

// GCC translates bitwise masking logic into fused 1-cycle rlwinm instructions
static inline uint32_t ddt_fast_luma(uint16_t c) {
	return  ddt_luma_r_lut[(c >> 11) & 0x1F] +
			ddt_luma_g_lut[(c >>  5) & 0x3F] +
			ddt_luma_b_lut[ c        & 0x1F];
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

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

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

		// Guard against misalignment
		bool aligned_chunking = (((width - w) % 8) == 0);
		int bx = (width - w) >> 3;
		int by = y >> 3;

		while (chunks--) {
			if (aligned_chunking && !render_mask[by + 1][bx + 1]) {
				p_mid += 8; p_bot += 8; Ep += 16;
				// DDT needs the current center pixels restored
				E = p_mid[0]; H = p_bot[0];
				bx++;
				continue;
			}

			__asm__ volatile ("dcbz 0, %0" :: "b" (Ep) : "memory");
			if (bot_aligned) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (Ep + nextlineDst) : "memory");
			}
			DCBT(p_mid + 16); DCBT(p_bot + 16);

			for (int i = 0; i < 8; i++) {
				PROCESS_DDT_PIXEL();
			}
			bx++;
		}

		// Phase 3: Trailing pixels
		while (tail--) {
			PROCESS_DDT_PIXEL();
		}

		p_base  += nextlineSrc;
		Ep_base += (nextlineDst << 1);
	}
}
