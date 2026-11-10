/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTextRenderer.cpp
 ***************************************************************************/

#include "GuiTextRenderer.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include FT_MODULE_H
#include FT_SYSTEM_H

GuiTextRenderer* fontSystem;

GlyphData GuiTextRenderer::missingGlyph;
uint32_t GuiTextRenderer::metricsGeneration = 0;

// FT_Init_FreeType() registers every font driver and renderer FreeType was
// built with, and because those are reached through function-pointer tables the
// linker has to keep all of them (about 600 KB) even though only one font is
// ever loaded. Register just the modules that font needs instead, using the
// same module classes FreeType's own ftinit.c does. They are declared as
// opaque data because their real types live in FreeType's internal headers;
// all that is needed is their address.
//
// This is enough for a TrueType font that has hinting instructions and a
// Unicode cmap, which is what the bundled font is. A font without instructions
// would also want autofit_module_class (otherwise it renders unhinted), and one
// without a Unicode cmap would want psnames_module_class.
extern "C" {
	extern const unsigned char tt_driver_class[];
	extern const unsigned char sfnt_module_class[];
	extern const unsigned char ft_smooth_renderer_class[];
}

namespace {
	void* ftAlloc(FT_Memory, long size) { return std::malloc(size); }
	void ftFree(FT_Memory, void* block) { std::free(block); }
	void* ftRealloc(FT_Memory, long, long newSize, void* block) { return std::realloc(block, newSize); }

	// Must outlive the library, so it is static
	FT_MemoryRec_ ftMemory = { nullptr, ftAlloc, ftFree, ftRealloc };

	FT_Library createFreeTypeLibrary()
	{
		FT_Library library = nullptr;
		if (FT_New_Library(&ftMemory, &library) != 0)
			return nullptr;

		if (FT_Add_Module(library, reinterpret_cast<const FT_Module_Class*>(tt_driver_class)) != 0 ||
		    FT_Add_Module(library, reinterpret_cast<const FT_Module_Class*>(sfnt_module_class)) != 0 ||
		    FT_Add_Module(library, reinterpret_cast<const FT_Module_Class*>(ft_smooth_renderer_class)) != 0)
		{
			FT_Done_Library(library);
			return nullptr;
		}
		return library;
	}

	// Converts an already->>6-shifted FreeType value (in physical/rasterized
	// pixels) back down to the design-pixel units
	inline int16_t ToDesignPixels(int v, float uiScale)
	{
		return uiScale == 1.0f ? (int16_t)v : (int16_t)std::lround(v / uiScale);
	}
}

GuiTextRenderer::GuiTextRenderer(const uint8_t* fontBuffer, FT_Long bufferSize, GlyphRenderer* glyphRenderer, float uiScale_)
    : currentPixelSize(0), renderer(glyphRenderer), current(nullptr), uiScale(uiScale_)
{
	++metricsGeneration;
	ftLibrary = createFreeTypeLibrary();
	FT_New_Memory_Face(ftLibrary, (FT_Byte*)fontBuffer, bufferSize, 0, &ftFace);
	ftKerningEnabled = ftFace && FT_HAS_KERNING(ftFace);
}

GuiTextRenderer::~GuiTextRenderer() {
	unloadFont();
	if (ftFace) FT_Done_Face(ftFace);
	if (ftLibrary) FT_Done_Library(ftLibrary);
}

void GuiTextRenderer::unloadFont() {
	for (auto& sizePair : fontData) {
		for (auto& charPair : sizePair.second.charMap) {
			if (charPair.second.texture) {
				renderer->destroyTexture(charPair.second.texture); // Delegate to injected renderer
				charPair.second.texture = nullptr;
			}
		}
	}
	fontData.clear();
	current = nullptr;
	++metricsGeneration;
}

void GuiTextRenderer::setPixelSize(int16_t pixelSize) {
	if (currentPixelSize != pixelSize) {
		currentPixelSize = pixelSize;
		current = nullptr;
		// currentPixelSize (and every size callers pass in/read back) stays
		// in design pixels - FreeType itself is asked to rasterize at
		// uiScale x that.
		FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::lround(currentPixelSize * uiScale));
	}
}

// --- Caching Engine ---

