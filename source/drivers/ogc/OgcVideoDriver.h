/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcVideoDriver.h
 ***************************************************************************/
#pragma once

#include <gccore.h>
#include "OgcEmulatorVideo.h"
#include "../VideoDriver.h"

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
		int getRefreshRate() const override { return vmode_60hz ? 60 : 50; }
		float getDeltaTime() const override { return vmode_60hz ? (1.0f / 60.0f) : (1.0f / 50.0f); }
		uint32_t getFrameTimer() override;
		void setFrameTimer(uint32_t frameTimer) override;

		ImageRenderer* getImageRenderer() override { return imageRenderer; }
		GlyphRenderer* getGlyphRenderer() override { return glyphRenderer; }
		OgcEmulatorVideo* getEmulatorVideo() override { return emulatorVideo; }

		GXRModeObj* getVideoMode() const { return videoMode; };
		GXRModeObj* findVideoMode();
		void setupVideoMode(GXRModeObj* mode);
		void waitForBufferReady();
		void presentBuffer();

	private:
		int screenWidth = 0;
		int screenHeight = 0;
		GXRModeObj *videoMode = nullptr; // Current video mode
		bool vmode_60hz = true;

		ImageRenderer* imageRenderer = nullptr;
		GlyphRenderer* glyphRenderer = nullptr;
		OgcEmulatorVideo* emulatorVideo = nullptr;
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
