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
#include <unistd.h>

// SNES filtered-framebuffer limits. Hi-res (512-wide) modes are NOT routed
// through these 2x filters, so 256x240 is the true maximum. Sizing to the real maximum
// keeps the signature ring buffers (sized by MAX_WIDTH) small in the 32KB L1 D-cache.
#define MAX_HEIGHT 240
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

// -------------------------------------------------------------------------
// Pixel Format Trait Classes (Zero-Overhead Compile-Time Abstraction)
// -------------------------------------------------------------------------
struct FormatRGB565 {
    typedef uint16_t Type;
    static inline uint16_t Read(const Type* ptr) { return *ptr; }

    // Deduplicator helpers (8 pixels = 16 bytes)
    static inline bool Compare8(const Type* a, const Type* b) {
        const uint32_t* wa = (const uint32_t*)a;
        const uint32_t* wb = (const uint32_t*)b;
        return (wa[0] == wb[0] && wa[1] == wb[1] && wa[2] == wb[2] && wa[3] == wb[3]);
    }
    static inline void Copy8(Type* dst, const Type* src) {
        uint32_t* wd = (uint32_t*)dst;
        const uint32_t* ws = (const uint32_t*)src;
        wd[0] = ws[0]; wd[1] = ws[1]; wd[2] = ws[2]; wd[3] = ws[3];
    }
};

extern unsigned short rgb565[256];

struct FormatPal8 {
    typedef uint8_t Type;
    static inline uint16_t Read(const Type* ptr) { return rgb565[*ptr]; }

    // Deduplicator helpers (8 pixels = 8 bytes)
    static inline bool Compare8(const Type* a, const Type* b) {
        const uint32_t* wa = (const uint32_t*)a;
        const uint32_t* wb = (const uint32_t*)b;
        return (wa[0] == wb[0] && wa[1] == wb[1]);
    }
    static inline void Copy8(Type* dst, const Type* src) {
        uint32_t* wd = (uint32_t*)dst;
        const uint32_t* ws = (const uint32_t*)src;
        wd[0] = ws[0]; wd[1] = ws[1];
    }
};

// Aliased at compile time based on the active emulator
#ifdef FORMAT_PAL8
    typedef FormatPal8 ActiveSrcFormat;
#else
    typedef FormatRGB565 ActiveSrcFormat;
#endif

// ------------------------------------------------------------------------------
// MACRO-BLOCK DEDUPLICATION STATE
// ------------------------------------------------------------------------------
// Padded to 34x34 to allow branchless dirty dilation (no bounds checking)
static bool render_mask[34][34] __attribute__((aligned(32)));
static bool invalidate_hashes = true; // Forces a full render on first pass or res change

// 128KB static buffer to hold the raw frame. Aligned to 32 bytes for cache.
static typename ActiveSrcFormat::Type previousEmuScreen[MAX_HEIGHT * MAX_WIDTH] __attribute__((aligned(32)));

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

	// Handle menu transition. Next frame will fully render
	invalidate_hashes = true;
	InitFilterTables();
}