GlyphData* GuiTextRenderer::cacheGlyphData(wchar_t charCode, int16_t pixelSize) {
	ftData* data = &fontData[pixelSize];

	// Initialize metrics on first run for this size
	if (!data->metricsReady) {
		data->metricsReady = true;
		setPixelSize(pixelSize);
		data->align.ascender = ToDesignPixels(ftFace->size->metrics.ascender >> 6, uiScale);
		data->align.descender = ToDesignPixels(ftFace->size->metrics.descender >> 6, uiScale);
		data->align.max = 0;
		data->align.min = 0;
	}

	const bool inFastRange = (uint32_t)charCode < 256;

	auto it = data->charMap.find(charCode);
	if (it != data->charMap.end()) {
		if (inFastRange) data->fast[charCode] = &it->second;
		return &it->second; // Already cached
	}

	FT_UInt gIndex = FT_Get_Char_Index(ftFace, (FT_ULong)charCode);
	bool loaded = gIndex != 0 && (FT_Load_Glyph(ftFace, gIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER | FT_LOAD_NO_BITMAP) == 0 ||
	                              FT_Load_Glyph(ftFace, gIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER) == 0);
	if (loaded) {
		if (ftFace->glyph->format == FT_GLYPH_FORMAT_BITMAP && ftFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			FT_Bitmap* glyphBitmap = &ftFace->glyph->bitmap;

			GlyphData& charData = data->charMap[charCode];
			charData.renderOffsetX = ToDesignPixels(ftFace->glyph->bitmap_left, uiScale);
			charData.glyphAdvanceX = (uint16_t)ToDesignPixels(ftFace->glyph->advance.x >> 6, uiScale);
			charData.glyphAdvanceY = (uint16_t)ToDesignPixels(ftFace->glyph->advance.y >> 6, uiScale);
			charData.glyphIndex = (uint32_t)gIndex;

			// Physical (rasterized-at-uiScale) bitmap dims - what the
			// texture is actually created/loaded at.
			charData.textureWidth = glyphBitmap->width;
			charData.textureHeight = glyphBitmap->rows;
			// Design-pixel dims the quad is drawn at, so text lays out and
			// sizes identically to uiScale 1.0 while the texture is crisper.
			charData.drawWidth = (uint16_t)ToDesignPixels(glyphBitmap->width, uiScale);
			charData.drawHeight = (uint16_t)ToDesignPixels(glyphBitmap->rows, uiScale);

			charData.renderOffsetY = ToDesignPixels(ftFace->glyph->bitmap_top, uiScale);
			charData.renderOffsetMax = charData.renderOffsetY;
			charData.renderOffsetMin = ToDesignPixels((int)glyphBitmap->rows - ftFace->glyph->bitmap_top, uiScale);

			// Delegate Texture creation and data loading to the active backend
			charData.texture = renderer->createTexture(charData.textureWidth, charData.textureHeight);
			if (charData.texture) {
				renderer->loadTextureData(charData.texture, glyphBitmap);
			}

			if (inFastRange) data->fast[charCode] = &charData;
			return &charData;
		}
	}
	if (inFastRange) data->fast[charCode] = &missingGlyph;
	return nullptr;
}

GlyphData* GuiTextRenderer::glyph(wchar_t charCode) {
	if (!current) current = &fontData[currentPixelSize];

	if ((uint32_t)charCode < 256) {
		GlyphData* g = current->fast[charCode];
		if (g) return g == &missingGlyph ? nullptr : g;
	}
	return cacheGlyphData(charCode, currentPixelSize);
}

// --- Layout & Styling Engine ---

namespace {
	// Steps through the characters of a list of runs as if they were one string
	struct RunCursor {
		const GuiTextRun* runs;
		int count;
		int run = 0;
		uint32_t index = 0;

		RunCursor(const GuiTextRun* r, int n) : runs(r), count(r ? n : 0) {}

		bool next(wchar_t& ch) {
			while (run < count) {
				const GuiTextRun& r = runs[run];
				if (r.text && index < r.len && r.text[index]) {
					ch = r.text[index++];
					return true;
				}
				++run;
				index = 0;
			}
			return false;
		}
	};

	inline uint16_t ClampWidth(int w) {
		return w < 0 ? 0 : (w > 0xFFFF ? 0xFFFF : (uint16_t)w);
	}
}

