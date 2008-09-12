/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
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
#include "snes9x.h"
#include "memmap.h"
#include "aram.h"
#include "snes9xGX.h"

#include "gui.h"

/*** Snes9x GFX Buffer ***/
static unsigned char snes9xgfx[1024 * 512 * 2];

/*** Memory ROM Loading ***/
extern unsigned long ARAM_ROMSIZE;
extern unsigned int SMBTimer;

/*** 2D Video ***/
unsigned int *xfb[2] = { NULL, NULL };		/*** Double buffered ***/
int whichfb = 0;				/*** Switch ***/
GXRModeObj *vmode;				/*** Menu video mode ***/
int screenheight;
extern u32* backdrop;

/*** GX ***/
#define TEX_WIDTH 512
#define TEX_HEIGHT 512
#define DEFAULT_FIFO_SIZE 256 * 1024
unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static unsigned char texturemem[TEX_WIDTH * (TEX_HEIGHT + 8)] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

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
  -HASPECT, VASPECT, 0,		// 0
    HASPECT, VASPECT, 0,	// 1
    HASPECT, -VASPECT, 0,	// 2
    -HASPECT, -VASPECT, 0,	// 3
};


static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};


/***
*** Custom Video modes (used to emulate original console video modes)
***/

/** Original SNES PAL Resolutions: **/

/* 239 lines progressive (PAL 50Hz) */
GXRModeObj TV_239p =
{
	VI_TVMODE_PAL_DS,       // viDisplayMode
	256,             // fbWidth
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
GXRModeObj TV_478i =
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    512,             // fbWidth
    478,             // efbHeight
    478,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL/2 - 478/2)/2,        // viYOrigin
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
GXRModeObj TV_224p =
{
	VI_TVMODE_EURGB60_DS,      // viDisplayMode
	256,             // fbWidth
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
GXRModeObj TV_448i =
{
    VI_TVMODE_EURGB60_INT,     // viDisplayMode
    512,             // fbWidth
    448,             // efbHeight
    448,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2,       // viYOrigin
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

/* TV Modes table */
GXRModeObj *tvmodes[4] = {
	&TV_239p, &TV_478i,			/* Snes PAL video modes */
	&TV_224p, &TV_448i,			/* Snes NTSC video modes */
};


#ifdef VIDEO_THREADING
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

#endif

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
	SMBTimer++;
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
	//GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
	//GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);
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

	gui_alphasetup ();

//	guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
//	GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
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
    int *romptr = (int *) 0x81000000;	// injected rom

	// init video
    VIDEO_Init ();
    PAD_Init ();

    AUDIO_Init (NULL);
    AR_Init (NULL, 0);

    // Before going any further, let's copy any attached ROM image
    if (memcmp ((char *) romptr, "SNESROM0", 8) == 0)
    {
        ARAM_ROMSIZE = romptr[2];
        romptr = (int *) 0x81000020;
        ARAMPut ((char *) romptr, (char *) AR_SNESROM, ARAM_ROMSIZE);
    }


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
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60hz)
			vmode_60hz = 1;
			break;

		default:
			// 480 lines (PAL 60Hz)
			vmode_60hz = 1;
			break;
	}

	// check for progressive scan
	if (vmode->viTVMode == VI_TVMODE_NTSC_PROG)
		progressive = true;


    VIDEO_Configure (vmode);

    screenheight = vmode->xfbHeight;


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

    #ifdef VIDEO_THREADING
    InitVideoThread ();
    #endif

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
	Mtx p;

	switch (vmode->viTVMode >> 2)
	{
		case VI_PAL:  /* 576 lines (PAL 50Hz) */

			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE_PAL_DS;
			TV_478i.viTVMode = VI_TVMODE_PAL_INT;
			TV_224p.viTVMode = VI_TVMODE_PAL_DS;
			TV_448i.viTVMode = VI_TVMODE_PAL_INT;
			// set VI sizing
			//TV_239p.viWidth = TV_478i.viWidth = TV_224p.viWidth = TV_448i.viWidth = 640;
			//TV_239p.viHeight = TV_478i.viHeight = TV_224p.viHeight = TV_448i.viHeight = 480;
			TV_239p.viXOrigin = TV_478i.viXOrigin = TV_224p.viXOrigin = TV_448i.viXOrigin = (VI_MAX_WIDTH_PAL - 640)/2;
			TV_239p.viYOrigin = TV_478i.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 478/2)/2;
			TV_224p.viYOrigin = TV_448i.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 448/2)/2;

			break;

		case VI_NTSC: /* 480 lines (NTSC 60hz) */

			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_478i.viTVMode = VI_TVMODE_NTSC_INT;
			TV_224p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_448i.viTVMode = VI_TVMODE_NTSC_INT;
			// set VI sizing
			//TV_239p.viWidth = TV_478i.viWidth = TV_224p.viWidth = TV_448i.viWidth = 640;
			//TV_239p.viHeight = TV_478i.viHeight = TV_224p.viHeight = TV_448i.viHeight = 480;
			TV_239p.viXOrigin = TV_224p.viXOrigin = TV_478i.viXOrigin = TV_448i.viXOrigin = (VI_MAX_WIDTH_NTSC - 640)/2;
			TV_239p.viYOrigin = TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_224p.viYOrigin = TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;

			break;

		default:  /* 480 lines (PAL 60Hz) */

			// set video signal mode
			TV_239p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_478i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
			TV_224p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_448i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
			// set VI sizing
			//TV_239p.viWidth = TV_478i.viWidth = TV_224p.viWidth = TV_448i.viWidth = 640;
			//TV_239p.viHeight = TV_478i.viHeight = TV_224p.viHeight = TV_448i.viHeight = 480;
			TV_239p.viXOrigin = TV_224p.viXOrigin = TV_478i.viXOrigin = TV_448i.viXOrigin = (VI_MAX_WIDTH_NTSC - 640)/2;
			TV_239p.viYOrigin = TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_224p.viYOrigin = TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;

			break;
	}


	if (GCSettings.render == 0)	// original render mode
	{
		int i;
		for (i=0; i<4; i++) {
			if (tvmodes[i]->efbHeight == vheight) {
				// FIX: ok?
				tvmodes[i]->fbWidth = vwidth;	// update width - some games are 512x224 (super pang)
				break;
			}
		}
		rmode = tvmodes[i];
	}
	else if (GCSettings.render == 2)	// unfiltered
	{
		rmode = vmode;
	}
	else	// filtered
	{
		rmode = vmode;		// same mode as menu
	}


	VIDEO_Configure (rmode);
	VIDEO_ClearFrameBuffer (rmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField())  VIDEO_WaitVSync();


	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
	GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, (GCSettings.render == 1) ? GX_TRUE : GX_FALSE, rmode->vfilter);	// AA on only for filtered mode

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

