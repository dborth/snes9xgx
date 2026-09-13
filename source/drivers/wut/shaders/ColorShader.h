/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * ColorShader.h
 ***************************************************************************/
#ifndef WUT_COLOR_SHADER_H_
#define WUT_COLOR_SHADER_H_

#include "VertexShader.h"
#include "PixelShader.h"
#include "FetchShader.h"
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/resource.h>

//!Flat-color quad shader (rectangles, keyboard/dialog chrome), used via
//!WutGlyphRenderer's drawFeature(). Singleton, accessed through instance().
class ColorShader : public Shader
{
	private:
		ColorShader();
		virtual ~ColorShader();

		static const uint32_t cuAttributeCount = 2;
		static const uint32_t cuPositionVtxsSize = 4 * cuVertexAttrSize;

		static ColorShader * shaderInstance;

		FetchShader * fetchShader;
		VertexShader vertexShader;
		PixelShader pixelShader;

		float * positionVtxs;

		// Per-draw color data can't safely share one reused buffer address:
		// GX2 draw calls are asynchronous, and within a single frame there
		// can be many colored-rectangle draws in flight before the GPU
		// catches up. A single shared buffer means draw N+1's memcpy can
		// stomp on data draw N's GX2DrawEx hasn't been read by the GPU yet.
		static const uint32_t cuMaxColorDraws = 512;
		GX2RBuffer colorBuffer;
		uint32_t colorSlot;

		uint32_t angleLocation;
		uint32_t offsetLocation;
		uint32_t scaleLocation;
		uint32_t colorLocation;
		uint32_t colorIntensityLocation;
		uint32_t positionLocation;

	public:
		static const uint32_t cuColorVtxsSize = 4 * cuColorAttrSize;

		static ColorShader * instance()
		{
			if(!shaderInstance)
				shaderInstance = new ColorShader();
			return shaderInstance;
		}

		static void destroyInstance()
		{
			delete shaderInstance;
			shaderInstance = nullptr;
		}

		void setShaders() const
		{
			fetchShader->setShader();
			vertexShader.setShader();
			pixelShader.setShader();
		}

		//!Call once per frame, before any draws - rewinds the per-draw
		//!color slot counter so this frame starts writing from slot 0
		//!again. Safe to call even before the first frame's draws: slots
		//!from a completed frame are free to reuse once GX2DrawDone has
		//!run (WHBGfxFinishRender does this every frame), so by the time
		//!prepareFrame() runs for the next frame, every slot used last
		//!frame is guaranteed consumed.
		void resetFrame()
		{
			colorSlot = 0;
		}

		//!\param colorAttr Packed RGBA8 color, 4 vertices (cuColorVtxsSize bytes).
		//!Copied into this draw's own slot in a GX2R buffer this shader
		//!owns - the caller's memory (a stack array at every current call
		//!site) isn't guaranteed to still be valid, unmodified, or
		//!cache-flushed by the time the GPU actually executes this draw,
		//!and a single shared destination wouldn't survive multiple
		//!in-flight draws within the same frame (see cuMaxColorDraws
		//!above).
		void setAttributeBuffer(const uint8_t * colorAttr)
		{
			VertexShader::setAttributeBuffer(0, cuPositionVtxsSize, cuVertexAttrSize, positionVtxs);

			// If a single frame ever draws more than cuMaxColorDraws
			// colored rectangles, keep reusing the last slot rather than
			// wrapping - wrapping could stomp a slot an earlier draw this
			// same frame is still relying on.
			uint32_t slot = colorSlot;
			if(colorSlot < cuMaxColorDraws - 1)
				colorSlot++;

			if(GX2RBufferExists(&colorBuffer))
			{
				uint8_t * dst = static_cast<uint8_t *>(GX2RLockBufferEx(&colorBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE));
				if(dst)
				{
					memcpy(dst + slot * cuColorVtxsSize, colorAttr, cuColorVtxsSize);
					GX2RUnlockBufferEx(&colorBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
				}
				GX2RSetAttributeBuffer(&colorBuffer, 1, cuColorAttrSize, slot * cuColorVtxsSize);
			}
		}

		void setAngle(float angleRadians)
		{
			VertexShader::setUniformReg(angleLocation, 4, &angleRadians);
		}
		void setOffset(const float offset[3])
		{
			VertexShader::setUniformReg(offsetLocation, 4, offset);
		}
		void setScale(const float scale[3])
		{
			VertexShader::setUniformReg(scaleLocation, 4, scale);
		}
		void setColorIntensity(const float colorIntensity[4])
		{
			PixelShader::setUniformReg(colorIntensityLocation, 4, colorIntensity);
		}
};

#endif // WUT_COLOR_SHADER_H_
