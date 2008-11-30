#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include "libpng/pngu/pngu.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include "new_gui.h"

struct sGui Gui;

FT_Library ftlibrary;
FT_Face face;
FT_GlyphSlot slot;
FT_UInt glyph_index;

extern char fontface[];		/*** From fontface.s ***/
extern int fontsize;		/*** From fontface.s ***/

/****************************************************************************
 * Initialisation of libfreetype
 ***************************************************************************/
int
FT_Init ()
{

  int err;

  err = FT_Init_FreeType (&ftlibrary);
  if (err)
    return 1;

  err =
    FT_New_Memory_Face (ftlibrary, (FT_Byte *) fontface, fontsize, 0, &face);
  if (err)
    return 1;

  setfontsize (16);
  //setfontcolour (0xff, 0xff, 0xff);

  slot = face->glyph;

  return 0;

}

/****************************************************************************
 * setfontsize
 *
 * Set the screen font size in pixels
 ***************************************************************************/
void
setfontsize (int pixelsize)
{
  int err;

  err = FT_Set_Pixel_Sizes (face, 0, pixelsize);

  if (err)
    printf ("Error setting pixel sizes!");
}

void
gui_draw ()
{
		gui_drawbox (0, 0, 640, 80, 255, 255, 255, 128);        // topbar
		gui_drawbox (0, 370, 640, 480, 255, 255, 255, 128);     // bottombar
		gui_setfontcolour (0,255,0,255);
		// top bar text
		setfontsize (32);       // 32/24 depending on whether selected or not
		gui_DrawText (-1, 35, (char *)"Menu");
		// main text
		setfontsize (24);      
		gui_DrawText (75, 113, (char *)"Hello World");
		// bottom bar text
		setfontsize (24);      
		gui_DrawText (75, 400, (char *)"Description");
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

