#include <xenos/xe.h>
#include <stdio.h>

#include "gfx.h"

extern "C" void do_edram_foo(struct XenosDevice *xe, int complete);

struct XenosSurface *gfxplane;
struct XenosDevice _xe, *xe;
struct XenosShader *sh_ps, *sh_vs;
struct XenosVertexBuffer *vb;
struct XenosIndexBuffer *ib;

void videoInit(void)
{
	xe = &_xe;
		/* initialize the GPU */
	Xe_Init(xe);

	gfxplane = Xe_CreateTexture(xe, MAX_SNES_WIDTH, MAX_SNES_HEIGHT, 1, 4 | 0x40, 0);
	
	GFX.Screen = (uint16*)(((unsigned char*)gfxplane->base) + gfxplane->pitch * 2 + 2 * 2);
	GFX.Pitch = gfxplane->pitch;

		/* create a render target (the framebuffer) */
	struct XenosSurface *fb = Xe_GetFramebufferSurface(xe);
	Xe_SetRenderTarget(xe, fb);

	printf("framebuffer at: %p, pitch %d\n", fb->base, fb->pitch);

		/* let's define a vertex buffer format */
	static const struct XenosVBFFormat vbf =
	{
		2, {
		  {XE_USAGE_POSITION, 0, XE_TYPE_FLOAT2},
		  {XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2},
		}
	};
	
	float rect[] = {
		-1,  1, 0, 0,
		-1, -1, 0, 1,
		 1,  1, 1, 0,
		 1, -1, 1, 1
	};
	
	unsigned short rect_indices[] = {0, 2, 1, 1, 2, 3};

	printf("loading pixel shader...\n");
		/* load pixel shader */
	sh_ps = Xe_LoadShader(xe, "ps.psu");
	Xe_InstantiateShader(xe, sh_ps, 0);

	printf("loading vertex shader...\n");
		/* load vertex shader */
	sh_vs = Xe_LoadShader(xe, "vs.vsu");
	Xe_InstantiateShader(xe, sh_vs, 0);
	Xe_ShaderApplyVFetchPatches(xe, sh_vs, 0, &vbf);

	printf("create vb...\n");
		/* create and fill vertex buffer */
	vb = Xe_CreateVertexBuffer(xe, sizeof(rect));
	void *v = Xe_VB_Lock(xe, vb, 0, sizeof(rect), XE_LOCK_WRITE);
	memcpy(v, rect, sizeof(rect));
	Xe_VB_Unlock(xe, vb);

	printf("create ib...\n");
		/* create and fill index buffer */
	ib = Xe_CreateIndexBuffer(xe, sizeof(rect_indices), XE_FMT_INDEX16);
	unsigned short *i = (unsigned short*)Xe_IB_Lock(xe, ib, 0, sizeof(rect_indices), XE_LOCK_WRITE);
	memcpy(i, rect_indices, sizeof(rect_indices));
	Xe_IB_Unlock(xe, ib);

	do_edram_foo(xe, 1);
}

void videoBlit(int xres, int yres)
{
		/* flush cache */
	Xe_Surface_LockRect(xe, gfxplane, 0, 0, 0, 0, XE_LOCK_WRITE);
	Xe_Surface_Unlock(xe, gfxplane);


		/* we don't want the border. Note that because of interpolation, some fractional pixel values might get through. */	
	float *v = (float*)Xe_VB_Lock(xe, vb, 0, 16*4, XE_LOCK_WRITE);
	v[2] = v[6] = 2.0 / xres;
	v[3] = v[11] = 2.0 / yres;
	Xe_VB_Unlock(xe, vb);

		/* create new surface with right size */
	XenosSurface surface = *gfxplane;
	surface.width = xres;
	surface.height = yres;
		/* begin a new frame, i.e. reset all renderstates to the default */
	Xe_InvalidateState(xe);

	int max_vertices = 4;
	int nr_primitives = 2;

			/* draw cube */
	Xe_SetShader(xe, SHADER_TYPE_PIXEL, sh_ps, 0);
	Xe_SetShader(xe, SHADER_TYPE_VERTEX, sh_vs, 0);
	Xe_SetStreamSource(xe, 0, vb, 0, 4); /* using this vertex buffer */
	Xe_SetIndices(xe, ib); /* ... this index buffer... */
	Xe_SetTexture(xe, 0, &surface); /* ... and this texture */
	Xe_DrawIndexedPrimitive(xe, XE_PRIMTYPE_TRIANGLELIST, 0, 0, max_vertices, 0, nr_primitives);

		/* clear to white */
	Xe_SetClearColor(xe, ~0);

		/* resolve (and clear) */
	Xe_Resolve(xe);

		/* wait for render finish */
 	Xe_Sync(xe);
}
