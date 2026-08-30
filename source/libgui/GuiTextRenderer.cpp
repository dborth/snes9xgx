/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextRenderer.cpp
 ***************************************************************************/

#include "GuiTextRenderer.h"
#include <cstdlib>
#include <cstring>

GuiTextRenderer* fontSystem;

GuiTextRenderer::GuiTextRenderer(const uint8_t* fontBuffer, FT_Long bufferSize, GlyphRenderer* glyphRenderer)
    : currentPixelSize(0), renderer(glyphRenderer)
{
	FT_Init_FreeType(&ftLibrary);
	FT_New_Memory_Face(ftLibrary, (FT_Byte*)fontBuffer, bufferSize, 0, &ftFace);
	ftKerningEnabled = FT_HAS_KERNING(ftFace);
}

GuiTextRenderer::~GuiTextRenderer() {
	unloadFont();
	if (ftFace) FT_Done_Face(ftFace);
	if (ftLibrary) FT_Done_FreeType(ftLibrary);
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
}

void GuiTextRenderer::setPixelSize(int16_t pixelSize) {
	if (currentPixelSize != pixelSize) {
		currentPixelSize = pixelSize;
		FT_Set_Pixel_Sizes(ftFace, 0, currentPixelSize);
	}
}

// --- Caching Engine ---

GlyphData* GuiTextRenderer::cacheGlyphData(wchar_t charCode, int16_t pixelSize) {
	ftData* data = &fontData[pixelSize];

	// Initialize metrics on first run for this size
	if (data->charMap.empty()) {
		setPixelSize(pixelSize);
		data->align.ascender = (int16_t)(ftFace->size->metrics.ascender >> 6);
		data->align.descender = (int16_t)(ftFace->size->metrics.descender >> 6);
		data->align.max = 0;
		data->align.min = 0;
	}

	auto it = data->charMap.find(charCode);
	if (it != data->charMap.end()) {
		return &it->second; // Already cached
	}

	FT_UInt gIndex = FT_Get_Char_Index(ftFace, (FT_ULong)charCode);
	if (gIndex != 0 && FT_Load_Glyph(ftFace, gIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER) == 0) {
		if (ftFace->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
			FT_Bitmap* glyphBitmap = &ftFace->glyph->bitmap;

			GlyphData& charData = data->charMap[charCode];
			charData.renderOffsetX = (int16_t)ftFace->glyph->bitmap_left;
			charData.glyphAdvanceX = (uint16_t)(ftFace->glyph->advance.x >> 6);
			charData.glyphAdvanceY = (uint16_t)(ftFace->glyph->advance.y >> 6);
			charData.glyphIndex = (uint32_t)gIndex;

			charData.textureWidth = glyphBitmap->width;
			charData.textureHeight = glyphBitmap->rows;
			charData.renderOffsetY = (int16_t)ftFace->glyph->bitmap_top;
			charData.renderOffsetMax = (int16_t)ftFace->glyph->bitmap_top;
			charData.renderOffsetMin = (int16_t)glyphBitmap->rows - ftFace->glyph->bitmap_top;

			// Delegate Texture creation and data loading to the active backend
			charData.texture = renderer->createTexture(charData.textureWidth, charData.textureHeight);
			if (charData.texture) {
				renderer->loadTextureData(charData.texture, glyphBitmap);
			}

			return &charData;
		}
	}
	return nullptr;
}

// --- Layout & Styling Engine ---

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

void GuiTextRenderer::getOffset(const wchar_t* text, FontOffset* offset) {
	int16_t strMax = 0, strMin = 9999;

	int i = 0;
	while (text[i]) {
		GlyphData* glyphData = cacheGlyphData(text[i], currentPixelSize);
		if (glyphData) {
			strMax = glyphData->renderOffsetMax > strMax ? glyphData->renderOffsetMax : strMax;
			strMin = glyphData->renderOffsetMin < strMin ? glyphData->renderOffsetMin : strMin;
		}
		++i;
	}

	offset->ascender = (int16_t)(ftFace->size->metrics.ascender >> 6);
	offset->descender = (int16_t)(ftFace->size->metrics.descender >> 6);
	offset->max = strMax;
	offset->min = strMin;
}

