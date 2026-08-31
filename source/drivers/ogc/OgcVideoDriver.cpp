/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcVideoDriver.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>

#include "OgcVideoDriver.h"
#include "../../libgui/Gui.h"
#include "../../video.h"

extern Mtx GXmodelView2D;

OgcVideoDriver::OgcVideoDriver()
    : screenWidth(0), screenHeight(0)
{

}

OgcVideoDriver::~OgcVideoDriver()
{
	delete imageRenderer;
	delete glyphRenderer;
}

void OgcVideoDriver::init(int width, int height)
{
	screenWidth = width;
	screenHeight = height;

	InitVideo();

	imageRenderer = new OgcImageRenderer();
	glyphRenderer = new OgcGlyphRenderer();
}

void OgcVideoDriver::shutdown()
{
	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
}

void OgcVideoDriver::render()
{
	Menu_Render();
}

void OgcVideoDriver::clearScreen(const PixelColor& color)
{
    GXColor background = { color.r, color.g, color.b, color.a };
    GX_SetCopyClear(background, GX_MAX_Z24);
}

void* OgcImageRenderer::createTexture(int width, int height)
{
	int padWidth = width + (4 - width % 4) % 4;
	int padHeight = height + (4 - height % 4) % 4;
	int len = (padWidth * padHeight) * 4;
	if (len % 32) len += (32 - len % 32);
	return memalign(32, len);
}

void OgcImageRenderer::loadTextureData(void* texture, const uint8_t* rgba, int width, int height)
{
	if(!texture || !rgba) return;
	uint8_t* dst = (uint8_t*)texture;
	int padWidth = width + (4 - width % 4) % 4;
	int padHeight = height + (4 - height % 4) % 4;

	for (int y = 0; y < padHeight; y++) {
		for (int x = 0; x < padWidth; x++) {
			uint32_t offset = ((((y >> 2) * (padWidth >> 2) + (x >> 2)) << 5) + ((y & 3) << 2) + (x & 3)) << 1;
			if (y >= height || x >= width) {
				dst[offset] = 0; dst[offset+1] = 255; dst[offset+32] = 255; dst[offset+33] = 255;
			} else {
				const uint8_t* src = rgba + (y * width + x) * 4;
				dst[offset]   = src[3]; // A
				dst[offset+1] = src[0]; // R
				dst[offset+32] = src[1]; // G
				dst[offset+33] = src[2]; // B
			}
		}
	}

	int len = (padWidth * padHeight) * 2;
	if (len % 32) len += (32 - len % 32);
	DCFlushRange(dst, len);
}

void OgcImageRenderer::destroyTexture(void * texture)
{
	if(texture)
		free(texture);
}

void OgcImageRenderer::drawTexture(void * texture, float xpos, float ypos, uint16_t width, uint16_t height, float degrees, float scaleX, float scaleY, uint8_t alpha)
{
	if(!texture)
		return;

	GXTexObj texObj;

	GX_InitTexObj(&texObj, texture, width, height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_InvalidateTexAll();

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	Mtx m,m1,m2, mv;
	width  >>= 1;
	height >>= 1;

	guMtxScale(m1, scaleX, scaleY, 1.0);
	guVector axis = (guVector) {0 , 0, 1 };
	guMtxRotAxisDeg (m2, &axis, degrees);
	guMtxConcat(m2,m1,m);

	guMtxTransApply(m,m, xpos+width,ypos+height,0);
	guMtxConcat (GXmodelView2D, m, mv);
	GX_LoadPosMtxImm (mv, GX_PNMTX0);

	GX_Begin(GX_QUADS, GX_VTXFMT0,4);
	GX_Position3f32(-width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(width, -height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(-width, height,  0);
	GX_Color4u8(0xFF,0xFF,0xFF,alpha);
	GX_TexCoord2f32(0, 1);
	GX_End();
	GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

void OgcImageRenderer::drawRectangle(float x, float y, float width, float height, PixelColor color)
{
	long n = 4;
	float x2 = x+width;
	float y2 = y+height;
	guVector v[] = {{x,y,0.0f}, {x2,y,0.0f}, {x2,y2,0.0f}, {x,y2,0.0f}, {x,y,0.0f}};

	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n);
	for(long i=0; i<n; ++i)
	{
		GX_Position3f32(v[i].x, v[i].y,  v[i].z);
		GX_Color4u8(color.r, color.g, color.b, color.a);
	}
	GX_End();
}
