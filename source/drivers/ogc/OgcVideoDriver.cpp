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
#include "../../snes9xgx.h"

/*** 2D Video ***/
static u32 *xfb[2] = { nullptr, nullptr }; // Double buffered
static int whichfb = 0; // Switch
GXRModeObj *vmode = nullptr; // Current video mode

#define MAX_FB_WIDTH 640
#define MAX_FB_HEIGHT 576
#define DEFAULT_FIFO_SIZE 256 * 1024

static volatile unsigned int copynow = GX_FALSE;
static unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
Mtx GXmodelView2D;

uint32_t FrameTimer = 0;
bool vmode_60hz = true;
bool progressive = 0;

/****************************************************************************
 * VideoThreading
 ***************************************************************************/
static lwp_t vbthread = LWP_THREAD_NULL;
static lwpq_t render_queue;          // Queue for the main thread to sleep on
static lwpq_t vb_queue;              // Queue for the VSync thread to sleep on
static volatile bool vb_done = true; // Tracks if the VSync thread has completed its wait
static volatile bool vb_wait = false; // Tracks if the VSync thread should begin waiting

/****************************************************************************
 * vbgetback
 *
 * This callback enables the emulator to keep running while waiting for a
 * vertical blank
 ***************************************************************************/
static void * vbgetback (void *arg)
{
	while (1)
	{
		u32 level;
		_CPU_ISR_Disable(level);
		while (!vb_wait)
		{
			LWP_ThreadSleep(vb_queue);     // Sleep safely until waitForBufferReady kicks us off
		}
		vb_wait = false;
		_CPU_ISR_Restore(level);

		VIDEO_WaitVSync();                 // Wait for video vertical blank

		_CPU_ISR_Disable(level);
		vb_done = true;
		LWP_ThreadSignal(render_queue);    // Instantly alert the main thread
		_CPU_ISR_Restore(level);
	}
	return nullptr;
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
 * setupVideoMode
 *
 * Applies the given video mode to the VI
 ***************************************************************************/
void OgcVideoDriver::setupVideoMode(GXRModeObj * mode)
{
	static u32 last_fbWidth = 0;

	// Force a video reset and XFB clear if the width was dynamically mutated
	if(vmode == mode && last_fbWidth == mode->fbWidth)
		return;

	// Detect if we are transitioning between Progressive and Interlaced
	bool mode_switch = false;
	if (vmode != nullptr) {
		bool was_progressive = (vmode->viTVMode & 3) == VI_NON_INTERLACE || (vmode->viTVMode & 3) == VI_PROGRESSIVE;
		bool is_progressive = (mode->viTVMode & 3) == VI_NON_INTERLACE || (mode->viTVMode & 3) == VI_PROGRESSIVE;
		if (was_progressive != is_progressive) {
			mode_switch = true;
		}
	}

	last_fbWidth = mode->fbWidth;

	VIDEO_SetPostRetraceCallback (nullptr);
	copynow = GX_FALSE;
	VIDEO_Configure (mode);
	VIDEO_Flush();

	// Clear framebuffers
	// Force clear the maximum allocated size (640*576*2 bytes) to YUYV Black
	// Prevents out-of-phase pink flashes when shrinking to original video mode
	u32 max_xfb_words = (MAX_FB_WIDTH * MAX_FB_HEIGHT * 2) / 4;
	for(u32 i = 0; i < max_xfb_words; i++) {
		xfb[0][i] = COLOR_BLACK;
		xfb[1][i] = COLOR_BLACK;
	}

	// Flush the CPU data cache so the VI immediately sees the cleared memory
	DCFlushRange(xfb[0], MAX_FB_WIDTH * MAX_FB_HEIGHT * 2);
	DCFlushRange(xfb[1], MAX_FB_WIDTH * MAX_FB_HEIGHT * 2);

	VIDEO_SetNextFramebuffer (xfb[0]);

	// If the hardware sync is changing, hold the black screen for one extra frame
	// to allow the TV DAC to lock before un-blanking.
	if (mode_switch) {
		VIDEO_SetBlack(true);
		VIDEO_Flush();
		VIDEO_WaitForFlush();
	}

	VIDEO_SetBlack (false);
	VIDEO_Flush ();
	VIDEO_WaitForFlush ();

	VIDEO_SetPostRetraceCallback ((VIRetraceCallback)copy_to_xfb);
	vmode = mode;
}

/****************************************************************************
 * findVideoMode
 *
 * Finds the optimal video mode, or uses the user-specified one
 ***************************************************************************/
GXRModeObj* OgcVideoDriver::findVideoMode()
{
	GXRModeObj * mode;

	// choose the desired video mode
	switch(GCSettings.videoMode)
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

	// detect broadcast standard, for playback rate / vsync purposes
	if ((mode->viTVMode >> 2) == VI_PAL)
		vmode_60hz = false;
	else
		vmode_60hz = true;

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

void OgcVideoDriver::startMenuVideo()
{
	Mtx44 p;
	f32 yscale;
	u32 xfbHeight;
	GXRModeObj * rmode = findVideoMode();

	setupVideoMode(rmode); // reconfigure VI

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

void OgcVideoDriver::waitForBufferReady()
{
	u32 level;

	_CPU_ISR_Disable(level);
	while (!vb_done || (copynow == GX_TRUE))
	{
		LWP_ThreadSleep(render_queue); // Halts main thread with 0 CPU load until signals occur
	}
	_CPU_ISR_Restore(level);

	// Guarantee the GPU has fully finished rendering the previous frame
	// before we begin swizzling new data into texture memory
	GX_DrawDone();

	whichfb ^= 1;
}

void OgcVideoDriver::presentBuffer()
{
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	copynow = GX_TRUE;

	// Reset state and signal background VSync thread to begin waiting for next blanking interval
	u32 level;
	_CPU_ISR_Disable(level);
	vb_done = false;
	vb_wait = true;
	LWP_ThreadSignal(vb_queue);
	_CPU_ISR_Restore(level);
}

OgcVideoDriver::OgcVideoDriver()
    : screenWidth(0), screenHeight(0), imageRenderer(nullptr), glyphRenderer(nullptr), emulatorVideoDriver(nullptr)
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
	emulatorVideoDriver = new OgcEmulatorVideoDriver();
	emulatorVideoDriver->init(this);

	VIDEO_Init();

	// Allocate the video buffers
	xfb[0] = (u32 *) memalign(32, MAX_FB_WIDTH*MAX_FB_HEIGHT*2);
	xfb[1] = (u32 *) memalign(32, MAX_FB_WIDTH*MAX_FB_HEIGHT*2);
	DCInvalidateRange(xfb[0], MAX_FB_WIDTH*MAX_FB_HEIGHT*2);
	DCInvalidateRange(xfb[1], MAX_FB_WIDTH*MAX_FB_HEIGHT*2);
	xfb[0] = (u32 *) MEM_K0_TO_K1 (xfb[0]);
	xfb[1] = (u32 *) MEM_K0_TO_K1 (xfb[1]);

	GXRModeObj *rmode = findVideoMode();

#ifdef HW_RVL
if (CONF_GetAspectRatio() == CONF_ASPECT_16_9 && (*(u32*)(0xCD8005A0) >> 16) == 0xCAFE) // Wii U
{
	write32(0xd8006a0, 0x30000004), mask32(0xd8006a8, 0, 2);
}
#endif

	setupVideoMode(rmode);

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

void OgcVideoDriver::renderMenu()
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
