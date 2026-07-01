/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * video.cpp
 *
 * Video routines
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/cond.h>
#include <ogc/machine/processor.h>

#include "snes9xgx.h"
#include "menu.h"
#include "videofilters.h"
#include "filelist.h"
#include "audio.h"
#include "gui/gui.h"
#include "input.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"

extern void UpdatePlaybackRate(void);

/*** Snes9x GFX Buffer ***/
#define EXT_WIDTH (MAX_SNES_WIDTH + 4)
#define EXT_PITCH (EXT_WIDTH * 2)
#define EXT_HEIGHT (MAX_SNES_HEIGHT + 4)
// Offset into buffer to allow a two pixel border around the whole rendered
// SNES image. This is a speed up hack to allow some of the image processing
// routines to access black pixel data outside the normal bounds of the buffer.
#define EXT_OFFSET (EXT_PITCH * 2 + 2 * 2)

#define SNES9XGFX_SIZE 		(EXT_PITCH*EXT_HEIGHT)

static unsigned char * snes9xgfx = NULL;

/*** 2D Video ***/
static u32 *xfb[2] = { NULL, NULL }; // Double buffered
static int whichfb = 0; // Switch
GXRModeObj *vmode = NULL; // Current video mode
int screenheight = 480;
int screenwidth = 640;
static int oldRenderMode = -1; // set to GCSettings.render when changing (temporarily) to another mode
int CheckVideo = 0; // for forcing video reset

/*** GX ***/
#define TEX_WIDTH 512
#define TEX_HEIGHT 512
#define TEXTUREMEM_SIZE 	TEX_WIDTH*(TEX_HEIGHT+8)*2
static unsigned char texturemem[TEXTUREMEM_SIZE] ATTRIBUTE_ALIGN (32);
static unsigned char scanline_tex_data[32] ATTRIBUTE_ALIGN (32);

#define DEFAULT_FIFO_SIZE 256 * 1024
static volatile unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static GXTexObj texobj;
static GXTexObj scanlineTexObj;
static Mtx view;
static Mtx modelView;
static Mtx GXmodelView2D;
static int vwidth, vheight, oldvwidth, oldvheight;

u8 * gameScreenPng = NULL;
int gameScreenPngSize = 0;

u32 FrameTimer = 0;

bool vmode_60hz = true;
int timerstyle = 0;
bool progressive = 0;

#define HASPECT 320
#define VASPECT 240

/* New texture based scaler */
typedef struct tagcamera
{
	guVector pos;
	guVector up;
	guVector view;
}
camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
	 Think of the output as a -80 x 80 by -60 x 60 graph.
***/
static s16 square[] ATTRIBUTE_ALIGN (32) =
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

/*** Custom Video modes (used to emulate original console video modes) ***/

/** Original SNES PAL Resolutions: **/

/* 239 lines progressive (PAL 50Hz) */
static GXRModeObj TV_239p =
{
	VI_TVMODE_PAL_DS,       // viDisplayMode
	512,             // fbWidth
	239,             // efbHeight
	239,             // xfbHeight
	(VI_MAX_WIDTH_PAL - 644)/2,         // viXOrigin
	(VI_MAX_HEIGHT_PAL/2 - 478/2)/2,        // viYOrigin
	644,             // viWidth
	478,             // viHeight
	VI_XFBMODE_SF,   // xFBmode
	GX_FALSE,        // field_rendering
	GX_FALSE,        // aa

	// sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

	// vertical filter[7], 1/64 units, 6 bits each
	{
		0,         // line n-1
		0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		0,         // line n+1
		0          // line n+1
	}
};

/* 478 lines interlaced (PAL 50Hz, Deflicker) */
static GXRModeObj TV_478i =
{
	VI_TVMODE_PAL_INT,      // viDisplayMode
	512,             // fbWidth
	478,             // efbHeight
	478,             // xfbHeight
	(VI_MAX_WIDTH_PAL - 644)/2,         // viXOrigin
	(VI_MAX_HEIGHT_PAL - 478)/2,        // viYOrigin
	644,             // viWidth
	478,             // viHeight
	VI_XFBMODE_DF,   // xFBmode
	GX_FALSE,         // field_rendering
	GX_FALSE,        // aa

	// sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

	// vertical filter[7], 1/64 units, 6 bits each
	{
		8,         // line n-1
		8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		8,         // line n+1
		8          // line n+1
	}
};

