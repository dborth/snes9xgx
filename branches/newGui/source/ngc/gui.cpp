/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Michniewski 2008
 *
 * gui.cpp
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <malloc.h>

#include <pngu/pngu.h>
#include <ft2build.h>
#include <zlib.h>
#include FT_FREETYPE_H

#include "video.h"
#include "snes9xGX.h"
#include "gui.h"
#include "MEM2.h"
#include "menudraw.h"

struct sGui Gui;

GXTexObj texobj_BG, texobj_MENU;
void * texdata_bg = NULL;	// stores the blended menu backdrop
void * texdata_menu = NULL;	// stores the menu overlay

bool mem_alloced = 0;

extern GXTexObj texobj;
extern int vwidth, vheight;
extern s16 square[];
extern Mtx view;
extern int whichfb;
extern unsigned int copynow;
extern int screenheight;
extern unsigned int *xfb[2];
extern GXRModeObj *vmode;

extern FT_Library ftlibrary;
extern FT_Face face;
extern FT_GlyphSlot slot;
extern FT_UInt glyph_index;

extern int WaitButtonAB ();

extern lwp_t vbthread;

extern unsigned char texturemem[512 * (512 + 8)] ATTRIBUTE_ALIGN (32);


// MAIN

typedef struct tagcamera
{
	Vector pos;
	Vector up;
	Vector view;
}
camera;

static camera cam = {
	{0.0F, 0.0F, 0.0F},
	{0.0F, 0.5F, 0.0F},
	{0.0F, 0.0F, -0.5F}
};

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

void
gui_draw_init ()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	gui_alphasetup ();
	
	GX_SetNumTexGens (1);
	GX_SetNumChans (1);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);	// GX_REPLACE
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	
	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
	
	GX_InvalidateTexAll();
}

void
gui_alphasetup ()
{
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);	// pxl = Asrc * SrcPixel + (1 - Asrc) * DstPixel
	GX_SetAlphaUpdate(GX_ENABLE);
}

void
gui_alloc ()
{
	if (!mem_alloced)
	{
		#ifdef HW_RVL
		//texdata_bg = (u8 *) TEXCACHE1_LO;
		//texdata_menu = (u8 *) TEXCACHE2_LO;
		//Gui.texmem = (u8 *) GUICACHE_LO;
		
		texdata_bg = (u8 *) SYS_AllocArena2MemLo(640 * 480 * 4, 32);
		texdata_menu = (u8 *) SYS_AllocArena2MemLo(640 * 480 * 4, 32);
		Gui.texmem = (u8 *) SYS_AllocArena2MemLo(640 * 480 * 4, 32);
		#else
		
		// perhaps store in aram
		texdata_bg = malloc(640 * 480 * 4);
		texdata_menu = malloc(640 * 480 * 4);
		Gui.texmem = malloc(640 * 480 * 4);
		
		#endif
		
		if ( texdata_bg == NULL || texdata_menu == NULL || Gui.texmem == NULL )
			WaitPrompt ((char*)"couldnt allocate video memory");
		
		mem_alloced = 1;
	}
}

void
gui_free ()
{
	if (mem_alloced)
	{
		#ifndef HW_RVL
		free (texdata_bg);
		free (texdata_menu);
		free (Gui.texmem);
		#endif
		
		mem_alloced = 0;
	}
}

/****************************************************************************
* make BG
*
* Blend the last rendered emulator screen and the menu backdrop.
* Save this as a texture to be loaded later 
 ****************************************************************************/
