/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTextRenderer.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H

//!A piece of text given by length rather than by a terminating NUL: `len` characters starting at `text`.
//!Text drawn or measured as a list of runs reads as one line, the runs joined end to end - so a wrapped
//!line, a cut-off line with "..." after it, or a scrolling window made of the end and the start of a text
//!can be handled in place, with no copy made of any of them. A NUL inside a run ends that run early.
//!\ingroup grp_core
struct GuiTextRun {
	const wchar_t* text; //!< First character of the run
	uint32_t len; //!< Number of characters in the run
};

#include "Gui.h"

// Legacy Text Styling Constants
#define GUI_TEXT_NULL               0x0000
#define GUI_TEXT_JUSTIFY_LEFT       0x0001
#define GUI_TEXT_JUSTIFY_CENTER     0x0002
#define GUI_TEXT_JUSTIFY_RIGHT      0x0004
#define GUI_TEXT_JUSTIFY_MASK       0x000f

#define GUI_TEXT_ALIGN_TOP          0x0010
#define GUI_TEXT_ALIGN_MIDDLE       0x0020
#define GUI_TEXT_ALIGN_BOTTOM       0x0040
#define GUI_TEXT_ALIGN_BASELINE     0x0080
#define GUI_TEXT_ALIGN_GLYPH_TOP    0x0100
#define GUI_TEXT_ALIGN_GLYPH_MIDDLE 0x0200
#define GUI_TEXT_ALIGN_GLYPH_BOTTOM 0x0400
#define GUI_TEXT_ALIGN_MASK         0x0ff0

#define GUI_TEXT_STYLE_UNDERLINE    0x1000
#define GUI_TEXT_STYLE_STRIKE       0x2000
#define GUI_TEXT_STYLE_MASK         0xf000

const PixelColor black = {0, 0, 0, 255};

//!Per-pixel-size font metrics used for text-block alignment/positioning.
//!\ingroup grp_core
struct FontOffset {
	int16_t ascender = 0; //!< Font ascender for the current size (distance above the baseline), in design pixels
	int16_t descender = 0; //!< Font descender for the current size (negative: distance below the baseline), in design pixels
	int16_t max = 0; //!< Height above the baseline of the tallest glyph in the measured string (see GuiTextRenderer::getOffset())
	int16_t min = 0; //!< Depth below the baseline of the lowest glyph in the measured string (see GuiTextRenderer::getOffset())
};

//!Cached per-glyph metrics and rasterized texture, kept per (pixel size, char code) by GuiTextRenderer.
//!\ingroup grp_core
struct GlyphData {
	int16_t renderOffsetX = 0; //!< Horizontal offset from the pen position to the bitmap's left edge
	uint16_t glyphAdvanceX = 0; //!< Horizontal pen advance after this glyph
	uint16_t glyphAdvanceY = 0; //!< Vertical pen advance after this glyph
	bool rendered = false; //!< The bitmap and texture below exist (or failed to): they are made on first draw, not on first measure
	uint32_t glyphIndex = 0; //!< FreeType glyph index

	//!Pixel dims of the rasterized bitmap/texture, at uiScale - always
	//!>= drawWidth/drawHeight, and equal to them when uiScale is 1.0.
	uint16_t textureWidth = 0;
	uint16_t textureHeight = 0; //!< Height counterpart of textureWidth

	//!Design-pixel dims to actually draw the quad at (textureWidth/Height
	//!divided back down by uiScale) - keeps text laid out and sized
	//!identically to uiScale 1.0 while the texture itself is crisper.
	uint16_t drawWidth = 0;
	uint16_t drawHeight = 0; //!< Height counterpart of drawWidth

	int16_t renderOffsetY = 0; //!< Height of the bitmap's top edge above the baseline
	int16_t renderOffsetMax = 0; //!< Height of the glyph bitmap above the baseline (feeds FontOffset::max)
	int16_t renderOffsetMin = 0; //!< Depth of the glyph bitmap below the baseline (feeds FontOffset::min)

	void* texture = nullptr; //!< Abstracted texture pointer
};

//!FreeType2-based glyph shaping/caching. Shapes and caches glyphs per
//!pixel size, then delegates only the final rasterized-quad draw to a
//!GlyphRenderer - GuiText calls through this rather than touching
//!FreeType or a platform texture directly.
//!\ingroup grp_core
class GuiTextRenderer {
#ifdef DIAG_BUILD
	friend struct DiagAccess; // source/diag/diag.cpp: read-only peek at the FT_Face (issue #1153 harness)
#endif
private:
	FT_Library ftLibrary = nullptr;
	FT_Face ftFace = nullptr;
	int16_t currentPixelSize = 0;
	bool ftKerningEnabled = false;

	GlyphRenderer* renderer;

	// The glyph cache. Each pixel size used has one SizeTable (one allocation). Its glyph records are kept
	// in chunks of GLYPHS_PER_CHUNK, so a glyph costs a share of a chunk rather than an allocation of its
	// own, and a record never moves once made (callers hold pointers to them while drawing). The first 256
	// characters are found through `fast` (which also remembers which ones the font lacks); above that a small
	// open-addressing table of the glyphs the font has, grown by doubling, is used.
	enum { GLYPHS_PER_CHUNK = 32 };
	struct GlyphChunk {
		GlyphChunk* next;
		alignas(GlyphData) unsigned char storage[GLYPHS_PER_CHUNK * sizeof(GlyphData)];
		GlyphData* at(int i) { return reinterpret_cast<GlyphData*>(storage) + i; }
	};
	struct WideEntry {
		wchar_t code;
		GlyphData* glyph; // null: this slot is empty
	};
	struct SizeTable {
		SizeTable* next;
		int16_t pixelSize;
		GlyphChunk* chunks; // newest first; only the first is partly used
		uint8_t chunkUsed; // records used in the first chunk
		WideEntry* wide; // null until a character above 255 is looked up; wideCap is a power of two
		uint16_t wideCap;
		uint16_t wideCount;
		// null: not looked up yet; &missingGlyph: the font has no such glyph.
		GlyphData* fast[256];
	};

