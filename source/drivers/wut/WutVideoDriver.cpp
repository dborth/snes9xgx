/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutVideoDriver.cpp
 ***************************************************************************/
#include <cmath>
#include <cstring>
#include <malloc.h>

#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <whb/gfx.h>
#include <proc_ui/procui.h>

#include "../Platform.h"
#include "WutVideoDriver.h"
#include "shaders/Texture2DShader.h"
#include "shaders/ColorShader.h"

namespace
{
	inline float DegToRad(float degrees)
	{
		return degrees * (3.14159265358979323846f / 180.0f);
	}

	void PixelRectToNdc(float x, float y, float w, float h, float scaleX, float scaleY, int designWidth, int designHeight, float offset[3], float scale[3])
	{
		float centerPxX = x + w * 0.5f;
		float centerPxY = y + h * 0.5f;

		offset[0] = (centerPxX / designWidth) * 2.0f - 1.0f;
		offset[1] = 1.0f - (centerPxY / designHeight) * 2.0f;
		offset[2] = 0.0f;

		scale[0] = (w * scaleX) / designWidth;
		scale[1] = (h * scaleY) / designHeight;
		scale[2] = 1.0f;
	}
}

/****************************************************************************
 * WutVideoDriver
 ***************************************************************************/

WutVideoDriver::WutVideoDriver()
	: screenWidth(0), screenHeight(0), frameTimer(0), clearColor{0, 0, 0, 255}
	, imageRenderer(nullptr), glyphRenderer(nullptr)
{
}

WutVideoDriver::~WutVideoDriver()
{
	delete imageRenderer;
	delete glyphRenderer;
}

void WutVideoDriver::init(int width, int height)
{
	WHBGfxInit();

	screenWidth = width;
	screenHeight = height;

	imageRenderer = new WutImageRenderer(this);
	glyphRenderer = new WutGlyphRenderer(this);

	prepareFrame();
}

void WutVideoDriver::shutdown()
{
	WHBGfxShutdown();
}

bool WutVideoDriver::isForeground() const
{
	return platform->getStatus() == Status::Running;
}

void WutVideoDriver::prepareFrame()
{
	if(!isForeground())
		return;

	WHBGfxBeginRender();

	auto drawPass = [&]() {
		WHBGfxClearColor(clearColor.r / 255.0f, clearColor.g / 255.0f, clearColor.b / 255.0f, clearColor.a / 255.0f);
		GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
		GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, GX2_DISABLE, GX2_ENABLE);
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);
		GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD, GX2_DISABLE, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
	};
	
	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC();	drawPass();

	// Every color-shaded draw this frame gets its own slot in ColorShader's
	// GX2R buffer (see ColorShader.h) - rewind the counter here, once per
	// frame, before anything draws into it.
	ColorShader::instance()->resetFrame();
}

void WutVideoDriver::renderMenu()
{
	if(isForeground())
	{
		WHBGfxFinishRenderTV();
		WHBGfxFinishRenderDRC();
		WHBGfxFinishRender();
	}

	frameTimer++;

	prepareFrame();
}

void WutVideoDriver::startMenuVideo()
{

}

void WutVideoDriver::presentBuffer()
{

}

void WutVideoDriver::clearScreen(const PixelColor& color)
{
	clearColor = color;
}

int WutVideoDriver::getRefreshRate() const
{
	return GX2GetSystemTVScanMode() == GX2_TV_SCAN_MODE_576I ? 50 : 60;
}

float WutVideoDriver::getDeltaTime() const
{
	return GX2GetSystemTVScanMode() == GX2_TV_SCAN_MODE_576I ? (1.0f / 50.0f) : (1.0f / 60.0f);
}

/****************************************************************************
 * WutImageRenderer
 ***************************************************************************/

WutImageRenderer::WutImageRenderer(WutVideoDriver * driver_)
	: driver(driver_)
{
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

void * WutImageRenderer::createTexture(int width, int height)
{
	if(width <= 0 || height <= 0)
		return nullptr;

	GX2Texture * texture = new GX2Texture();
	GX2InitTexture(texture, width, height, 1, 0, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_SURFACE_DIM_TEXTURE_2D, GX2_TILE_MODE_LINEAR_ALIGNED);

	GX2CalcSurfaceSizeAndAlignment(&texture->surface);
	GX2InitTextureRegs(texture);

	texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);
	if(!texture->surface.image)
	{
		delete texture;
		return nullptr;
	}

	return texture;
}

void WutImageRenderer::loadTextureData(void * texture, const uint8_t * rgba, int width, int height)
{
	if(!texture || !rgba || width <= 0 || height <= 0)
		return;

	GX2Texture * tex = static_cast<GX2Texture *>(texture);
	if(!tex->surface.image)
		return;

	uint8_t * dst = static_cast<uint8_t *>(tex->surface.image);
	const uint32_t dstStride = tex->surface.pitch * 4;
	const uint32_t srcStride = static_cast<uint32_t>(width) * 4;
	for(int y = 0; y < height; y++)
		memcpy(dst + y * dstStride, rgba + y * srcStride, srcStride);

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex->surface.image, tex->surface.imageSize);
}

void WutImageRenderer::fillTexture(void* texture, int width, int height, ImageRenderer::PixelSourceFn source, void* userdata)
{

}

void WutImageRenderer::destroyTexture(void * texture)
{
	if(!texture)
		return;

	GX2Texture * tex = static_cast<GX2Texture *>(texture);
	if(tex->surface.image)
		MEMFreeToDefaultHeap(tex->surface.image);
	delete tex;
}

