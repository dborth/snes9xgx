/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutScaleFX.cpp
 ***************************************************************************/
#include <string.h>

#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>

#include "WutUpscaleFilters.h"
#include "WutScaleFX.h"
#include "shaders/ScaleFX.h"

namespace
{
	const GX2SurfaceFormat FORMAT_RGBA8 = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	const GX2SurfaceFormat FORMAT_RGBA32F = GX2_SURFACE_FORMAT_FLOAT_R32_G32_B32_A32;

	// ScaleFX defaults
	const float SCALEFX_PARAMS[4] = { 0.50f, 1.0f, 1.0f, 0.0f };

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

	int findSampler(const GX2PixelShader* s, const char* name, uint32_t fallbackIndex)
	{
		for (uint32_t i = 0; i < s->samplerVarCount; i++)
			if (!strcmp(s->samplerVars[i].name, name))
				return (int) s->samplerVars[i].location;
		return fallbackIndex < s->samplerVarCount ? (int) s->samplerVars[fallbackIndex].location : -1;
	}
}

WutScaleFX* WutScaleFX::instance()
{
	static WutScaleFX* inst = new WutScaleFX();
	return inst;
}

WutScaleFX::WutScaleFX()
	: initialized(false), initFailed(false), context(nullptr), posBuffer(nullptr), uvBuffer(nullptr), srcW(0), srcH(0)
{
	memset(&progP0, 0, sizeof progP0); memset(&progP1, 0, sizeof progP1); memset(&progP2, 0, sizeof progP2);
	memset(&progP3, 0, sizeof progP3); memset(&progP4, 0, sizeof progP4);
	memset(&progSmooth, 0, sizeof progSmooth);
	memset(&t0, 0, sizeof t0); memset(&t1, 0, sizeof t1); memset(&t2, 0, sizeof t2);
	memset(&t3, 0, sizeof t3); memset(&t4, 0, sizeof t4);
	GX2InitSampler(&samplerPoint, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
	GX2InitSampler(&samplerLinear, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

bool WutScaleFX::loadProgram(Program& p, const uint8_t* gsh, uint32_t size)
{
	p.ok = false;

	void* buf = MEMAllocFromDefaultHeapEx(size, 0x100);
	if (!buf)
		return false;
	memcpy(buf, gsh, size);

	if (!WHBGfxLoadGFDShaderGroup(&p.group, 0, buf))
		return false;

	bool ok = WHBGfxInitShaderAttribute(&p.group, "aPos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)
	       && WHBGfxInitShaderAttribute(&p.group, "aUV", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)
	       && WHBGfxInitFetchShader(&p.group);
	p.ok = ok;
	return ok;
}

bool WutScaleFX::init()
{
	if (initialized)
		return true;
	if (initFailed)
		return false;

	initFailed = true; // until proven otherwise

	context = (GX2ContextState*) MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	posBuffer = (float*) MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
	uvBuffer = (float*) MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
	if (!context || !posBuffer || !uvBuffer)
		return false;

	GX2SetupContextStateEx(context, TRUE);
	GX2SetContextState(context);
	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

	// same quad and UV orientation as Texture2DShader: QUADS, v = 0 at the top
	const float pos[8] = { -1, -1,  1, -1,  1, 1,  -1, 1 };
	const float uv[8] = { 0, 1,  1, 1,  1, 0,  0, 0 };
	memcpy(posBuffer, pos, sizeof pos);
	memcpy(uvBuffer, uv, sizeof uv);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, posBuffer, sizeof pos);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, uvBuffer, sizeof uv);

	bool ok = loadProgram(progP0, scalefx::pass0, sizeof scalefx::pass0)
	       && loadProgram(progP1, scalefx::pass1, sizeof scalefx::pass1)
	       && loadProgram(progP2, scalefx::pass2, sizeof scalefx::pass2)
	       && loadProgram(progP3, scalefx::pass3, sizeof scalefx::pass3)
	       && loadProgram(progP4, scalefx::pass4, sizeof scalefx::pass4)
	       && loadProgram(progSmooth, scalefx::smooth, sizeof scalefx::smooth);
	if (!ok)
		return false;

	initialized = true;
	initFailed = false;
	return true;
}

bool WutScaleFX::createTarget(Target& t, int w, int h, GX2SurfaceFormat format)
{
	memset(&t, 0, sizeof t);
	t.w = w;
	t.h = h;

	GX2Surface& s = t.cb.surface;
	s.use = (GX2SurfaceUse) (GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER);
	s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	s.width = w;
	s.height = h;
	s.depth = 1;
	s.mipLevels = 1;
	s.format = format;
	s.aa = GX2_AA_MODE1X;
	s.tileMode = GX2_TILE_MODE_DEFAULT;
	t.cb.viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&s);
	GX2InitColorBufferRegs(&t.cb);

	s.image = MEMAllocFromDefaultHeapEx(s.imageSize, s.alignment);
	if (!s.image)
		return false;
	memset(s.image, 0, s.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, s.image, s.imageSize);

	// sampled view of the same memory
	t.tex.surface = s;
	t.tex.viewFirstMip = 0;
	t.tex.viewNumMips = 1;
	t.tex.viewFirstSlice = 0;
	t.tex.viewNumSlices = 1;
	t.tex.compMap = 0x00010203;
	GX2InitTextureRegs(&t.tex);

	t.ok = true;
	return true;
}

void WutScaleFX::destroyTarget(Target& t)
{
	if (t.ok && t.cb.surface.image)
		MEMFreeToDefaultHeap(t.cb.surface.image);
	memset(&t, 0, sizeof t);
}

void WutScaleFX::destroyTargets()
{
	GX2DrawDone();
	destroyTarget(t0); destroyTarget(t1); destroyTarget(t2); destroyTarget(t3); destroyTarget(t4);
	srcW = srcH = 0;
}

void WutScaleFX::release()
{
	if (initialized && t4.ok)
		destroyTargets();
}

bool WutScaleFX::prepare(int width, int height)
{
	// P4 is 3x the source: SNES 256x224 -> 768x672. Larger sources (hi-res / interlaced) fall back.
	if (width <= 0 || height <= 0 || width > 256 || height > 240)
		return false;

	if (!init())
		return false;

	if (width == srcW && height == srcH && t4.ok)
		return true;

	destroyTargets();
	if (!(createTarget(t0, width, height, FORMAT_RGBA32F) && createTarget(t1, width, height, FORMAT_RGBA32F)
	   && createTarget(t2, width, height, FORMAT_RGBA8) && createTarget(t3, width, height, FORMAT_RGBA8)
	   && createTarget(t4, width * 3, height * 3, FORMAT_RGBA8)))
	{
		destroyTargets();
		return false;
	}

	srcW = width;
	srcH = height;
	return true;
}

void WutScaleFX::pass(Program& p, Target* dst, const Target* in0, const Target* in1, const GX2Texture* srcTex, const float uSize[4], const float uParams[4])
{
	GX2SetColorBuffer(&dst->cb, GX2_RENDER_TARGET_0);
	GX2SetViewport(0.0f, 0.0f, (float) dst->w, (float) dst->h, 0.0f, 1.0f);
	GX2SetScissor(0, 0, dst->w, dst->h);
	GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
	GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, GX2_DISABLE, GX2_ENABLE); // blending off
	GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);

	GX2SetFetchShader(&p.group.fetchShader);
	GX2SetVertexShader(p.group.vertexShader);
	GX2SetPixelShader(p.group.pixelShader);
	GX2SetAttribBuffer(0, 8 * sizeof(float), 2 * sizeof(float), posBuffer);
	GX2SetAttribBuffer(1, 8 * sizeof(float), 2 * sizeof(float), uvBuffer);

	int o;
	if ((o = findPixelUniform(p.group.pixelShader, "uSize")) >= 0)
		GX2SetPixelUniformReg(o, 4, uSize);
	if ((o = findPixelUniform(p.group.pixelShader, "uParams")) >= 0)
		GX2SetPixelUniformReg(o, 4, uParams);

	// inputs: uTex0 / uTex1 (in0 / in1), or the emulator's source texture as the first input
	const GX2Texture* ins[2] = { in0 ? &in0->tex : srcTex, in1 ? &in1->tex : nullptr };
	static const char* names[2] = { "uTex0", "uTex1" };
	for (uint32_t i = 0; i < 2; i++)
	{
		if (!ins[i])
			continue;
		int loc = findSampler(p.group.pixelShader, names[i], i);
		if (loc < 0)
			continue;
		GX2SetPixelTexture(ins[i], loc);
		GX2SetPixelSampler(&samplerPoint, loc);
	}

	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

	// make the result visible to the next pass' texture fetches
	GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, dst->cb.surface.image, dst->cb.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, dst->tex.surface.image, dst->tex.surface.imageSize);
}

