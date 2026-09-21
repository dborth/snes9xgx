/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutScaleFX.h
 *
 * GPU implementation of Sp00kyFox's 5-pass ScaleFX (MIT) for Wii U (GX2),
 * followed by a final resample onto the game quad:
 *
 *   SRC (RGBA8) -> P0 metrics -> P1 strength -> P2 resolve -> P3 selectors
 *       -> P4 subpixel output (3x)  ->  FINAL  -> TV colour buffer
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/context.h>
#include <gx2/sampler.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <whb/gfx.h>

class WutScaleFX
{
	public:
		static WutScaleFX* instance();

		//! Loads the shaders and (re)allocates the render targets for a source of this size.
		//! Cheap once set up. Returns false if ScaleFX cannot be used for this frame
		//! (shader load / allocation failure, or a source too large for a 3x pass);
		//! the caller then draws the plain texture instead.
		bool prepare(int width, int height);

		//! Runs P0..P4 into the offscreen targets. Selects the ScaleFX GX2ContextState: the caller
		//! must select its own (TV) context again afterwards (WHBGfxBeginRenderTV()).
		void run(const GX2Texture* source);

		//! Draws the ScaleFX result with the currently selected (TV) context. offset/scale place the quad in NDC
		//! (same values as Texture2DShader).
		void drawTV(const float offset[3], const float scale[3]);

		void release();

	private:
		WutScaleFX();

		struct Target
		{
			GX2ColorBuffer cb;
			GX2Texture tex;
			bool ok;
			int w, h;
		};
		struct Program
		{
			WHBGfxShaderGroup group;
			bool ok;
		};

		bool init();
		bool loadProgram(Program& p, const uint8_t* gsh, uint32_t size);
		bool createTarget(Target& t, int w, int h, GX2SurfaceFormat format);
		void destroyTarget(Target& t);
		void destroyTargets();
		void pass(Program& p, Target* dst, const Target* in0, const Target* in1, const GX2Texture* srcTex, const float uSize[4], const float uParams[4]);

		bool initialized;
		bool initFailed;
		GX2ContextState* context;
		float* posBuffer;
		float* uvBuffer;
		GX2Sampler samplerPoint, samplerLinear;

		Program progP0, progP1, progP2, progP3, progP4, progSmooth, progSharp;
		Target t0, t1, t2, t3, t4;
		int srcW, srcH;
};