void WutImageRenderer::drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha)
{
	if(!texture || !driver->isForeground())
		return;

	float offset[3];
	float scale[3];
	PixelRectToNdc(xpos, ypos, width, height, scaleX, scaleY, driver->getScreenWidth(), driver->getScreenHeight(), offset, scale);

	float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, alpha / 255.0f };

	Texture2DShader * shader = Texture2DShader::instance();

	auto drawPass = [&]() {
		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(DegToRad(degrees));
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(static_cast<GX2Texture *>(texture), &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC();	drawPass();
}

void WutImageRenderer::drawRectangle(float x, float y, float width, float height, PixelColor color)
{
	if(!driver->isForeground())
		return;

	float offset[3];
	float scale[3];
	PixelRectToNdc(x, y, width, height, 1.0f, 1.0f, driver->getScreenWidth(), driver->getScreenHeight(), offset, scale);

	// Use a persistent, static white vertex buffer so the GPU pointer remains valid
	static uint8_t whiteVtxs[ColorShader::cuColorVtxsSize];
	static bool vtxsInit = false;
	if (!vtxsInit)
	{
		memset(whiteVtxs, 0xFF, sizeof(whiteVtxs)); // 255 = Solid White
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU, whiteVtxs, sizeof(whiteVtxs));
		vtxsInit = true;
	}

	float colorIntensity[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

	auto drawPass = [&]() {
		ColorShader * shader = ColorShader::instance();
		shader->setShaders();
		shader->setAttributeBuffer(whiteVtxs);
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};
	
	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC();	drawPass();
}

/****************************************************************************
 * WutGlyphRenderer
 ***************************************************************************/

WutGlyphRenderer::WutGlyphRenderer(WutVideoDriver * driver_)
	: driver(driver_)
{
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

void * WutGlyphRenderer::createTexture(uint16_t width, uint16_t height)
{
	GX2Texture * texture = new GX2Texture();
	GX2InitTexture(texture, width == 0 ? 1 : width, height == 0 ? 1 : height, 1, 0, GX2_SURFACE_FORMAT_UNORM_R8, GX2_SURFACE_DIM_TEXTURE_2D, GX2_TILE_MODE_LINEAR_ALIGNED);

	GX2CalcSurfaceSizeAndAlignment(&texture->surface);

	texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);
	if(!texture->surface.image)
	{
		delete texture;
		return nullptr;
	}
	memset(texture->surface.image, 0x00, texture->surface.imageSize);

	// Broadcast the single R8 channel to R,G,B,A on sample (compMap's four
	// 8-bit fields each select source component 0/R) - same trick as the
	// Wii driver's GX_TF_I4 intensity textures, so Texture2DShader's
	// texture*colorIntensity modulate produces anti-aliased, colorable
	// glyphs without a dedicated single-channel shader.
	texture->compMap = 0x00000000;
	GX2InitTextureRegs(texture);

	return texture;
}

void WutGlyphRenderer::loadTextureData(void * texturePtr, FT_Bitmap * bitmap)
{
	if(!texturePtr || !bitmap || !bitmap->buffer)
		return;

	GX2Texture * texture = static_cast<GX2Texture *>(texturePtr);
	if(!texture->surface.image)
		return;

	uint8_t * dst = static_cast<uint8_t *>(texture->surface.image);
	uint8_t * src = bitmap->buffer;

	uint32_t copyWidth = (bitmap->width < texture->surface.width) ? bitmap->width : texture->surface.width;
	uint32_t copyHeight = (bitmap->rows < texture->surface.height) ? bitmap->rows : texture->surface.height;

	for(uint32_t y = 0; y < copyHeight; y++)
		memcpy(dst + y * texture->surface.pitch, src + y * bitmap->width, copyWidth);

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);
}

void WutGlyphRenderer::destroyTexture(void * texturePtr)
{
	if(!texturePtr)
		return;

	GX2Texture * texture = static_cast<GX2Texture *>(texturePtr);
	if(texture->surface.image)
		MEMFreeToDefaultHeap(texture->surface.image);
	delete texture;
}

void WutGlyphRenderer::drawQuad(void * texturePtr, int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color)
{
	if(!texturePtr || !driver->isForeground())
		return;

	float offset[3];
	float scale[3];
	PixelRectToNdc(screenX, screenY, width, height, 1.0f, 1.0f, driver->getScreenWidth(), driver->getScreenHeight(), offset, scale);

	float colorIntensity[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

	Texture2DShader * shader = Texture2DShader::instance();
	
	auto drawPass = [&]() {
		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(static_cast<GX2Texture *>(texturePtr), &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC();	drawPass();
}

void WutGlyphRenderer::drawFeature(int16_t screenX, int16_t screenY, uint16_t width, uint16_t height, const PixelColor& color)
{
	if(!driver->isForeground())
		return;

	float offset[3];
	float scale[3];
	PixelRectToNdc(screenX, screenY, width, height, 1.0f, 1.0f, driver->getScreenWidth(), driver->getScreenHeight(), offset, scale);

	// Use a persistent, static white vertex buffer so the GPU pointer remains valid
	static uint8_t whiteVtxs[ColorShader::cuColorVtxsSize];
	static bool vtxsInit = false;
	if (!vtxsInit)
	{
		memset(whiteVtxs, 0xFF, sizeof(whiteVtxs)); // 255 = Solid White
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU, whiteVtxs, sizeof(whiteVtxs));
		vtxsInit = true;
	}

	float colorIntensity[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };

	ColorShader * shader = ColorShader::instance();
	
	auto drawPass = [&]() {
		shader->setShaders();
		shader->setAttributeBuffer(whiteVtxs);
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	WHBGfxBeginRenderTV(); drawPass();
	WHBGfxBeginRenderDRC();	drawPass();
}
