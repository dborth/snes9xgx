/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTextRenderer.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

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

//!Cached per-glyph metrics and rasterized texture, keyed by (pixel size, char code) in GuiTextRenderer::fontData.
//!\ingroup grp_core
struct GlyphData {
	int16_t renderOffsetX = 0; //!< Horizontal offset from the pen position to the bitmap's left edge
	uint16_t glyphAdvanceX = 0; //!< Horizontal pen advance after this glyph
	uint16_t glyphAdvanceY = 0; //!< Vertical pen advance after this glyph
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
private:
	FT_Library ftLibrary = nullptr;
	FT_Face ftFace = nullptr;
	int16_t currentPixelSize = 0;
	bool ftKerningEnabled = false;

	GlyphRenderer* renderer;

	struct ftData {
		FontOffset align;
		bool metricsReady;
		std::map<wchar_t, GlyphData> charMap;

		// Direct index for the first 256 characters, so the common lookup is one array read rather than
		// two std::map searches. null: not looked up yet; &missingGlyph: the font has no such glyph.
		GlyphData* fast[256];

		ftData() : metricsReady(false) {
			align.ascender = align.descender = align.max = align.min = 0;
			for (int i = 0; i < 256; ++i) fast[i] = nullptr;
		}
	};

	std::map<int16_t, ftData> fontData;
	ftData* current;	// fontData[currentPixelSize], or null until it is needed (map nodes never move)
	std::vector<uint16_t> widthScratch;	// scrollWindow()'s running widths, kept so it doesn't allocate
	std::vector<wchar_t> lineScratch;
	static GlyphData missingGlyph;
	static uint32_t metricsGeneration;

	// Internal Calculations
	int16_t getStyleOffsetWidth(uint16_t width, uint32_t format);
	int16_t getStyleOffsetHeight(FontOffset* offset, uint32_t format);
	void drawTextFeature(int16_t x, int16_t y, uint16_t width, FontOffset* offsetData, uint32_t format, const PixelColor& color);

	// Font Management
	void unloadFont();
	GlyphData* cacheGlyphData(wchar_t charCode, int16_t pixelSize);
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

	//!Selects the pixel size subsequent drawText()/getWidth()/getHeight()
	//!calls use. Shaped glyphs are cached per size, so switching sizes
	//!repeatedly doesn't re-shape glyphs already seen at that size.
	void setPixelSize(int16_t pixelSize);

	//! Destroys every cached glyph texture. Glyphs are re-rendered when next used.
	void clearGlyphCache() { unloadFont(); }

	// Core Drawing Signatures
	//!Draws text at (x, y) using the current pixel size.
	//!\param x Left edge, in pixels
	//!\param y Top edge, in pixels
	//!\param text Text to draw
	//!\param color Text color
	//!\param renderFlags Bitmask of GUI_TEXT_JUSTIFY_*/GUI_TEXT_ALIGN_*/GUI_TEXT_STYLE_* flags
	//!\return the drawn text's width in pixels
	uint16_t drawText(int16_t x, int16_t y, const wchar_t* text, PixelColor color = black, uint32_t renderFlags = 0);
	//!\overload
	uint16_t drawText(int16_t x, int16_t y, const char* text, PixelColor color = black, uint32_t renderFlags = 0);

	// Dimensions & Offsets
	//!\return text width in pixels at the current pixel size
	uint16_t getWidth(const wchar_t* text);

	//! How many leading characters of `text` fit in `maxWidth` (the largest n for which the first n
	//! characters measure no wider than that), found in one pass. Same answer as trimming the string
	//! one character at a time and calling getWidth() each time, without the repeated measuring.
	int fitCount(const wchar_t* text, int maxWidth);

	//! The part of a horizontally scrolling line that is shown at one scroll position: the text from
	//! `pos` on, two spaces, then the start of the text again, cut to fit `maxWidth`. Measures the line
	//! once rather than after every character added or removed, with the same result as doing that.
	//! \param textlen Length of `text`
	//! \param out Room for textlen + 1 characters
	void scrollWindow(const wchar_t* text, uint32_t textlen, uint32_t pos, int maxWidth, wchar_t* out);

	//! Changes whenever a measurement made earlier could now come out differently (a renderer was
	//! made or its glyphs were thrown away). Anything that keeps text sizes can compare it.
	static uint32_t getMetricsGeneration() { return metricsGeneration; }
	//!\overload
	uint16_t getWidth(const char* text);
	//!\return text height in pixels at the current pixel size
	uint16_t getHeight(const wchar_t* text);
	//!The height of a line of text at the current pixel size: from the top of the ascender to the bottom of the
	//!descender, the same for every string.
	uint16_t getLineHeight();
	//!\overload
	uint16_t getHeight(const char* text);

	//!\param text Text to measure
	//!\param offset Filled with the ascender/descender/max/min metrics for text at the current pixel size
	void getOffset(const wchar_t* text, FontOffset* offset);

	// Utilities
	//!\return a newly-allocated wide-char copy of a UTF-8 string p - caller owns the result (delete[])
	static wchar_t* charToWideChar(const char* p);
};

extern GuiTextRenderer *fontSystem;
