/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric October 2008
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
#include <wiiuse/wpad.h>
#include <ogc/texconv.h>
#include "snes9x.h"
#include "memmap.h"
#include "aram.h"
#include "snes9xGX.h"

#include "filter.h"
#include "gui.h"

/*** Snes9x GFX Buffer ***/
static unsigned char snes9xgfx[EXT_PITCH * EXT_HEIGHT];	// changed.
unsigned char filtermem[512 * MAX_SNES_HEIGHT * 4];	// only want ((512*2) X (239*2))

/*** 2D Video ***/
unsigned int *xfb[2] = { NULL, NULL }; // Double buffered
int whichfb = 0; // Switch
GXRModeObj *vmode; // Menu video mode
int screenheight;
extern u32* backdrop;

/*** GX ***/
#define TEX_WIDTH 512
#define TEX_HEIGHT 512
static unsigned char texturemem[TEX_WIDTH * (TEX_HEIGHT + 8) * 2] ATTRIBUTE_ALIGN (32);
#define DEFAULT_FIFO_SIZE 256 * 1024
unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
Mtx view;
static int vwidth, vheight, oldvwidth, oldvheight;

u32 FrameTimer = 0;

u8 vmode_60hz = 0;
bool progressive = 0;

#define HASPECT 320
#define VASPECT 240

/* New texture based scaler */
typedef struct tagcamera
{
	Vector pos;
	Vector up;
	Vector view;
}
camera;

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


/***
*** Custom Video modes (used to emulate original console video modes)
***/

/** Original SNES PAL Resolutions: **/

/* 239 lines progressive (PAL 50Hz) */
static GXRModeObj TV_239p =
{
	VI_TVMODE_PAL_DS,       // viDisplayMode
	512,             // fbWidth
	239,             // efbHeight
	239,             // xfbHeight
	(VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
	(VI_MAX_HEIGHT_PAL/2 - 478/2)/2,        // viYOrigin
	640,             // viWidth
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
	(VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
	(VI_MAX_HEIGHT_PAL - 478)/2,        // viYOrigin
	640,             // viWidth
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
	(VI_MAX_WIDTH_NTSC - 640)/2,	// viXOrigin
	(VI_MAX_HEIGHT_NTSC/2 - 448/2)/2,	// viYOrigin
	640,             // viWidth
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
	(VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
	(VI_MAX_HEIGHT_NTSC - 448)/2,       // viYOrigin
	640,             // viWidth
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
	&TV_239p, &TV_478i,			/* Snes PAL video modes */
	&TV_224p, &TV_448i,			/* Snes NTSC video modes */
};

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
#define TSTACK 16384
static lwpq_t videoblankqueue;
static lwp_t vbthread;
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
	LWP_CreateThread (&vbthread, vbgetback, NULL, vbstack, TSTACK, 150);
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
		GX_CopyDisp (xfb[whichfb], GX_TRUE);
		GX_Flush ();
		copynow = GX_FALSE;
	}

	FrameTimer++;
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
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
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
	Mtx44 p;

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

	gui_alphasetup ();

	guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	GX_CopyDisp (xfb[whichfb], GX_TRUE); // reset xfb

	vwidth = 100;
	vheight = 100;
}

/****************************************************************************
 * UpdatePadsCB
 *
 * called by postRetraceCallback in InitGCVideo - scans gcpad and wpad
 ***************************************************************************/
static void
UpdatePadsCB ()
{
#ifdef HW_RVL
	WPAD_ScanPads();
#endif
	PAD_ScanPads();
}

/****************************************************************************
 * MakeTexture
 *
 * - modified for a buffer with an offset (border)
 ****************************************************************************/
static void
MakeTexture (const void *src, void *dst, s32 width, s32 height)
{
  register u32 tmp0 = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0;

  __asm__ __volatile__ ("       srwi            %6,%6,2\n"
                        "       srwi            %7,%7,2\n"
                        "       subi            %3,%4,4\n"
                        "       mr              %4,%3\n"
                        "       subi            %4,%4,4\n"
                        "2:     mtctr           %6\n"
                        "       mr                      %0,%5\n"
                        //
                        "1:     lwz                     %1,0(%5)\n"		//1
                        "       stwu            %1,8(%4)\n"
                        "       lwz                     %2,4(%5)\n"		//1
                        "       stwu            %2,8(%3)\n"
                        "       lwz                     %1,1032(%5)\n"		//2
                        "       stwu            %1,8(%4)\n"
                        "       lwz                     %2,1036(%5)\n"		//2
                        "       stwu            %2,8(%3)\n"
                        "       lwz                     %1,2064(%5)\n"		//3
                        "       stwu            %1,8(%4)\n"
                        "       lwz                     %2,2068(%5)\n"		//3
                        "       stwu            %2,8(%3)\n"
                        "       lwz                     %1,3096(%5)\n"		//4
                        "       stwu            %1,8(%4)\n"
                        "       lwz                     %2,3100(%5)\n"		//4
                        "       stwu            %2,8(%3)\n"
                        "       addi            %5,%5,8\n"
                        "       bdnz            1b\n"
                        "       addi            %5,%0,4128\n"		//5
                        "       subic.          %7,%7,1\n"
                        "       bne                     2b"
                        // regs 0-7
                        :"=&r" (tmp0), "=&r" (tmp1), "=&r" (tmp2),
                        "=&r" (tmp3), "+r" (dst):"r" (src), "r" (width),
                        "r" (height));
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

	switch (vmode->viTVMode >> 2)
	{
		case VI_PAL:
			// 576 lines (PAL 50Hz)
			// display should be centered vertically (borders)
			vmode = &TVPal574IntDfScale;
			vmode->xfbHeight = 480;
			vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
			vmode->viHeight = 480;
			vmode_60hz = 0;

			// Original Video modes (forced to PAL 50hz)
      		// set video signal mode
			TV_224p.viTVMode = VI_TVMODE_PAL_DS;
			TV_448i.viTVMode = VI_TVMODE_PAL_INT;
			// set VI position
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 448/2)/2;
      		TV_448i.viYOrigin = (VI_MAX_HEIGHT_PAL - 448)/2;
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60hz)
			vmode_60hz = 1;

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
			vmode_60hz = 1;

			// Original Video modes (forced to PAL 60hz)
			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_478i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
			TV_224p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_448i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
			// set VI position
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
      		TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;
      		TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 448)/2;
			break;
	}

	#ifdef HW_DOL
	/* we have component cables, but the preferred mode is interlaced
	 * why don't we switch into progressive?
	 * on the Wii, the user can do this themselves on their Wii Settings */
	if(VIDEO_HaveComponentCable())
		vmode = &TVNtsc480Prog;
	#endif

	// check for progressive scan
	if (vmode->viTVMode == VI_TVMODE_NTSC_PROG)
		progressive = true;

