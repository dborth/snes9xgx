/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * ColorShader.cpp
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include "ColorShader.h"

static const uint32_t cpVertexShaderProgram[] =
{
	0x00000000,0x00008009,0x20000000,0x000078a0,
	0x3c200000,0x88060094,0x00c00000,0x88062014,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00a11f00,0xfc00620f,0x02490001,0x80000040,
	0xfd041f80,0x900c0060,0x83f9223e,0x0000803f,
	0xfe282001,0x10000040,0xfe001f80,0x00080060,
	0xfeac9f80,0xfd00624f,0xdb0f49c0,0xdb0fc940,
	0xfea81f80,0x9000e02f,0x83f9223e,0x00000000,
	0xfe041f80,0x00370000,0xffa01f00,0x80000000,
	0xff101f00,0x800c0020,0x7f041f80,0x80370000,
	0x0000103f,0x00000000,0x02c51f00,0x80000000,
	0xfea41f00,0x80000020,0xffa09f00,0x80000040,
	0xff001f80,0x800c0060,0x398ee33f,0x0000103f,
	0x02c41f00,0x9000e00f,0x02c59f01,0x80000020,
	0xfea81f00,0x80000040,0x02c19f80,0x9000e06f,
	0x398ee33f,0x00000000,0x02c11f01,0x80000000,
	0x02c49f80,0x80000060,0x02e08f01,0xfe0c620f,
	0x02c01f80,0x7f00622f,0xfe242000,0x10000000,
	0xfe20a080,0x10000020,0xf2178647,0x49c0e9fb,
	0xfbbdb2ab,0x768ac733
};

static const uint32_t cpVertexShaderRegs[] = {
	0x00000103,0x00000000,0x00000000,0x00000001,
	0xffffff00,0xffffffff,0xffffffff,0xffffffff,
	0xffffffff,0xffffffff,0xffffffff,0xffffffff,
	0xffffffff,0xffffffff,0x00000000,0xfffffffc,
	0x00000002,0x00000001,0x00000000,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x000000ff,0x000000ff,0x000000ff,
	0x000000ff,0x00000000,0x0000000e,0x00000010
};

static const uint32_t cpPixelShaderProgram[] =
{
	0x20000000,0x00000ca0,0x00000000,0x88062094,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00002000,0x90000000,0x0004a000,0x90000020,
	0x00082001,0x90000040,0x000ca081,0x90000060,
	0xbb7dd898,0x9746c59c,0xc69b00e7,0x03c36218
};
static const uint32_t cpPixelShaderRegs[] = {
	0x00000001,0x00000002,0x14000001,0x00000000,
	0x00000001,0x00000100,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x0000000f,0x00000001,0x00000010,
	0x00000000
};

ColorShader * ColorShader::shaderInstance = nullptr;

ColorShader::ColorShader()
	: vertexShader(cuAttributeCount)
{
	pixelShader.setProgram(cpPixelShaderProgram, sizeof(cpPixelShaderProgram), cpPixelShaderRegs, sizeof(cpPixelShaderRegs));

	colorIntensityLocation = 0;
	pixelShader.addUniformVar((GX2UniformVar){ "unf_color_intensity", GX2_SHADER_VAR_TYPE_FLOAT4, 1, colorIntensityLocation, -1 });

	vertexShader.setProgram(cpVertexShaderProgram, sizeof(cpVertexShaderProgram), cpVertexShaderRegs, sizeof(cpVertexShaderRegs));

	angleLocation = 0;
	offsetLocation = 4;
	scaleLocation = 8;
	vertexShader.addUniformVar((GX2UniformVar){ "unf_angle", GX2_SHADER_VAR_TYPE_FLOAT, 1, angleLocation, -1 });
	vertexShader.addUniformVar((GX2UniformVar){ "unf_offset", GX2_SHADER_VAR_TYPE_FLOAT3, 1, offsetLocation, -1 });
	vertexShader.addUniformVar((GX2UniformVar){ "unf_scale", GX2_SHADER_VAR_TYPE_FLOAT3, 1, scaleLocation, -1 });

	colorLocation = 1;
	positionLocation = 0;
	vertexShader.addAttribVar((GX2AttribVar){ "attr_color", GX2_SHADER_VAR_TYPE_FLOAT4, 0, colorLocation });
	vertexShader.addAttribVar((GX2AttribVar){ "attr_position", GX2_SHADER_VAR_TYPE_FLOAT3, 0, positionLocation });

	GX2InitAttribStream(vertexShader.getAttributeBuffer(0), positionLocation, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	GX2InitAttribStream(vertexShader.getAttributeBuffer(1), colorLocation, 1, 0, GX2_ATTRIB_FORMAT_UNORM_8_8_8_8);

	fetchShader = new FetchShader(vertexShader.getAttributeBuffer(), vertexShader.getAttributesCount());

	positionVtxs = static_cast<float *>(memalign(GX2_VERTEX_BUFFER_ALIGNMENT, cuPositionVtxsSize));
	if(positionVtxs)
	{
		int i = 0;
		positionVtxs[i++] = -1.0f; positionVtxs[i++] = -1.0f; positionVtxs[i++] = 0.0f;
		positionVtxs[i++] =  1.0f; positionVtxs[i++] = -1.0f; positionVtxs[i++] = 0.0f;
		positionVtxs[i++] =  1.0f; positionVtxs[i++] =  1.0f; positionVtxs[i++] = 0.0f;
		positionVtxs[i++] = -1.0f; positionVtxs[i++] =  1.0f; positionVtxs[i++] = 0.0f;
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, positionVtxs, cuPositionVtxsSize);
	}

	colorBuffer.flags = static_cast<GX2RResourceFlags>(
		GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
	colorBuffer.elemSize = cuColorVtxsSize;
	colorBuffer.elemCount = cuMaxColorDraws;
	GX2RCreateBuffer(&colorBuffer);
	colorSlot = 0;
}

ColorShader::~ColorShader()
{
	free(positionVtxs);
	if(GX2RBufferExists(&colorBuffer))
		GX2RDestroyBufferEx(&colorBuffer, static_cast<GX2RResourceFlags>(0));
	delete fetchShader;
}
