/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutEmulatorVideo.cpp
 ***************************************************************************/
#include <coreinit/memdefaultheap.h>
#include <whb/gfx.h>

#include "WutEmulatorVideo.h"
#include "WutVideoDriver.h"
#include "shaders/Texture2DShader.h"
#include "../../snes9xgx.h"
#include "../../video.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/gfx.h"
#include "snes9x/ppu.h"

namespace
{
	void PixelRectToNdc(float x, float y, float w, float h, int designWidth, int designHeight, float offset[3], float scale[3])
	{
		float centerPxX = x + w * 0.5f;
		float centerPxY = y + h * 0.5f;

		offset[0] = (centerPxX / designWidth) * 2.0f - 1.0f;
		offset[1] = 1.0f - (centerPxY / designHeight) * 2.0f;
		offset[2] = 0.0f;

		scale[0] = w / designWidth;
		scale[1] = h / designHeight;
		scale[2] = 1.0f;
	}
}

WutEmulatorVideo::WutEmulatorVideo()
	: videoDriver(nullptr), texture(nullptr)
	, vwidth(100), vheight(100), oldvwidth(0), oldvheight(0)
	, checkVideo(0), prevRenderedFrameCount(0)
	, quadX(0), quadY(0), quadWidth(0), quadHeight(0)
{
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

WutEmulatorVideo::~WutEmulatorVideo()
{
	destroyTexture();
}

void WutEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<WutVideoDriver*>(driver);
	vwidth = 100;
	vheight = 100;
}

/****************************************************************************
 * forceVideoUpdate
 *
 * Forces the next presentFrame() to rebuild scaling/texture state, and
 * primes the "have we actually rendered a frame yet" check so presentFrame
 * doesn't try to draw a texture before the core has produced one (eg. right
 * after a ROM load).
 ***************************************************************************/
void WutEmulatorVideo::forceVideoUpdate()
{
	checkVideo = 2;
	prevRenderedFrameCount = IPPU.RenderedFramesCount;
}

/****************************************************************************
 * resetVideo
 *
 * Recomputes the on-screen placement of the game quad.
 ***************************************************************************/
void WutEmulatorVideo::resetVideo()
{
	float xscale, yscale;
	bool tallField = (vheight == 224 || vheight == 448);

	if (EmuSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9)
	{
		float base_height = tallField ? 224.0f : 239.0f;
		float scale_factor = (videoDriver->getScreenHeight() / 2.0f) / base_height;

		xscale = (256.0f * scale_factor * 15.0f) / 16.0f;
		yscale = videoDriver->getScreenHeight() / 2.0f;
	}
	else if (EmuSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED)
	{
		xscale = tallField ? 224.0f : 239.0f;
		yscale = xscale;
	}
	else
	{
		xscale = 256.0f;
		yscale = tallField ? 224.0f : 239.0f;
	}

	xscale *= EmuSettings.videoZoomHor;
	yscale *= EmuSettings.videoZoomVert;

	quadWidth  = 2.0f * xscale;
	quadHeight = 2.0f * yscale;
	quadX = (videoDriver->getScreenWidth()  / 2.0f) + EmuSettings.videoXshift - quadWidth  / 2.0f;
	quadY = (videoDriver->getScreenHeight() / 2.0f) - EmuSettings.videoYshift - quadHeight / 2.0f;

	// Record where/how big the quad is so we can composite gameScreenPng 
	// back at the exact spot and size it was actually drawn at.
	gameScreenPng.width  = vwidth;
	gameScreenPng.height = vheight;
	gameScreenPng.scaleX = quadWidth  / (float) vwidth;
	gameScreenPng.scaleY = quadHeight / (float) vheight;
	gameScreenPng.xoffset = (int) ((quadX + quadWidth  / 2.0f) - videoDriver->getScreenWidth()  / 2.0f);
	gameScreenPng.yoffset = (int) ((quadY + quadHeight / 2.0f) - videoDriver->getScreenHeight() / 2.0f);
}

/****************************************************************************
 * rebuildTexture / destroyTexture
 *
 * The game texture is recreated only when the emulator's rendered
 * width/height actually changes (see presentFrame), not every frame.
 ***************************************************************************/
void WutEmulatorVideo::destroyTexture()
{
	if (!texture)
		return;

	if (texture->surface.image)
		MEMFreeToDefaultHeap(texture->surface.image);

	delete texture;
	texture = nullptr;
}