#ifdef HW_RVL
	// widescreen fix
	if(CONF_GetAspectRatio())
	{
		vmode->viWidth = 678;
		vmode->viXOrigin = (VI_MAX_WIDTH_PAL - 678) / 2;
	}
#endif

	VIDEO_Configure (vmode);

	screenheight = vmode->xfbHeight;

	// Allocate the video buffers
	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

	// A console is always useful while debugging
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

	InitLUTs();	// init LUTs for hq2x

	InitVideoThread ();

	// Finally, the video is up and ready for use :)
}

/****************************************************************************
 * ResetVideo_Emu
 *
 * Reset the video/rendering mode for the emulator rendering
****************************************************************************/
void
ResetVideo_Emu ()
{
	GXRModeObj *rmode;
	Mtx44 p;

	int i = -1;
	if (GCSettings.render == 0 || GCSettings.FilterMethod != FILTER_NONE)	// original render mode or hq2x
	{
		for (i=0; i<4; i++)
		{
			if (tvmodes[i]->efbHeight == vheight)
				break;
		}
		rmode = tvmodes[i];

		// hack to fix video output for hq2x
		if (GCSettings.FilterMethod != FILTER_NONE)
		{
			memcpy(&TV_Custom, tvmodes[i], sizeof(TV_Custom));
			rmode = &TV_Custom;

			rmode->fbWidth = 512;
			rmode->efbHeight *= 2;
			rmode->xfbHeight *= 2;
			rmode->xfbMode = VI_XFBMODE_DF;
			rmode->viTVMode |= VI_INTERLACE;
		}
	}
	else
	{
		rmode = vmode;		// same mode as menu
	}

	VIDEO_Configure (rmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();


	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render == 1) ? GX_TRUE : GX_FALSE, rmode->vfilter);	// deflicker ON only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	#ifdef _DEBUG_VIDEO
	// log stuff
	fprintf(debughandle, "\n\nrmode = tvmodes[%d], field_rendering: %d", i, (rmode->viHeight == 2 * rmode->xfbHeight));
	fprintf(debughandle, "\nInterlaced: %i,\t vwidth: %d, vheight: %d,\t fb_W: %u, efb_H: %u", IPPU.Interlace, vwidth, vheight, rmode->fbWidth, rmode->efbHeight);
	#endif

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

	VIDEO_Configure (vmode);
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
		while (VIDEO_GetNextField())
			VIDEO_WaitVSync();

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

/****************************************************************************
 * Update Video
 ***************************************************************************/
uint32 prevRenderedFrameCount = 0;
extern bool CheckVideo;
int fscale;

