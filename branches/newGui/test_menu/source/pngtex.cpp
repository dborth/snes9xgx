#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include "libpng/pngu/pngu.h"

#include "new_gui.h"

/*** GX ***/
unsigned int *xfb[2] = { NULL, NULL }; // Double buffered
int whichfb = 0; // Switch
GXRModeObj * vmode; // Menu video mode

#define TEX_WIDTH 512
#define TEX_HEIGHT 512
#define DEFAULT_FIFO_SIZE 256 * 1024
unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
unsigned char texturemem[TEX_WIDTH * (TEX_HEIGHT + 8)] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
Mtx view;

/*** Functions ***/

#define HASPECT 320
#define VASPECT 240

/* New texture based scaler */
typedef struct tagcamera
{
	Vector pos;
	Vector up;
	Vector view;
} camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
s16 square[] ATTRIBUTE_ALIGN (32) =
{
  /*
   * X,   Y,  Z
   * Values set are for roughly 4:3 aspect
   */
	-HASPECT,  VASPECT, 0,		// 0
	 HASPECT,  VASPECT, 0,	// 1
	 HASPECT, -VASPECT, 0,	// 2
	-HASPECT, -VASPECT, 0	// 3
};


static camera cam = {
	{0.0F, 0.0F, 0.0F},
	{0.0F, 0.5F, 0.0F},
	{0.0F, 0.0F, -0.5F}
};

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
#define TSTACK 16384
lwpq_t videoblankqueue;
lwp_t vbthread;
static unsigned char vbstack[TSTACK];

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank.
 *
 * Putting LWP to good use :)
 ***************************************************************************/
static void *
vbgetback (void *arg)
{
	while (1)
	{
		VIDEO_WaitVSync ();	 /**< Wait for video vertical blank */
		LWP_SuspendThread (vbthread);
	}

	return NULL;

}

/****************************************************************************
 * InitVideoThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ***************************************************************************/
void
InitVideoThread ()
{
	/*** Initialise a new queue ***/
	LWP_InitQueue (&videoblankqueue);

	/*** Create the thread on this queue ***/
	LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 80);
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ***************************************************************************/
static void
copy_to_xfb (u32 arg)
{
	if (copynow == GX_TRUE)
	{
		GX_SetAlphaUpdate(GX_ENABLE);
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
	}

//	FrameTimer++;
//	SMBTimer++;
}

/****************************************************************************
 * Scaler Support Functions
 ***************************************************************************/
static void
draw_init ()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	GX_SetNumTexGens (1);
	GX_SetNumChans (0);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	
	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);
	
	//GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

	GX_InvVtxCache ();	// update vertex cache
	//GX_InvalidateTexAll();
}

static void
draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
	GX_Position1x8 (pos);
	GX_Color1x8 (c);
	GX_TexCoord2f32 (s, t);
}

static void
draw_square (Mtx v)
{
	Mtx m;			// model matrix.
	Mtx mv;			// modelview matrix.

	guMtxIdentity (m);
	guMtxTransApply (m, m, 0, 0, -100);
	guMtxConcat (v, m, mv);

	GX_LoadPosMtxImm (mv, GX_PNMTX0);
	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
	draw_vert (0, 0, 0.0, 0.0);
	draw_vert (1, 0, 1.0, 0.0);
	draw_vert (2, 0, 1.0, 1.0);
	draw_vert (3, 0, 0.0, 1.0);
	GX_End ();
}

/****************************************************************************
 * StartGX
 *
 * This function initialises the GX.
 ***************************************************************************/
static void
StartGX ()
{
	Mtx p;

	GXColor background = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
	memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (background, 0x00ffffff);

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode (GX_CULL_NONE);
	GX_SetDispCopyGamma (GX_GM_1_0);
	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);
	
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);	// pxl = Asrc * SrcPixel + (1 - Asrc) * DstPixel
	GX_SetAlphaUpdate(GX_ENABLE);

	guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);


	GX_CopyDisp (xfb[whichfb], GX_TRUE); // reset xfb

