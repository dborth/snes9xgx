/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * FetchShader.h
 ***************************************************************************/
#ifndef WUT_FETCH_SHADER_H_
#define WUT_FETCH_SHADER_H_

#include <malloc.h>
#include "Shader.h"

//!Owns the GX2 fetch shader program that binds vertex attribute streams
//!to a vertex shader. Built once from a fixed GX2AttribStream list and
//!bound via setShader() before each draw.
//!\ingroup grp_wut
class FetchShader : public Shader
{
	public:
		//!Builds a fetch shader for the given vertex attribute streams.
		//!\param attributes Attribute stream descriptions, in vertex shader input order
		//!\param attrCount Number of entries in attributes
		//!\param type GX2 fetch shader type
		//!\param tess GX2 tessellation mode
		FetchShader(GX2AttribStream * attributes, uint32_t attrCount,
			GX2FetchShaderType type = GX2_FETCH_SHADER_TESSELLATION_NONE,
			GX2TessellationMode tess = GX2_TESSELLATION_MODE_DISCRETE)
			: fetchShader(nullptr), fetchShaderProgram(nullptr)
		{
			uint32_t shaderSize = GX2CalcFetchShaderSizeEx(attrCount, type, tess);
			fetchShaderProgram = memalign(GX2_SHADER_PROGRAM_ALIGNMENT, shaderSize);
			if(fetchShaderProgram)
			{
				fetchShader = new GX2FetchShader;
				GX2InitFetchShaderEx(fetchShader, static_cast<uint8_t *>(fetchShaderProgram), attrCount, attributes, type, tess);
				GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, fetchShaderProgram, shaderSize);
			}
		}

		virtual ~FetchShader()
		{
			if(fetchShaderProgram)
				free(fetchShaderProgram);
			if(fetchShader)
				delete fetchShader;
		}

		//!Binds this fetch shader for subsequent draws.
		void setShader() const
		{
			GX2SetFetchShader(fetchShader);
		}

	protected:
		GX2FetchShader * fetchShader; //!< The GX2 fetch shader object (nullptr if allocation failed)
		void * fetchShaderProgram; //!< Aligned program memory the fetch shader executes from
};

#endif // WUT_FETCH_SHADER_H_
