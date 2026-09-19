/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * Texture2DShader.h
 ***************************************************************************/
#ifndef WUT_TEXTURE_2D_SHADER_H_
#define WUT_TEXTURE_2D_SHADER_H_

#include "VertexShader.h"
#include "PixelShader.h"
#include "FetchShader.h"
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/resource.h>

//!Textured quad shader (images, glyphs), used by WutImageRenderer and
//!WutGlyphRenderer. Singleton, accessed through instance().
class Texture2DShader : public Shader
{
	private:
		Texture2DShader();
		virtual ~Texture2DShader();

		static const uint32_t cuAttributeCount = 2;
		static const uint32_t ciPositionVtxsSize = 4 * cuVertexAttrSize;
		static const uint32_t ciTexCoordsVtxsSize = 4 * cuTexCoordAttrSize;

		static Texture2DShader * shaderInstance;

		FetchShader * fetchShader;
		VertexShader vertexShader;
		PixelShader pixelShader;

		float * posVtxs;
		float * texCoords;

		// Rotated quads are positioned on the CPU (see WutImageRenderer::drawTexture)
		// and drawn with an identity transform, because the compiled vertex
		// shader's rotation assumes a 16:9 canvas and warps on a 4:3 one.
		// Each rotated draw needs its own vertex slot: GX2 draws are
		// asynchronous, so a single shared buffer would be overwritten while
		// an earlier draw this frame is still queued (same reasoning as
		// ColorShader's per-draw color slots). Slots are padded to
		// GX2_VERTEX_BUFFER_ALIGNMENT so every slot offset stays aligned.
		static const uint32_t cuMaxRotatedDraws = 64;
		static const uint32_t cuRotatedSlotSize = GX2_VERTEX_BUFFER_ALIGNMENT;
		static_assert(4 * cuVertexAttrSize <= cuRotatedSlotSize, "rotated quad must fit in one slot");
		GX2RBuffer rotatedBuffer;
		uint32_t rotatedSlot;

		uint32_t angleLocation;
		uint32_t offsetLocation;
		uint32_t scaleLocation;
		uint32_t colorIntensityLocation;
		uint32_t blurLocation;
		uint32_t samplerLocation;
		uint32_t positionLocation;
		uint32_t texCoordLocation;

	public:
		static Texture2DShader * instance()
		{
			if(!shaderInstance)
				shaderInstance = new Texture2DShader();
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

		void setAttributeBuffer() const
		{
			VertexShader::setAttributeBuffer(0, ciPositionVtxsSize, cuVertexAttrSize, posVtxs);
			VertexShader::setAttributeBuffer(1, ciTexCoordsVtxsSize, cuTexCoordAttrSize, texCoords);
		}

		//!Call once per frame, before any draws - rewinds the rotated-quad
		//!slot counter (see WutVideoDriver::prepareFrame()).
		void resetFrame()
		{
			rotatedSlot = 0;
		}

		//!Copies a pre-rotated quad into its own vertex slot.
		//!\param ndcCorners 4 (x, y) pairs in NDC, in the same vertex order as
		//!the default unit square (bottom-left, bottom-right, top-right, top-left).
		//!\param slotOut Receives the slot to pass to setRotatedAttributeBuffer().
		//!\return false if the per-frame slots are exhausted (or the buffer
		//!couldn't be created) - the caller should fall back to setAngle().
		bool uploadRotatedQuad(const float ndcCorners[8], uint32_t & slotOut)
		{
			if(!GX2RBufferExists(&rotatedBuffer) || rotatedSlot >= cuMaxRotatedDraws)
				return false;

			float * dst = static_cast<float *>(GX2RLockBufferEx(&rotatedBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE));
			if(!dst)
				return false;

			slotOut = rotatedSlot++;
			dst += slotOut * (cuRotatedSlotSize / sizeof(float));
			for(int i = 0; i < 4; i++)
			{
				dst[i * 3 + 0] = ndcCorners[i * 2 + 0];
				dst[i * 3 + 1] = ndcCorners[i * 2 + 1];
				dst[i * 3 + 2] = 0.0f;
			}
			GX2RUnlockBufferEx(&rotatedBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
			return true;
		}

		//!Binds a slot from uploadRotatedQuad() as the position stream (plus the
		//!shared texcoords). Pair with setAngle(0), setOffset({0,0,0}) and
		//!setScale({1,1,1}) so the vertex shader leaves the corners untouched.
		void setRotatedAttributeBuffer(uint32_t slot) const
		{
			GX2RSetAttributeBuffer(const_cast<GX2RBuffer *>(&rotatedBuffer), 0, cuVertexAttrSize, slot * cuRotatedSlotSize);
			VertexShader::setAttributeBuffer(1, ciTexCoordsVtxsSize, cuTexCoordAttrSize, texCoords);
		}

		//!\param angleRadians Rotation about the quad's own center, in radians.
		//!Only correct for a 16:9 canvas - for anything else draw rotated quads
		//!via uploadRotatedQuad() instead.
		void setAngle(float angleRadians)
		{
			VertexShader::setUniformReg(angleLocation, 4, &angleRadians);
		}
		//!\param offset NDC-space (x, y, z) position of the quad's center.
		void setOffset(const float offset[3])
		{
			VertexShader::setUniformReg(offsetLocation, 4, offset);
		}
		//!\param scale NDC-space (x, y, z) half-extents of the quad.
		void setScale(const float scale[3])
		{
			VertexShader::setUniformReg(scaleLocation, 4, scale);
		}
		//!\param colorIntensity (r, g, b, a) modulation, 0..1 per channel - alpha carries the draw's overall opacity.
		void setColorIntensity(const float colorIntensity[4])
		{
			PixelShader::setUniformReg(colorIntensityLocation, 4, colorIntensity);
		}

		void setTextureAndSampler(const GX2Texture * texture, const GX2Sampler * sampler) const
		{
			GX2SetPixelTexture(texture, samplerLocation);
			GX2SetPixelSampler(sampler, samplerLocation);
		}

		//!The compiled pixel shader reads a per-draw blur-direction uniform
		//!that this driver doesn't expose as a feature. Uniform registers
		//!aren't guaranteed to hold their value across a shader switch, so
		//!this must be called every draw (not just once at init) to keep
		//!blurLocation pinned at zero.
		void clearBlur() const
		{
			static const float zero[3] = {0.0f, 0.0f, 0.0f};
			PixelShader::setUniformReg(blurLocation, 4, zero);
		}
};

#endif // WUT_TEXTURE_2D_SHADER_H_