void
update_video (int width, int height)
{

	vwidth = width;
	vheight = height;

	// Ensure previous vb has complete
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
	{
		usleep (50);
	}

	whichfb ^= 1;

	if ( oldvheight != vheight || oldvwidth != vwidth )	// if rendered width/height changes, update scaling
		CheckVideo = 1;

	if ( CheckVideo && (IPPU.RenderedFramesCount != prevRenderedFrameCount) )	// if we get back from the menu, and have rendered at least 1 frame
	{
		int xscale, yscale;

		fscale = GetFilterScale((RenderFilter)GCSettings.FilterMethod);

		ResetVideo_Emu ();	// reset video to emulator rendering settings

		/** Update scaling **/
		if (GCSettings.FilterMethod != FILTER_NONE)	// hq2x filters
		{
			xscale = vwidth;
			yscale = vheight;
		}
		else if (GCSettings.render == 0)	// original render mode
		{
			xscale = 256;
			yscale = vheight / 2;
		}
		else // unfiltered and filtered mode
		{
			xscale = 320;
			yscale = (vheight > (vmode->efbHeight/2)) ? (vheight / 2) : vheight;
		}

		// aspect ratio scaling (change width scale)
		// yes its pretty cheap and ugly, but its easy!
		if (GCSettings.widescreen)
			xscale = (3*xscale)/4;

		xscale *= GCSettings.ZoomLevel;
		yscale *= GCSettings.ZoomLevel;

		square[6] = square[3]  =  xscale + GCSettings.xshift;
		square[0] = square[9]  = -xscale + GCSettings.xshift;
		square[4] = square[1]  =  yscale - GCSettings.yshift;
		square[7] = square[10] = -yscale - GCSettings.yshift;
		DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!
    	draw_init ();

		// initialize the texture obj we are going to use
		GX_InitTexObj (&texobj, texturemem, vwidth*fscale, vheight*fscale, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

	    if (GCSettings.render == 0 || GCSettings.render == 2)
			GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF

		GX_LoadTexObj (&texobj, GX_TEXMAP0);	// load texture object so its ready to use

		#ifdef _DEBUG_VIDEO
		// log stuff
		fprintf(debughandle, "\nxscale: %d, yscale: %d", xscale, yscale);
		#endif

		oldvwidth = vwidth;
		oldvheight = vheight;
		CheckVideo = 0;
	}

	// convert image to texture
	if (GCSettings.FilterMethod != FILTER_NONE)
	{
		FilterMethod ((uint8*) GFX.Screen, EXT_PITCH, (uint8*) filtermem, vwidth*fscale*2, vwidth, vheight);
		MakeTexture565((char *) filtermem, (char *) texturemem, vwidth*fscale, vheight*fscale);
	} else {
		MakeTexture((char *) GFX.Screen, (char *) texturemem, vwidth, vheight);
	}

	DCFlushRange (texturemem, TEX_WIDTH * TEX_HEIGHT * 2);	// update the texture memory
	GX_InvalidateTexAll ();

	draw_square (view);		// draw the quad

	GX_DrawDone ();
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	copynow = GX_TRUE;

	// Return to caller, don't waste time waiting for vb
	LWP_ResumeThread (vbthread);
}

/****************************************************************************
 * Zoom Functions
 ***************************************************************************/
void
zoom (float speed)
{
	if (GCSettings.ZoomLevel > 1)
		GCSettings.ZoomLevel += (speed / -100.0);
	else
		GCSettings.ZoomLevel += (speed / -200.0);

	if (GCSettings.ZoomLevel < 0.5)
		GCSettings.ZoomLevel = 0.5;
	else if (GCSettings.ZoomLevel > 2.0)
		GCSettings.ZoomLevel = 2.0;

	oldvheight = 0;	// update video
}

void
zoom_reset ()
{
	GCSettings.ZoomLevel = 1.0;
	oldvheight = 0;	// update video
}

/****************************************************************************
 * Drawing screen
 ***************************************************************************/
void
clearscreen (int colour)
{
	whichfb ^= 1;
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], colour);
#ifdef HW_RVL
	// on wii copy from memory
	memcpy ((char *) xfb[whichfb], (char *) backdrop, 640 * screenheight * 2);
#else
	// on gc copy from aram
	ARAMFetch ((char *) xfb[whichfb], (char *) AR_BACKDROP, 640 * screenheight * 2);
#endif
}

void
showscreen ()
{
	copynow = GX_FALSE;
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	VIDEO_WaitVSync ();
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
	GFX.Screen = (uint16*)(snes9xgfx + EXT_OFFSET);

	memset (snes9xgfx, 0, sizeof(snes9xgfx));
}