/** Original SNES NTSC Resolutions: **/

/* 224 lines progressive (NTSC or PAL 60Hz) */
static GXRModeObj TV_224p =
{
	VI_TVMODE_EURGB60_DS,      // viDisplayMode
	512,             // fbWidth
	224,             // efbHeight
	224,             // xfbHeight
	(VI_MAX_WIDTH_NTSC - 644)/2,	// viXOrigin
	(VI_MAX_HEIGHT_NTSC/2 - 448/2)/2,	// viYOrigin
	644,             // viWidth
	448,             // viHeight
	VI_XFBMODE_SF,   // xFBmode
	GX_FALSE,        // field_rendering
	GX_FALSE,        // aa

	// sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

	// vertical filter[7], 1/64 units, 6 bits each
	{
		0,         // line n-1
		0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		0,         // line n+1
		0          // line n+1
	}
};

/* 448 lines interlaced (NTSC or PAL 60Hz, Deflicker) */
static GXRModeObj TV_448i =
{
	VI_TVMODE_EURGB60_INT,     // viDisplayMode
	512,             // fbWidth
	448,             // efbHeight
	448,             // xfbHeight
	(VI_MAX_WIDTH_NTSC - 644)/2,        // viXOrigin
	(VI_MAX_HEIGHT_NTSC - 448)/2,       // viYOrigin
	644,             // viWidth
	448,             // viHeight
	VI_XFBMODE_DF,   // xFBmode
	GX_FALSE,         // field_rendering
	GX_FALSE,        // aa


	// sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

	// vertical filter[7], 1/64 units, 6 bits each
	{
		8,         // line n-1
		8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		8,         // line n+1
		8          // line n+1
	}
};

static GXRModeObj TV_Custom;

/* TV Modes table */
static GXRModeObj *tvmodes[4] = {
	&TV_239p, &TV_478i,			/* SNES PAL video modes */
	&TV_224p, &TV_448i,			/* SNES NTSC video modes */
};

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
static lwp_t vbthread = LWP_THREAD_NULL;
static lwpq_t render_queue;          // Queue for the main thread to sleep on
static lwpq_t vb_queue;              // Queue for the VSync thread to sleep on
static volatile bool vb_done = true; // Tracks if the VSync thread has completed its wait

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank
 ***************************************************************************/
static void *
vbgetback (void *arg)
{
	while (1)
	{
		LWP_ThreadSleep(vb_queue);     // Sleep until kicked off by copy_to_xfb
		VIDEO_WaitVSync ();	         /**< Wait for video vertical blank */
		vb_done = true;
		LWP_ThreadSignal(render_queue); // Instantly alert the main thread if it is waiting
	}

	return NULL;
}

/****************************************************************************
 * copy_to_xfb
 *
 * Stock code to copy the GX buffer to the current display mode.
 * Also increments the frameticker, as it's called for each vb.
 ***************************************************************************/
static inline void
copy_to_xfb (u32 arg)
{
	if (copynow == GX_TRUE)
	{
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
		LWP_ThreadSignal(render_queue); // Wake up the main thread if it is waiting for the copy
	}
	++FrameTimer;
}

/****************************************************************************
 * Scanline Support Functions
 ***************************************************************************/

static void InitScanlineTexture() {
	// GX_TF_I8 represents one byte per pixel.
	// We create an 8x4 tile: Rows 0 and 2 are white (0xFF), Rows 1 and 3 are dark (0xA0).
	for (int y = 0; y < 4; y++) {
		u8 intensity = (y % 2 == 0) ? 0xFF : 0xA0; // 0xA0 controls the scanline darkness
		for (int x = 0; x < 8; x++) {
			scanline_tex_data[y * 8 + x] = intensity;
		}
	}

	// CRITICAL: Flush the CPU data cache. GX reads directly from main memory.
	DCStoreRange(scanline_tex_data, 32);

	// Initialize the texture object. Wrap modes MUST be GX_REPEAT to tile across the screen.
	GX_InitTexObj(&scanlineTexObj, scanline_tex_data, 8, 4, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);

	// CRITICAL: Filter mode MUST be GX_NEAR. GX_LINEAR will blur the lines into a muddy gray.
	GX_InitTexObjFilterMode(&scanlineTexObj, GX_NEAR, GX_NEAR);

	// Load the scanline texture into MAP1
	GX_LoadTexObj(&scanlineTexObj, GX_TEXMAP1);
}