static bool isValidDimensions (int width, int height) {
	if (height > MAX_HEIGHT || width > MAX_WIDTH || height <= 0 || width <= 0)
		return false;
	return true;
}

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
	const uint32_t prevPitch = MAX_WIDTH * sizeof(typename ActiveSrcFormat::Type);

	// Pass 1: Compare Blocks and Copy if Dirty
	for (int by = 0; by < blocks_h; by++) {
		const uint8_t* row_base = srcPtr + (by << 3) * srcPitch;
		uint8_t* prev_row_base  = (uint8_t*)previousEmuScreen + (by << 3) * prevPitch;

		for (int bx = 0; bx < blocks_w; bx++) {
			const typename ActiveSrcFormat::Type* block_ptr = (const typename ActiveSrcFormat::Type*)row_base;
			typename ActiveSrcFormat::Type* prev_ptr        = (typename ActiveSrcFormat::Type*)prev_row_base;
			bool is_dirty = force;

			// Exact memory comparison reading 16 bytes (four 32-bit ints) per row
			if (!is_dirty) {
				for (int y = 0; y < 8 && ((by << 3) + y) < height; y++) {
					if (!ActiveSrcFormat::Compare8(block_ptr, prev_ptr)) {
						is_dirty = true;
						break;
					}
					block_ptr = (const typename ActiveSrcFormat::Type*)((const uint8_t*)block_ptr + srcPitch);
					prev_ptr  = (typename ActiveSrcFormat::Type*)((uint8_t*)prev_ptr + prevPitch);
				}
			}

			// If block changed, flag dirty AND update previous buffer immediately
			if (is_dirty) {
				dirty[by + 1][bx + 1] = true;

				block_ptr = (const typename ActiveSrcFormat::Type*)row_base;
				prev_ptr  = (typename ActiveSrcFormat::Type*)prev_row_base;

				for (int y = 0; y < 8 && ((by << 3) + y) < height; y++) {
                    ActiveSrcFormat::Copy8(prev_ptr, block_ptr);
					block_ptr = (const typename ActiveSrcFormat::Type*)((const uint8_t*)block_ptr + srcPitch);
					prev_ptr  = (typename ActiveSrcFormat::Type*)((uint8_t*)prev_ptr + prevPitch);
				}
			}

			row_base += 8 * sizeof(typename ActiveSrcFormat::Type);
			prev_row_base += 8 * sizeof(typename ActiveSrcFormat::Type);
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

//
// HQ2X Filter Code:
//

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

// YUV threshold compare on two ALREADY-CACHED packed YUV values
// Thresholds - 48/7/6
static inline bool DiffYUVCached(uint32_t a, uint32_t b) {
	if (a == b) return false;
	uint32_t dy = __builtin_abs((int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF));
	uint32_t du = __builtin_abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF));
	uint32_t dv = __builtin_abs((int)(a & 0xFF) - (int)(b & 0xFF));
	return (dy > 48) | (du > 7) | (dv > 6);
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

// SIGNATURE ROW BUILDERS
//
// Convert one full source scanline into cached signatures, ONCE. Guard cells at
// index 0 and width+1 replicate the edge columns so the seeded sliding window
// (which starts at x=-1) reads defined values without per-pixel clamping.

// DEDUP - Returns true iff two source rows are bit-identical (early-out).
template<typename Format>
static inline bool RowsEqual(const typename Format::Type *a, const typename Format::Type *b, int width) {
	for (int x = 0; x < width; x++) {
		if (a[x] != b[x])
			return false;
	}
	return true;
}

// YUV row builder with run-length + row-repeat
template<typename Format>
static inline void BuildYuvRow(uint32_t *dst, const typename Format::Type *src, const typename Format::Type *prevSrc, const uint32_t *prevSig, int width) {
	// whole-row repeat. If this source row equals the previous one, its
	// signatures equal the previous signature row verbatim (incl. guard cells).
	if (prevSrc && RowsEqual<Format>(src, prevSrc, width)) {
		__builtin_memcpy(dst, prevSig, (size_t)(width + 2) * sizeof(uint32_t));
		return;
	}
	uint16_t prevPix = Format::Read(&src[0]);
	uint32_t prevVal = inlineRGBtoYUV(prevPix);
	dst[0] = prevVal; // left guard mirrors column 0
	dst[1] = prevVal; // column 0 itself
	for (int x = 1; x < width; x++) {
		uint16_t p = Format::Read(&src[x]);
		if (p != prevPix) { prevVal = inlineRGBtoYUV(p); prevPix = p; }
		dst[x + 1] = prevVal;
	}
	dst[width + 1] = dst[width]; // right guard mirrors last column
}

template<typename Format>
static inline void BuildBrtRow(uint16_t *dst, const typename Format::Type *src, const typename Format::Type *prevSrc, const uint16_t *prevSig, int width) {
	if (prevSrc && RowsEqual<Format>(src, prevSrc, width)) {
		__builtin_memcpy(dst, prevSig, (size_t)(width + 2) * sizeof(uint16_t));
		return;
	}
	uint16_t prevPix = Format::Read(&src[0]);
	uint16_t prevVal = inlineRGBtoBright(prevPix);
	dst[0] = prevVal; dst[1] = prevVal;
	for (int x = 1; x < width; x++) {
		uint16_t p = Format::Read(&src[x]);
		if (p != prevPix) { prevVal = inlineRGBtoBright(p); prevPix = p; }
		dst[x + 1] = prevVal;
	}
	dst[width + 1] = dst[width];
}

// -------------------------------------------------------------------------
// Core HQ2X Subpixel Evaluator
// -------------------------------------------------------------------------
template <int GuiScale, typename Format>
static inline void EvaluateHQ2XSubpixels(
	int cx, int width,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	const uint32_t *yTop, const uint32_t *yMid, const uint32_t *yBot,
	const uint16_t *bTop, const uint16_t *bMid, const uint16_t *bBot,
	uint32_t U1, uint32_t U2, uint32_t U4, uint32_t U5, uint32_t U7, uint32_t U8,
	uint32_t &U3, uint32_t &U6, uint32_t &U9,
	uint32_t &w0, uint32_t &w1) __attribute__((always_inline));

template <int GuiScale, typename Format>
static inline void EvaluateHQ2XSubpixels(
	int cx, int width,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	const uint32_t *yTop, const uint32_t *yMid, const uint32_t *yBot,
	const uint16_t *bTop, const uint16_t *bMid, const uint16_t *bBot,
	uint32_t U1, uint32_t U2, uint32_t U4, uint32_t U5, uint32_t U7, uint32_t U8,
	uint32_t &U3, uint32_t &U6, uint32_t &U9,
	uint32_t &w0, uint32_t &w1)
{
	uint32_t mask = (cx + 1 - width) >> 31;
	int rx = cx + (1 & mask);

	U3 = Unpack565(Format::Read(&rowTop[rx]));
	U6 = Unpack565(Format::Read(&rowMid[rx]));
	U9 = Unpack565(Format::Read(&rowBot[rx]));

	uint32_t flatBits = (U1 ^ U5) | (U2 ^ U5) | (U3 ^ U5) | (U4 ^ U5) | (U6 ^ U5) | (U7 ^ U5) | (U8 ^ U5) | (U9 ^ U5);

	if (flatBits == 0) {
		uint16_t w5 = Pack565(U5);
		w0 = w1 = (w5 << 16) | w5;
	} else {
		uint32_t pattern = 0, c_y2 = 0, c_y4 = 0, c_y6 = 0, c_y8 = 0;
		if (GuiScale == FILTER_HQ2X) {
			// Standard: each neighbour's cached YUV vs center cached YUV
			uint32_t y5 = yMid[cx+1];
			pattern = DiffYUVCached(yTop[cx], y5) << 0 | DiffYUVCached(yTop[cx+1], y5) << 1 | DiffYUVCached(yTop[cx+2], y5) << 2 |
					  DiffYUVCached(yMid[cx], y5) << 3 | DiffYUVCached(yMid[cx+2], y5) << 4 | DiffYUVCached(yBot[cx], y5) << 5 |
					  DiffYUVCached(yBot[cx+1], y5) << 6 | DiffYUVCached(yBot[cx+2], y5) << 7;
			c_y2 = yTop[cx+1]; c_y4 = yMid[cx]; c_y6 = yMid[cx+2]; c_y8 = yBot[cx+1];
		} else if (GuiScale == FILTER_HQ2XBOLD) {
			// Brightness vs 3x3 mean. sum<=9*186=1674; (sum*7282)>>16 ~= sum/9
			uint32_t mean = ((bTop[cx] + bTop[cx+1] + bTop[cx+2] + bMid[cx] + bMid[cx+1] + bMid[cx+2] + bBot[cx] + bBot[cx+1] + bBot[cx+2]) * 7282) >> 16;
			bool c5 = (bMid[cx+1] > mean);
			pattern = ((U1 != U5) && ((bTop[cx] > mean) != c5)) << 0 | ((U2 != U5) && ((bTop[cx+1] > mean) != c5)) << 1 |
					  ((U3 != U5) && ((bTop[cx+2] > mean) != c5)) << 2 | ((U4 != U5) && ((bMid[cx] > mean) != c5)) << 3 |
					  ((U6 != U5) && ((bMid[cx+2] > mean) != c5)) << 4 | ((U7 != U5) && ((bBot[cx] > mean) != c5)) << 5 |
					  ((U8 != U5) && ((bBot[cx+1] > mean) != c5)) << 6 | ((U9 != U5) && ((bBot[cx+2] > mean) != c5)) << 7;
			c_y2 = inlineRGBtoYUV(Pack565(U2)); c_y4 = inlineRGBtoYUV(Pack565(U4));
			c_y6 = inlineRGBtoYUV(Pack565(U6)); c_y8 = inlineRGBtoYUV(Pack565(U8));
		} else { // FILTER_HQ2XS - hybrid
			// use yuv
			if ((U1 == U5) | (U3 == U5) | (U7 == U5) | (U9 == U5)) {
				uint32_t y5 = yMid[cx+1];
				pattern = DiffYUVCached(yTop[cx], y5) << 0 | DiffYUVCached(yTop[cx+1], y5) << 1 | DiffYUVCached(yTop[cx+2], y5) << 2 |
						  DiffYUVCached(yMid[cx], y5) << 3 | DiffYUVCached(yMid[cx+2], y5) << 4 | DiffYUVCached(yBot[cx], y5) << 5 |
						  DiffYUVCached(yBot[cx+1], y5) << 6 | DiffYUVCached(yBot[cx+2], y5) << 7;
			} else {
				uint32_t mean = ((bTop[cx] + bTop[cx+1] + bTop[cx+2] + bMid[cx] + bMid[cx+1] + bMid[cx+2] + bBot[cx] + bBot[cx+1] + bBot[cx+2]) * 7282) >> 16;
				bool c5 = (bMid[cx+1] > mean);
				pattern = ((bTop[cx] > mean) != c5) << 0 | ((bTop[cx+1] > mean) != c5) << 1 | ((bTop[cx+2] > mean) != c5) << 2 |
						  ((bMid[cx] > mean) != c5) << 3 | ((bMid[cx+2] > mean) != c5) << 4 | ((bBot[cx] > mean) != c5) << 5 |
						  ((bBot[cx+1] > mean) != c5) << 6 | ((bBot[cx+2] > mean) != c5) << 7;
			}
			c_y2 = yTop[cx+1]; c_y4 = yMid[cx]; c_y6 = yMid[cx+2]; c_y8 = yBot[cx+1];
		}

		uint8_t p = pattern;
		
		// evaluate subpixels
		uint16_t p0 = BlendU(ResolveOp(p, c_y2, c_y4, c_y6, c_y8), U5, U1, U2, U4); p = rotateTable[p];
		uint16_t p1 = BlendU(ResolveOp(p, c_y6, c_y2, c_y8, c_y4), U5, U3, U6, U2); p = rotateTable[p];
		uint16_t p3 = BlendU(ResolveOp(p, c_y8, c_y6, c_y4, c_y2), U5, U9, U8, U6); p = rotateTable[p];
		uint16_t p2 = BlendU(ResolveOp(p, c_y4, c_y8, c_y2, c_y6), U5, U7, U4, U8);

		w0 = (p0 << 16) | p1;
		w1 = (p2 << 16) | p3;
	}
}

// -------------------------------------------------------------------------
// HQ2X Templated Row Processor (Eliminates Source Duplication)
// -------------------------------------------------------------------------
template<int GuiScale, bool YIsEven, typename Format>
static inline void ProcessHQ2XRow(
	int width, int by,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	const uint32_t *yTop, const uint32_t *yMid, const uint32_t *yBot,
	const uint16_t *bTop, const uint16_t *bMid, const uint16_t *bBot,
	uint8_t *tile_ptr) __attribute__((always_inline));

template<int GuiScale, bool YIsEven, typename Format>
static inline void ProcessHQ2XRow(
	int width, int by,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	const uint32_t *yTop, const uint32_t *yMid, const uint32_t *yBot,
	const uint16_t *bTop, const uint16_t *bMid, const uint16_t *bBot,
	uint8_t *tile_ptr)
{
	int chunks = width >> 3;
	int tail_pairs = (width & 7) >> 1;
	int bx = 0;
	int cx = 0;

	// Seed initial sliding window
	uint32_t U1 = Unpack565(Format::Read(&rowTop[0])), U2 = U1;
	uint32_t U4 = Unpack565(Format::Read(&rowMid[0])), U5 = U4;
	uint32_t U7 = Unpack565(Format::Read(&rowBot[0])), U8 = U7;

	while (chunks--) {
		// Macro-Block Frame Skip Check (+1 offset due to padded grid)
		if (!render_mask[by + 1][bx + 1]) {
			cx += 8;
			tile_ptr += 128;
			int left = cx - 1;
			int center = cx;
			U1 = Unpack565(Format::Read(&rowTop[left])); U4 = Unpack565(Format::Read(&rowMid[left])); U7 = Unpack565(Format::Read(&rowBot[left]));
			U2 = Unpack565(Format::Read(&rowTop[center])); U5 = Unpack565(Format::Read(&rowMid[center])); U8 = Unpack565(Format::Read(&rowBot[center]));
			bx++;
			continue;
		}
		for (int i = 0; i < 4; i++) {
			if (YIsEven) {
				__asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory");
			}
			uint32_t w0, w1, U3, U6, U9;

			// Even Pixel (Left Subpixels)
			EvaluateHQ2XSubpixels<GuiScale, Format>(
				cx, width, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot,
				U1, U2, U4, U5, U7, U8, U3, U6, U9, w0, w1
			);
			*(uint32_t*)(tile_ptr + 0) = w0; *(uint32_t*)(tile_ptr + 8) = w1;
			U1 = U2; U4 = U5; U7 = U8; U2 = U3; U5 = U6; U8 = U9;
			cx++;

			// Odd Pixel (Right Subpixels)
			EvaluateHQ2XSubpixels<GuiScale, Format>(
				cx, width, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot,
				U1, U2, U4, U5, U7, U8, U3, U6, U9, w0, w1
			);
			*(uint32_t*)(tile_ptr + 4) = w0; *(uint32_t*)(tile_ptr + 12) = w1;
			U1 = U2; U4 = U5; U7 = U8; U2 = U3; U5 = U6; U8 = U9;
			cx++;

			tile_ptr += 32;
		}
		bx++;
	}

	while (tail_pairs--) {
		if (YIsEven) {
			__asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory");
		}
		uint32_t w0, w1, U3, U6, U9;

		EvaluateHQ2XSubpixels<GuiScale, Format>(
			cx, width, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot,
			U1, U2, U4, U5, U7, U8, U3, U6, U9, w0, w1
		);
		*(uint32_t*)(tile_ptr + 0) = w0; *(uint32_t*)(tile_ptr + 8) = w1;
		U1 = U2; U4 = U5; U7 = U8; U2 = U3; U5 = U6; U8 = U9;
		cx++;

		EvaluateHQ2XSubpixels<GuiScale, Format>(
			cx, width, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot,
			U1, U2, U4, U5, U7, U8, U3, U6, U9, w0, w1
		);
		*(uint32_t*)(tile_ptr + 4) = w0; *(uint32_t*)(tile_ptr + 12) = w1;
		U1 = U2; U4 = U5; U7 = U8; U2 = U3; U5 = U6; U8 = U9;
		cx++;

		tile_ptr += 32;
	}
}

// -------------------------------------------------------------------------
// HQ2X Templated Entry Point
// -------------------------------------------------------------------------

template<int GuiScale>
void RenderHQ2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height) {
	if(!isValidDimensions(width, height)) return;
	
	// Execute Pre-Pass Deduplication (Combined Exact Compare & Copy)
	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	const uint32_t src1line = srcPitch / sizeof(typename ActiveSrcFormat::Type);
	uint32_t tile_row_pitch = width * 16;
	typename ActiveSrcFormat::Type *srcBase = (typename ActiveSrcFormat::Type *)srcPtr;

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

	if (USE_YUV) {
		BuildYuvRow<ActiveSrcFormat>(yTop, srcBase, nullptr, nullptr, width);
		BuildYuvRow<ActiveSrcFormat>(yMid, srcBase, srcBase, yTop, width);
	}
	if (USE_BRIGHT) {
		BuildBrtRow<ActiveSrcFormat>(bTop, srcBase, nullptr, nullptr, width);
		BuildBrtRow<ActiveSrcFormat>(bMid, srcBase, srcBase, bTop, width);
	}

	for (int y = 0; y < height; y++) {
		int by = y >> 3;
		const typename ActiveSrcFormat::Type *rowMid = srcBase + (uint32_t)y * src1line;
		const typename ActiveSrcFormat::Type *rowTop = (y > 0) ? rowMid - src1line : rowMid;
		const typename ActiveSrcFormat::Type *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

		// Build ONLY the new bottom signature row(s) this iteration.
		// DEDUP - The row above rowBot is rowMid, whose signatures are in
		// yMid/bMid; passing them enables the whole-row-repeat skip (A3) when
		// rowBot == rowMid (common in vertical flat regions / letterboxing).
		if (USE_YUV) BuildYuvRow<ActiveSrcFormat>(yBot, rowBot, rowMid, yMid, width);
		if (USE_BRIGHT) BuildBrtRow<ActiveSrcFormat>(bBot, rowBot, rowMid, bMid, width);

		// Prefetch the scanline we'll convert next iteration (~1 row ahead).
		if (y + 2 < height)
			DCBT(rowBot + src1line);

		bool y_is_even = ((y & 1) == 0);
		uint8_t *tile_ptr = dstPtr + (y >> 1) * tile_row_pitch + (y_is_even ? 0 : 16);

		// Resolve branch duplication gracefully via Templates
		if (y_is_even) {
			ProcessHQ2XRow<GuiScale, true, ActiveSrcFormat>(
				width, by, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot, tile_ptr
			);
		} else {
			ProcessHQ2XRow<GuiScale, false, ActiveSrcFormat>(
				width, by, rowTop, rowMid, rowBot, yTop, yMid, yBot, bTop, bMid, bBot, tile_ptr
			);
		}

		if (USE_YUV) { uint32_t *t = yTop; yTop = yMid; yMid = yBot; yBot = t; }
		if (USE_BRIGHT) { uint16_t *t = bTop; bTop = bMid; bMid = bBot; bBot = t; }
	}
}

