/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutEmulatorVideo.cpp
 ***************************************************************************/
#include <math.h>
#include <algorithm>

#include <coreinit/memdefaultheap.h>
#include <whb/gfx.h>

#include "WutEmulatorVideo.h"
#include "WutVideoDriver.h"
#include "WutScaleFX.h"
#include "WutOutputFilter.h"
#include "WutUpscaleFilters.h"
#include "shaders/Texture2DShader.h"
#include "../../snes9xgx.h"
#include "../../video.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/gfx.h"
#include "snes9x/ppu.h"

namespace
{
	// Darkness of the scanline gaps (0..1) when Scanline Overlay is on
	const float SCANLINE_STRENGTH = 0.5f;

	// Emulator video is placed in the physical pixels of each output target.
	// The constants below are NOT a design canvas - they are the units the
	// saved settings are defined in.

	// Units of the Screen Position (videoXshift/videoYshift) setting: one unit
	// is 1/640 of the screen width, 1/480 of the screen height. Same on every
	// platform, so a saved shift looks the same everywhere.
	const float SHIFT_UNITS_X = 640.0f;
	const float SHIFT_UNITS_Y = 480.0f;

	// 16:9 (Fixed Pixel Ratio) is for pixel-perfect scaling: the largest whole
	// number N such that (256 * N) x (lines * N) fits the target, drawn
	// unfiltered-square in the middle with bars around it. Same shape as on
	// GC/Wii (2x = 512x448 on a 480-line screen), with N growing on bigger targets.

	// "16:9" correction: picture aspect for a 240-line frame
	const float PICTURE_ASPECT_240 = 4.0f / 3.0f;
	const float PICTURE_LINES = 240.0f;

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
	, placement{ {0, 0, 0, 0}, {0, 0, 0, 0} }
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
 * Computes where the game quad goes on each output target, in that target's
 * own physical pixels (placement[]), from the current vheight and EmuSettings'
 * aspect ratio / zoom / shift options.
 *
 *   None:             fill the target (stretch)
 *   16:9:             picture aspect 4:3 * 240/lines, fitted inside the target
 *   16:9 Fixed Ratio: largest whole-number scale that fits, square pixels
 *                     (e.g. 1080p: 4x = 1024x896; 480p: 2x = 512x448)
 *
 * quadX/Y/Width/Height (and gameScreenPng) are the TV placement expressed in
 * UI-canvas pixels. They only exist for the menu's game screenshot, which
 * lives in canvas space; nothing is drawn from them.
 ***************************************************************************/
void WutEmulatorVideo::resetVideo()
{
	const bool tallField = (vheight == 224 || vheight == 448);
	const float baseHeight = tallField ? 224.0f : 239.0f;

	for (int i = 0; i < OUTPUT_TARGET_COUNT; i++)
	{
		const OutputTarget target = static_cast<OutputTarget>(i);
		const float targetW = (float) videoDriver->getTargetWidth(target);
		const float targetH = (float) videoDriver->getTargetHeight(target);

		float w, h;
		bool pixelExact = false;

		if (EmuSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED)
		{
			// Largest whole scale that fits both axes (never below 1x)
			const float scale = std::max(1.0f,
				std::min(floorf(targetW / (float) SNES_WIDTH), floorf(targetH / baseHeight)));
			w = (float) SNES_WIDTH * scale;
			h = baseHeight * scale;
			pixelExact = true;
		}
		else
		{
			if (EmuSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9)
			{
				// Fit the picture inside the target at its own aspect ratio
				const float pictureAspect = PICTURE_ASPECT_240 * (PICTURE_LINES / baseHeight);
				const float targetAspect = targetW / targetH;

				if (targetAspect > pictureAspect)
				{
					h = targetH;
					w = targetH * pictureAspect;
				}
				else
				{
					w = targetW;
					h = targetW / pictureAspect;
				}
			}
			else
			{
				// No correction: the picture fills the whole target
				w = targetW;
				h = targetH;
			}

			w *= EmuSettings.videoZoomHor;
			h *= EmuSettings.videoZoomVert;
		}

		// Positive shift moves the picture right / down
		float x = (targetW - w) * 0.5f + EmuSettings.videoXshift * (targetW / SHIFT_UNITS_X);
		float y = (targetH - h) * 0.5f + EmuSettings.videoYshift * (targetH / SHIFT_UNITS_Y);

		if (pixelExact)
		{
			// Pixel-exact only if the quad also starts on a pixel boundary
			x = floorf(x + 0.5f);
			y = floorf(y + 0.5f);
		}

		placement[i].x = x;
		placement[i].y = y;
		placement[i].w = w;
		placement[i].h = h;
	}

	// The same placement in UI-canvas pixels (menu screenshot), from the TV's placement
	const TargetPlacement& tv = placement[static_cast<int>(OutputTarget::TV)];
	const float toCanvasX = (float) videoDriver->getScreenWidth()  / (float) videoDriver->getTargetWidth(OutputTarget::TV);
	const float toCanvasY = (float) videoDriver->getScreenHeight() / (float) videoDriver->getTargetHeight(OutputTarget::TV);

	quadX      = tv.x * toCanvasX;
	quadY      = tv.y * toCanvasY;
	quadWidth  = tv.w * toCanvasX;
	quadHeight = tv.h * toCanvasY;

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
 * mapPointerToFrame
 *
 * The pointer is reported in UI-canvas coordinates, which span the whole
 * screen on every output, so they are a fraction of the target the pointer is
 * on. That is mapped through that target's own placement (the TV and the
 * GamePad are fitted independently, so they differ).
 ***************************************************************************/
bool WutEmulatorVideo::mapPointerToFrame(float canvasX, float canvasY, bool onGamePad, int* frameX, int* frameY)
{
	if (!frameX || !frameY)
		return false;

	const OutputTarget target = onGamePad ? OutputTarget::DRC : OutputTarget::TV;
	const TargetPlacement& p = placement[static_cast<int>(target)];
	if (p.w <= 0.0f || p.h <= 0.0f) // resetVideo() hasn't run yet
		return false;

	const float px = (canvasX / (float) videoDriver->getScreenWidth())  * (float) videoDriver->getTargetWidth(target);
	const float py = (canvasY / (float) videoDriver->getScreenHeight()) * (float) videoDriver->getTargetHeight(target);

	float u = (px - p.x) / p.w;
	float v = (py - p.y) / p.h;
	u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
	v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

	const int gunHeight = PPU.ScreenHeight > 0 ? PPU.ScreenHeight : SNES_HEIGHT;

	int x = (int)(u * SNES_WIDTH);
	int y = (int)(v * gunHeight);
	if (x > SNES_WIDTH - 1) x = SNES_WIDTH - 1;
	if (y > gunHeight - 1) y = gunHeight - 1;

	*frameX = x;
	*frameY = y;
	return true;
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
	videoDriver->flushDrawQueue();

	if (!texture || !videoDriver->isForeground())
		return;

	// Cheap enough to just re-set every frame rather than tracking whether
	// the setting changed since the last draw.
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP,
		EmuSettings.videoBilinearFilter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);

	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	Texture2DShader* shader = Texture2DShader::instance();

