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
#include <stddef.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "Mesh.h"

// PixelColor and MeshVertex live in Mesh.h so host builds can use them without FreeType

class ImageRenderer;
class GlyphRenderer;
class EmulatorVideoDriver;

class VideoDriver
{
	public:
		virtual ~VideoDriver() = default;

		virtual void init(int width, int height) = 0;
		virtual void shutdown() = 0;

		virtual void startMenuVideo() = 0;

		//! Flushes the current frame to the screen and swaps buffers
		virtual void renderMenu() = 0;

		//! Clears the current frame buffer
		virtual void clearScreen(const PixelColor& color) = 0;

		virtual int getScreenWidth() const = 0;
		virtual int getScreenHeight() const = 0;
		virtual uint32_t getFrameTimer() = 0;
		virtual void setFrameTimer(uint32_t frameTimer) = 0;
		//! Caps the pending tick count at maxTicks. Safe against the tick source
		//! running concurrently (VI retrace / OSAlarm)
		virtual void limitFrameTimer(uint32_t maxTicks) = 0;
		//! Consumes one pending tick if there is one. Same concurrency guarantee.
		virtual void consumeFrameTick() = 0;
		virtual int getRefreshRate() const = 0;
		virtual float getDeltaTime() const = 0;

		//! Ratio of physical render-target pixels to design-canvas pixels
		//! (the width/height passed to init()). Greater than 1.0 wherever
		//! the driver upscales the design canvas to fill a higher-resolution.
		virtual float getUIScale() const { return 1.0f; }

		virtual ImageRenderer* getImageRenderer() = 0;
		virtual GlyphRenderer* getGlyphRenderer() = 0;
		virtual EmulatorVideoDriver* getEmulatorVideo() = 0;
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
		//! Callback used by fillTexture(): supplies the RGBA8 color for pixel (x,y).
		typedef void (*PixelSourceFn)(int x, int y, PixelColor * outColor, void * userdata);
		//! Fills a texture created by createTexture() by invoking `source` once per destination
		//! pixel and writing the returned color directly into the texture's native memory layout
		//! (eg. GX's 4x4-tiled RGBA8). Lets platform-agnostic code synthesize or composite a
		//! full-size texture (blur/overlay/procedural effects, etc.) without ever needing to
		//! allocate a plain row-major RGBA8 buffer the size of the destination - callers only
		//! need scratch space for whatever smaller working set their pixel math requires.
		virtual void fillTexture(void * texture, int width, int height, PixelSourceFn source, void * userdata) = 0;
		//!Whether a texture allocated for allocWidth x allocHeight can be reloaded in place
		virtual bool canReuseTexture(int allocWidth, int allocHeight, int width, int height) const
		{
			return width <= allocWidth && height <= allocHeight;
		}
		//!Destroys a texture created by createTexture.
		virtual void destroyTexture(void * texture) = 0;
		//!Draws a texture created by createTexture.
		virtual void drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha) = 0;
		virtual void drawRectangle(float x, float y, float width, float height, PixelColor color) = 0;
		//!Draws a triangle list: three vertices per triangle, each with its own color.
		//!Positions are in design-canvas pixels (the same space as drawRectangle()), colors are
		//!straight (non-premultiplied) RGBA blended source-over, so per-vertex alpha gives
		//!gradients and soft edges (see GuiMesh). Winding does not matter. A count that is not a
		//!multiple of 3 is rounded down.
		//!The vertices are copied (or consumed) before this returns, so the caller can free or
		//!reuse the array immediately.
		//!\param vertices Triangle list
		//!\param count Number of vertices
		virtual void drawMesh(const MeshVertex* vertices, size_t count) = 0;

		//!Hands a triangle list to the backend to keep in its own ready-to-draw form (a GX
		//!display list, a GX2 vertex buffer), so drawing it again with drawMeshInstance() costs
		//!the CPU almost nothing, where drawMesh() sends every vertex every time. Same vertex
		//!rules as drawMesh(); the vertices are copied. Call while drawing, since a backend may
		//!record from its current render state.
		//!\return An opaque handle for drawMeshInstance() and destroyMesh(), or nullptr if the
		//!backend can't, in which case the caller keeps using drawMesh()
		virtual void* createMesh(const MeshVertex* vertices, size_t count) { (void)vertices; (void)count; return nullptr; }
		//!Releases a mesh from createMesh(). Safe to call while draws that use it are still
		//!waiting to be submitted; the backend keeps it until they are done.
		virtual void destroyMesh(void* mesh) { (void)mesh; }
		//!Draws a mesh from createMesh() fully opaque, as its vertex colors say. Mesh vertex
		//!(vx, vy) lands at (x + vx * scaleX, y + vy * scaleY) in design-canvas pixels.
		//!\param mesh Handle from createMesh()
		//!\param x Where mesh (0,0) lands
		//!\param y Where mesh (0,0) lands
		//!\param scaleX Horizontal stretch about (x,y)
		//!\param scaleY Vertical stretch about (x,y)
		virtual void drawMeshInstance(void* mesh, float x, float y, float scaleX, float scaleY) { (void)mesh; (void)x; (void)y; (void)scaleX; (void)scaleY; }
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