//	guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
//	GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
	guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);


	/*
		// DEBUG
		char* msg = (char*) malloc(256*sizeof(char));
		sprintf (msg, (char*)"Interlaced: %i, vwidth: %d, vheight: %d, fb_W: %u, efb_H: %u", IPPU.Interlace, vwidth, vheight, rmode->fbWidth, rmode->efbHeight);
		S9xMessage (0, 0, msg);
		free(msg);
	*/


}

/****************************************************************************
 * ResetVideo_Menu
 *
 * Reset the video/rendering mode for the menu
****************************************************************************/
void
ResetVideo_Menu ()
{
	Mtx p;

	VIDEO_Configure (vmode);
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField())  VIDEO_WaitVSync();

	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);

	GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);

	GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

//	guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
//	GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
	guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 10, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

/****************************************************************************
 * MakeTexture
 *
 * Proper GNU Asm rendition of the above, converted by shagkur. - Thanks!
 ***************************************************************************/
void
MakeTexture (const void *src, void *dst, s32 width, s32 height)
{
  register u32 tmp0 = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0;

  __asm__ __volatile__ ("	srwi		%6,%6,2\n"
			"	srwi		%7,%7,2\n"
			"	subi		%3,%4,4\n"
			"	mr		%4,%3\n"
			"	subi		%4,%4,4\n"
			"2:	mtctr		%6\n"
			"	mr			%0,%5\n"
			//
			"1:	lwz			%1,0(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,4(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,1024(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,1028(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,2048(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,2052(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	lwz			%1,3072(%5)\n"
			"	stwu		%1,8(%4)\n"
			"	lwz			%2,3076(%5)\n"
			"	stwu		%2,8(%3)\n"
			"	addi		%5,%5,8\n"
			"	bdnz		1b\n"
			"	addi		%5,%0,4096\n"
			"	subic.		%7,%7,1\n"
			"	bne			2b"
			//              0                        1                        2                        3              4                      5                 6               7
			:"=&r" (tmp0), "=&r" (tmp1), "=&r" (tmp2),
			"=&r" (tmp3), "+r" (dst):"r" (src), "r" (width),
			"r" (height));
}