	// NDC placement of the game quad on a target, from its physical-pixel rect
	auto placementNdc = [&](OutputTarget target, float offset[3], float scale[3]) {
		const TargetPlacement& p = placement[static_cast<int>(target)];
		PixelRectToNdc(p.x, p.y, p.w, p.h, videoDriver->getTargetWidth(target), videoDriver->getTargetHeight(target), offset, scale);
	};

	auto drawPass = [&](OutputTarget target) {
		float offset[3];
		float scale[3];
		placementNdc(target, offset, scale);

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

	const bool sharp = EmuSettings.videoUpscalingFilter == UPSCALE_SHARP_BILINEAR;
	const float scanlines = EmuSettings.videoScanlines ? SCANLINE_STRENGTH : 0.0f;

	// Output filter: sharp bilinear and/or scanlines. Returns false if it is unavailable.
	auto outputFilterPass = [&](OutputTarget target, const GX2Texture* tex, bool linear, bool sharpSampling) {
		const TargetPlacement& p = placement[static_cast<int>(target)];

		WutOutputFilter::Params pp;
		pp.texture = tex;
		placementNdc(target, pp.offset, pp.scale);
		pp.outWidth = p.w;
		pp.outHeight = p.h;
		pp.linear = linear;
		pp.sharp = sharpSampling;
		pp.scanlineStrength = scanlines;
		pp.sourceLines = (float) texture->surface.height;
		return WutOutputFilter::instance()->draw(pp);
	};

	// The frame texture on a target: plain textured quad, or the output filter when it has work to do
	auto drawGame = [&](OutputTarget target) {
		if ((sharp || scanlines > 0.0f) && outputFilterPass(target, texture, EmuSettings.videoBilinearFilter, sharp))
			return;
		drawPass(target);
	};

	// ScaleFX (TV output only)
	WutScaleFX* scalefx = WutScaleFX::instance();
	bool useScaleFX = false;

	if (EmuSettings.videoUpscalingFilter == UPSCALE_SCALEFX)
	{
		if (scalefx->prepare(texture->surface.width, texture->surface.height))
		{
			scalefx->run(texture);
			useScaleFX = true;
		}
	}
	else
	{
		scalefx->release();
	}

	WHBGfxBeginRenderTV();
	if (useScaleFX)
	{
		// Scanlines go through the output filter, otherwise the ScaleFX final stage draws it
		if (scanlines <= 0.0f || !outputFilterPass(OutputTarget::TV, scalefx->outputTexture(), true, false))
		{
			float offset[3];
			float scale[3];
			placementNdc(OutputTarget::TV, offset, scale);
			scalefx->drawTV(offset, scale);
		}
	}
	else
	{
		drawGame(OutputTarget::TV);
	}
	WHBGfxBeginRenderDRC(); drawGame(OutputTarget::DRC);
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

	videoDriver->prepareFrame();

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