//	vwidth = 100;
//	vheight = 100;
}

/****************************************************************************
 * UpdatePadsCB
 *
 * called by postRetraceCallback in InitGCVideo - scans gcpad and wpad
 ***************************************************************************/
void
UpdatePadsCB ()
{
#ifdef HW_RVL
	WPAD_ScanPads();
#endif
	PAD_ScanPads();
}

/****************************************************************************
 * InitGCVideo
 *
 * This function MUST be called at startup.
 * - also sets up menu video mode
 ***************************************************************************/
void
InitGCVideo ()
{
	// init video
    VIDEO_Init ();

	// get default video mode
	vmode = VIDEO_GetPreferredMode(NULL);

    VIDEO_Configure (vmode);

//    screenheight = vmode->xfbHeight;


	// Allocate the video buffers
    xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

    // A console is always useful while debugging.
    console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

    // Clear framebuffers etc.
    VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer (xfb[0]);

	// video callbacks
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)UpdatePadsCB);
    VIDEO_SetPreRetraceCallback ((VIRetraceCallback)copy_to_xfb);

    VIDEO_SetBlack (FALSE);
    VIDEO_Flush ();
    VIDEO_WaitVSync ();
    if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync ();

    copynow = GX_FALSE;
    StartGX ();

	draw_init ();

    InitVideoThread ();

    // Finally, the video is up and ready for use :)
}

GXTexObj texObj1;
GXTexObj texObj2;
GXTexObj texObj3;
GXTexObj texObj4;
GXTexObj texObj5;
GXTexObj texObj6;
void *texture_data1 = NULL;
void *texture_data2 = NULL;
void *texture_data3 = NULL;
void *texture_data4 = NULL;
void *texture_data5 = NULL;
void *texture_data6 = NULL;
PNGUPROP imgProp;


int main() 
{
	Mtx v;
	IMGCTX ctx;

	InitGCVideo ();
	
	WPAD_Init();

	// Init libfat
	fatInitDefault ();
	
	FT_Init ();

	// Load textures using PNGU
	
	// RGB 565
	ctx = PNGU_SelectImageFromDevice ("smw_bg.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data1 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 2);
	GX_InitTexObj (&texObj1, texture_data1, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGB565 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data1);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data1, imgProp.imgWidth * imgProp.imgHeight * 2);
	
	// RGBA8
	ctx = PNGU_SelectImageFromDevice ("bg.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data6 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	GX_InitTexObj (&texObj6, texture_data6, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data6, 255);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data6, imgProp.imgWidth * imgProp.imgHeight * 4);

/*
	ctx = PNGU_SelectImageFromDevice ("textRGB.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data2 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 2);
	GX_InitTexObj (&texObj2, texture_data2, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGB565 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data2, 255);		// 565
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data2, imgProp.imgWidth * imgProp.imgHeight * 2);

	ctx = PNGU_SelectImageFromDevice ("bg.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data3 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	GX_InitTexObj (&texObj3, texture_data3, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data3, 255);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data3, imgProp.imgWidth * imgProp.imgHeight * 4);

	ctx = PNGU_SelectImageFromDevice ("textRGBA.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data4 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 2);
	GX_InitTexObj (&texObj4, texture_data4, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGB565 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data4);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data4, imgProp.imgWidth * imgProp.imgHeight * 2);

	ctx = PNGU_SelectImageFromDevice ("textRGBA.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data5 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 2);
	GX_InitTexObj (&texObj5, texture_data5, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGB5A3 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data5, 255);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data5, imgProp.imgWidth * imgProp.imgHeight * 2);

	ctx = PNGU_SelectImageFromDevice ("textRGBA.png");
	PNGU_GetImageProperties (ctx, &imgProp);
	texture_data6 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	GX_InitTexObj (&texObj6, texture_data6, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, texture_data6, 255);
	PNGU_ReleaseImageContext (ctx);
	DCFlushRange (texture_data6, imgProp.imgWidth * imgProp.imgHeight * 4);
*/

	// DRAW
	guLookAt(v, &cam.pos, &cam.up, &cam.view);	// need!
	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);	// need!
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);	// pxl = Asrc * SrcPixel + (1 - Asrc) * DstPixel
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);	// pass only texture info
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetNumChans(0);
	
	//draw_init ();
	
	// draw 640x480 quads
	int xscale, yscale, xshift, yshift;
	xshift = yshift = 0;
	xscale = 320;
	yscale = 240;
	square[6] = square[3]  =  xscale + xshift;
	square[0] = square[9]  = -xscale + xshift;
	square[4] = square[1]  =  yscale + yshift;
	square[7] = square[10] = -yscale + yshift;
	
	square[2] = square[5] = square[8] = square[11] = 0;     // z value
	DCFlushRange (square, 32);

	GX_LoadTexObj(&texObj1, GX_TEXMAP0);	// snes bg
	draw_square(v);
	
	square[2] = square[5] = square[8] = square[11] = 1;     // z value
	DCFlushRange (square, 32);
	
	GX_LoadTexObj(&texObj6, GX_TEXMAP0);	// menu overlay
	draw_square(v);
