/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * VideoDriver.h
 *
 * Platform video backend GuiImage and GuiText delegates to. Exactly one driver
 * implements this and assigns the single global instance below
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct {
	uint8_t r;			/*!< Red color component. */
	uint8_t g;			/*!< Green color component. */
	uint8_t b;			/*!< Blue alpha component. */
	uint8_t a;			/*!< Alpha component. If a function does not use the alpha value, it is safely ignored. */
} PixelColor;

class ImageRenderer;
class GlyphRenderer;

class VideoDriver
{
	public:
		virtual ~VideoDriver() = default;

		virtual void init(int width, int height) = 0;
		virtual void shutdown() = 0;

		//! Flushes the current frame to the screen and swaps buffers
		virtual void render() = 0;

		//! Clears the current frame buffer
		virtual void clearScreen(const PixelColor& color) = 0;

		virtual int getScreenWidth() const = 0;
		virtual int getScreenHeight() const = 0;
		virtual uint32_t getFrameTimer() = 0;

		virtual ImageRenderer* getImageRenderer() = 0;
		virtual GlyphRenderer* getGlyphRenderer() = 0;
};

//!Platform image/texture backend GuiImageData and GuiImage delegate to.
//!Exactly one driver implements this and assigns the single
//!global instance below. GuiImageData/GuiImage never touch any platform
//!texture type directly, only imageSystem.
class ImageRenderer
{
	public:
		virtual ~ImageRenderer() = default;
		//! Allocates an empty hardware-aligned texture buffer
		virtual void * createTexture(int width, int height) = 0;
		//! Loads raw RGBA8 pixels into the pre-allocated texture
		virtual void loadTextureData(void * texture, const uint8_t * rgba, int width, int height) = 0;
		//!Destroys a texture created by createTexture.
		virtual void destroyTexture(void * texture) = 0;
		//!Draws a texture created by createTexture.
		virtual void drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha) = 0;
		virtual void drawRectangle(float x, float y, float width, float height, PixelColor color) = 0;
};

class GlyphRenderer {
	public:
		virtual ~GlyphRenderer() = default;

		virtual void* createTexture(uint16_t width, uint16_t height) = 0;
		virtual void loadTextureData(void* texture, FT_Bitmap* bitmap) = 0;
		virtual void destroyTexture(void* texture) = 0;

		virtual void drawQuad(void* texture, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) = 0;
		virtual void drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color) = 0;
};