// -------------------------------------------------------------------------
// RenderScale2X
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// BRANCHLESS SWAR EVALUATOR: Scale2X
// -------------------------------------------------------------------------
template<typename Format>
static inline void EvaluateScale2XSubpixels(
	uint32_t B, uint32_t D, uint32_t E, uint32_t F, uint32_t H,
	uint32_t &w0, uint32_t &w1) __attribute__((always_inline));

template<typename Format>
static inline void EvaluateScale2XSubpixels(
	uint32_t B, uint32_t D, uint32_t E, uint32_t F, uint32_t H,
	uint32_t &w0, uint32_t &w1)
{
	// Predictable Early Out: If edges do not contrast, return center pixel.
	// We use bitwise & to force condition registers rather than short-circuit branches.
	if (__builtin_expect(!((B != H) & (D != F)), 1)) {
		w0 = (E << 16) | E;
		w1 = w0;
		return;
	}

	// Branchless Selection: Casting the boolean equality to uint32_t and negating it
	// creates a 0xFFFFFFFF mask if true, or 0x00000000 mask if false.
	// This invokes PowerPC subfc/subfe instructions instead of branches.
	uint32_t m0 = -(uint32_t)(D == B);
	uint32_t E0 = (D & m0) | (E & ~m0);

	uint32_t m1 = -(uint32_t)(B == F);
	uint32_t E1 = (F & m1) | (E & ~m1);

	uint32_t m2 = -(uint32_t)(D == H);
	uint32_t E2 = (D & m2) | (E & ~m2);

	uint32_t m3 = -(uint32_t)(H == F);
	uint32_t E3 = (F & m3) | (E & ~m3);

	// Pack 16-bit subpixels directly into 32-bit payloads for the WGP
	w0 = (E0 << 16) | E1;
	w1 = (E2 << 16) | E3;
}