static void SetupScanlineFilterTEV() {
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);

	// Allow a second texture coordinate to be passed to the vertex stream
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	// Enable two textures and two TEV stages
	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
	GX_SetNumChans(0);

	// Configure Texture Coordinate Generation for both textures
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);

	// --- STAGE 0: Sample the Game Screen ---
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// Configure Stage 0 Alpha path
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// --- STAGE 1: Multiply by Scanlines ---
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLORNULL);
	// Formula: d + ((1.0 - c) * a + c * b)
	// By setting: a=ZERO, b=CPREV, c=TEXC, d=ZERO -> (TEXC * CPREV)
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	// Configure Stage 1 Alpha path (Pass-through blend)
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

/****************************************************************************
 * Scaler Support Functions
 ***************************************************************************/
static inline void
draw_init ()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		SetupScanlineFilterTEV();
	}
	else {
		GX_SetNumTexGens (1);
		GX_SetNumTevStages (1);
		GX_SetNumChans (0);

		GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

		GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	}

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	
	Mtx m;
	guMtxTrans (m, 0, 0, -100);
	guMtxConcat (view, m, modelView);

	GX_LoadPosMtxImm (modelView, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
}

static inline void
draw_vert (u8 pos, f32 s, f32 t)
{
	GX_Position1x8 (pos);
	GX_TexCoord2f32 (s, t);
}

static inline void
draw_square ()
{
	GX_LoadPosMtxImm (modelView, GX_PNMTX0);
	
	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);

	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		// Calculate physical dimensions of the rendering quad in EFB pixels
		// We use the static 'square' array which holds the final scaled/zoomed screen footprint
		// square[3] and square[0] are the Right and Left X bounds
		// square[1] and square[7] are the Top and Bottom Y bounds
		f32 quad_width = (f32)(square[3] - square[0]);
		f32 quad_height = (f32)(square[1] - square[7]);

		// Map exactly 1 texel to 1 EFB physical TV pixel
		// Our scanline texture is 8 pixels wide and 4 pixels high
		f32 u_repeat = quad_width / 8.0f;
		f32 v_repeat = quad_height / 4.0f;

		// The "Half-Texel Offset" Epsilon.
		// By shifting the UV start coordinates by exactly half a texel, we force the
		// GPU sampler to hit the 'dead center' of the texture pixels (e.g. 0.5, 1.5, 2.5),
		// preventing the moiré effect caused by floating-point edge-rounding.
		// U: 1/8 texel = 0.125. Half of that = 0.0625f
		// V: 1/4 texel = 0.25. Half of that = 0.125f
		f32 u_off = 0.0625f;
		f32 v_off = 0.125f;

		draw_vert (0, 0.0f, 0.0f); // TEX0
		GX_TexCoord2f32 (u_off, v_off); // TEX1

		draw_vert (1, 1.0f, 0.0f); // TEX0
		GX_TexCoord2f32 (u_repeat + u_off, v_off); // TEX1

		draw_vert (2, 1.0f, 1.0f); // TEX0
		GX_TexCoord2f32 (u_repeat + u_off, v_repeat + v_off); // TEX1

		draw_vert (3, 0.0f, 1.0f); // TEX0
		GX_TexCoord2f32 (u_off, v_repeat + v_off); // TEX1
	}
	else {
		draw_vert (0, 0.0f, 0.0f);
		draw_vert (1, 1.0f, 0.0f);
		draw_vert (2, 1.0f, 1.0f);
		draw_vert (3, 0.0f, 1.0f);
	}
	GX_End ();

	if(GCSettings.FilterMethod == FILTER_SCANLINES) {
		// force identity matrix to ensure texture mapping is pristine and devoid of stray scaling
		Mtx texMtx;
		guMtxIdentity(texMtx);
		GX_LoadTexMtxImm(texMtx, GX_TEXMTX1, GX_MTX2x4);
	}
}