/*
	GX_LoadTexObj(&texObj3, GX_TEXMAP0);
	draw_square(v);
	GX_LoadTexObj(&texObj4, GX_TEXMAP0);
	draw_square(v);
	GX_LoadTexObj(&texObj5, GX_TEXMAP0);
	draw_square(v);
	GX_LoadTexObj(&texObj6, GX_TEXMAP0);
	draw_square(v);
*/
	GX_DrawDone();
	
	//-----------------------------------//
	
	// want to copy blended image to texture for later use
	
	// load blended image from efb to a texture
	texture_data2 = memalign (32, vmode->fbWidth * vmode->efbHeight * 4);
	GX_InitTexObj (&texObj2, texture_data2, vmode->fbWidth, vmode->efbHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	// copy efb to texture
	GX_SetTexCopySrc ( 0, 0, vmode->fbWidth, vmode->efbHeight );
	GX_SetTexCopyDst ( vmode->fbWidth, vmode->efbHeight, GX_TF_RGBA8, 0 );
	GX_CopyTex (texture_data2, 0);
	GX_PixModeSync ();      // wait until copy has completed
	DCFlushRange (texture_data2, vmode->fbWidth * vmode->efbHeight * 4);

	//-------------------------------------//
	
	// alloc gui draw memory
	Gui.texmem = (u8 *) SYS_AllocArena2MemLo(640 * 480 * 4, 32);
	memset ( Gui.texmem, 0, 640*480*4 );
	
	// draw something on it
	gui_draw ();
	
	// REUSE texOBJ6 and texture_data6
	GX_InitTexObj (&texObj6, texture_data6, 640, 480, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);

	// convert to texture
	Make_Texture_RGBA8 (texture_data6, Gui.texmem, 640, 480);
	
	// update texture memory
	DCFlushRange (texture_data6, 640 * 480 * 4);
	
	//------------------------------------//
	
	// DRAW
	guLookAt(v, &cam.pos, &cam.up, &cam.view);	// need!
	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);	// need!
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);	// pxl = Asrc * SrcPixel + (1 - Asrc) * DstPixel
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);	// pass only texture info
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetNumChans(0);
	
	square[2] = square[5] = square[8] = square[11] = 0;     // z value
	DCFlushRange (square, 32);

	GX_LoadTexObj(&texObj2, GX_TEXMAP0);	// backdrop
	draw_square(v);
	
	square[2] = square[5] = square[8] = square[11] = 1;     // z value
	DCFlushRange (square, 32);
	
	GX_LoadTexObj(&texObj6, GX_TEXMAP0);	// menu
	draw_square(v);
	
	GX_DrawDone();

	
	//---------------------------//
	copynow = GX_TRUE;	// draw to screen and erase efb
	VIDEO_WaitVSync();
	
	while(1)
	{
//		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);

		if ( pressed & WPAD_BUTTON_HOME ) exit(0);

		//copynow = GX_TRUE;
		VIDEO_WaitVSync();
	}
	return 0;
}