// -------------------------------------------------------------------------
// ROW PROCESSOR: Scale2X (Handles sliding window, dcbz, and deduplication)
// -------------------------------------------------------------------------
template<bool YIsEven, typename Format>
static inline void ProcessScale2XRow(
	int width, int by,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	uint8_t *tile_ptr) __attribute__((always_inline));

template<bool YIsEven, typename Format>
static inline void ProcessScale2XRow(
	int width, int by,
	const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	uint8_t *tile_ptr)
{
	int chunks = width >> 3;
	int tail_pairs = (width & 7) >> 1;
	int bx = 0;
	int cx = 0;

	// Seed sliding window natively. No Unpack565 overhead required until blending.
	uint32_t D = Format::Read(&rowMid[0]), E = D, F;

	while (chunks--) {
		if (!render_mask[by + 1][bx + 1]) {
			cx += 8;
			tile_ptr += 128; // Advance destination pointer across skipped 8x8 block (4 tiles)

			// Re-seed anchors correctly at the new offset to prevent seam tearing
			D = Format::Read(&rowMid[cx - 1]);
			E = Format::Read(&rowMid[cx]);
			bx++;
			continue;
		}

		// Pre-fetch memory 16 pixels (32 bytes) ahead to hide L1 cache latency
		DCBT(rowTop + cx + 16); DCBT(rowMid + cx + 16); DCBT(rowBot + cx + 16);

		for (int i = 0; i < 4; i++) {
			// Zero the 32-byte cache line ONLY on even rows to bypass write-allocate stalls
			if (YIsEven) { __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory"); }

			uint32_t w0, w1;

			// Subpixel 1 (Top/Left Swizzle: offsets +0, +8)
			uint32_t B = Format::Read(&rowTop[cx]);
			F = Format::Read(&rowMid[cx + 1]);
			uint32_t H = Format::Read(&rowBot[cx]);
			EvaluateScale2XSubpixels<Format>(B, D, E, F, H, w0, w1);
			*(uint32_t*)(tile_ptr + 0) = w0;
			*(uint32_t*)(tile_ptr + 8) = w1;

			D = E; E = F; cx++;

			// Subpixel 2 (Top/Right Swizzle: offsets +4, +12)
			B = Format::Read(&rowTop[cx]);
			F = Format::Read(&rowMid[cx + 1]);
			H = Format::Read(&rowBot[cx]);
			EvaluateScale2XSubpixels<Format>(B, D, E, F, H, w0, w1);
			*(uint32_t*)(tile_ptr + 4) = w0;
			*(uint32_t*)(tile_ptr + 12) = w1;

			D = E; E = F; cx++;
			tile_ptr += 32;
		}
		bx++;
	}

	// Process remainder tail (Guaranteed pairs)
	while (tail_pairs--) {
		if (YIsEven) { __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory"); }

		uint32_t w0, w1;
		uint32_t B = Format::Read(&rowTop[cx]);
		F = Format::Read(&rowMid[cx + 1]);
		uint32_t H = Format::Read(&rowBot[cx]);

		EvaluateScale2XSubpixels<Format>(B, D, E, F, H, w0, w1);
		*(uint32_t*)(tile_ptr + 0) = w0; *(uint32_t*)(tile_ptr + 8) = w1;
		D = E; E = F; cx++;

		B = Format::Read(&rowTop[cx]);
		F = Format::Read(&rowMid[cx + 1]);
		H = Format::Read(&rowBot[cx]);

		EvaluateScale2XSubpixels<Format>(B, D, E, F, H, w0, w1);
		*(uint32_t*)(tile_ptr + 4) = w0; *(uint32_t*)(tile_ptr + 12) = w1;
		D = E; E = F; cx++;
		tile_ptr += 32;
	}
}

template<int GuiScale>
void RenderScale2X (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	const uint32_t src1line = srcPitch / sizeof(typename ActiveSrcFormat::Type);
	uint32_t tile_row_pitch = width * 16;
	typename ActiveSrcFormat::Type *srcBase = (typename ActiveSrcFormat::Type *)srcPtr;

	for (int y = 0; y < height; y++) {
		int by = y >> 3;
		const typename ActiveSrcFormat::Type *rowMid = srcBase + (uint32_t)y * src1line;
		const typename ActiveSrcFormat::Type *rowTop = (y > 0) ? rowMid - src1line : rowMid;
		const typename ActiveSrcFormat::Type *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

		bool y_is_even = ((y & 1) == 0);
		uint8_t *tile_ptr = dstPtr + (y >> 1) * tile_row_pitch + (y_is_even ? 0 : 16);

		// Resolve dcbz optimization via Templates without introducing inner loop branches
		if (y_is_even) {
			ProcessScale2XRow<true, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
		} else {
			ProcessScale2XRow<false, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
		}
	}
}

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

// PowerPC rlwinm masks for 16-bit uints in 32-bit GPRs
// Optimized with L1 Micro-LUTs to eliminate mullw cycle stalls
static inline int RGB565_to_Lum(uint16_t c) {
	uint32_t r, g, b;
	__asm__ volatile ("rlwinm %0, %1, 21, 27, 31" : "=r"(r) : "r"(c)); // Red: shift right 11 -> rotate 21
	__asm__ volatile ("rlwinm %0, %1, 27, 26, 31" : "=r"(g) : "r"(c)); // Green: shift right 5 -> rotate 27
	__asm__ volatile ("rlwinm %0, %1, 0,  27, 31" : "=r"(b) : "r"(c)); // Blue: shift right 0 -> rotate 0
	return xbr_luma_r_lut[r] + xbr_luma_g_lut[g] + xbr_luma_b_lut[b];
}
// -------------------------------------------------------------------------
// BRANCHLESS SWAR EVALUATOR (Broadway Optimized - Color Safe)
// -------------------------------------------------------------------------

template<bool IsLv1>
static inline void Evaluate2XBRSubpixels(
    uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t E, uint32_t F,
    uint32_t G, uint32_t H, uint32_t I, uint32_t &w0, uint32_t &w1) __attribute__((always_inline));

template<bool IsLv1>
static inline void Evaluate2XBRSubpixels(
    uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t E, uint32_t F,
    uint32_t G, uint32_t H, uint32_t I, uint32_t &w0, uint32_t &w1)
{
    // PREDICTABLE EARLY OUT: Skip complex edge-detection if E matches horizontal OR vertical lines.
    // Predictor hits 80%+ of the time, saving ~60 cycles.
    if (__builtin_expect((E == F && E == D) || (E == H && E == B), 1)) {
        w0 = (E << 16) | E;
        w1 = w0;
        return;
    }

    // Initialize subpixels to the 16-bit packed Center pixel
    uint32_t E0 = E, E1 = E, E2 = E, E3 = E;

    // Pre-calculate Lumas natively (A-I are already 16-bit packed, avoiding SWAR overhead)
    uint32_t lA = RGB565_to_Lum(A); uint32_t lB = RGB565_to_Lum(B);
    uint32_t lC = RGB565_to_Lum(C); uint32_t lD = RGB565_to_Lum(D);
    uint32_t lE = RGB565_to_Lum(E); uint32_t lF = RGB565_to_Lum(F);
    uint32_t lG = RGB565_to_Lum(G); uint32_t lH = RGB565_to_Lum(H);
    uint32_t lI = RGB565_to_Lum(I);

    #define EVAL_CORNER(PE, PI, PH, PF, PG, PC, PD, PB, PA, lPE, lPI, lPH, lPF, lPG, lPC, lPD, lPB, lPA, OUT_N) \
    do { \
        if (PE != PH && PE != PF) { \
            uint32_t wd1 = df(lPH, lPF); \
            uint32_t wd2 = df(lPE, lPI); \
            \
            /* Branchless Select: if (df(lPE,lPF) <= df(lPE,lPH)) px=PF; else px=PH; */ \
            /* Casting the boolean to uint32_t and negating creates a 0xFFFFFFFF or 0x0 mask */ \
            uint32_t px_mask = -(uint32_t)(df(lPE, lPF) <= df(lPE, lPH)); \
            uint32_t px = (PF & px_mask) | (PH & ~px_mask); \
            \
            if (IsLv1) { \
                /* Pure Level 1 Logic */ \
                if (wd1 <= wd2) { OUT_N = ALPHA_BLEND_128_W(OUT_N, px); } \
            } else { \
                /* Level 2 Edge Tracking Logic */ \
                if ((wd1 << 1) < wd2) { \
                    bool n_eq_PF_PB = df(lPF, lPB) >= 155; \
                    bool n_eq_PF_PC = df(lPF, lPC) >= 155; \
                    bool n_eq_PH_PD = df(lPH, lPD) >= 155; \
                    bool n_eq_PH_PG = df(lPH, lPG) >= 155; \
                    bool eq_PE_PG = df(lPE, lPG) < 155; \
                    bool eq_PE_PC = df(lPE, lPC) < 155; \
                    \
                    /* Forced condition register (CR) bitwise evaluation */ \
                    bool edge_mask = (n_eq_PF_PB & n_eq_PF_PC) | (n_eq_PH_PD & n_eq_PH_PG) | eq_PE_PG | eq_PE_PC; \
                    if (edge_mask) { \
                        uint32_t dFG = df(lPF, lPG); uint32_t dHC = df(lPH, lPC); \
                        bool irlv2u = (PE != PC) & (PB != PC); \
                        bool irlv2l = (PE != PG) & (PD != PG); \
                        bool condA = (dFG <= (dHC << 1)); \
                        bool condB = ((dFG << 1) >= dHC); \
                        \
                        if (irlv2l & irlv2u & condA & condB) { OUT_N = ALPHA_BLEND_224_W(OUT_N, px); } \
                        else if (irlv2l & condA) { OUT_N = ALPHA_BLEND_192_W(OUT_N, px); } \
                        else if (irlv2u & condB) { OUT_N = ALPHA_BLEND_192_W(OUT_N, px); } \
                        else { OUT_N = ALPHA_BLEND_128_W(OUT_N, px); } \
                    } \
                } else if (wd1 <= wd2) { OUT_N = ALPHA_BLEND_64_W(OUT_N, px); } \
            } \
        } \
    } while(0)

    EVAL_CORNER(E, I, H, F, G, C, D, B, A, lE, lI, lH, lF, lG, lC, lD, lB, lA, E3);
    EVAL_CORNER(E, C, F, B, I, A, H, D, G, lE, lC, lF, lB, lI, lA, lH, lD, lG, E1);
    EVAL_CORNER(E, A, B, D, C, G, F, H, I, lE, lA, lB, lD, lC, lG, lF, lH, lI, E0);
    EVAL_CORNER(E, G, D, H, A, I, B, F, C, lE, lG, lD, lH, lA, lI, lB, lF, lC, E2);
    #undef EVAL_CORNER

    // WGP-Optimized packing. Pack natively updated 16-bit subpixels directly into 32-bit payloads.
    w0 = (E0 << 16) | E1;
    w1 = (E2 << 16) | E3;
}

// -------------------------------------------------------------------------
// Templated Row Processor (Eliminates Code Duplication)
// -------------------------------------------------------------------------
template<bool IsLv1, bool YIsEven, typename Format>
static inline void Process2XBRRow(
    int width, int by,
    const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
    uint8_t *tile_ptr) __attribute__((always_inline));

template<bool IsLv1, bool YIsEven, typename Format>
static inline void Process2XBRRow(
    int width, int by,
    const typename Format::Type *rowTop, const typename Format::Type *rowMid, const typename Format::Type *rowBot,
    uint8_t *tile_ptr)
{
    int chunks = width >> 3;
    int tail_pairs = (width & 7) >> 1;
    int bx = 0;
    int cx = 0;

    // Seed sliding window natively. No Unpack565 overhead required.
    uint32_t A = Format::Read(&rowTop[0]), B = A, C;
    uint32_t D = Format::Read(&rowMid[0]), E = D, F;
    uint32_t G = Format::Read(&rowBot[0]), H = G, I;

    while (chunks--) {
        // Macro-Block Deduplication Frame Skip Check
        if (!render_mask[by + 1][bx + 1]) {
            cx += 8;
            tile_ptr += 128; // Advance destination pointer across skipped block

            // Re-seed anchors correctly at the new offset to prevent seam tearing
            int left = cx - 1;
            A = Format::Read(&rowTop[left]); D = Format::Read(&rowMid[left]); G = Format::Read(&rowBot[left]);
            B = Format::Read(&rowTop[cx]);   E = Format::Read(&rowMid[cx]);   H = Format::Read(&rowBot[cx]);
            bx++;
            continue;
        }

        DCBT(rowTop + cx + 16); DCBT(rowMid + cx + 16); DCBT(rowBot + cx + 16);

        // Process 2 pixels per iteration. This perfectly aligns the 32-byte stride
        // required by dcbz and the 4x4 swizzled texture map offsets.
        for (int i = 0; i < 4; i++) {
            if (YIsEven) {
                __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory");
            }

            uint32_t w0, w1;

            // Subpixel 1 (Top/Left Swizzle: offsets +0, +8)
            C = Format::Read(&rowTop[cx + 1]);
            F = Format::Read(&rowMid[cx + 1]);
            I = Format::Read(&rowBot[cx + 1]);
            Evaluate2XBRSubpixels<IsLv1>(A, B, C, D, E, F, G, H, I, w0, w1);
            *(uint32_t*)(tile_ptr + 0) = w0;
            *(uint32_t*)(tile_ptr + 8) = w1;

            A = B; B = C; D = E; E = F; G = H; H = I; cx++;

            // Subpixel 2 (Top/Right Swizzle: offsets +4, +12)
            C = Format::Read(&rowTop[cx + 1]);
            F = Format::Read(&rowMid[cx + 1]);
            I = Format::Read(&rowBot[cx + 1]);
            Evaluate2XBRSubpixels<IsLv1>(A, B, C, D, E, F, G, H, I, w0, w1);
            *(uint32_t*)(tile_ptr + 4) = w0;
            *(uint32_t*)(tile_ptr + 12) = w1;

            A = B; B = C; D = E; E = F; G = H; H = I; cx++;

            tile_ptr += 32;
        }
        bx++;
    }

    // Process remainder
    while (tail_pairs--) {
        if (YIsEven) {
            __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory");
        }

        uint32_t w0, w1;

        C = Format::Read(&rowTop[cx + 1]);
        F = Format::Read(&rowMid[cx + 1]);
        I = Format::Read(&rowBot[cx + 1]);
        Evaluate2XBRSubpixels<IsLv1>(A, B, C, D, E, F, G, H, I, w0, w1);
        *(uint32_t*)(tile_ptr + 0) = w0; *(uint32_t*)(tile_ptr + 8) = w1;
        A = B; B = C; D = E; E = F; G = H; H = I; cx++;

        C = Format::Read(&rowTop[cx + 1]);
        F = Format::Read(&rowMid[cx + 1]);
        I = Format::Read(&rowBot[cx + 1]);
        Evaluate2XBRSubpixels<IsLv1>(A, B, C, D, E, F, G, H, I, w0, w1);
        *(uint32_t*)(tile_ptr + 4) = w0; *(uint32_t*)(tile_ptr + 12) = w1;
        A = B; B = C; D = E; E = F; G = H; H = I; cx++;

        tile_ptr += 32;
    }
}

// -------------------------------------------------------------------------
// Drop-in Replacement Functions
// -------------------------------------------------------------------------
template<int GuiScale>
void Render2xBR (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    if(!isValidDimensions(width, height)) return;

    ComputeBlockDifferences(srcPtr, srcPitch, width, height);

    const uint32_t src1line = srcPitch / sizeof(typename ActiveSrcFormat::Type);
    uint32_t tile_row_pitch = width * 16;
    typename ActiveSrcFormat::Type *srcBase = (typename ActiveSrcFormat::Type *)srcPtr;

    for (int y = 0; y < height; y++) {
        int by = y >> 3;
        const typename ActiveSrcFormat::Type *rowMid = srcBase + (uint32_t)y * src1line;
        const typename ActiveSrcFormat::Type *rowTop = (y > 0) ? rowMid - src1line : rowMid;
        const typename ActiveSrcFormat::Type *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

        bool y_is_even = ((y & 1) == 0);
        uint8_t *tile_ptr = dstPtr + (y >> 1) * tile_row_pitch + (y_is_even ? 0 : 16);

        if (y_is_even) {
            Process2XBRRow<false, true, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
        } else {
            Process2XBRRow<false, false, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
        }
    }
}

template<int GuiScale>
void Render2xBRlv1 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    if(!isValidDimensions(width, height)) return;

    ComputeBlockDifferences(srcPtr, srcPitch, width, height);

    const uint32_t src1line = srcPitch / sizeof(typename ActiveSrcFormat::Type);
    uint32_t tile_row_pitch = width * 16;
    typename ActiveSrcFormat::Type *srcBase = (typename ActiveSrcFormat::Type *)srcPtr;

    for (int y = 0; y < height; y++) {
        int by = y >> 3;
        const typename ActiveSrcFormat::Type *rowMid = srcBase + (uint32_t)y * src1line;
        const typename ActiveSrcFormat::Type *rowTop = (y > 0) ? rowMid - src1line : rowMid;
        const typename ActiveSrcFormat::Type *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

        bool y_is_even = ((y & 1) == 0);
        uint8_t *tile_ptr = dstPtr + (y >> 1) * tile_row_pitch + (y_is_even ? 0 : 16);

        if (y_is_even) {
            Process2XBRRow<true, true, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
        } else {
            Process2XBRRow<true, false, ActiveSrcFormat>(width, by, rowTop, rowMid, rowBot, tile_ptr);
        }
    }
}

// -------------------------------------------------------------------------
// RenderDDT
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// BRANCHLESS SWAR EVALUATOR: DDT
// -------------------------------------------------------------------------

static inline uint32_t ddt_fast_luma(uint16_t c) __attribute__((always_inline));
static inline uint32_t ddt_fast_luma(uint16_t c) {
	uint32_t r5 = (c >> 11) & 0x1F;
	uint32_t g6 = (c >>  5) & 0x3F;
	uint32_t b5 =  c        & 0x1F;

	// 5/6-to-8 bit exact hardware replication
	uint32_t r8 = (r5 << 3) | (r5 >> 2);
	uint32_t g8 = (g6 << 2) | (g6 >> 4);
	uint32_t b8 = (b5 << 3) | (b5 >> 2);

	// 17*R, 28*G, 8*B - floor(B/2) using pure single-cycle ALU shifts
	return ((r8 << 4) + r8) + ((g8 << 5) - (g8 << 2)) + ((b8 << 3) - (b8 >> 1));
}

template<typename Format>
static inline void EvaluateDDTSubpixels(
	uint32_t E, uint32_t F, uint32_t H, uint32_t I,
	uint32_t &w0, uint32_t &w1) __attribute__((always_inline));

template<typename Format>
static inline void EvaluateDDTSubpixels(
	uint32_t E, uint32_t F, uint32_t H, uint32_t I,
	uint32_t &w0, uint32_t &w1)
{
	// Transitive Equality Early Out
	if (__builtin_expect((E == F) & (E == H) & (E == I), 1)) {
		w0 = (E << 16) | E;
		w1 = w0;
		return;
	}

	// Calculate exact ALU Luma
	uint32_t lE = ddt_fast_luma(E);
	uint32_t lH = ddt_fast_luma(H);
	uint32_t lF = ddt_fast_luma(F);
	uint32_t lI = ddt_fast_luma(I);

	// Branchlessly compute strict unsigned absolute difference
	uint32_t wd1 = (lH > lF) ? (lH - lF) : (lF - lH);
	uint32_t wd2 = (lE > lI) ? (lE - lI) : (lI - lE);

	// Native 16-bit RGB565 SWAR blending (prevents cross-channel color bleed)
	// 0xF7DE masks out the LSB of R, G, and B prior to shifting.
	uint32_t out00 = E;
	uint32_t out01 = ((E & 0xF7DE) >> 1) + ((F & 0xF7DE) >> 1);
	uint32_t out10 = ((E & 0xF7DE) >> 1) + ((H & 0xF7DE) >> 1);

	// Branchless route selector masks
	uint32_t is_gt = -(uint32_t)(wd1 > wd2);
	uint32_t is_lt = -(uint32_t)(wd1 < wd2);
	uint32_t is_eq = ~(is_gt | is_lt);

	uint32_t bEI = ((E & 0xF7DE) >> 1) + ((I & 0xF7DE) >> 1);
	uint32_t bFH = ((F & 0xF7DE) >> 1) + ((H & 0xF7DE) >> 1);

	// Equal weight blend mathematically requires sequential truncated addition
	uint32_t aux = ((H & 0xF7DE) >> 1) + ((I & 0xF7DE) >> 1);
	uint32_t bEQ = ((out01 & 0xF7DE) >> 1) + ((aux & 0xF7DE) >> 1);

	// Route the correct subpixel blend
	uint32_t out11 = (bEI & is_gt) | (bFH & is_lt) | (bEQ & is_eq);

	w0 = (out00 << 16) | out01;
	w1 = (out10 << 16) | out11;
}

// -------------------------------------------------------------------------
// ROW PROCESSOR: DDT
// -------------------------------------------------------------------------
template<bool YIsEven, typename Format>
static inline void ProcessDDTRow(
	int width, int by,
	const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	uint8_t *tile_ptr) __attribute__((always_inline));

template<bool YIsEven, typename Format>
static inline void ProcessDDTRow(
	int width, int by,
	const typename Format::Type *rowMid, const typename Format::Type *rowBot,
	uint8_t *tile_ptr)
{
	int chunks = width >> 3;
	int tail_pairs = (width & 7) >> 1;
	int bx = 0;
	int cx = 0;

	uint32_t E = Format::Read(&rowMid[0]), F;
	uint32_t H = Format::Read(&rowBot[0]), I;

	while (chunks--) {
		if (!render_mask[by + 1][bx + 1]) {
			cx += 8;
			tile_ptr += 128;

			E = Format::Read(&rowMid[cx]);
			H = Format::Read(&rowBot[cx]);
			bx++;
			continue;
		}

		DCBT(rowMid + cx + 16); DCBT(rowBot + cx + 16);

		for (int j = 0; j < 4; j++) {
			if (YIsEven) { __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory"); }

			uint32_t w0, w1;

			F = Format::Read(&rowMid[cx + 1]);
			I = Format::Read(&rowBot[cx + 1]);
			EvaluateDDTSubpixels<Format>(E, F, H, I, w0, w1);
			*(uint32_t*)(tile_ptr + 0) = w0;
			*(uint32_t*)(tile_ptr + 8) = w1;

			E = F; H = I; cx++;

			F = Format::Read(&rowMid[cx + 1]);
			I = Format::Read(&rowBot[cx + 1]);
			EvaluateDDTSubpixels<Format>(E, F, H, I, w0, w1);
			*(uint32_t*)(tile_ptr + 4) = w0;
			*(uint32_t*)(tile_ptr + 12) = w1;

			E = F; H = I; cx++;
			tile_ptr += 32;
		}
		bx++;
	}

	while (tail_pairs--) {
		if (YIsEven) { __asm__ volatile ("dcbz 0, %0" :: "b" (tile_ptr) : "memory"); }

		uint32_t w0, w1;

		F = Format::Read(&rowMid[cx + 1]);
		I = Format::Read(&rowBot[cx + 1]);
		EvaluateDDTSubpixels<Format>(E, F, H, I, w0, w1);
		*(uint32_t*)(tile_ptr + 0) = w0; *(uint32_t*)(tile_ptr + 8) = w1;
		E = F; H = I; cx++;

		F = Format::Read(&rowMid[cx + 1]);
		I = Format::Read(&rowBot[cx + 1]);
		EvaluateDDTSubpixels<Format>(E, F, H, I, w0, w1);
		*(uint32_t*)(tile_ptr + 4) = w0; *(uint32_t*)(tile_ptr + 12) = w1;
		E = F; H = I; cx++;

		tile_ptr += 32;
	}
}

template<int GuiScale>
void RenderDDT (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
	if(!isValidDimensions(width, height)) return;

	ComputeBlockDifferences(srcPtr, srcPitch, width, height);

	const uint32_t src1line = srcPitch / sizeof(typename ActiveSrcFormat::Type);
	uint32_t tile_row_pitch = width * 16;
	typename ActiveSrcFormat::Type *srcBase = (typename ActiveSrcFormat::Type *)srcPtr;

	for (int y = 0; y < height; ++y) {
		int by = y >> 3;
		const typename ActiveSrcFormat::Type *rowMid = srcBase + (uint32_t)y * src1line;
		const typename ActiveSrcFormat::Type *rowBot = (y + 1 < height) ? rowMid + src1line : rowMid;

		bool y_is_even = ((y & 1) == 0);
		uint8_t *tile_ptr = dstPtr + (y >> 1) * tile_row_pitch + (y_is_even ? 0 : 16);

		if (y_is_even) {
			ProcessDDTRow<true, ActiveSrcFormat>(width, by, rowMid, rowBot, tile_ptr);
		} else {
			ProcessDDTRow<false, ActiveSrcFormat>(width, by, rowMid, rowBot, tile_ptr);
		}
	}
}
