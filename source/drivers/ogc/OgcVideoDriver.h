/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcVideoDriver.h
 ***************************************************************************/
#pragma once

#include <gccore.h>
#include "OgcEmulatorVideoDriver.h"
#include "../VideoDriver.h"

extern uint32_t FrameTimer;

class OgcVideoDriver : public VideoDriver
{
	public:
		OgcVideoDriver();
		~OgcVideoDriver() override;

		void init(int width, int height) override;
		void shutdown() override;
		void renderMenu() override;
		void startMenuVideo() override;
		void clearScreen(const PixelColor& color) override;

		int getScreenWidth() const override { return screenWidth; }
		int getScreenHeight() const override { return screenHeight; }
		uint32_t getFrameTimer() override { return FrameTimer; }

		ImageRenderer* getImageRenderer() override { return imageRenderer; }
		GlyphRenderer* getGlyphRenderer() override { return glyphRenderer; }
		OgcEmulatorVideoDriver* getEmulatorVideo() override { return emulatorVideoDriver; }

		GXRModeObj* findVideoMode();
		void setupVideoMode(GXRModeObj* mode);
		void waitForBufferReady();
		void presentBuffer();

	private:
		int screenWidth;
		int screenHeight;

		ImageRenderer* imageRenderer;
		GlyphRenderer* glyphRenderer;
		OgcEmulatorVideoDriver* emulatorVideoDriver;
};

class OgcImageRenderer : public ImageRenderer
{
	public:
		void * createTexture(int width, int height) override;
		void loadTextureData(void * texture, const uint8_t * rgba, int width, int height) override;
		void destroyTexture(void * texture) override;
		void drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha) override;
		void drawRectangle(float x, float y, float width, float height, PixelColor color) override;
};

class OgcGlyphRenderer : public GlyphRenderer {
	private:
		uint8_t vertexIndex;

	public:
		OgcGlyphRenderer(uint8_t vtxFmtIndex = GX_VTXFMT1);
		~OgcGlyphRenderer() override;

		void* createTexture(uint16_t width, uint16_t height) override;
		void loadTextureData(void* texture, FT_Bitmap* bitmap) override;
		void destroyTexture(void* texture) override;

		void drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;
		void drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;

		void setVertexFormat(uint8_t vtxFmtIndex);
};