/****************************************************************************
 * StopGX
 *
 * Stops GX (when exiting)
 ***************************************************************************/
void StopGX()
{
	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(true);
	VIDEO_Flush();
}

/****************************************************************************
 * FindVideoMode
 *
 * Finds the optimal video mode, or uses the user-specified one
 * Also configures original video modes
 ***************************************************************************/
static GXRModeObj * FindVideoMode()
{
	GXRModeObj * mode;

	// choose the desired video mode
	switch(GCSettings.videomode)
	{
		case VIDEOMODE_NTSC: // NTSC (480i)
			mode = &TVNtsc480IntDf;
			break;
		case VIDEOMODE_PROGRESSIVE: // Progressive (480p)
			mode = &TVNtsc480Prog;
			break;
		case VIDEOMODE_PAL: // PAL (50Hz)
			mode = &TVPal576IntDfScale;
			break;
		case VIDEOMODE_PAL60: // PAL (60Hz)
			mode = &TVEurgb60Hz480IntDf;
			break;
		case VIDEOMODE_PROGRESSIVE_576P: // Progressive (576p)
			mode = &TVPal576ProgScale;
			break;
		default:
			mode = VIDEO_GetPreferredMode(NULL);
			break;
	}

	// configure original modes
	switch (mode->viTVMode >> 2)
	{
		case VI_PAL:
			// 576 lines (PAL 50Hz)
			vmode_60hz = false;

			// Original Video modes (forced to PAL 50Hz)
			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE_PAL_DS;
			TV_478i.viTVMode = VI_TVMODE_PAL_INT;
			TV_224p.viTVMode = VI_TVMODE_PAL_DS;
			TV_448i.viTVMode = VI_TVMODE_PAL_INT;
			// set VI position
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_PAL - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_PAL - 448)/2;
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60Hz)
			vmode_60hz = true;

			// Original Video modes (forced to NTSC 60hz)
			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_478i.viTVMode = VI_TVMODE_NTSC_INT;
			TV_224p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_448i.viTVMode = VI_TVMODE_NTSC_INT;
			// set VI position
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 448)/2;
			break;

		default:
			// 480 lines (PAL 60Hz)
			vmode_60hz = true;

			// Original Video modes (forced to PAL 60hz)
			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE(mode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_478i.viTVMode = VI_TVMODE(mode->viTVMode >> 2, VI_INTERLACE);
			TV_224p.viTVMode = VI_TVMODE(mode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_448i.viTVMode = VI_TVMODE(mode->viTVMode >> 2, VI_INTERLACE);
			// set VI position
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 448)/2;
			break;
	}

	// check for progressive scan
	if ((mode->viTVMode & 3) == VI_PROGRESSIVE)
		progressive = true;
	else
		progressive = false;

	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		mode->viWidth = 678;
	else
		mode->viWidth = 672;

	if (vmode_60hz)
	{
		mode->viXOrigin = (VI_MAX_WIDTH_NTSC - mode->viWidth) / 2;
		mode->viYOrigin = (VI_MAX_HEIGHT_NTSC - mode->viHeight) / 2;
	}
	else
	{
		mode->viXOrigin = (VI_MAX_WIDTH_PAL - mode->viWidth) / 2;
		mode->viYOrigin = (VI_MAX_HEIGHT_PAL - mode->viHeight) / 2;
	}
	#endif
	return mode;
}

/****************************************************************************
 * SetupVideoMode
 *
 * Sets up the given video mode
 ***************************************************************************/
static void SetupVideoMode(GXRModeObj * mode)
{
	if(vmode == mode)
		return;

	VIDEO_SetPostRetraceCallback (NULL);
	copynow = GX_FALSE;
	VIDEO_Configure (mode);
	VIDEO_Flush();

	// Clear framebuffers etc.
	VIDEO_ClearFrameBuffer (mode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (mode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer (xfb[0]);

	VIDEO_SetBlack (false);
	VIDEO_Flush ();
	VIDEO_WaitForFlush ();
	
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)copy_to_xfb);
	vmode = mode;
}

/****************************************************************************
 * InitVideo
 *
 * This function MUST be called at startup.
 * - also sets up menu video mode
 ***************************************************************************/
