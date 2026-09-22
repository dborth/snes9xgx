/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutOutputFilter.cpp
 ***************************************************************************/
#include <string.h>

#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/mem.h>
#include <gx2/shaders.h>

#include "WutOutputFilter.h"
#include "shaders/OutputFilter.h"

namespace
{
	int findPixelUniform(const GX2PixelShader* s, const char* name)
	{
		for (uint32_t i = 0; i < s->uniformVarCount; i++)
			if (!strcmp(s->uniformVars[i].name, name))
				return (int) s->uniformVars[i].offset;
		return -1;
	}

	int findVertexUniform(const GX2VertexShader* s, const char* name)
	{
		for (uint32_t i = 0; i < s->uniformVarCount; i++)
			if (!strcmp(s->uniformVars[i].name, name))
				return (int) s->uniformVars[i].offset;
		return -1;
	}

	int findSampler(const GX2PixelShader* s, const char* name)
	{
		for (uint32_t i = 0; i < s->samplerVarCount; i++)
			if (!strcmp(s->samplerVars[i].name, name))
				return (int) s->samplerVars[i].location;
		return -1;
	}
}

WutOutputFilter* WutOutputFilter::instance()
{
	static WutOutputFilter* inst = new WutOutputFilter();
	return inst;
}

WutOutputFilter::WutOutputFilter()
	: initialized(false), initFailed(false), posBuffer(nullptr), uvBuffer(nullptr)
	, locXf(-1), locSize(-1), locOut(-1), locScan(-1), locTex(-1)
{
	memset(&group, 0, sizeof group);
	GX2InitSampler(&samplerPoint, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
	GX2InitSampler(&samplerLinear, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

bool WutOutputFilter::init()
{
	if (initialized)
		return true;
	if (initFailed)
		return false;

	initFailed = true; // until proven otherwise

	// The GFD buffer stays allocated, like WutScaleFX's programs
	void* buf = MEMAllocFromDefaultHeapEx(sizeof outputfilter::program, 0x100);
	posBuffer = (float*) MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
	uvBuffer = (float*) MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
	if (!buf || !posBuffer || !uvBuffer)
		return false;
	memcpy(buf, outputfilter::program, sizeof outputfilter::program);

	if (!WHBGfxLoadGFDShaderGroup(&group, 0, buf))
		return false;

	if (!(WHBGfxInitShaderAttribute(&group, "aPos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)
	   && WHBGfxInitShaderAttribute(&group, "aUV", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)
	   && WHBGfxInitFetchShader(&group)))
		return false;

	locXf = findVertexUniform(group.vertexShader, "uXf");
	locSize = findPixelUniform(group.pixelShader, "uSize");
	locOut = findPixelUniform(group.pixelShader, "uOut");
	locScan = findPixelUniform(group.pixelShader, "uScan");
	locTex = findSampler(group.pixelShader, "uTex0");
	if (locXf < 0 || locSize < 0 || locOut < 0 || locScan < 0 || locTex < 0)
		return false;

	// same quad and UV orientation as Texture2DShader: QUADS, v = 0 at the top
	const float pos[8] = { -1, -1,  1, -1,  1, 1,  -1, 1 };
	const float uv[8] = { 0, 1,  1, 1,  1, 0,  0, 0 };
	memcpy(posBuffer, pos, sizeof pos);
	memcpy(uvBuffer, uv, sizeof uv);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, posBuffer, sizeof pos);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, uvBuffer, sizeof uv);

	initialized = true;
	initFailed = false;
	return true;
}

bool WutOutputFilter::draw(const Params& p)
{
	if (!p.texture || !init())
		return false;

	GX2SetFetchShader(&group.fetchShader);
	GX2SetVertexShader(group.vertexShader);
	GX2SetPixelShader(group.pixelShader);
	GX2SetAttribBuffer(0, 8 * sizeof(float), 2 * sizeof(float), posBuffer);
	GX2SetAttribBuffer(1, 8 * sizeof(float), 2 * sizeof(float), uvBuffer);

	const float w = (float) p.texture->surface.width;
	const float h = (float) p.texture->surface.height;

	const float xf[4] = { p.offset[0], p.offset[1], p.scale[0], p.scale[1] };
	const float size[4] = { 1.0f / w, 1.0f / h, w, h };
	const float out[4] = { p.outWidth, p.outHeight, p.sharp ? 1.0f : 0.0f, 0.0f };
	const float scan[4] = { p.scanlineStrength, p.sourceLines, 0.0f, 0.0f };

	GX2SetVertexUniformReg(locXf, 4, xf);
	GX2SetPixelUniformReg(locSize, 4, size);
	GX2SetPixelUniformReg(locOut, 4, out);
	GX2SetPixelUniformReg(locScan, 4, scan);

	GX2SetPixelTexture(p.texture, locTex);
	GX2SetPixelSampler((p.linear || p.sharp) ? &samplerLinear : &samplerPoint, locTex);

	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
	return true;
}
