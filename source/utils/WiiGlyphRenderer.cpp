#include "WiiGlyphRenderer.h"
#include <malloc.h>
#include <string.h>

#define ALIGN8(x) (((x) + 7) & ~7)

GlyphRenderer* glyphRenderer;

WiiGlyphRenderer::WiiGlyphRenderer(uint8_t vtxFmtIndex)
{
	setVertexFormat(vtxFmtIndex);
}
WiiGlyphRenderer::~WiiGlyphRenderer() {

}

void WiiGlyphRenderer::setVertexFormat(uint8_t vtxFmtIndex) {
	this->vertexIndex = vtxFmtIndex;

	// Configure vertex attribute formats for immediate-mode drawing
	GX_SetVtxAttrFmt(this->vertexIndex, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(this->vertexIndex, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(this->vertexIndex, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
}

void* WiiGlyphRenderer::createTexture(uint16_t width, uint16_t height) {
	width = ALIGN8(width);
	if (width == 0) width = 8;

	height = ALIGN8(height);
	if (height == 0) height = 8;

	// GX_TF_I4 format uses 4 bits per pixel, halving the required footprint
	uint32_t glyphSize = (width * height) >> 1;

	void* texture = memalign(32, glyphSize);
	if (texture) {
		memset(texture, 0x00, glyphSize);
	}

	return texture;
}

void WiiGlyphRenderer::loadTextureData(void* texture, FT_Bitmap* bitmap) {
	if (!texture || !bitmap) return;

	uint16_t texWidth = ALIGN8(bitmap->width);
	if (texWidth == 0) texWidth = 8;

	uint16_t texHeight = ALIGN8(bitmap->rows);
	if (texHeight == 0) texHeight = 8;

	uint32_t glyphSize = (texWidth * texHeight) >> 1;

	uint8_t* dst = static_cast<uint8_t*>(texture);
	uint8_t* src = static_cast<uint8_t*>(bitmap->buffer);
	uint32_t pos, x1, y1, x, y;

	// 8x8 tiled block processing for GX_TF_I4
	for (y1 = 0; y1 < bitmap->rows; y1 += 8) {
		for (x1 = 0; x1 < bitmap->width; x1 += 8) {
			for (y = y1; y < (y1 + 8); y++) {
				for (x = x1; x < (x1 + 8); x += 2, dst++) {
					if (x >= bitmap->width || y >= bitmap->rows) {
						continue;
					}

					pos = y * bitmap->width + x;

					// Extract high bits from current and adjacent pixels
					*dst = (src[pos] & 0xF0);
					if (x + 1 < bitmap->width) {
						*dst |= (src[pos + 1] >> 4);
					}
				}
			}
		}
	}

	// Flush the CPU cache to ensure GPU reads valid data
	DCFlushRange(texture, glyphSize);
}

void WiiGlyphRenderer::destroyTexture(void* texture) {
	if (texture) {
		free(texture);
	}
}

void WiiGlyphRenderer::drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) {
	if (!texture) return;

	GXTexObj glyphTexture;

	// GX_CLAMP required to avoid wrapping artifacts on small font textures
	GX_InitTexObj(&glyphTexture, texture, width, height, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&glyphTexture, GX_TEXMAP0);
	GX_InvalidateTexAll();

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_Begin(GX_QUADS, this->vertexIndex, 4);

	// Top-Left
	GX_Position2s16(screenX, screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);

	// Top-Right
	GX_Position2s16(width + screenX, screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(1.0f, 0.0f);

	// Bottom-Right
	GX_Position2s16(width + screenX, height + screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(1.0f, 1.0f);

	// Bottom-Left
	GX_Position2s16(screenX, height + screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 1.0f);

	GX_End();

	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
}

void WiiGlyphRenderer::drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) {
	// Disable textures to draw flat colored quad
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

	GX_Begin(GX_QUADS, this->vertexIndex, 4);

	GX_Position2s16(screenX, screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);

	GX_Position2s16(width + screenX, screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);

	GX_Position2s16(width + screenX, height + screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);

	GX_Position2s16(screenX, height + screenY);
	GX_Color4u8(color.r, color.g, color.b, color.a);

	GX_End();

	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
}