// --- Metrics ---

uint16_t GuiTextRenderer::getWidth(const wchar_t* text) {
	uint16_t strWidth = 0;
	FT_Vector pairDelta;

	int i = 0;
	while (text[i]) {
		GlyphData* glyphData = cacheGlyphData(text[i], currentPixelSize);
		if (glyphData) {
			if (ftKerningEnabled && i > 0) {
				FT_Get_Kerning(ftFace, fontData[currentPixelSize].charMap[text[i - 1]].glyphIndex, glyphData->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
				strWidth += pairDelta.x >> 6;
			}
			strWidth += glyphData->glyphAdvanceX;
		}
		++i;
	}
	return strWidth;
}

uint16_t GuiTextRenderer::getHeight(const wchar_t* text) {
	FontOffset offset;
	getOffset(text, &offset);
	return offset.max - offset.min;
}

// --- Drawing Pipeline ---

uint16_t GuiTextRenderer::drawText(int16_t x, int16_t y, const wchar_t* text, PixelColor color, uint32_t renderFlags) {
	if (!text) return 0;

	uint16_t x_pos = x, printed = 0;
	int16_t x_offset = 0, y_offset = 0;
	FT_Vector pairDelta;
	FontOffset offset;

	// Layout Alignment Calculations
	if (renderFlags & GUI_TEXT_JUSTIFY_MASK) {
		x_offset = getStyleOffsetWidth(getWidth(text), renderFlags);
	}
	if (renderFlags & GUI_TEXT_ALIGN_MASK) {
		getOffset(text, &offset);
		y_offset = getStyleOffsetHeight(&offset, renderFlags);
	}

	int i = 0;
	while (text[i]) {
		GlyphData* glyphData = cacheGlyphData(text[i], currentPixelSize);

		if (glyphData) {
			// Kerning adjustments
			if (ftKerningEnabled && i > 0) {
				FT_Get_Kerning(ftFace, fontData[currentPixelSize].charMap[text[i - 1]].glyphIndex, glyphData->glyphIndex, FT_KERNING_DEFAULT, &pairDelta);
				x_pos += pairDelta.x >> 6;
			}

			// Draw current glyph via generic renderer
			int16_t screenX = x_pos + glyphData->renderOffsetX + x_offset;
			int16_t screenY = y - glyphData->renderOffsetY + y_offset;
			renderer->drawQuad(glyphData->texture, screenX, screenY, glyphData->textureWidth, glyphData->textureHeight, color);

			x_pos += glyphData->glyphAdvanceX;
			++printed;
		}
		++i;
	}

	// Process additional layout features (Underlines/Strikethrough)
	if (renderFlags & GUI_TEXT_STYLE_MASK) {
		getOffset(text, &offset);
		drawTextFeature(x + x_offset, y + y_offset, getWidth(text), &offset, renderFlags, color);
	}

	return printed;
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

// --- UTF-8 Wrapper Overloads ---

wchar_t* GuiTextRenderer::charToWideChar(const char* strChar) {
	if (!strChar) return nullptr;
	wchar_t* strWChar = new wchar_t[strlen(strChar) + 1];
	int bt = mbstowcs(strWChar, strChar, strlen(strChar));
	if (bt > 0) {
		strWChar[bt] = L'\0';
		return strWChar;
	}

	// Fallback
	wchar_t* tempDest = strWChar;
	while ((*tempDest++ = *strChar++));
	return strWChar;
}

uint16_t GuiTextRenderer::drawText(int16_t x, int16_t y, const char* text, PixelColor color, uint32_t renderFlags) {
	wchar_t* wText = charToWideChar(text);
	uint16_t result = drawText(x, y, wText, color, renderFlags);
	delete[] wText;
	return result;
}

uint16_t GuiTextRenderer::getWidth(const char* text) {
	wchar_t* wText = charToWideChar(text);
	uint16_t result = getWidth(wText);
	delete[] wText;
	return result;
}

uint16_t GuiTextRenderer::getHeight(const char* text) {
	wchar_t* wText = charToWideChar(text);
	uint16_t result = getHeight(wText);
	delete[] wText;
	return result;
}
