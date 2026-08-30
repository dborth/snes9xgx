#pragma once

#include <gccore.h>
#include "libgui/GuiTextRenderer.h"

class WiiGlyphRenderer : public GlyphRenderer {
private:
	uint8_t vertexIndex;

public:
	WiiGlyphRenderer(uint8_t vtxFmtIndex = GX_VTXFMT1);
	~WiiGlyphRenderer() override;

	// Interface implementations
	void* createTexture(uint16_t width, uint16_t height) override;
	void loadTextureData(void* texture, FT_Bitmap* bitmap) override;
	void destroyTexture(void* texture) override;

	void drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;
	void drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;

	void setVertexFormat(uint8_t vtxFmtIndex);
};