void
gui_makebg ()
{

	// Ensure previous vb has completed
	while ((LWP_ThreadIsSuspended (vbthread) == 0) || (copynow == GX_TRUE))
	{
		usleep (50);
	}

	whichfb ^= 1;

		IMGCTX ctx;
		PNGUPROP imgProp;

		/** Load menu backdrop (either from file or buffer) **/

		if ( !(ctx = PNGU_SelectImageFromDevice ("bg.png")) )
			WaitPrompt((char*)"Failed to open bg.png");
		PNGU_GetImageProperties (ctx, &imgProp);
		// can check image dimensions here
		//texdata_bg = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
		
		GX_InitTexObj (&texobj_BG, texdata_bg, imgProp.imgWidth, imgProp.imgHeight, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		
		PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, texdata_bg, 255);
		//PNGU_DecodeToRGBA8 (ctx, 640, 480, Gui.texmem, 0, 7);
		//Make_Texture_RGBA8 (texdata_bg, Gui.texmem, 640, 480);
		PNGU_ReleaseImageContext (ctx);
		DCFlushRange (texdata_bg, imgProp.imgWidth * imgProp.imgHeight * 4);

		//GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
		

		/** blend last rendered snes frame and menu backdrop **/

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
		GX_InvVtxCache ();
		
		gui_draw_init ();
		
		// draw 2 quads

		GX_InvalidateTexAll ();

		// behind (last snes frame)
		//square[2] = square[5] = square[8] = square[11] = 0;     // z value
		
		
		//GX_LoadTexObj (&texobj, GX_TEXMAP0);    // load last rendered snes frame
		//draw_square (view);

		// in front (menu backdrop)
		//square[2] = square[5] = square[8] = square[11] = 1;     // z value
		
		GX_LoadTexObj (&texobj_BG, GX_TEXMAP0);	// load menu overlay
		draw_square (view);

		GX_DrawDone ();

		 //DEBUG -----------
		// show the output
		VIDEO_SetNextFramebuffer (xfb[whichfb]);
		VIDEO_Flush ();
		copynow = GX_TRUE;
		#ifdef VIDEO_THREADING
		// Return to caller, don't waste time waiting for vb
		LWP_ResumeThread (vbthread);
		#endif
		WaitButtonAB();
		
		exit(0); ///////////////////////////////////////////////////////////////////////
		
       
		// load blended image from efb to a texture
		GX_InitTexObj (&texobj_BG, texdata_bg, 640, 480, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_SetTexCopySrc ( 0,0,vmode->fbWidth,vmode->efbHeight );
		GX_SetTexCopyDst ( 640, 480, GX_TF_RGBA8, 0 );
		GX_CopyTex (texdata_bg, 0);    // assuming that the efb is 640x480, which it should be
		GX_PixModeSync ();      // wait until copy has completed
		DCFlushRange (texdata_bg, 640 * 480 * 4);
		GX_InvalidateTexAll ();

		//GX_LoadTexObj (&texobj_BG, GX_TEXMAP0);

		square[2] = square[5] = square[8] = square[11] = 0;     // reset z value
		GX_InvVtxCache ();
}


void
gui_clearscreen ()
{
		whichfb ^= 1;
		VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
		memset ( Gui.texmem, 0, 640*480*4 );
		Gui.fontcolour = 0;
}

void
gui_draw ()
{
		gui_drawbox (0, 0, 640, 80, 255, 255, 255, 128);        // topbar
		gui_drawbox (0, 370, 640, 480, 255, 255, 255, 128);     // bottombar
		gui_setfontcolour (0,255,0,255);
		// top bar text
		//setfontsize (32);       // 32/24 depending on whether selected or not
		//gui_DrawText (-1, 35, (char *)"Menu");
		// main text
		//setfontsize (24);      
		//gui_DrawText (75, 113, (char *)"Hello World");
		// bottom bar text
		//setfontsize (24);      
		//gui_DrawText (75, 400, (char *)"Description");
}

void
gui_savescreen ()
{
		FILE* handle;
		handle = fopen ("out.txt", "wb");
		fwrite (Gui.texmem, 1, sizeof(Gui.texmem), handle);
		fclose (handle);

		printf("\nsaved screen.");
}

void
gui_showscreen ()
{

		/** Screen to Texture **/
		GX_InitTexObj (&texobj_BG, texdata_bg, 640, 480, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObj (&texobj_MENU, texdata_menu, 640, 480, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		
		Make_Texture_RGBA8 (texdata_menu, Gui.texmem, 640, 480);
		DCFlushRange (&texdata_menu, 640 * 480 * 4);

		GX_InvalidateTexAll ();

		/** thats nice, but will it blend? **/

		// draw 640x480 quads
		int xscale, yscale, xshift, yshift;
		xshift = yshift = 0;
		xscale = 320;
		yscale = 240;
		square[6] = square[3]  =  xscale + xshift;
		square[0] = square[9]  = -xscale + xshift;
		square[4] = square[1]  =  yscale + yshift;
		square[7] = square[10] = -yscale + yshift;

		// draw 2 quads

		// backdrop
		square[2] = square[5] = square[8] = square[11] = 0;     // z value
		DCFlushRange (square, 32);
		GX_InvVtxCache ();
		gui_draw_init ();
		GX_LoadTexObj (&texobj_BG, GX_TEXMAP0);
		draw_square (view);

		// menu overlay
		square[2] = square[5] = square[8] = square[11] = 1;     // z value
		DCFlushRange (square, 32);
		GX_InvVtxCache ();
		gui_draw_init ();
		GX_LoadTexObj (&texobj_MENU, GX_TEXMAP0);
		draw_square (view);

		GX_DrawDone ();

		/** Display **/

		// show the output
		VIDEO_SetNextFramebuffer (xfb[whichfb]);
		VIDEO_Flush ();
		copynow = GX_TRUE;
		#ifdef VIDEO_THREADING
		/* Return to caller, don't waste time waiting for vb */
		LWP_ResumeThread (vbthread);
		#endif
		//WaitButtonAB();

		square[2] = square[5] = square[8] = square[11] = 0;     // z value
		GX_InvVtxCache ();
}



/****************************************************************************
* Make Texture RGBA8
* 
* input: pointer to RGBA data
* output: formatted texture data (GX_TF_RGBA8)
* code modified from quake wii (thanks :)
* todo: fix last few lines (?)
 ****************************************************************************/
void 
Make_Texture_RGBA8 (void * dst_tex, void * src_data, int width, int height)
{
	if ( (width % 4) || (height % 4) ) {
		printf ("Error: make_texture_rgba8 width/height not multiple of 4");
		return;
	}
	
	int i, x, y;
	u8 *pos;

	pos = (u8 *)dst_tex;
	for (y = 0; y < height; y += 4)	// process 4 lines at a time to make rows of tiles
	{
		u8* row1 = (u8 *) src_data;
		u8* row2 = (u8 *) src_data;
		u8* row3 = (u8 *) src_data;
		u8* row4 = (u8 *) src_data;
		row1 += width * 4 * (y + 0);	// move 640 pixels x 4 bytes * line #
		row2 += width * 4 * (y + 1);
		row3 += width * 4 * (y + 2);
		row4 += width * 4 * (y + 3);

		for (x = 0; x < width; x += 4)	// move across 4 pixels per tile
		{
			u8 AR[32];
			u8 GB[32];

			for (i = 0; i < 4; i++)	// save those 4 pixels of data in texture format
			{
				u8* ptr1 = &(row1[(x + i) * 4]);	// start at beginning of a row
				u8* ptr2 = &(row2[(x + i) * 4]);	// move across (4 pixels per tile + pixel offset within tile) * 4 bytes per pixel
				u8* ptr3 = &(row3[(x + i) * 4]);
				u8* ptr4 = &(row4[(x + i) * 4]);

				AR[(i * 2) +  0] = ptr1[3];		// fill columns of tile with rgba data
				AR[(i * 2) +  1] = ptr1[0];
				AR[(i * 2) +  8] = ptr2[3];
				AR[(i * 2) +  9] = ptr2[0];
				AR[(i * 2) + 16] = ptr3[3];
				AR[(i * 2) + 17] = ptr3[0];
				AR[(i * 2) + 24] = ptr4[3];
				AR[(i * 2) + 25] = ptr4[0];

				GB[(i * 2) +  0] = ptr1[1];
				GB[(i * 2) +  1] = ptr1[2];
				GB[(i * 2) +  8] = ptr2[1];
				GB[(i * 2) +  9] = ptr2[2];
				GB[(i * 2) + 16] = ptr3[1];
				GB[(i * 2) + 17] = ptr3[2];
				GB[(i * 2) + 24] = ptr4[1];
				GB[(i * 2) + 25] = ptr4[2];
			}

			memcpy(pos, AR, sizeof(AR));	// copy over to resulting texture
			pos += sizeof(AR);
			memcpy(pos, GB, sizeof(GB));
			pos += sizeof(GB);
		}
	}

}

void
gui_drawbox (int x1, int y1, int width, int height, int r, int g, int b, int a)
{
	u32 colour = ((u8)r << 24) | ((u8)g << 16) | ((u8)b << 8) | (u8)a;
	
	int i, j;
	
	u32* memory = (u32*) Gui.texmem;
	for (j = y1; j<height; j++) {
		for (i = x1; i<width; i++) {
			memory[(j*640) + i] = colour;
		}
	}
}

/****************************************************************************
* DrawCharacter
* Draws a single character on the screen
 ****************************************************************************/
static void
gui_DrawCharacter (FT_Bitmap * bmp, FT_Int x, FT_Int y)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bmp->width;
	FT_Int y_max = y + bmp->rows;
	int c;

	u32* memory = (u32*)Gui.texmem;
	for (i = x, p = 0; i < x_max; i++, p++)
	{
		for (j = y, q = 0; j < y_max; j++, q++)
		{
			if (i < 0 || j < 0 || i >= 640 || j >= 480)
				continue;

			c = bmp->buffer[q * bmp->width + p];

			/*** Cool Anti-Aliasing doesn't work too well at hires on GC ***/
			if (c > 128)
				memory[(j * 640) + i] = Gui.fontcolour;
		}
	}
}

/****************************************************************************
 * DrawText
 *
 * Place the font bitmap on the screen
 ****************************************************************************/
void
gui_DrawText (int x, int y, char *text)
{
	int px, n;
	int i;
	int err;
	int value, count;

	n = strlen (text);
	if (n == 0)
		return;

	/*** x == -1, auto centre ***/
	if (x == -1)
	{
		value = 0;
		px = 0;
	}
	else
	{
		value = 1;
		px = x;
	}

	for (count = value; count < 2; count++)
	{
		/*** Draw the string ***/
		for (i = 0; i < n; i++)
		{
			err = FT_Load_Char (face, text[i], FT_LOAD_RENDER);

			if (err)
			{
				printf ("Error %c %d\n", text[i], err);
				continue;				/*** Skip unprintable characters ***/
			}

			if (count)
				gui_DrawCharacter (&slot->bitmap, px + slot->bitmap_left, y - slot->bitmap_top);

			px += slot->advance.x >> 6;
		}

		px = (640 - px) >> 1;

	}
}

/****************************************************************************
 * setfontcolour
 *
 * Uses RGB triple values.
 ****************************************************************************/
void
gui_setfontcolour (int r, int g, int b, int a)
{
	Gui.fontcolour = ((u8)r << 24) | ((u8)g << 16) | ((u8)b << 8) | (u8)a;
}

/****************************************************************************
 * DrawLine
 *
 * Quick'n'Dirty Bresenham line drawing routine.
 ****************************************************************************/
#define SIGN(x) ((x<0)?-1:((x>0)?1:0))

void
gui_DrawLine (int x1, int y1, int x2, int y2, int r, int g, int b, int a)
{
	u32 colour;
	int i, dx, dy, sdx, sdy, dxabs, dyabs, x, y, px, py;
	int sp;
	
	u32* memory = (u32*)Gui.texmem;

	colour = ((u8)r << 24) | ((u8)g << 16) | ((u8)b << 8) | (u8)a;

	dx = x2 - x1;		/*** Horizontal distance ***/
	dy = y2 - y1;		/*** Vertical distance ***/

	dxabs = abs (dx);
	dyabs = abs (dy);
	sdx = SIGN (dx);
	sdy = SIGN (dy);
	x = dyabs >> 1;
	y = dxabs >> 1;
	px = x1;
	py = y1;

	sp = (py * 640) + px;
	
	/*** Plot this pixel ***/
	memory[sp] = colour;

	if (dxabs >= dyabs)		/*** Mostly horizontal ***/
	{
		for (i = 0; i < dxabs; i++)
		{
			y += dyabs;
			if (y >= dxabs)
			{
				y -= dxabs;
				py += sdy;
			}

			px += sdx;
			sp = (py * 640) + px;
			memory[sp] = colour;
		}
	}
	else
	{
		for (i = 0; i < dyabs; i++)
		{
			x += dxabs;
			if (x >= dyabs)
			{
				x -= dyabs;
				px += sdx;
			}

			py += sdy;
			sp = (py * 640) + px;
			memory[sp] = colour;
		}
	}
}

/* */

	/**  draw menu, etc **/ 
		// - draw topbar, bottombar
		// - draw icons
		// - draw bar text
		
		// draw main text
	
	/** make textures **/
		// - load background texture
		// - draw onto quad
		// - make foreground texture
		// - draw onto quad
	
	/** render image **/
	
	/** update framebuffer **/

/* */

// EOF
