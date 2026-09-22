/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutOutputFilter.h
 *
 * Final draw of the game quad on a render target (TV or GamePad): optional
 * sharp bilinear sampling and a scanline overlay (shaders/OutputFilter.h),
 * sampling either the emulator's frame texture or the ScaleFX result. This is
 * the last shader pass before the target's colour buffer, whatever texture it
 * is fed - named for what it does (filter the output), not for GX2's use of
 * "present" for finishing/submitting a frame (WutVideoDriver::presentBuffer()).
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include <whb/gfx.h>

class WutOutputFilter
{
	public:
		static WutOutputFilter* instance();

		struct Params
		{
			const GX2Texture* texture;	// texture to sample (any size)
			float offset[3];			// NDC placement of the quad (same values as Texture2DShader)
			float scale[3];
			float outWidth;				// size of the quad in target pixels
			float outHeight;
			bool linear;				// hardware bilinear sampler (otherwise point); always used with sharp
			bool sharp;					// sharp bilinear sampling
			float scanlineStrength;		// 0 = off, otherwise darkness (0..1) between emulated lines
			float sourceLines;			// emulated lines the quad covers (not the sampled texture's height)
		};

		//! Draws with the currently selected (TV or DRC) context. Returns false if the shader could
		//! not be set up; the caller then draws the plain textured quad instead.
		bool draw(const Params& p);

	private:
		WutOutputFilter();

		bool init();

		bool initialized;
		bool initFailed;
		WHBGfxShaderGroup group;
		float* posBuffer;
		float* uvBuffer;
		GX2Sampler samplerPoint, samplerLinear;
		int locXf, locSize, locOut, locScan, locTex;
};