void
InitVideo ()
{
	VIDEO_Init();

	// Allocate the video buffers
	xfb[0] = (u32 *) memalign(32, 640*576*2);
	xfb[1] = (u32 *) memalign(32, 640*576*2);
	DCInvalidateRange(xfb[0], 640*576*2);
	DCInvalidateRange(xfb[1], 640*576*2);
	xfb[0] = (u32 *) MEM_K0_TO_K1 (xfb[0]);
	xfb[1] = (u32 *) MEM_K0_TO_K1 (xfb[1]);

	GXRModeObj *rmode = FindVideoMode();

#ifdef HW_RVL
if (CONF_GetAspectRatio() == CONF_ASPECT_16_9 && (*(u32*)(0xCD8005A0) >> 16) == 0xCAFE) // Wii U
{
	write32(0xd8006a0, 0x30000004), mask32(0xd8006a8, 0, 2);
}
#endif

	SetupVideoMode(rmode);

	// Setup synchronization queues
	LWP_InitQueue(&render_queue);
	LWP_InitQueue(&vb_queue);
	vb_done = true;
	LWP_CreateThread (&vbthread, vbgetback, NULL, NULL, 0, 68);
	
	// Initialize GX
	GXColor background = { 0, 0, 0, 0xff };
	memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear (background, GX_MAX_Z24);
	GX_SetDispCopyGamma (GX_GM_1_0);
	GX_SetCullMode (GX_CULL_NONE);

	vwidth = 100;
	vheight = 100;
}

void ResetFbWidth(int width, GXRModeObj *rmode)
{
	if(rmode->fbWidth == width)
		return;
	
	rmode->fbWidth = width;
	
	if(rmode != vmode)
		return;
	
	GX_InvVtxCache();
	VIDEO_Configure(rmode);
	VIDEO_Flush();
}

/****************************************************************************
 * ResetVideo_Emu
 *
 * Reset the video/rendering mode for the emulator rendering
****************************************************************************/
void
ResetVideo_Emu ()
{
	GXRModeObj *rmode = FindVideoMode();

	Mtx44 p;
	int i = -1;

	// original render mode or hq2x
	if (GCSettings.render == RENDER_ORIGINAL)
	{
		for (int j=0; j<4; j++)
		{
			if (tvmodes[j]->efbHeight == vheight)
			{
				i = j;
				break;
			}
		}
	}

	if(i >= 0) // we found a matching original mode
	{
		rmode = tvmodes[i];

		// hack to fix video output for hq2x (only when actually filtering; h<=239, w<=256)
		if (GCSettings.FilterMethod != FILTER_NONE && vheight <= 239 && vwidth <= 256)
		{
			memcpy(&TV_Custom, tvmodes[i], sizeof(TV_Custom));
			rmode = &TV_Custom;

			rmode->fbWidth = 512;
			rmode->efbHeight *= 2;
			rmode->xfbHeight *= 2;
			rmode->xfbMode = VI_XFBMODE_DF;
			rmode->viTVMode |= VI_INTERLACE;
		}

		if (Settings.PAL == 1)
			Settings.SoundInputRate = 32090;
		else
			Settings.SoundInputRate = 31894;
		UpdatePlaybackRate();
	}
	else
	{
		if (GCSettings.widescreen)
			ResetFbWidth(640, rmode);
		else
			ResetFbWidth(512, rmode);
		
		Settings.SoundInputRate = 31920;
		UpdatePlaybackRate();
	}

	SetupVideoMode(rmode); // reconfigure VI

	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, GX_MAX_Z24);

	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	u8 sharp[7] = {0,0,21,22,21,0,0};
	u8 soft[7] = {8,8,10,12,10,8,8};
	u8* vfilter =
		GCSettings.render == RENDER_FILTERED_SHARP ? sharp
		: GCSettings.render == RENDER_FILTERED_SOFT ? soft
		: rmode->vfilter;
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, (rmode->xfbMode == VI_XFBMODE_SF) ? GX_FALSE : GX_TRUE, vfilter);

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);
	GX_SetBlendMode (GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	draw_init ();
}

