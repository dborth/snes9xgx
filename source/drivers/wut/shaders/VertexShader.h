/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * VertexShader.h
 ***************************************************************************/
#ifndef WUT_VERTEX_SHADER_H_
#define WUT_VERTEX_SHADER_H_

#include <string.h>
#include <malloc.h>
#include "Shader.h"

//!Owns a GX2VertexShader's compiled program and uniform/attribute
//!metadata. setProgram() uploads the compiled bytecode; addUniformVar()/
//!addAttribVar() register the reflection info GX2 needs to bind it.
class VertexShader : public Shader
{
	public:
		VertexShader(uint32_t numAttr)
			: attributesCount(numAttr)
			, attributes(new GX2AttribStream[attributesCount])
			, vertexShader(static_cast<GX2VertexShader *>(memalign(0x40, sizeof(GX2VertexShader))))
		{
			if(vertexShader)
			{
				memset(vertexShader, 0, sizeof(GX2VertexShader));
				vertexShader->mode = GX2_SHADER_MODE_UNIFORM_REGISTER;
			}
		}

		virtual ~VertexShader()
		{
			delete [] attributes;

			if(vertexShader)
			{
				if(vertexShader->program)
					free(vertexShader->program);

				for(uint32_t i = 0; i < vertexShader->uniformVarCount; i++)
					free(const_cast<char *>(vertexShader->uniformVars[i].name));
				if(vertexShader->uniformVars)
					free(vertexShader->uniformVars);

				for(uint32_t i = 0; i < vertexShader->attribVarCount; i++)
					free(const_cast<char *>(vertexShader->attribVars[i].name));
				if(vertexShader->attribVars)
					free(vertexShader->attribVars);

				free(vertexShader);
			}
		}

		void setProgram(const uint32_t * program, uint32_t programSize, const uint32_t * regs, uint32_t regsSize)
		{
			if(!vertexShader)
				return;

			// This must live where the GPU can see it, aligned to 0x100.
			vertexShader->size = programSize;
			vertexShader->program = memalign(GX2_SHADER_PROGRAM_ALIGNMENT, vertexShader->size);
			if(vertexShader->program)
			{
				memcpy(vertexShader->program, program, vertexShader->size);
				GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, vertexShader->program, vertexShader->size);
			}

			memcpy(&vertexShader->regs, regs, regsSize);
		}

		void addUniformVar(const GX2UniformVar & var)
		{
			if(!vertexShader)
				return;

			uint32_t idx = vertexShader->uniformVarCount;
			GX2UniformVar * newVar = static_cast<GX2UniformVar *>(malloc((idx + 1) * sizeof(GX2UniformVar)));
			if(!newVar)
				return;

			if(idx > 0)
			{
				memcpy(newVar, vertexShader->uniformVars, idx * sizeof(GX2UniformVar));
				free(vertexShader->uniformVars);
			}
			vertexShader->uniformVars = newVar;
			memcpy(vertexShader->uniformVars + idx, &var, sizeof(GX2UniformVar));
			vertexShader->uniformVars[idx].name = strdup(var.name);
			vertexShader->uniformVarCount++;
		}

		void addAttribVar(const GX2AttribVar & var)
		{
			if(!vertexShader)
				return;

			uint32_t idx = vertexShader->attribVarCount;
			GX2AttribVar * newVar = static_cast<GX2AttribVar *>(malloc((idx + 1) * sizeof(GX2AttribVar)));
			if(!newVar)
				return;

			if(idx > 0)
			{
				memcpy(newVar, vertexShader->attribVars, idx * sizeof(GX2AttribVar));
				free(vertexShader->attribVars);
			}
			vertexShader->attribVars = newVar;
			memcpy(vertexShader->attribVars + idx, &var, sizeof(GX2AttribVar));
			vertexShader->attribVars[idx].name = strdup(var.name);
			vertexShader->attribVarCount++;
		}

		static inline void setAttributeBuffer(uint32_t bufferIdx, uint32_t bufferSize, uint32_t stride, const void * buffer)
		{
			GX2SetAttribBuffer(bufferIdx, bufferSize, stride, buffer);
		}

		void setShader() const
		{
			GX2SetVertexShader(vertexShader);
		}

		GX2AttribStream * getAttributeBuffer(uint32_t idx = 0) const
		{
			if(idx >= attributesCount)
				return nullptr;
			return &attributes[idx];
		}

		uint32_t getAttributesCount() const { return attributesCount; }

		static void setUniformReg(uint32_t location, uint32_t size, const void * reg)
		{
			GX2SetVertexUniformReg(location, size, reg);
		}

	protected:
		uint32_t attributesCount;
		GX2AttribStream * attributes;
		GX2VertexShader * vertexShader;
};

#endif // WUT_VERTEX_SHADER_H_
