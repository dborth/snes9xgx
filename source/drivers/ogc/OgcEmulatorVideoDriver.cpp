/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 * Daryl Borth 2008-2026
 *
 * OgcEmulatorVideoDriver.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/machine/processor.h>

#include "OgcEmulatorVideoDriver.h"
#include "OgcVideoDriver.h"
#include "../../snes9xgx.h"
#include "../../video.h"
#include "videofilters.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"

extern void UpdatePlaybackRate(void);

/*** GX ***/
#define TEX_WIDTH 512
#define TEX_HEIGHT 512
#define TEXTUREMEM_SIZE 	TEX_WIDTH*(TEX_HEIGHT+8)*2
static unsigned char texturemem[TEXTUREMEM_SIZE] ATTRIBUTE_ALIGN (32);
static unsigned char scanline_tex_data[32] ATTRIBUTE_ALIGN (32);
static GXTexObj texobj;
static GXTexObj scanlineTexObj;
static Mtx view;
static Mtx modelView;
static int vwidth, vheight, oldvwidth, oldvheight;
static int fscale = 1;

int CheckVideo = 0; // for forcing video reset
uint32 prevRenderedFrameCount = 0;

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
	-HASPECT,  VASPECT, 0,	// 0
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
 * configureOriginalModeTables
 *
 * Patches the PAL/NTSC/EURGB60 timing fields into the SNES original-
 * resolution mode table, keyed off the broadcast standard of baseMode
 * (as returned by OgcVideoDriver::findVideoMode()).
 ***************************************************************************/
void OgcEmulatorVideoDriver::configureOriginalModeTables(GXRModeObj* baseMode)
{
	switch (baseMode->viTVMode >> 2)
	{
		case VI_PAL:
			// 576 lines (PAL 50Hz)
			// Original Video modes (forced to PAL 50Hz)
			TV_239p.viTVMode = VI_TVMODE_PAL_DS;
			TV_478i.viTVMode = VI_TVMODE_PAL_INT;
			TV_224p.viTVMode = VI_TVMODE_PAL_DS;
			TV_448i.viTVMode = VI_TVMODE_PAL_INT;
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_PAL - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_PAL/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_PAL - 448)/2;
			break;

		case VI_NTSC:
			// 480 lines (NTSC 60Hz)
			// Original Video modes (forced to NTSC 60hz)
			TV_239p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_478i.viTVMode = VI_TVMODE_NTSC_INT;
			TV_224p.viTVMode = VI_TVMODE_NTSC_DS;
			TV_448i.viTVMode = VI_TVMODE_NTSC_INT;
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 448)/2;
			break;

		default:
			// 480 lines (PAL 60Hz)
			// Original Video modes (forced to PAL 60hz)
			TV_239p.viTVMode = VI_TVMODE(baseMode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_478i.viTVMode = VI_TVMODE(baseMode->viTVMode >> 2, VI_INTERLACE);
			TV_224p.viTVMode = VI_TVMODE(baseMode->viTVMode >> 2, VI_NON_INTERLACE);
			TV_448i.viTVMode = VI_TVMODE(baseMode->viTVMode >> 2, VI_INTERLACE);
			TV_239p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 478/2)/2;
			TV_478i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 478)/2;
			TV_224p.viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2;
			TV_448i.viYOrigin = (VI_MAX_HEIGHT_NTSC - 448)/2;
			break;
	}
}

/****************************************************************************
 * Scanline Support Functions
 ***************************************************************************/
void OgcEmulatorVideoDriver::initScanlineTexture()
{
	// GX_TF_I8 represents one byte per pixel.
	// We create an 8x4 tile: Rows 0 and 2 are white (0xFF), Rows 1 and 3 are dark (0xA0).
	for (int y = 0; y < 4; y++) {
		u8 intensity = (y % 2 == 0) ? 0xFF : 0xA0; // 0xA0 controls the scanline darkness
		for (int x = 0; x < 8; x++) {
			scanline_tex_data[y * 8 + x] = intensity;
		}
	}

	// Flush the CPU data cache. GX reads directly from main memory.
	DCStoreRange(scanline_tex_data, 32);

	// Initialize the texture object. Wrap modes MUST be GX_REPEAT to tile across the screen.
	GX_InitTexObj(&scanlineTexObj, scanline_tex_data, 8, 4, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);

	// Filter mode MUST be GX_NEAR. GX_LINEAR will blur the lines into a muddy gray.
	GX_InitTexObjFilterMode(&scanlineTexObj, GX_NEAR, GX_NEAR);

	// Load the scanline texture into MAP1
	GX_LoadTexObj(&scanlineTexObj, GX_TEXMAP1);
}