/****************************************************************************
 * MakeTexturePitch1032

 * High-performance texture swizzling (Linear to 4x4 Tiled)
 * Specifically optimized for 1032-byte stride (SNES buffer padding)
 * - Eliminates pipeline stalls via interleaved load/store sequences
 * - Utilizes dcbz (Data Cache Block Zero) to bypass read-allocate memory penalty
 * - Avoids stwu pointer-update instructions to enable out-of-order execution
 * - Maximizes GPR utilization for sustained Instruction Level Parallelism
 * COMPATIBILITY:
 * - Optimized for Snes9x internal video buffers (1032-byte pitch)
 * - Requires width and height divisible by 4
 * - Assumes 16-bit RGB565 format (2 bytes per pixel)
 * ASSUMPTIONS:
 * - Source pointer is aligned to 4-byte boundary
 * - Destination pointer is aligned to 32-byte boundary
 ***************************************************************************/

void MakeTexturePitch1032(const void *src, void *dst, s32 width, s32 height)
{
    // Request dedicated registers from GCC to allow superscalar interleaving
    u32 r_src_row=0, tmpA=0, tmpB=0, tmpC=0, tmpD=0;

    __asm__ __volatile__ (
        "srwi   %[width], %[width], 2\n"       // num_tiles_x = width / 4
        "srwi   %[height], %[height], 2\n"     // num_tiles_y = height / 4

    "2: mtctr   %[width]\n"                    // Set inner loop counter (X)
        "mr     %[r_src_row], %[src]\n"        // Save the start of the current source row

    "1: dcbz    0, %[dst]\n"                   // ZERO L1 CACHE: Skips read-from-RAM penalty

        // -- Load Tile Half 1 (Rows 0 & 1) --
        "lwz    %[tmpA], 0(%[src])\n"
        "lwz    %[tmpB], 4(%[src])\n"
        "lwz    %[tmpC], 1032(%[src])\n"
        "lwz    %[tmpD], 1036(%[src])\n"

        // -- Store Half 1 while Loading Tile Half 2 (Rows 2 & 3) --
        // By interleaving here, we completely hide the memory load latency!
        "stw    %[tmpA], 0(%[dst])\n"
        "lwz    %[tmpA], 2064(%[src])\n"

        "stw    %[tmpB], 4(%[dst])\n"
        "lwz    %[tmpB], 2068(%[src])\n"

        "stw    %[tmpC], 8(%[dst])\n"
        "lwz    %[tmpC], 3096(%[src])\n"

        "stw    %[tmpD], 12(%[dst])\n"
        "lwz    %[tmpD], 3100(%[src])\n"

        // -- Store Half 2 --
        "stw    %[tmpA], 16(%[dst])\n"
        "stw    %[tmpB], 20(%[dst])\n"
        "stw    %[tmpC], 24(%[dst])\n"
        "stw    %[tmpD], 28(%[dst])\n"

        // -- Advance Pointers --
        "addi   %[src], %[src], 8\n"           // Advance X by 2 pixels (8 bytes)
        "addi   %[dst], %[dst], 32\n"          // Advance dst by 1 full tile
        "bdnz   1b\n"                          // Decrement CTR, loop if > 0

        // -- Next Tile Row --
        "addi   %[src], %[r_src_row], 4128\n"  // Jump 4 rows down (1032 * 4)
        "subic. %[height], %[height], 1\n"     // Decrement height counter
        "bne    2b"                            // Loop Y

        // Constraints mapping
        : [r_src_row] "=&b" (r_src_row),
          [tmpA] "=&r" (tmpA),
          [tmpB] "=&r" (tmpB),
          [tmpC] "=&r" (tmpC),
          [tmpD] "=&r" (tmpD),
          [dst] "+b" (dst),
          [src] "+b" (src),
          [width] "+r" (width),
          [height] "+r" (height)
        : // No input-only operands
        : "memory"
    );
}

/****************************************************************************
 * Update Video
 ***************************************************************************/
uint32 prevRenderedFrameCount = 0;
static int fscale = 1;