void WutScaleFX::run(const GX2Texture* source)
{
	if (!initialized || !t4.ok || !source)
		return;

	const float sz[4] = { 1.0f / srcW, 1.0f / srcH, (float) srcW, (float) srcH };

	GX2SetContextState(context);

	pass(progP0, &t0, nullptr, nullptr, source, sz, SCALEFX_PARAMS);
	pass(progP1, &t1, &t0, nullptr, nullptr, sz, SCALEFX_PARAMS);
	pass(progP2, &t2, &t1, &t0, nullptr, sz, SCALEFX_PARAMS);
	pass(progP3, &t3, &t2, nullptr, nullptr, sz, SCALEFX_PARAMS);

	// P4 reads the tag map (t3) and the ORIGINAL frame: the source texture is the second input here
	Target* dst = &t4;
	GX2SetColorBuffer(&dst->cb, GX2_RENDER_TARGET_0);
	GX2SetViewport(0.0f, 0.0f, (float) dst->w, (float) dst->h, 0.0f, 1.0f);
	GX2SetScissor(0, 0, dst->w, dst->h);
	GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
	GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, GX2_DISABLE, GX2_ENABLE);
	GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);
	GX2SetFetchShader(&progP4.group.fetchShader);
	GX2SetVertexShader(progP4.group.vertexShader);
	GX2SetPixelShader(progP4.group.pixelShader);
	GX2SetAttribBuffer(0, 8 * sizeof(float), 2 * sizeof(float), posBuffer);
	GX2SetAttribBuffer(1, 8 * sizeof(float), 2 * sizeof(float), uvBuffer);
	int o = findPixelUniform(progP4.group.pixelShader, "uSize");
	if (o >= 0)
		GX2SetPixelUniformReg(o, 4, sz);
	int loc0 = findSampler(progP4.group.pixelShader, "uTex0", 0);
	int loc1 = findSampler(progP4.group.pixelShader, "uTex1", 1);
	GX2SetPixelTexture(&t3.tex, loc0);
	GX2SetPixelSampler(&samplerPoint, loc0);
	GX2SetPixelTexture(source, loc1);
	GX2SetPixelSampler(&samplerPoint, loc1);
	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
	GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, dst->cb.surface.image, dst->cb.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, dst->tex.surface.image, dst->tex.surface.imageSize);
}

void WutScaleFX::drawTV(const float offset[3], const float scale[3])
{
	if (!initialized || !t4.ok)
		return;

	Program& p = progSmooth;

	GX2SetFetchShader(&p.group.fetchShader);
	GX2SetVertexShader(p.group.vertexShader);
	GX2SetPixelShader(p.group.pixelShader);
	GX2SetAttribBuffer(0, 8 * sizeof(float), 2 * sizeof(float), posBuffer);
	GX2SetAttribBuffer(1, 8 * sizeof(float), 2 * sizeof(float), uvBuffer);

	const float xf[4] = { offset[0], offset[1], scale[0], scale[1] };
	int o = findVertexUniform(p.group.vertexShader, "uXf");
	if (o >= 0)
		GX2SetVertexUniformReg(o, 4, xf);

	const float sz[4] = { 1.0f / t4.w, 1.0f / t4.h, (float) t4.w, (float) t4.h };
	if ((o = findPixelUniform(p.group.pixelShader, "uSize")) >= 0)
		GX2SetPixelUniformReg(o, 4, sz);

	int loc = findSampler(p.group.pixelShader, "uTex0", 0);
	GX2SetPixelTexture(&t4.tex, loc);
	GX2SetPixelSampler(&samplerLinear, loc);

	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}
