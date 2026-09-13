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
class FetchShader : public Shader
{
	public:
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

		void setShader() const
		{
			GX2SetFetchShader(fetchShader);
		}

	protected:
		GX2FetchShader * fetchShader;
		void * fetchShaderProgram;
};

#endif // WUT_FETCH_SHADER_H_