	SizeTable* sizeTables = nullptr; // every pixel size used so far
	SizeTable* current = nullptr; // the table for currentPixelSize, or null until it is needed
	static GlyphData missingGlyph;
	static uint32_t metricsGeneration;

	SizeTable* findSize(int16_t pixelSize);
	void freeSize(SizeTable* table);
	GlyphData* find(SizeTable* table, wchar_t charCode);
	bool reserveWide(SizeTable* table);
	GlyphData* newGlyph(SizeTable* table);
	void store(SizeTable* table, wchar_t charCode, GlyphData* glyph);

	// Internal Calculations
	int16_t getKerning(const GlyphData* prev, const GlyphData* glyph);
	int16_t getStyleOffsetWidth(uint16_t width, uint32_t format);
	int16_t getStyleOffsetHeight(FontOffset* offset, uint32_t format);
	void drawTextFeature(int16_t x, int16_t y, uint16_t width, FontOffset* offsetData, uint32_t format, const PixelColor& color);

	// Font Management
	void unloadFont();
	// Looks a character up at the current size and keeps what measuring needs: its advance and
	// glyph index. No bitmap is rasterized and no texture made here (see renderGlyph()).
	GlyphData* cacheGlyphData(wchar_t charCode);
	// Rasterizes a glyph and makes its texture, once, at the current size. Needed to draw it and to know how
	// far it reaches above and below the baseline. A glyph that cannot be rendered keeps its advance and draws nothing.
	void renderGlyph(GlyphData* glyph);
	// The glyph for a character at the current size: the fast table first, cacheGlyphData() on a miss.
	// null if the font has none.
	GlyphData* glyph(wchar_t charCode);

	// Physical-pixels-per-design-pixel glyphs are rasterized at (see
	// VideoDriver::getUIScale()).
	float uiScale;

public:
	//!\param fontBuffer TTF/OTF font data - must remain valid for the
	//!lifetime of this GuiTextRenderer, FreeType keeps a pointer into it
	//!\param bufferSize Length of fontBuffer in bytes
	//!\param glyphRenderer Platform renderer rasterized glyph quads are drawn through
	//!\param uiScale Physical-pixels-per-design-pixel to rasterize glyphs at
	//!(pass VideoDriver::getUIScale()). Sizes, metrics and layout stay in
	//!design pixels; only the glyph textures get sharper. 1.0 rasterizes at
	//!the design size.
	GuiTextRenderer(const uint8_t* fontBuffer, FT_Long bufferSize, GlyphRenderer* glyphRenderer, float uiScale = 1.0f);
	~GuiTextRenderer();

	//!Selects the pixel size subsequent drawText()/getWidth()/getOffset()
	//!calls use. Shaped glyphs are cached per size, so switching sizes
	//!repeatedly doesn't re-shape glyphs already seen at that size.
	void setPixelSize(int16_t pixelSize);

	//! Destroys every cached glyph texture. Glyphs are re-rendered when next used.
	void clearGlyphCache() { unloadFont(); }

	// Core Drawing Signatures
	//!Draws text at (x, y) using the current pixel size.
	//!\param x Left edge, in pixels
	//!\param y Top edge, in pixels
	//!\param runs The text, as runs joined end to end
	//!\param runCount Number of runs
	//!\param color Text color
	//!\param renderFlags Bitmask of GUI_TEXT_JUSTIFY_*/GUI_TEXT_ALIGN_*/GUI_TEXT_STYLE_* flags
	void drawText(int16_t x, int16_t y, const GuiTextRun* runs, int runCount, PixelColor color = black, uint32_t renderFlags = 0);

	// Dimensions & Offsets
	//!\return text width in pixels at the current pixel size
	uint16_t getWidth(const GuiTextRun* runs, int runCount);

	//! How many leading characters of the text fit in `maxWidth` (the largest n for which the first n
	//! characters measure no wider than that), found in one pass. Same answer as trimming the text one
	//! character at a time and calling getWidth() each time, without the repeated measuring.
	int fitCount(const GuiTextRun* runs, int runCount, int maxWidth);

	//!The height of a line of text at the current pixel size: from the top of the ascender to the bottom of the
	//!descender, the same for every string.
	uint16_t getLineHeight();

	//!\param runs Text to measure
	//!\param runCount Number of runs
	//!\param offset Filled with the ascender/descender/max/min metrics for the text at the current pixel size
	void getOffset(const GuiTextRun* runs, int runCount, FontOffset* offset);

	//! Changes whenever a measurement made earlier could now come out differently (a renderer was
	//! made or its glyphs were thrown away). Anything that keeps text sizes or line breaks can compare it.
	static uint32_t getMetricsGeneration() { return metricsGeneration; }

	// Utilities
	//!Converts UTF-8 to wide characters without allocating, so it can be run once to size a buffer
	//!and again to fill it. A malformed sequence becomes its lead byte read as Latin-1.
	//!\param utf8 NUL-terminated UTF-8 (null is an empty string)
	//!\param out Where to write the result and a terminating NUL, or null to only count
	//!\param outCap Room in `out`, in characters, counting the NUL. The result is cut to fit.
	//!\return the number of characters converted, not counting the NUL (what `out` would need, less one, when out is null)
	static size_t utf8ToWide(const char* utf8, wchar_t* out, size_t outCap);
};

extern GuiTextRenderer *fontSystem;