/****************************************************************************
 * Update Video
 ***************************************************************************/
uint32 prevRenderedFrameCount = 0;
extern bool CheckVideo;

void
update_video (int width, int height)
{

	vwidth = width;
	vheight = height;

#ifdef VIDEO_THREADING
	// Ensure previous vb has complete
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
#else
	while (copynow == GX_TRUE)
#endif
	{
	  usleep (50);
	}

	whichfb ^= 1;

	if ( oldvheight != vheight || oldvwidth != vwidth )	// if rendered width/height changes, update scaling
		CheckVideo = 1;

	if ( CheckVideo && (IPPU.RenderedFramesCount != prevRenderedFrameCount) )	// if we get back from the menu, and have rendered at least 1 frame
	{
		int xscale, yscale, xshift, yshift;
		yshift = xshift = 0;

		ResetVideo_Emu ();	// reset video to emulator rendering settings

		/** Update scaling **/
		if (GCSettings.render == 0)	// original render mode
		{
			xscale = vwidth / 2;
			yscale = vheight / 2;
		} else {	// unfiltered and filtered mode
			xscale = vmode->fbWidth / 2;
			yscale = vmode->efbHeight / 2;
		}

		// aspect ratio scaling (change width scale)
		// yes its pretty cheap and ugly, but its easy!
		if (GCSettings.widescreen)
			xscale -= (4.0*yscale)/9;

		square[6] = square[3]  =  xscale + xshift;
		square[0] = square[9]  = -xscale + xshift;
		square[4] = square[1]  =  yscale + yshift;
		square[7] = square[10] = -yscale + yshift;

		GX_InvVtxCache ();	// update vertex cache

		GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);	// initialize the texture obj we are going to use

	    if (GCSettings.render == 0 || GCSettings.render == 2)
			GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1); // original/unfiltered video mode: force texture filtering OFF

		GX_LoadTexObj (&texobj, GX_TEXMAP0);	// load texture object so its ready to use

		/*
		// DEBUG
		char* msg = (char*) malloc(256*sizeof(char));
		sprintf (msg, (char*)"xscale: %d, yscale: %d", xscale, yscale);
		S9xMessage (0, 0, msg);
		free(msg);
		*/

		oldvwidth = vwidth;
		oldvheight = vheight;
		CheckVideo = 0;
	}

	/*
	// for zooming
	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);
	*/

	MakeTexture ((char *) GFX.Screen, (char *) texturemem, vwidth, vheight);	// convert image to texture

	DCFlushRange (texturemem, TEX_WIDTH * TEX_HEIGHT * 2);	// update the texture memory
	GX_InvalidateTexAll ();

	draw_square (view);		// draw the quad

	GX_DrawDone ();
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	copynow = GX_TRUE;

#ifdef VIDEO_THREADING
	// Return to caller, don't waste time waiting for vb
	LWP_ResumeThread (vbthread);
#endif

}

// FIX
/****************************************************************************
 * Zoom Functions
 ***************************************************************************/
void
zoom (float speed)
{
	Vector v;

	v.x = cam.view.x - cam.pos.x;
	v.y = cam.view.y - cam.pos.y;
	v.z = cam.view.z - cam.pos.z;

	cam.pos.x += v.x * speed;
	cam.pos.z += v.z * speed;
	cam.view.x += v.x * speed;
	cam.view.z += v.z * speed;

	oldvheight = 0;	// update video
}

void
zoom_reset ()
{
	// reset cam to defaults
	cam.pos.x = cam.pos.y = cam.pos.z = cam.up.x = cam.up.z = 0.0F;
	cam.up.y = 0.0F;
	cam.view.z = -0.5F;

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
	GFX.Screen = (unsigned short *) snes9xgfx;
	GFX.Pitch = 1024;
}