void
update_video (int width, int height)
{
	vwidth = width;
	vheight = height;

	if(CheckVideo == 2 && IPPU.RenderedFramesCount == prevRenderedFrameCount)
		return; // we haven't rendered any frames yet, so we can't draw anything!

	// Wait for the VI to display the PREVIOUSLY submitted frame
	// This naturally throttles the emulator to the TV's refresh rate
	while (!vb_done || (copynow == GX_TRUE))
	{
		LWP_ThreadSleep(render_queue); // Halts main thread with 0 CPU load until signals occur
	}

	// Guarantee the GPU has fully finished rendering the previous frame
	// before we begin swizzling new data into texturemem
	GX_DrawDone();

	whichfb ^= 1;

	if (oldvheight != vheight || oldvwidth != vwidth) // if rendered width/height changes, update scaling
		CheckVideo = 1;

	if (CheckVideo)	// if we get back from the menu, and have rendered at least 1 frame
	{
		int xscale, yscale;
#ifdef HW_RVL
		if(vwidth <= 256)
			fscale = GetFilterScale();
		else
			fscale = 1;
#endif
		ResetVideo_Emu ();	// reset video to emulator rendering settings

		/** Update scaling **/
		if (GCSettings.render == RENDER_ORIGINAL)	// original render mode
		{
			if (GCSettings.FilterMethod != FILTER_NONE && vheight <= 239 && vwidth <= 256)
			{	// filters; normal operation
				xscale = vwidth;
				yscale = vheight;
			}
			else
			{	// no filtering
				fscale = 1;
				xscale = 256;
				yscale = vheight / 2;
			}

			if (GCSettings.widescreen) {
				xscale = (3*xscale)/4;
			}
		}
		else // unfiltered and filtered mode
		{
			if (GCSettings.widescreen) {
				// Determine the raw height of the SNES signal
				float base_height = (vheight == 224 || vheight == 448) ? 224.0f : 239.0f;

				// Calculate the uniform scale required to make the height fill the 480 screen
				float scale_factor = (vmode->efbHeight / 2.0f) / base_height;

				// Apply the exact same scale factor to both the width and the height
				xscale = (256.0f * scale_factor * 15) / 16; // Mathematically perfect compensation for the 640 widescreen EFB
				yscale = vmode->efbHeight / 2;
			}
			else {
				xscale = 256;

				if(vheight == 224 || vheight == 448)
					yscale = 224;
				else
					yscale = 239;
			}
		}

		xscale *= GCSettings.zoomHor;
		yscale *= GCSettings.zoomVert;

		square[6] = square[3]  =  xscale + GCSettings.xshift;
		square[0] = square[9]  = -xscale + GCSettings.xshift;
		square[4] = square[1]  =  yscale - GCSettings.yshift;
		square[7] = square[10] = -yscale - GCSettings.yshift;
		DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!
    	draw_init ();

		// initialize the texture obj we are going to use
		GX_InitTexObj (&texobj, texturemem, vwidth*fscale, vheight*fscale, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

		if (GCSettings.render == RENDER_ORIGINAL || GCSettings.render == RENDER_UNFILTERED)
			GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR); // original/unfiltered video mode: force texture filtering OFF

		GX_LoadTexObj (&texobj, GX_TEXMAP0); // load texture object so its ready to use

		if(GCSettings.FilterMethod == FILTER_SCANLINES)
			InitScanlineTexture();

		oldvwidth = vwidth;
		oldvheight = vheight;
		CheckVideo = 0;
	}
#ifdef HW_RVL
	// convert image to texture
	if (GCSettings.FilterMethod != FILTER_NONE &&
		GCSettings.FilterMethod != FILTER_SCANLINES &&
		vheight <= 239 && vwidth <= 256)	// don't do filtering on game textures > 256 x 239
	{
		FilterMethod ((uint8*) GFX.Screen, EXT_PITCH, (uint8*) texturemem, vwidth*fscale*2, vwidth, vheight);
	}
	else
#endif
	{
		MakeTexturePitch1032((char *) GFX.Screen, (char *) texturemem, vwidth, vheight);
	}
	
	// Pad dimensions to 4x4 tile boundaries
	u32 padded_width = (vwidth * fscale + 3) & ~3;
	u32 padded_height = (vheight * fscale + 3) & ~3;

	// A 4x4 tile is 16 pixels * 2 bytes = 32 bytes
	// Padded dimensions guarantee the result is naturally a multiple of 32
	u32 flush_size = padded_width * padded_height * 2;

	DCStoreRange(texturemem, flush_size); // update the texture memory
	GX_InvalidateTexAll ();

	draw_square ();		// draw the quad

	if(ScreenshotRequested)
	{
		if(GCSettings.render == RENDER_ORIGINAL) // we can't take a screenshot in Original mode
		{
			oldRenderMode = RENDER_ORIGINAL;
			GCSettings.render = RENDER_UNFILTERED; // switch to unfiltered mode
			CheckVideo = 1; // request the switch
		}
		else
		{
			// We MUST wait for the GPU to finish the CURRENT frame before 
			// reading from the EFB to encode the PNG
			GX_DrawDone();
			ScreenshotRequested = 0;
			TakeScreenshot();
			if(oldRenderMode != -1)
			{
				GCSettings.render = oldRenderMode;
				oldRenderMode = -1;
			}
			ConfigRequested = 1;
		}
	}

	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	copynow = GX_TRUE;

	// Reset state and signal background VSync thread to begin waiting for next blanking interval
	vb_done = false;
	LWP_ThreadSignal(vb_queue);
}

