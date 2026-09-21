/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutVideoDriver.h
 ***************************************************************************/
#pragma once

#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "WutEmulatorVideo.h"
#include "WutOutputTarget.h"
#include "../VideoDriver.h"

//!Wii U VideoDriver: GX2 + libwhb's WHBGfx* helpers. Every draw pass runs
//!twice per frame - once for the TV, once for the GamePad - so the same
//!UI always reaches both screens; there's no separate dual-display mode.
class WutVideoDriver : public VideoDriver
{
	public:
		WutVideoDriver();
		~WutVideoDriver() override;

		void init(int width, int height) override;
		void shutdown() override;
		void renderMenu() override;
		void startMenuVideo() override;
		void clearScreen(const PixelColor& color) override;

		int getScreenWidth() const override { return screenWidth; }
		int getScreenHeight() const override { return screenHeight; }
		uint32_t getFrameTimer() override { return frameTimer; }
		void setFrameTimer(uint32_t _frameTimer) override { frameTimer = _frameTimer; };

		int getRefreshRate() const override;
		float getDeltaTime() const override;
		float getUIScale() const override { return uiScale; }

		//!Physical pixel size of a render target (the TV follows the console's
		//!output setting, the GamePad is always 854x480). Unrelated to the
		//!design canvas returned by getScreenWidth()/getScreenHeight(), which
		//!is stretched onto each target independently per axis.
		int getTargetWidth(OutputTarget target) const { return targetWidth[(int)target]; }
		int getTargetHeight(OutputTarget target) const { return targetHeight[(int)target]; }

		ImageRenderer* getImageRenderer() override { return imageRenderer; }
		GlyphRenderer* getGlyphRenderer() override { return glyphRenderer; }
		WutEmulatorVideo* getEmulatorVideo() override { return emulatorVideo; }

		//!False once the OS has taken away the foreground (HOME menu overlay,
		//!forced exit, etc.) - GX2 is off-limits at that point, so every
		//!draw/render entry point below checks this first and no-ops rather
		//!than issuing a GX2 call into a context we no longer own.
		bool isForeground() const;

		void presentBuffer();
	private:
		// Binds the TV context state and resets the per-frame render
		// state (viewport/scissor/blend/depth/cull) that WHBGfxInit()
		// doesn't set on its own. Called once at the end of init() so
		// the first frame's draws land somewhere valid, then again at
		// the top of every render() pass.
		void prepareFrame();

		// Queries GX2's current TV scan mode/aspect ratio and derives the
		// physical TV and DRC target dims
		void computeUIScale();

		int screenWidth;
		int screenHeight;
		float uiScale = 1.0f;
		int targetWidth[OUTPUT_TARGET_COUNT] = { 0, 0 };
		int targetHeight[OUTPUT_TARGET_COUNT] = { 0, 0 };
		uint32_t frameTimer;
		PixelColor clearColor;

		ImageRenderer * imageRenderer;
		GlyphRenderer * glyphRenderer;
		WutEmulatorVideo* emulatorVideo = nullptr;
};

//!GX2-backed ImageRenderer for GuiImage/GuiImageData, using Texture2DShader.
class WutImageRenderer : public ImageRenderer
{
	public:
		WutImageRenderer(WutVideoDriver * driver);

		void * createTexture(int width, int height) override;
		void loadTextureData(void * texture, const uint8_t * rgba, int width, int height) override;
		void fillTexture(void * texture, int width, int height, PixelSourceFn source, void * userdata) override;
		void destroyTexture(void * texture) override;
		void drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha) override;
		void drawRectangle(float x, float y, float width, float height, PixelColor color) override;

	private:
		WutVideoDriver * driver;
		GX2Sampler sampler;
};

//!GX2-backed GlyphRenderer for GuiTextRenderer, using Texture2DShader for
//!glyph quads and ColorShader for solid "feature" rectangles.
class WutGlyphRenderer : public GlyphRenderer
{
	public:
		WutGlyphRenderer(WutVideoDriver * driver);

		void* createTexture(uint16_t width, uint16_t height) override;
		void loadTextureData(void* texture, FT_Bitmap* bitmap) override;
		void destroyTexture(void* texture) override;

		void drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;
		void drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) override;

	private:
		WutVideoDriver * driver;
		GX2Sampler sampler;
};