int16_t GuiTextRenderer::getKerning(const GlyphData* prev, const GlyphData* glyph) {
	if (!ftKerningEnabled || !prev || !glyph) return 0;
	FT_Vector pairDelta;
	FT_Get_Kerning(ftFace, prev->glyphIndex, glyph->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
	return ToDesignPixels(pairDelta.x >> 6, uiScale);
}

int16_t GuiTextRenderer::getStyleOffsetWidth(uint16_t width, uint32_t format) {
	if (format & GUI_TEXT_JUSTIFY_LEFT) return 0;
	else if (format & GUI_TEXT_JUSTIFY_CENTER) return -(width >> 1);
	else if (format & GUI_TEXT_JUSTIFY_RIGHT) return -width;
	return 0;
}

int16_t GuiTextRenderer::getStyleOffsetHeight(FontOffset* offset, uint32_t format) {
	switch (format & GUI_TEXT_ALIGN_MASK) {
		case GUI_TEXT_ALIGN_TOP: return offset->ascender;
		case GUI_TEXT_ALIGN_BOTTOM: return offset->descender;
		case GUI_TEXT_ALIGN_BASELINE: return 0;
		case GUI_TEXT_ALIGN_GLYPH_TOP: return offset->max;
		case GUI_TEXT_ALIGN_GLYPH_MIDDLE: return (offset->max + offset->min + 1) >> 1;
		case GUI_TEXT_ALIGN_GLYPH_BOTTOM: return offset->min;
		case GUI_TEXT_ALIGN_MIDDLE:
		default: return (offset->ascender + offset->descender + 1) >> 1;
	}
}

void GuiTextRenderer::getOffset(const GuiTextRun* runs, int runCount, FontOffset* offset) {
	int16_t strMax = 0, strMin = 9999;

	RunCursor cursor(runs, runCount);
	wchar_t ch;
	while (cursor.next(ch)) {
		GlyphData* glyphData = glyph(ch);
		if (glyphData) {
			strMax = glyphData->renderOffsetMax > strMax ? glyphData->renderOffsetMax : strMax;
			strMin = glyphData->renderOffsetMin < strMin ? glyphData->renderOffsetMin : strMin;
		}
	}

	offset->ascender = ToDesignPixels(ftFace->size->metrics.ascender >> 6, uiScale);
	offset->descender = ToDesignPixels(ftFace->size->metrics.descender >> 6, uiScale);
	offset->max = strMax;
	offset->min = strMin;
}

// --- Metrics ---

uint16_t GuiTextRenderer::getWidth(const GuiTextRun* runs, int runCount) {
	int strWidth = 0;
	GlyphData* prev = nullptr;

	RunCursor cursor(runs, runCount);
	wchar_t ch;
	while (cursor.next(ch)) {
		GlyphData* glyphData = glyph(ch);
		if (glyphData) {
			strWidth += getKerning(prev, glyphData);
			strWidth += glyphData->glyphAdvanceX;
		}
		prev = glyphData;
	}
	return ClampWidth(strWidth);
}

int GuiTextRenderer::fitCount(const GuiTextRun* runs, int runCount, int maxWidth) {
	int strWidth = 0;
	GlyphData* prev = nullptr;
	int count = 0;
	int best = 0;

	RunCursor cursor(runs, runCount);
	wchar_t ch;
	while (cursor.next(ch)) {
		GlyphData* glyphData = glyph(ch);
		if (glyphData) {
			strWidth += getKerning(prev, glyphData);
			strWidth += glyphData->glyphAdvanceX;
		}
		prev = glyphData;
		++count;
		if (strWidth <= maxWidth) best = count;
	}
	return best;
}

uint16_t GuiTextRenderer::getLineHeight() {
	const int ascender = ToDesignPixels(ftFace->size->metrics.ascender >> 6, uiScale);
	const int descender = ToDesignPixels(ftFace->size->metrics.descender >> 6, uiScale);
	return ascender - descender; // the descender is negative
}

// --- Drawing Pipeline ---

void GuiTextRenderer::drawText(int16_t x, int16_t y, const GuiTextRun* runs, int runCount, PixelColor color, uint32_t renderFlags) {
	if (!runs || runCount <= 0) return;

	uint16_t x_pos = x;
	int16_t x_offset = 0, y_offset = 0;
	FontOffset offset;

	// Layout Alignment Calculations. Left-justified text needs no width, and only the glyph-based
	// vertical alignments need the extents of the text itself (the others use the font's own metrics),
	// so the text is measured only when the answer is used.
	if ((renderFlags & GUI_TEXT_JUSTIFY_MASK) && !(renderFlags & GUI_TEXT_JUSTIFY_LEFT)) {
		x_offset = getStyleOffsetWidth(getWidth(runs, runCount), renderFlags);
	}
	if (renderFlags & GUI_TEXT_ALIGN_MASK) {
		const uint32_t vertical = renderFlags & GUI_TEXT_ALIGN_MASK;
		if (vertical == GUI_TEXT_ALIGN_GLYPH_TOP || vertical == GUI_TEXT_ALIGN_GLYPH_MIDDLE || vertical == GUI_TEXT_ALIGN_GLYPH_BOTTOM) {
			getOffset(runs, runCount, &offset);
		} else {
			offset.ascender = ToDesignPixels(ftFace->size->metrics.ascender >> 6, uiScale);
			offset.descender = ToDesignPixels(ftFace->size->metrics.descender >> 6, uiScale);
			offset.max = 0;
			offset.min = 0;
		}
		y_offset = getStyleOffsetHeight(&offset, renderFlags);
	}

	GlyphData* prev = nullptr;
	renderer->beginText();
	RunCursor cursor(runs, runCount);
	wchar_t ch;
	while (cursor.next(ch)) {
		GlyphData* glyphData = glyph(ch);

		if (glyphData) {
			x_pos += getKerning(prev, glyphData);

			// Draw current glyph via generic renderer
			int16_t screenX = x_pos + glyphData->renderOffsetX + x_offset;
			int16_t screenY = y - glyphData->renderOffsetY + y_offset;
			renderer->drawQuad(glyphData->texture, screenX, screenY, glyphData->drawWidth, glyphData->drawHeight, color);

			x_pos += glyphData->glyphAdvanceX;
		}
		prev = glyphData;
	}
	renderer->endText();

	// Process additional layout features (Underlines/Strikethrough)
	if (renderFlags & GUI_TEXT_STYLE_MASK) {
		getOffset(runs, runCount, &offset);
		drawTextFeature(x + x_offset, y + y_offset, getWidth(runs, runCount), &offset, renderFlags, color);
	}
}

void GuiTextRenderer::drawTextFeature(int16_t x, int16_t y, uint16_t width, FontOffset* offsetData, uint32_t format, const PixelColor& color) {
	uint16_t featureHeight = currentPixelSize >> 4 > 0 ? currentPixelSize >> 4 : 1;

	if (format & GUI_TEXT_STYLE_UNDERLINE) {
		renderer->drawFeature(x, y + 1, width, featureHeight, color);
	}
	if (format & GUI_TEXT_STYLE_STRIKE) {
		renderer->drawFeature(x, y - (offsetData->max >> 1), width, featureHeight, color);
	}
}

// --- UTF-8 ---

size_t GuiTextRenderer::utf8ToWide(const char* strChar, wchar_t* out, size_t outCap) {
	// how many characters may be written: all of them when only counting
	const size_t limit = out ? (outCap ? outCap - 1 : 0) : (size_t)-1;
	size_t count = 0;

	if (strChar) {
		const unsigned char* s = reinterpret_cast<const unsigned char*>(strChar);

		while (*s && count < limit) {
			uint32_t cp = *s;
			int extra = 0;
			uint32_t minCp = 0;
			uint32_t result = 0;
			size_t used = 1;

			if (cp < 0x80) {
				// ASCII
			}
			else if (cp >= 0xC2 && cp <= 0xDF) { extra = 1; cp &= 0x1F; minCp = 0x80; }
			else if (cp >= 0xE0 && cp <= 0xEF) { extra = 2; cp &= 0x0F; minCp = 0x800; }
			else if (cp >= 0xF0 && cp <= 0xF4) { extra = 3; cp &= 0x07; minCp = 0x10000; }
			// else: stray continuation byte / invalid lead - handled below as Latin-1

			result = cp;
			if (extra) {
				int n = 0;
				uint32_t v = cp;
				while (n < extra && (s[1 + n] & 0xC0) == 0x80) {
					v = (v << 6) | (s[1 + n] & 0x3F);
					++n;
				}

				bool valid = (n == extra) && v >= minCp && v <= 0x10FFFF && !(v >= 0xD800 && v <= 0xDFFF);
				if (valid && v > (uint32_t)WCHAR_MAX) valid = false; // can't represent in a 16-bit wchar_t
				if (valid) {
					result = v;
					used = 1 + extra;
				} else {
					result = *s; // invalid sequence: keep the lead byte as Latin-1
				}
			}

			if (out) out[count] = (wchar_t)result;
			++count;
			s += used;
		}
	}

	if (out && outCap) out[count] = L'\0';
	return count;
}

// FreeType's sfnt module uses brotli decoder (~175 KB). WOFF2 support is not needed - so we override it here
// The signature and the return value come from brotli/decode.h
// (BROTLI_DECODER_RESULT_ERROR is 0)
extern "C" int BrotliDecoderDecompress(size_t encodedSize, const uint8_t* encodedBuffer, size_t* decodedSize, uint8_t* decodedBuffer)
{
	(void)encodedSize;
	(void)encodedBuffer;
	(void)decodedSize;
	(void)decodedBuffer;
	return 0;
}