void AllocGfxMem()
{
	snes9xgfx = (unsigned char *)memalign(32, SNES9XGFX_SIZE);
	memset(snes9xgfx, 0, SNES9XGFX_SIZE);

	GFX.Pitch = EXT_PITCH;
	GFX.Screen = (uint16*)(snes9xgfx + EXT_OFFSET);
}

/****************************************************************************
 * setGFX
 *
 * Setup the global GFX information for Snes9x
 ***************************************************************************/
void
setGFX ()
{
	GFX.Pitch = EXT_PITCH;
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current screen into a GX texture
 ***************************************************************************/
void TakeScreenshot()
{
	IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

	if (pngContext != NULL)
	{
		gameScreenPngSize = PNGU_EncodeFromEFB(pngContext, vmode->fbWidth, vmode->efbHeight);
		PNGU_ReleaseImageContext(pngContext);

		if (gameScreenPngSize <= 0) {
			gameScreenPngSize = 0;
			return;
		}

		gameScreenPng = (u8 *) malloc(gameScreenPngSize);
		if (gameScreenPng == NULL) {
			gameScreenPngSize = 0;
			return;
		}
		memcpy(gameScreenPng, savebuffer, gameScreenPngSize);
	}
}

void ClearScreenshot()
{
	if(gameScreenPng)
	{
		gameScreenPngSize = 0;
		free(gameScreenPng);
		gameScreenPng = NULL;
	}
}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	Mtx44 p;
	f32 yscale;
	u32 xfbHeight;
	GXRModeObj * rmode = FindVideoMode();

	SetupVideoMode(rmode); // reconfigure VI

	// clears the bg to color and clears the z buffer
	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, GX_MAX_Z24);

	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_InvVtxCache ();
	GX_InvalidateTexAll();

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc (GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_TRUE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

	guOrtho(p,0,479,0,639,0,300);
	GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);

	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
}

/****************************************************************************
 * Menu_Render
 *
 * Renders everything current sent to GX, and flushes video
 ***************************************************************************/
void Menu_Render()
{
	whichfb ^= 1; // flip framebuffer
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb],GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	VIDEO_WaitForFlush();
}

/****************************************************************************
 * Menu_DrawImg
 *
 * Draws the specified image on screen using GX
 ***************************************************************************/
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[],
	f32 degrees, f32 scaleX, f32 scaleY, u8 alpha)
{
	if(data == NULL)
		return;

	GXTexObj texObj;

	GX_InitTexObj(&texObj, data, width,height, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
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

/****************************************************************************
 * Menu_DrawRectangle
 *
 * Draws a rectangle at the specified coordinates using GX
 ***************************************************************************/
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled)
{
	long n = 4;
	f32 x2 = x+width;
	f32 y2 = y+height;
	guVector v[] = {{x,y,0.0f}, {x2,y,0.0f}, {x2,y2,0.0f}, {x,y2,0.0f}, {x,y,0.0f}};
	u8 fmt = GX_TRIANGLEFAN;

	if(!filled)
	{
		fmt = GX_LINESTRIP;
		n = 5;
	}

	GX_Begin(fmt, GX_VTXFMT0, n);
	for(long i=0; i<n; ++i)
	{
		GX_Position3f32(v[i].x, v[i].y,  v[i].z);
		GX_Color4u8(color.r, color.g, color.b, color.a);
	}
	GX_End();
}
