/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * PixelShader.h
 ***************************************************************************/
#ifndef WUT_PIXEL_SHADER_H_
#define WUT_PIXEL_SHADER_H_

#include <malloc.h>
#include <string.h>
#include "Shader.h"

//!Owns a GX2PixelShader's compiled program and uniform/sampler metadata,
//!mirroring VertexShader's role on the pixel-shader side.
//!\ingroup grp_wut
class PixelShader : public Shader
{
	public:
		PixelShader()
			: pixelShader(static_cast<GX2PixelShader *>(memalign(0x40, sizeof(GX2PixelShader))))
		{
			if(pixelShader)
			{
				memset(pixelShader, 0, sizeof(GX2PixelShader));
				pixelShader->mode = GX2_SHADER_MODE_UNIFORM_REGISTER;
			}
		}

		virtual ~PixelShader()
		{
			if(pixelShader)
			{
				if(pixelShader->program)
					free(pixelShader->program);

				for(uint32_t i = 0; i < pixelShader->uniformVarCount; i++)
					free(const_cast<char *>(pixelShader->uniformVars[i].name));
				if(pixelShader->uniformVars)
					free(pixelShader->uniformVars);

				for(uint32_t i = 0; i < pixelShader->samplerVarCount; i++)
					free(const_cast<char *>(pixelShader->samplerVars[i].name));
				if(pixelShader->samplerVars)
					free(pixelShader->samplerVars);

				free(pixelShader);
			}
		}

		//!Uploads the compiled shader program.
		//!\param program Compiled GX2 pixel shader bytecode
		//!\param programSize Size of program in bytes
		//!\param regs Precomputed shader register values
		//!\param regsSize Size of regs in bytes
		void setProgram(const uint32_t * program, uint32_t programSize, const uint32_t * regs, uint32_t regsSize)
		{
			if(!pixelShader)
				return;

			pixelShader->size = programSize;
			pixelShader->program = memalign(GX2_SHADER_PROGRAM_ALIGNMENT, pixelShader->size);
			if(pixelShader->program)
			{
				memcpy(pixelShader->program, program, pixelShader->size);
				GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, pixelShader->program, pixelShader->size);
			}

			memcpy(&pixelShader->regs, regs, regsSize);
		}

		//!Registers a uniform variable so it can be set by location.
		void addUniformVar(const GX2UniformVar & var)
		{
			if(!pixelShader)
				return;

			uint32_t idx = pixelShader->uniformVarCount;
			GX2UniformVar * newVar = static_cast<GX2UniformVar *>(malloc((idx + 1) * sizeof(GX2UniformVar)));
			if(!newVar)
				return;

			if(idx > 0)
			{
				memcpy(newVar, pixelShader->uniformVars, idx * sizeof(GX2UniformVar));
				free(pixelShader->uniformVars);
			}
			pixelShader->uniformVars = newVar;
			memcpy(pixelShader->uniformVars + idx, &var, sizeof(GX2UniformVar));
			pixelShader->uniformVars[idx].name = strdup(var.name);
			pixelShader->uniformVarCount++;
		}

		//!Registers a texture sampler variable.
		void addSamplerVar(const GX2SamplerVar & var)
		{
			if(!pixelShader)
				return;

			uint32_t idx = pixelShader->samplerVarCount;
			GX2SamplerVar * newVar = static_cast<GX2SamplerVar *>(malloc((idx + 1) * sizeof(GX2SamplerVar)));
			if(!newVar)
				return;

			if(idx > 0)
			{
				memcpy(newVar, pixelShader->samplerVars, idx * sizeof(GX2SamplerVar));
				free(pixelShader->samplerVars);
			}
			pixelShader->samplerVars = newVar;
			memcpy(pixelShader->samplerVars + idx, &var, sizeof(GX2SamplerVar));
			pixelShader->samplerVars[idx].name = strdup(var.name);
			pixelShader->samplerVarCount++;
		}

		//!\return the underlying GX2PixelShader
		GX2PixelShader * getPixelShader() const { return pixelShader; }

		//!Binds this pixel shader for subsequent draws.
		void setShader() const
		{
			GX2SetPixelShader(pixelShader);
		}

		//!Writes uniform registers for the bound pixel shader.
		//!\param location Register location of the uniform
		//!\param size Number of registers to write
		//!\param reg Source values
		static inline void setUniformReg(uint32_t location, uint32_t size, const void * reg)
		{
			GX2SetPixelUniformReg(location, size, reg);
		}

	protected:
		GX2PixelShader * pixelShader; //!< The GX2 pixel shader object
};

#endif // WUT_PIXEL_SHADER_H_
