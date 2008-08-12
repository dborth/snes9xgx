/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Video
 *
 * This is a modified renderer from the Genesis Plus Project.
 * Well - you didn't expect me to write another one did ya ? -;)
 *
 * softdev July 2006
 * crunchy2 May 2007
 ****************************************************************************/
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

/*** Snes9x GFX Buffer ***/
static unsigned char snes9xgfx[1024 * 512 * 2];

/*** Memory ROM Loading ***/
extern unsigned long ARAM_ROMSIZE;
extern unsigned int SMBTimer;

/*** 2D Video ***/
unsigned int *xfb[2] = { NULL, NULL };		/*** Double buffered ***/
int whichfb = 0;				/*** Switch ***/
GXRModeObj *vmode;				/*** General video mode ***/
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
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

u32 FrameTimer = 0;

u8 vmode_60hz = 0;

#define HASPECT 76
#define VASPECT 54

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

#ifdef VIDEO_THREADING
/****************************************************************************
 * VideoThreading
 ****************************************************************************/
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
 ****************************************************************************/
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
 ****************************************************************************/
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
 ****************************************************************************/
static void
copy_to_xfb (u32 arg)
{

  if (copynow == GX_TRUE)
    {
      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetColorUpdate (GX_TRUE);
      GX_CopyDisp (xfb[whichfb], GX_TRUE);
      GX_Flush ();
      copynow = GX_FALSE;
    }

  FrameTimer++;
  SMBTimer++;
}

/****************************************************************************
 * WIP3 - Scaler Support Functions
 ****************************************************************************/
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
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  GX_InvalidateTexAll ();
  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
		 GX_CLAMP, GX_CLAMP, GX_FALSE);
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

  /*
   * Use C functions!
   * Calling the faster asm ones cause reboot!
   */
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
 * WIP3 - Based on texturetest from libOGC examples.
 ****************************************************************************/
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
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE,
		    vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering,
		   ((vmode->viHeight ==
		     2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);

  vwidth = 100;
  vheight = 100;
}

/****************************************************************************
 * UpdatePads
 *
 * called by postRetraceCallback in InitGCVideo - scans gcpad and wpad
 ****************************************************************************/
void UpdatePadsCB()
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
 ****************************************************************************/

void
InitGCVideo ()
{
    /*
    * Before doing anything else under libogc,
    * Call VIDEO_Init
    */

    int *romptr = (int *) 0x81000000;

    VIDEO_Init ();
    PAD_Init ();

    AUDIO_Init (NULL);
    AR_Init (NULL, 0);

    /* Before going any further, let's copy any attached ROM image ** */
    if (memcmp ((char *) romptr, "SNESROM0", 8) == 0)
    {
        ARAM_ROMSIZE = romptr[2];
        romptr = (int *) 0x81000020;
        ARAMPut ((char *) romptr, (char *) AR_SNESROM, ARAM_ROMSIZE);
    }

	/*
	* Always use NTSC mode - this works on NTSC and PAL, GC and Wii
	vmode = &TVNtsc480IntDf;
	*/

	vmode = VIDEO_GetPreferredMode(NULL);

	switch(vmode->viTVMode)
	{
		case VI_TVMODE_PAL_DS:
		case VI_TVMODE_PAL_INT:
			vmode_60hz = 0;
			break;

		case VI_TVMODE_EURGB60_PROG:
		case VI_TVMODE_EURGB60_DS:
		case VI_TVMODE_NTSC_DS:
		case VI_TVMODE_NTSC_INT:
		case VI_TVMODE_NTSC_PROG:
		case VI_TVMODE_MPAL_INT:
		default:
			vmode_60hz = 1;
			break;
	}

    VIDEO_Configure (vmode);

    screenheight = vmode->xfbHeight;

    /*
    * Allocate the video buffers
    */
    xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

    /*
    * A console is always useful while debugging.
    */
    console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

    /*
    * Clear framebuffers etc.
    */
    VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer (xfb[0]);

    /*
    * Let libogc populate manage the PADs for us
    */
    //VIDEO_SetPostRetraceCallback ((VIRetraceCallback)PAD_ScanPads);
	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)UpdatePadsCB);
    VIDEO_SetPreRetraceCallback ((VIRetraceCallback)copy_to_xfb);
    VIDEO_SetBlack (FALSE);
    VIDEO_Flush ();
    VIDEO_WaitVSync ();
    if (vmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();

    copynow = GX_FALSE;
    StartGX ();

    #ifdef VIDEO_THREADING
    InitVideoThread ();
    #endif

    /*
    * Finally, the video is up and ready for use :)
    */
}

void ReInitGCVideo()
{
  Mtx p;

  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GCSettings.render ? GX_TRUE : GX_FALSE,
		    vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering,
		   ((vmode->viHeight ==
		     2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

//  GX_CopyDisp (xfb[whichfb], GX_TRUE);

  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
}

/****************************************************************************
 * Drawing screen
 ****************************************************************************/
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
  ARAMFetch ((char *) xfb[whichfb], (char *) AR_BACKDROP, 640 * screenheight * 2);			// FIX
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
 ****************************************************************************/
void
setGFX ()
{
  GFX.Screen = (unsigned short *) snes9xgfx;
  GFX.Pitch = 1024;
}

/****************************************************************************
 * MakeTexture
 *
 * Proper GNU Asm rendition of the above, converted by shagkur. - Thanks!
 ****************************************************************************/
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
 ****************************************************************************/
void
update_video (int width, int height)
{
  vwidth = width;
  vheight = height;

#ifdef VIDEO_THREADING
  /* Ensure previous vb has complete */
  while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
#else
  while (copynow == GX_TRUE)
#endif
    {
      usleep (50);
    }

  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
    {
		/** Update scaling **/
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();

      memset (&view, 0, sizeof (Mtx));
	  guLookAt(view, &cam.pos, &cam.up, &cam.view);
      GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
    }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);



  MakeTexture ((char *) GFX.Screen, (char *) texturemem, vwidth, vheight);



  DCFlushRange (texturemem, TEX_WIDTH * TEX_HEIGHT * 2);

  GX_SetNumChans (1);

  GX_LoadTexObj (&texobj, GX_TEXMAP0);

  draw_square (view);

  GX_DrawDone ();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  copynow = GX_TRUE;

#ifdef VIDEO_THREADING
  /* Return to caller, don't waste time waiting for vb */
  LWP_ResumeThread (vbthread);
#endif

}

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

  oldvheight = 0;
}
