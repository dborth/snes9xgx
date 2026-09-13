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

		//!\param angleRadians Rotation about the quad's own center, in radians.
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