void WutEmulatorVideo::rebuildTexture(int width, int height)
{
	destroyTexture();

	if (width <= 0 || height <= 0)
		return;

	texture = new GX2Texture();
	GX2InitTexture(texture, width, height, 1, 0, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_SURFACE_DIM_TEXTURE_2D, GX2_TILE_MODE_LINEAR_ALIGNED);

	GX2CalcSurfaceSizeAndAlignment(&texture->surface);
	GX2InitTextureRegs(texture);

	texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);
	if (!texture->surface.image)
	{
		delete texture;
		texture = nullptr;
	}
}

/****************************************************************************
 * uploadFrame
 *
 * Expands the emulator's raw 15-bit RGB555 framebuffer directly into the 
 * linear RGBA8 texture. No tiling/swizzle step is needed here:
 * GX2's LINEAR_ALIGNED tiling mode already accepts a row-major
 * upload, same as WutImageRenderer::loadTextureData.
 ***************************************************************************/
void WutEmulatorVideo::uploadFrame()
{
	if (!texture || !texture->surface.image)
		return;

	const uint8_t* srcBase = reinterpret_cast<const uint8_t*>(GFX.Screen);
	uint8_t* dst = static_cast<uint8_t*>(texture->surface.image);
	const uint32_t dstStride = texture->surface.pitch * 4;

	for (int y = 0; y < vheight; y++)
	{
		const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(srcBase + y * EXT_PITCH);
		uint8_t* dstRow = dst + y * dstStride;

		for (int x = 0; x < vwidth; x++)
		{
			uint16_t color = srcRow[x];

			// RGB555 format (bit 15 unused)
			uint8_t r = (color >> 10) & 0x1F;
			uint8_t g = (color >> 5) & 0x1F;
			uint8_t b = color & 0x1F;

			dstRow[x * 4 + 0] = (r << 3) | (r >> 2);
			dstRow[x * 4 + 1] = (g << 3) | (g >> 2);
			dstRow[x * 4 + 2] = (b << 3) | (b >> 2);
			dstRow[x * 4 + 3] = 0xFF;
		}
	}

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);
}

/****************************************************************************
 * drawQuad
 ***************************************************************************/
void WutEmulatorVideo::drawQuad()
{
	if (!texture || !videoDriver->isForeground())
		return;

	// Cheap enough to just re-set every frame rather than tracking whether
	// the setting changed since the last draw.
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP,
		EmuSettings.videoBilinearFilter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);

	float offset[3];
	float scale[3];
	PixelRectToNdc(quadX, quadY, quadWidth, quadHeight, videoDriver->getScreenWidth(), videoDriver->getScreenHeight(), offset, scale);

	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	Texture2DShader* shader = Texture2DShader::instance();

	auto drawPass = [&]() {
		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(texture, &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC(); drawPass();
}

/****************************************************************************
 * presentFrame
 ***************************************************************************/
void WutEmulatorVideo::presentFrame(int width, int height)
{
	vwidth = width;
	vheight = height;

	if (checkVideo == 2 && IPPU.RenderedFramesCount == prevRenderedFrameCount)
		return; // we haven't rendered any frames yet, so we can't draw anything!

	if (oldvwidth != vwidth || oldvheight != vheight) // if rendered width/height changes, update scaling
		checkVideo = 1;

	if (checkVideo) // if we get back from the menu, and have rendered at least 1 frame
	{
		resetVideo(); // reset scaling to emulator rendering settings
		rebuildTexture(vwidth, vheight);

		oldvwidth = vwidth;
		oldvheight = vheight;
		checkVideo = 0;
	}

	uploadFrame();
	drawQuad();

	videoDriver->presentBuffer();
}

/****************************************************************************
 * readFrameRGB24
 *
 * Converts straight from the emulator's raw framebuffer (same source as
 * uploadFrame) rather than reading back the GX2 texture - simpler, and
 * avoids depending on GX2 surface padding/pitch for a CPU readback.
 ***************************************************************************/
void WutEmulatorVideo::readFrameRGB24(uint8_t* dst)
{
	const uint8_t* srcBase = reinterpret_cast<const uint8_t*>(GFX.Screen);
	int width = gameScreenPng.width;
	int height = gameScreenPng.height;

	for (int y = 0; y < height; y++)
	{
		const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(srcBase + y * EXT_PITCH);

		for (int x = 0; x < width; x++)
		{
			uint16_t color = srcRow[x];

			uint8_t r = (color >> 10) & 0x1F;
			uint8_t g = (color >> 5) & 0x1F;
			uint8_t b = color & 0x1F;

			int outIdx = (y * width + x) * 3;
			dst[outIdx]     = (r << 3) | (r >> 2);
			dst[outIdx + 1] = (g << 3) | (g >> 2);
			dst[outIdx + 2] = (b << 3) | (b >> 2);
		}
	}
}
