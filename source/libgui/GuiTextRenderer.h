/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextRenderer.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <map>
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

struct FontOffset {
	int16_t ascender;
	int16_t descender;
	int16_t max;
	int16_t min;
};

struct GlyphData {
	int16_t renderOffsetX;
	uint16_t glyphAdvanceX;
	uint16_t glyphAdvanceY;
	uint32_t glyphIndex;

	uint16_t textureWidth;
	uint16_t textureHeight;

	int16_t renderOffsetY;
	int16_t renderOffsetMax;
	int16_t renderOffsetMin;

	void* texture; // Abstracted texture pointer
};

class GuiTextRenderer {
private:
	FT_Library ftLibrary;
	FT_Face ftFace;
	int16_t currentPixelSize;
	bool ftKerningEnabled;

	GlyphRenderer* renderer;

	struct ftData {
		FontOffset align;
		std::map<wchar_t, GlyphData> charMap;
	};

	std::map<int16_t, ftData> fontData;

	// Internal Calculations
	int16_t getStyleOffsetWidth(uint16_t width, uint32_t format);
	int16_t getStyleOffsetHeight(FontOffset* offset, uint32_t format);
	void drawTextFeature(int16_t x, int16_t y, uint16_t width, FontOffset* offsetData, uint32_t format, const PixelColor& color);

	// Font Management
	void unloadFont();
	GlyphData* cacheGlyphData(wchar_t charCode, int16_t pixelSize);

public:
	GuiTextRenderer(const uint8_t* fontBuffer, FT_Long bufferSize, GlyphRenderer* glyphRenderer);
	~GuiTextRenderer();

	void setPixelSize(int16_t pixelSize);

	// Core Drawing Signatures
	uint16_t drawText(int16_t x, int16_t y, const wchar_t* text, PixelColor color = black, uint32_t renderFlags = 0);
	uint16_t drawText(int16_t x, int16_t y, const char* text, PixelColor color = black, uint32_t renderFlags = 0);

	// Dimensions & Offsets
	uint16_t getWidth(const wchar_t* text);
	uint16_t getWidth(const char* text);
	uint16_t getHeight(const wchar_t* text);
	uint16_t getHeight(const char* text);

	void getOffset(const wchar_t* text, FontOffset* offset);

	// Utilities
	static wchar_t* charToWideChar(const char* p);
};

extern GuiTextRenderer *fontSystem;
