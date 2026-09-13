/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * Shader.h
 ***************************************************************************/
#ifndef WUT_SHADER_H_
#define WUT_SHADER_H_

#include <string.h>
#include <stdint.h>
#include <gx2/state.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>

//!Fills in a GX2AttribStream for a single per-vertex attribute with an
//!identity component mapping (no swizzle).
static inline void GX2InitAttribStream(GX2AttribStream *attrib, uint32_t location, uint32_t buffer, uint32_t offset, GX2AttribFormat format)
{
	attrib->location   = location;
	attrib->buffer     = buffer;
	attrib->offset     = offset;
	attrib->format     = format;
	attrib->type       = GX2_ATTRIB_INDEX_PER_VERTEX;
	attrib->aluDivisor = 0;
	attrib->mask       = 0x00010203; // identity component mapping
	attrib->endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
}

//!Fills in and computes size/alignment for a single-mip, single-slice
//!GX2Texture with an identity RGBA swizzle.
static inline void GX2InitTexture(GX2Texture *texture, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, GX2SurfaceFormat format, GX2SurfaceDim dim, GX2TileMode tileMode)
{
	memset(texture, 0, sizeof(GX2Texture));
	texture->surface.dim       = dim;
	texture->surface.width     = width;
	texture->surface.height    = height;
	texture->surface.depth     = (depth == 0) ? 1 : depth;
	texture->surface.mipLevels = (mipLevels == 0) ? 1 : mipLevels;
	texture->surface.format    = format;
	texture->surface.aa        = GX2_AA_MODE1X;
	texture->surface.use       = GX2_SURFACE_USE_TEXTURE;
	texture->surface.tileMode  = tileMode;
	GX2CalcSurfaceSizeAndAlignment(&texture->surface);

	texture->viewFirstMip   = 0;
	texture->viewNumMips    = texture->surface.mipLevels;
	texture->viewFirstSlice = 0;
	texture->viewNumSlices  = 1;
	texture->compMap        = 0x00010203; // identity RGBA swizzle

	GX2InitTextureRegs(texture);
}

//!Common base for the GX2 shader wrappers below - just the vertex
//!attribute size constants and the shared GX2DrawEx() call.
class Shader
{
	protected:
		Shader() {}
		virtual ~Shader() {}
	public:
		static const uint16_t cuVertexAttrSize = sizeof(float) * 3;
		static const uint16_t cuTexCoordAttrSize = sizeof(float) * 2;
		static const uint16_t cuColorAttrSize = sizeof(uint8_t) * 4;

		static void draw(int32_t primitive = GX2_PRIMITIVE_MODE_QUADS, uint32_t vtxCount = 4)
		{
			GX2DrawEx(static_cast<GX2PrimitiveMode>(primitive), vtxCount, 0, 1);
		}
};

#endif // WUT_SHADER_H_