void OgcEmulatorVideoDriver::setupScanlineFilterTEV()
{
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

bool OgcEmulatorVideoDriver::shouldApplyScanlines()
{
	return GCSettings.videoScanlines && vmode->efbHeight > 300;
}

/****************************************************************************
 * Scaler Support Functions
 ***************************************************************************/
void OgcEmulatorVideoDriver::drawInit()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	if(shouldApplyScanlines()) {
		setupScanlineFilterTEV();
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

void OgcEmulatorVideoDriver::drawSquare()
{
	GX_LoadPosMtxImm (modelView, GX_PNMTX0);

	GX_Begin (GX_QUADS, GX_VTXFMT0, 4);

	int scanlines = shouldApplyScanlines();

	if(scanlines) {
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
		// preventing the moir? effect caused by floating-point edge-rounding.
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

	if(scanlines) {
		// force identity matrix to ensure texture mapping is pristine and devoid of stray scaling
		Mtx texMtx;
		guMtxIdentity(texMtx);
		GX_LoadTexMtxImm(texMtx, GX_TEXMTX1, GX_MTX2x4);
	}
}

void OgcEmulatorVideoDriver::resetFbWidth(int width, GXRModeObj *rmode)
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
 * resetVideo
 *
 * Reset the video/rendering mode for the emulator rendering
 ***************************************************************************/
void OgcEmulatorVideoDriver::resetVideo()
{
	GXRModeObj *rmode = videoDriver->findVideoMode();
	configureOriginalModeTables(rmode);

	Mtx44 p;
	int i = -1;

	if (GCSettings.videoMode == VIDEOMODE_ORIGINAL_240P)
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

		// fix original video output for 2X filters (only when actually filtering; h<=239, w<=256)
		if (fscale > 1 && vheight <= 239 && vwidth <= 256)
		{
			memcpy(&TV_Custom, tvmodes[i], sizeof(TV_Custom));
			rmode = &TV_Custom;

			rmode->fbWidth = 512;
			rmode->efbHeight *= 2;
			rmode->xfbHeight *= 2;
			rmode->xfbMode = VI_XFBMODE_DF;
			rmode->viTVMode = VI_TVMODE(rmode->viTVMode >> 2, VI_INTERLACE);

			// Calculate and enforce hardware Y-origin centering
			int tvFormat = rmode->viTVMode >> 2;
			int maxPhysicalHeight = (tvFormat == VI_PAL) ? 576 : 480;

			// Center the hardware output based on the physical screen height
			rmode->viYOrigin = (maxPhysicalHeight - rmode->viHeight) / 2;
		}

		if (Settings.PAL == 1)
			Settings.SoundInputRate = 32090;
		else
			Settings.SoundInputRate = 31894;
		UpdatePlaybackRate();
	}
	else
	{
		if (GCSettings.videoAspectRatioCorrection != VIDEO_ASPECT_RATIO_CORRECTION_NONE)
			resetFbWidth(640, rmode);
		else
			resetFbWidth(512, rmode);

		Settings.SoundInputRate = 31920;
		UpdatePlaybackRate();
	}

	videoDriver->setupVideoMode(rmode); // reconfigure VI

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
		GCSettings.videoHardwareSoften == VIDEO_HW_SOFTEN_SHARP ? sharp
		: GCSettings.videoHardwareSoften == VIDEO_HW_SOFTEN_SOFT ? soft
		: rmode->vfilter;

	// Enable the copy filter if not in SF mode, OR if the user explicitly selected a filter
	u8 vf_enable = (rmode->xfbMode != VI_XFBMODE_SF || GCSettings.videoHardwareSoften != VIDEO_HW_SOFTEN_OFF) ? GX_TRUE : GX_FALSE;
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, vf_enable, vfilter);

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

	drawInit ();
}

// Un-swizzles a 4x4-tiled GX_TF_RGB5A3 texture into dst (RGB24)
void OgcEmulatorVideoDriver::untileRGB5A3ToRGB24(const void * tiledTexture, int width, int height, uint8_t* dst)
{
	int padded_width = (width + 3) & ~3;
	const u16 * tex16 = (const u16 *) tiledTexture;

	for(int y = 0; y < height; y++) {
		int tile_y = y / 4;
		int in_tile_y = y % 4;
		for(int x = 0; x < width; x++) {
			int tile_x = x / 4;
			int in_tile_x = x % 4;

			int tex_pixel_idx = (tile_y * (padded_width / 4) + tile_x) * 16 + (in_tile_y * 4 + in_tile_x);
			u16 color = tex16[tex_pixel_idx];

			// RGB555 format
			u8 r = (color >> 10) & 0x1F;
			u8 g = (color >> 5) & 0x1F;
			u8 b = color & 0x1F;

			int out_idx = (y * width + x) * 3;
			dst[out_idx]     = (r << 3) | (r >> 2);
			dst[out_idx + 1] = (g << 3) | (g >> 2);
			dst[out_idx + 2] = (b << 3) | (b >> 2);
		}
	}
}

void OgcEmulatorVideoDriver::readFrameRGB24(uint8_t* dst)
{
	untileRGB5A3ToRGB24(texturemem, gameScreenPng.width, gameScreenPng.height, dst);
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
 * - Assumes 15-bit RGB555 format (2 bytes per pixel)
 * ASSUMPTIONS:
 * - Source pointer is aligned to 4-byte boundary
 * - Destination pointer is aligned to 32-byte boundary
 ***************************************************************************/
static void MakeTexturePitch1032(const void *src, void *dst, s32 width, s32 height)
{
    u32 r_src_row=0, tmpA=0, tmpB=0, tmpC=0, tmpD=0, mask=0;

    __asm__ __volatile__ (
        "lis    %[mask], 0x8000\n"             // mask = 0x80000000
        "ori    %[mask], %[mask], 0x8000\n"    // mask = 0x80008000 (Sets MSB for 2x RGB555 pixels)

        "srwi   %[width], %[width], 2\n"       // num_tiles_x = width / 4
        "srwi   %[height], %[height], 2\n"     // num_tiles_y = height / 4

    "2: mtctr   %[width]\n"                    // Set inner loop counter (X)
        "mr     %[r_src_row], %[src]\n"        // Save start of source row

    "1: dcbz    0, %[dst]\n"                   // ZERO L1 CACHE

        // -- Load Tile Half 1 (Rows 0 & 1) --
        "lwz    %[tmpA], 0(%[src])\n"
        "lwz    %[tmpB], 4(%[src])\n"
        "lwz    %[tmpC], 1032(%[src])\n"
        "lwz    %[tmpD], 1036(%[src])\n"

        // -- Force MSB high for GX_TF_RGB5A3 --
        "or     %[tmpA], %[tmpA], %[mask]\n"
        "or     %[tmpB], %[tmpB], %[mask]\n"
        "or     %[tmpC], %[tmpC], %[mask]\n"
        "or     %[tmpD], %[tmpD], %[mask]\n"

        // -- Store Half 1 while Loading Tile Half 2 (Rows 2 & 3) --
        "stw    %[tmpA], 0(%[dst])\n"
        "lwz    %[tmpA], 2064(%[src])\n"

        "stw    %[tmpB], 4(%[dst])\n"
        "lwz    %[tmpB], 2068(%[src])\n"

        "stw    %[tmpC], 8(%[dst])\n"
        "lwz    %[tmpC], 3096(%[src])\n"

        "stw    %[tmpD], 12(%[dst])\n"
        "lwz    %[tmpD], 3100(%[src])\n"

        // -- Force MSB high for Tile Half 2 --
        "or     %[tmpA], %[tmpA], %[mask]\n"
        "or     %[tmpB], %[tmpB], %[mask]\n"
        "or     %[tmpC], %[tmpC], %[mask]\n"
        "or     %[tmpD], %[tmpD], %[mask]\n"

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
          [mask] "=&r" (mask),
          [dst] "+b" (dst),
          [src] "+b" (src),
          [width] "+r" (width),
          [height] "+r" (height)
        :
        : "memory"
    );
}

void OgcEmulatorVideoDriver::init(VideoDriver* driver)
{
	videoDriver = static_cast<OgcVideoDriver*>(driver);
	vwidth = 100;
	vheight = 100;
}

/****************************************************************************
 * presentFrame
 ***************************************************************************/
void OgcEmulatorVideoDriver::presentFrame(int width, int height)
{
	vwidth = width;
	vheight = height;

	if(CheckVideo == 2 && IPPU.RenderedFramesCount == prevRenderedFrameCount)
		return; // we haven't rendered any frames yet, so we can't draw anything!

	// Wait for the VI to display the PREVIOUSLY submitted frame
	// This naturally throttles the emulator to the TV's refresh rate
	videoDriver->waitForBufferReady();

	if (oldvheight != vheight || oldvwidth != vwidth) // if rendered width/height changes, update scaling
		CheckVideo = 1;

	if (CheckVideo)	// if we get back from the menu, and have rendered at least 1 frame
	{
		int xscale, yscale;

		if(vwidth <= 256)
			fscale = GetFilterScale();
		else
			fscale = 1;

		resetVideo();	// reset video to emulator rendering settings

		/** Update scaling **/
		if (GCSettings.videoMode == VIDEOMODE_ORIGINAL_240P)
		{
			if (fscale > 1)
			{
				xscale = vwidth;
				yscale = vheight;
			}
			else
			{
				xscale = 256;
				yscale = vheight / 2;
			}

			// Original Mode 16:9 corrections
			if (GCSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9 || GCSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED) {
				xscale = (3*xscale)/4;
			}
		}
		else
		{
			if (GCSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9) {
				// Determine the raw height of the SNES signal
				float base_height = (vheight == 224 || vheight == 448) ? 224.0f : 239.0f;

				// Calculate the uniform scale required to make the height fill the 480 screen
				float scale_factor = (vmode->efbHeight / 2.0f) / base_height;

				// Apply the exact same scale factor to both the width and the height
				xscale = (256.0f * scale_factor * 15) / 16; // Mathematically perfect compensation for the 640 widescreen EFB
				yscale = vmode->efbHeight / 2;
			}
			else if (GCSettings.videoAspectRatioCorrection == VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED) {
				if(vheight == 224 || vheight == 448) {
					xscale = 224;
					yscale = 224;
				} else {
					xscale = 239;
					yscale = 239;
				}
			}
			else {
				xscale = 256;

				if(vheight == 224 || vheight == 448)
					yscale = 224;
				else
					yscale = 239;
			}
		}

		xscale *= GCSettings.videoZoomHor;
		yscale *= GCSettings.videoZoomVert;

		square[6] = square[3]  =  xscale + GCSettings.videoXshift;
		square[0] = square[9]  = -xscale + GCSettings.videoXshift;
		square[4] = square[1]  =  yscale - GCSettings.videoYshift;
		square[7] = square[10] = -yscale - GCSettings.videoYshift;

		DCFlushRange (square, 32); // update memory BEFORE the GPU accesses it!

		GXRModeObj *menu_vmode = videoDriver->findVideoMode();

		// 1. Compensate for progressive/interlaced physical line density
		float viHeightAdjusted = (vmode->viHeight < 300) ? (vmode->viHeight * 2.0f) : (float)vmode->viHeight;
		float menuViHeightAdjusted = (menu_vmode->viHeight < 300) ? (menu_vmode->viHeight * 2.0f) : (float)menu_vmode->viHeight;

		// 2. Calculate physical fraction of the TV screen the hardware is utilizing
		float physical_width_ratio = (float)vmode->viWidth / (float)menu_vmode->viWidth;
		float physical_height_ratio = viHeightAdjusted / menuViHeightAdjusted;

		// 3. Calculate fraction of the EFB utilized by the game quad
		float width_frac  = (2.0f * xscale) / (float)vmode->fbWidth;
		float height_frac = (2.0f * yscale) / (float)vmode->efbHeight;

		// 4. Map completely into the Menu's 640x480 logical canvas
		float targetWidth  = videoDriver->getScreenWidth() * width_frac * physical_width_ratio;
		float targetHeight = videoDriver->getScreenHeight() * height_frac * physical_height_ratio;

		gameScreenPng.width  = vwidth * fscale;
		gameScreenPng.height = vheight * fscale;

		gameScreenPng.scaleX = targetWidth / (float)gameScreenPng.width;
		gameScreenPng.scaleY = targetHeight / (float)gameScreenPng.height;

		// 5. Shift calculations must map EFB distances physically through to the Menu canvas
		gameScreenPng.xoffset = GCSettings.videoXshift * (videoDriver->getScreenWidth() / (float)menu_vmode->viWidth) * ((float)vmode->viWidth / (float)vmode->fbWidth);
		gameScreenPng.yoffset = GCSettings.videoYshift * (videoDriver->getScreenHeight() / menuViHeightAdjusted) * (viHeightAdjusted / (float)vmode->efbHeight);

    	drawInit ();

		// initialize the texture obj we are going to use
		GX_InitTexObj (&texobj, texturemem, vwidth*fscale, vheight*fscale, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);

		if (!GCSettings.videoBilinearFilter)
			GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR);
		else
			GX_InitTexObjFilterMode(&texobj,GX_LINEAR,GX_LINEAR);

		GX_LoadTexObj (&texobj, GX_TEXMAP0); // load texture object so its ready to use

		if(shouldApplyScanlines())
			initScanlineTexture();

		oldvwidth = vwidth;
		oldvheight = vheight;
		CheckVideo = 0;
	}

	// convert image to texture
	if (fscale > 1 && vheight <= 239 && vwidth <= 256) // don't do filtering on game textures > 256 x 239
	{
		FilterMethod ((uint8*) GFX.Screen, EXT_PITCH, (uint8*) texturemem, vwidth*fscale*2, vwidth, vheight);
	}
	else
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

	drawSquare();	// draw the quad

	videoDriver->presentBuffer();
}
