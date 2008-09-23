/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 June 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * menudraw.cpp
 *
 * Menu drawing routines
 *
 * Uses libfreetype 2.2.1 compiled for GC with TTF support only.
 * TTF only reduces the library by some 900k bytes!
 *
 * **WARNING***
 *
 * ONLY USE GUARANTEED PATENT FREE FONTS.
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiiuse/wpad.h>
#include <ft2build.h>
#include <zlib.h>
#include FT_FREETYPE_H

#include "memmap.h"

#include "video.h"
#include "menudraw.h"
#include "snes9xGX.h"
#include "filesel.h"
#include "dvd.h"
#include "aram.h"
#include "images/gfx_bg.h"
#include "input.h"

/*** Globals ***/
FT_Library ftlibrary;
FT_Face face;
FT_GlyphSlot slot;
FT_UInt glyph_index;
static unsigned int fonthi, fontlo;

extern char fontface[];		/*** From fontface.s ***/
extern int fontsize;		/*** From fontface.s ***/
extern int screenheight;
extern unsigned int *xfb[2];
extern int whichfb;

/*** Permanent backdrop ***/
#ifdef HW_RVL
u32 *backdrop;
#else
static u32 *backdrop;
#endif
unsigned int getcolour (u8 r1, u8 g1, u8 b1);
void DrawLineFast( int startx, int endx, int y, u8 r, u8 g, u8 b );
u32 getrgb( u32 ycbr, u32 low );

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
  setfontcolour (0xff, 0xff, 0xff);

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

/****************************************************************************
 * DrawCharacter
 * Draws a single character on the screen
 ***************************************************************************/
static void
DrawCharacter (FT_Bitmap * bmp, FT_Int x, FT_Int y)
{
  FT_Int i, j, p, q;
  FT_Int x_max = x + bmp->width;
  FT_Int y_max = y + bmp->rows;
  int spos;
  unsigned int pixel;
  int c;

  for (i = x, p = 0; i < x_max; i++, p++)
    {
      for (j = y, q = 0; j < y_max; j++, q++)
	{
	  if (i < 0 || j < 0 || i >= 640 || j >= screenheight)
	    continue;

			/*** Convert pixel position to GC int sizes ***/
	  spos = (j * 320) + (i >> 1);

	  pixel = xfb[whichfb][spos];
	  c = bmp->buffer[q * bmp->width + p];

	  /*** Cool Anti-Aliasing doesn't work too well at hires on GC ***/
	  if (c > 128)
	    {
	      if (i & 1)
		pixel = (pixel & 0xffff0000) | fontlo;
	      else
		pixel = ((pixel & 0xffff) | fonthi);

	      xfb[whichfb][spos] = pixel;
	    }
	}
    }
}

/****************************************************************************
 * DrawText
 *
 * Place the font bitmap on the screen
 ***************************************************************************/
void
DrawText (int x, int y, char *text)
{
  int px, n;
  int i;
  int err;
  int value, count;

  n = strlen (text);
  if (n == 0)
    return;

  setfontcolour (0x00, 0x00, 0x00);

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
	    DrawCharacter (&slot->bitmap, px + slot->bitmap_left,
			   y - slot->bitmap_top);

	  px += slot->advance.x >> 6;
	}

      px = (640 - px) >> 1;

    }
}

/****************************************************************************
 * setfontcolour
 *
 * Uses RGB triple values.
 ***************************************************************************/
void
setfontcolour (u8 r, u8 g, u8 b)
{
  u32 fontcolour;

  fontcolour = getcolour (r, g, b);
  fonthi = fontcolour & 0xffff0000;
  fontlo = fontcolour & 0xffff;
}

/****************************************************************************
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED IN ANY DERIVATIVE WORK.
 ***************************************************************************/
void
Credits ()
{
	clearscreen ();

	setfontcolour (0x00, 0x00, 0x00);

	setfontsize (28);
	DrawText (-1, 60, (char*)"Credits");

	int ypos = 25;

	if (screenheight == 480)
		ypos += 52;
	else
		ypos += 32;

	setfontsize (18);
	DrawText (-1, ypos += 30, (char*)"Technical");

	setfontsize (14);
	DrawText (-1, ypos += 22, (char*)"Snes9x v1.5.1 - Snes9x Team");
	DrawText (-1, ypos += 18, (char*)"GameCube Port 2.0 WIP6 and earlier - SoftDev");
	DrawText (-1, ypos += 18, (char*)"Additional improvements to 2.0 WIP6 - eke-eke");
	DrawText (-1, ypos += 18, (char*)"GameCube 2.0.1bx enhancements - crunchy2");
	DrawText (-1, ypos += 18, (char*)"v00x updates - michniewski & Tantric");
	DrawText (-1, ypos += 18, (char*)"GX - http://www.gc-linux.org");
	DrawText (-1, ypos += 18, (char*)"libogc - Shagkur & wintermute");

	setfontsize (18);
	DrawText (-1, ypos += 30, (char*)"Testing");

	setfontsize (14);
	DrawText (-1, ypos += 22, (char*)"crunchy2 / tehskeen users / others");

	setfontsize (18);
	DrawText (-1, ypos += 30, (char*)"Documentation");

	setfontsize (14);
	DrawText (-1, ypos += 22, (char*)"brakken, crunchy2, michniewski");

	setfontsize (12);
	DrawText (-1, ypos += 50, (char*)"Snes9x - Copyright (c) Snes9x Team 1996 - 2006");
	DrawText (-1, ypos += 15, (char*)"This software is open source and may be copied, distributed, or modified");
	DrawText (-1, ypos += 15, (char*)"under the terms of the GNU General Public License (GPL) Version 2.");

	showscreen ();
}



/****************************************************************************
 * getcolour
 *
 * Simply converts RGB to Y1CbY2Cr format
 *
 * I got this from a pastebin, so thanks to whoever originally wrote it!
 ***************************************************************************/

unsigned int
getcolour (u8 r1, u8 g1, u8 b1)
{
  int y1, cb1, cr1, y2, cb2, cr2, cb, cr;
  u8 r2, g2, b2;

  r2 = r1;
  g2 = g1;
  b2 = b1;

  y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
  cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
  cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

  y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
  cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
  cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

  cb = (cb1 + cb2) >> 1;
  cr = (cr1 + cr2) >> 1;

  return ((y1 << 24) | (cb << 16) | (y2 << 8) | cr);
}

/****************************************************************************
 * Unpackbackdrop
 *
 * Decompress menu background and store it in ARAM or memory
 ***************************************************************************/
void
unpackbackdrop ()
{
	unsigned long res, inbytes, outbytes;
	unsigned int colour;
	int offset;
	int i;

	//  backdrop = (unsigned int *) malloc (screenheight * 1280);
	backdrop = (u32 *) malloc (screenheight * 1280);
	colour = getcolour (0x00, 0x00, 0x00);

	/*** Fill with black for now ***/
	for (i = 0; i < (320 * screenheight); i++)
	backdrop[i] = colour;

	/*** If it's PAL50, need to move down a few lines ***/
	offset = ((screenheight - 480) >> 1) * 320;
	inbytes = BG_COMPRESSED;
	outbytes = BG_RAW;

	res =
	uncompress ((Bytef *) backdrop + offset, &outbytes, (Bytef *) bg,
	inbytes);

	#ifndef HW_RVL
	/*** Now store the backdrop in ARAM ***/
	ARAMPut ((char *) backdrop, (char *) AR_BACKDROP, 640 * screenheight * 2);
	free (backdrop);
	#endif
	// otherwise (on wii) backdrop is stored in memory
	clearscreen ();
	showscreen ();
}

/****************************************************************************
 * Wait for user to press A
 ***************************************************************************/
void
WaitButtonA ()
{
#ifdef HW_RVL
  while ( (PAD_ButtonsDown (0) & PAD_BUTTON_A) || (WPAD_ButtonsDown(0) & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) ) VIDEO_WaitVSync();
  while (!(PAD_ButtonsDown (0) & PAD_BUTTON_A) && !(WPAD_ButtonsDown(0) & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) ) VIDEO_WaitVSync();
#else
  while ( PAD_ButtonsDown (0) & PAD_BUTTON_A ) VIDEO_WaitVSync();
  while (!(PAD_ButtonsDown (0) & PAD_BUTTON_A) ) VIDEO_WaitVSync();
#endif
}

/****************************************************************************
 * Wait for user to press A or B. Returns 0 = B; 1 = A
 ***************************************************************************/

int
WaitButtonAB ()
{
#ifdef HW_RVL
    u32 gc_btns, wm_btns;

    while ( (PAD_ButtonsDown (0) & (PAD_BUTTON_A | PAD_BUTTON_B))
			|| (WPAD_ButtonsDown(0) & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_A | WPAD_CLASSIC_BUTTON_B))
			) VIDEO_WaitVSync();

    while ( TRUE )
    {
        gc_btns = PAD_ButtonsDown (0);
		wm_btns = WPAD_ButtonsDown (0);
        if ( (gc_btns & PAD_BUTTON_A) || (wm_btns & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)) )
            return 1;
        else if ( (gc_btns & PAD_BUTTON_B) || (wm_btns & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)) )
            return 0;
    }
#else
    u32 gc_btns;

    while ( (PAD_ButtonsDown (0) & (PAD_BUTTON_A | PAD_BUTTON_B)) ) VIDEO_WaitVSync();

    while ( TRUE )
    {
        gc_btns = PAD_ButtonsDown (0);
        if ( gc_btns & PAD_BUTTON_A )
            return 1;
        else if ( gc_btns & PAD_BUTTON_B )
            return 0;
    }
#endif
}

/****************************************************************************
 * Show a prompt
 ***************************************************************************/
void
WaitPrompt (char *msg)
{
	int ypos = (screenheight - 64) >> 1;

	if (screenheight == 480)
		ypos += 52;
	else
		ypos += 32;

	clearscreen ();
	DrawText (-1, ypos, msg);
	ypos += 30;
	DrawText (-1, ypos, (char*)"Press A to continue");
	showscreen ();
	WaitButtonA ();
}

/****************************************************************************
 * Show a prompt with choice of two options. Returns 1 if A button was pressed
   and 0 if B button was pressed.
 ***************************************************************************/
int
WaitPromptChoice (char *msg, char *bmsg, char *amsg)
{
	int ypos = (screenheight - 64) >> 1;

	if (screenheight == 480)
		ypos += 37;
	else
		ypos += 17;

	clearscreen ();
	DrawText (-1, ypos, msg);
	ypos += 60;
	char txt[80];
	sprintf (txt, "B = %s   :   A = %s", bmsg, amsg);
	DrawText (-1, ypos, txt);
	showscreen ();
	return WaitButtonAB ();
}

/****************************************************************************
 * Show an action in progress
 ***************************************************************************/
void
ShowAction (char *msg)
{
	int ypos = (screenheight - 30) >> 1;

	if (screenheight == 480)
		ypos += 52;
	else
		ypos += 32;

	clearscreen ();
	DrawText (-1, ypos, msg);
	showscreen ();
}

/****************************************************************************
 * Generic Menu Routines
 ***************************************************************************/
void
DrawMenu (char items[][50], char *title, int maxitems, int selected, int fontsize, int x)
{
	int i, w = 0;
	int ypos = 0;
	int n = 1;
	int line_height;

	ypos = 45;

	if (screenheight == 480)
		ypos += 52;
	else
		ypos += 32;

	clearscreen ();

	setfontcolour (0, 0, 0);

	if (title != NULL)
	{
		setfontsize (28);
		DrawText (-1, 60, title);
	}

	setfontsize (12);
	DrawText (510, screenheight - 20, (char *)"Snes9x GX 005");

	// Draw menu items

	setfontsize (fontsize);	// set font size

	line_height = (fontsize + 8);

	for (i = 0; i < maxitems; i++)
	{
		if(strlen(items[i]) > 0)
		{
			if ( items[i] == NULL )
				ypos -= line_height;
			else if (i == selected)
			{
				for( w = 0; w < line_height; w++ )
					DrawLineFast( 30, 610, n * line_height + (ypos-line_height+6) + w, 0x80, 0x80, 0x80 );

				setfontcolour (0xff, 0xff, 0xff);
				DrawText (x, n * line_height + ypos, items[i]);
				setfontcolour (0x00, 0x00, 0x00);
			}
			else
			{
				DrawText (x, n * line_height + ypos, items[i]);
			}
			n++;
		}
	}

	showscreen ();

}

/****************************************************************************
 * FindMenuItem
 *
 * Help function to find the next visible menu item on the list
 * Supports menu wrap-around
 ***************************************************************************/

int FindMenuItem(char items[][50], int maxitems, int currentItem, int direction)
{
	int nextItem = currentItem + direction;

	if(nextItem < 0)
		nextItem = maxitems-1;
	else if(nextItem >= maxitems)
		nextItem = 0;

	if(strlen(items[nextItem]) > 0)
		return nextItem;
	else
		return FindMenuItem(&items[0], maxitems, nextItem, direction);
}

/****************************************************************************
 * RunMenu
 *
 * Call this with the menu array defined in menu.cpp
 * It's here to keep all the font / interface stuff together.
 ***************************************************************************/
int menu = 0;

int
RunMenu (char items[][50], int maxitems, char *title, int fontsize, int x)
{
    int redraw = 1;
    int quit = 0;
    int ret = 0;

    u32 p = 0;
	u32 wp = 0;
    signed char gc_ay = 0;
	signed char wm_ay = 0;

    while (quit == 0)
    {
        if (redraw)
        {
            DrawMenu (&items[0], title, maxitems, menu, fontsize);
            redraw = 0;
        }

		gc_ay = PAD_StickY (0);
        p = PAD_ButtonsDown (0);
#ifdef HW_RVL
		wm_ay = WPAD_StickY (0,0);
		wp = WPAD_ButtonsDown (0);
#endif


		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads

        /*** Look for up ***/
        if ( (p & PAD_BUTTON_UP) || (wp & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)) || (gc_ay > PADCAL) || (wm_ay > PADCAL) )
        {
            redraw = 1;
            menu = FindMenuItem(&items[0], maxitems, menu, -1);
        }

        /*** Look for down ***/
        if ( (p & PAD_BUTTON_DOWN) || (wp & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)) || (gc_ay < -PADCAL) || (wm_ay < -PADCAL) )
        {
            redraw = 1;
            menu = FindMenuItem(&items[0], maxitems, menu, +1);
        }

        if ((p & PAD_BUTTON_A) || (wp & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)))
        {
            quit = 1;
            ret = menu;
        }

        if ((p & PAD_BUTTON_B) || (wp & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)))
        {
            quit = -1;
            ret = -1;
        }
    }

	/*** Wait for B button to be released before proceeding ***/
	while ( (PAD_ButtonsDown(0) & PAD_BUTTON_B)
#ifdef HW_RVL
			|| (WPAD_ButtonsDown(0) & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B))
#endif
			)
	{
		ret = -1;
		VIDEO_WaitVSync();
	}

    return ret;

}

/****************************************************************************
 * Showfile screen
 *
 * Display the file selection to the user
 ***************************************************************************/

void
ShowFiles (FILEENTRIES filelist[], int maxfiles, int offset, int selection)
{
	int i, j;
	char text[MAXPATHLEN];
	int ypos;
	int w;

	clearscreen ();

	setfontsize (28);
	DrawText (-1, 60, (char*)"Choose Game");

	setfontsize(18);

	ypos = (screenheight - ((PAGESIZE - 1) * 20)) >> 1;

	if (screenheight == 480)
		ypos += 24;
	else
		ypos += 10;

	j = 0;
	for (i = offset; i < (offset + PAGESIZE) && (i < maxfiles); i++)
	{
		if (filelist[i].flags)	// if a dir
		{
			strcpy (text, "[");
			strcat (text, filelist[i].displayname);
			strcat (text, "]");
		}
		else
		{
			// hide file extension on listing (.7z, .fig, .smc)
			StripExt(text, filelist[i].displayname);
		}
		if (j == (selection - offset))
		{
			/*** Highlighted text entry ***/
			for ( w = 0; w < 20; w++ )
				DrawLineFast( 30, 610, ( j * 20 ) + (ypos-16) + w, 0x80, 0x80, 0x80 );

			setfontcolour (0x00, 0x00, 0xe0);
			DrawText (50, (j * 20) + ypos, text);
			setfontcolour (0x00, 0x00, 0x00);
		}
		else
		{
			/*** Normal entry ***/
			DrawText (50, (j * 20) + ypos, text);
		}
		j++;
	}
	showscreen ();
}

/****************************************************************************
 * Cheats screen
 *
 * Displays a scrollable list of cheats to the user
 ***************************************************************************/

void
ShowCheats (char items[][50], char itemvalues[][50], int maxitems, int offset, int selection)
{
	int i, j = 0;
	int ypos;
	int w;

	clearscreen ();

	setfontsize (28);
	DrawText (-1, 60, (char*)"Cheats");

	setfontsize(18);

	ypos = (screenheight - ((PAGESIZE - 1) * 20)) >> 1;

	if (screenheight == 480)
		ypos += 24;
	else
		ypos += 10;

	for (i = offset; i < (offset + PAGESIZE) && (i < maxitems); i++)
	{
		if (i == selection)
		{
			/*** Highlighted text entry ***/
			for ( w = 0; w < 20; w++ )
				DrawLineFast( 30, 610, ( j * 20 ) + (ypos-16) + w, 0x80, 0x80, 0x80 );

			DrawText (150, (j * 20) + ypos, items[i]);
			DrawText (400, (j * 20) + ypos, itemvalues[i]);
		}
		else
		{
			/*** Normal entry ***/
			DrawText (150, (j * 20) + ypos, items[i]);
			DrawText (400, (j * 20) + ypos, itemvalues[i]);
		}
		j++;
	}
	showscreen ();
}

/****************************************************************************
 * ROM Information Screen
 ***************************************************************************/

void RomInfo()
{
	clearscreen ();

	int ypos = 65;

	if (screenheight == 480)
		ypos += 52;
	else
		ypos += 32;

	setfontsize (28);
	DrawText (-1, 60, (char*)"Rom Information");

	setfontsize (16);
	setfontcolour (0x00, 0x00, 0x00);

	#define MENU_INFO_ROM           "ROM"
	#define MENU_INFO_ROMID         "ROMID"
	#define MENU_INFO_COMPANY       "Company"
	#define MENU_INFO_SIZE          "Size"
	#define MENU_INFO_SRAM          "SRAM"
	#define MENU_INFO_TYPE          "Type"
	#define MENU_INFO_CHECKSUM      "Checksum"
	#define MENU_INFO_TVTYPE        "TV Type"
	#define MENU_INFO_FRAMES        "Frames"
	#define MENU_INFO_CRC32         "CRC32"

	char fmtString[1024];

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_ROM);
	DrawText (300, ypos, Memory.ROMName);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_ROMID);
	DrawText (300, ypos, Memory.ROMId);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_COMPANY);
	DrawText (300, ypos, Memory.CompanyId);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_SIZE);
	sprintf(fmtString, "%d", Memory.ROMSize);
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_SRAM);
	sprintf(fmtString, "%d", Memory.SRAMSize);
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_TYPE);
	sprintf(fmtString, "%d", Memory.ROMType);
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_CHECKSUM);
	sprintf(fmtString, "%04x / %04x", Memory.ROMChecksum, Memory.ROMComplementChecksum);
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_TVTYPE);
	sprintf(fmtString, "%s", Settings.PAL ? "PAL" : "NTSC");
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_FRAMES);
	sprintf(fmtString, "%d", Memory.ROMFramesPerSecond);
	DrawText (300, ypos, fmtString);

	ypos += 20;
	DrawText (150, ypos, (char *)MENU_INFO_CRC32);
	sprintf(fmtString, "%08X", Memory.ROMCRC32);
	DrawText (300, ypos, fmtString);

	showscreen ();
}


/****************************************************************************
 * DrawLine
 *
 * Quick'n'Dirty Bresenham line drawing routine.
 ***************************************************************************/
#define SIGN(x) ((x<0)?-1:((x>0)?1:0))

void
DrawLine (int x1, int y1, int x2, int y2, u8 r, u8 g, u8 b)
{
  u32 colour, pixel;
  u32 colourhi, colourlo;
  int i, dx, dy, sdx, sdy, dxabs, dyabs, x, y, px, py;
  int sp;

  colour = getcolour (r, g, b);
  colourhi = colour & 0xffff0000;
  colourlo = colour & 0xffff;

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

  sp = (py * 320) + (px >> 1);
  pixel = xfb[whichfb][sp];
	/*** Plot this pixel ***/
  if (px & 1)
    xfb[whichfb][sp] = (pixel & 0xffff0000) | colourlo;
  else
    xfb[whichfb][sp] = (pixel & 0xffff) | colourhi;

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

	  sp = (py * 320) + (px >> 1);
	  pixel = xfb[whichfb][sp];

	  if (px & 1)
	    xfb[whichfb][sp] = (pixel & 0xffff0000) | colourlo;
	  else
	    xfb[whichfb][sp] = (pixel & 0xffff) | colourhi;
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

	  sp = (py * 320) + (px >> 1);
	  pixel = xfb[whichfb][sp];

	  if (px & 1)
	    xfb[whichfb][sp] = (pixel & 0xffff0000) | colourlo;
	  else
	    xfb[whichfb][sp] = (pixel & 0xffff) | colourhi;
	}
    }
}

/****************************************************************************
 * Progress Bar
 *
 * Show the user what's happening
 ***************************************************************************/
void
ShowProgress (char *msg, int done, int total)
{
  int ypos = (screenheight - 30) >> 1;

  if (screenheight == 480)
    ypos += 52;
  else
    ypos += 32;

  int xpos;
  int i;

  clearscreen ();
  DrawText (-1, ypos, msg);

	/*** Draw a white outline box ***/
  for (i = 380; i < 401; i++)
    DrawLine (100, i, 540, i, 0xff, 0xff, 0xff);

	/*** Show progess ***/
  xpos = (int) (((float) done / (float) total) * 438);

  for (i = 381; i < 400; i++)
    DrawLine (101, i, 101 + xpos, i, 0x00, 0x00, 0x80);

  showscreen ();
}

/****************************************************************************
 * DrawPolygon
 ***************************************************************************/
void
DrawPolygon (int vertices, int varray[], u8 r, u8 g, u8 b)
{
  int i;

  for (i = 0; i < vertices - 1; i++)
    {
      DrawLine (varray[(i << 1)], varray[(i << 1) + 1], varray[(i << 1) + 2],
		varray[(i << 1) + 3], r, g, b);
    }

  DrawLine (varray[0], varray[1], varray[(vertices << 1) - 2],
	    varray[(vertices << 1) - 1], r, g, b);
}

/****************************************************************************
 * Draw Line Fast
 *
 * This routine requires that start and endx are 32bit aligned.
 * It tries to perform a semi-transparency over the existing image.
 ***************************************************************************/

#define SRCWEIGHT 0.7f
#define DSTWEIGHT (1.0f - SRCWEIGHT)

static inline u8 c_adjust( u8 c , float weight )
{
	return (u8)((float)c * weight);
}

void DrawLineFast( int startx, int endx, int y, u8 r, u8 g, u8 b )
{
	int width;
	u32 offset;
	int i;
	u32 colour, clo, chi;
	u32 lo,hi;
	u8 *s, *d;

	//colour = getcolour(r, g, b);
	colour = ( r << 16 | g << 8 | b );
	d = (u8 *)&colour;
	d[1] = c_adjust(d[1], DSTWEIGHT);
	d[2] = c_adjust(d[2], DSTWEIGHT);
	d[3] = c_adjust(d[3], DSTWEIGHT);

	width = ( endx - startx ) >> 1;
	offset = ( y << 8 ) + ( y << 6 ) + ( startx >> 1 );

	for ( i = 0; i < width; i++ )
	{
		lo = getrgb(xfb[whichfb][offset], 0);
		hi = getrgb(xfb[whichfb][offset], 1);

		s = (u8 *)&hi;
		s[1] = ( ( c_adjust(s[1],SRCWEIGHT) ) + d[1] );
		s[2] = ( ( c_adjust(s[2],SRCWEIGHT) ) + d[2] );
		s[3] = ( ( c_adjust(s[3],SRCWEIGHT) ) + d[3] );

		s = (u8 *)&lo;
                s[1] = ( ( c_adjust(s[1],SRCWEIGHT) ) + d[1] );
                s[2] = ( ( c_adjust(s[2],SRCWEIGHT) ) + d[2] );
                s[3] = ( ( c_adjust(s[3],SRCWEIGHT) ) + d[3] );

		clo = getcolour( s[1], s[2], s[3] );
		s = (u8 *)&hi;
		chi = getcolour( s[1], s[2], s[3] );

		xfb[whichfb][offset++] = (chi & 0xffff0000 ) | ( clo & 0xffff) ;
	}
}

/****************************************************************************
 * Ok, I'm useless with Y1CBY2CR colour.
 * So convert back to RGB so I can work with it -;)
 ***************************************************************************/
u32 getrgb( u32 ycbr, u32 low )
{
	u8 r,g,b;
	u32 y;
	s8 cb,cr;

	if ( low )
		y = ( ycbr & 0xff00 ) >> 8;
	else
		y = ( ycbr & 0xff000000 ) >> 24;

	cr = ycbr & 0xff;
	cb = ( ycbr & 0xff0000 ) >> 16;

	cr -= 128;
	cb -= 128;

	r = (u8)((float)y + 1.371 * (float)cr);
	g = (u8)((float)y - 0.698 * (float)cr - 0.336 * (float)cb);
	b = (u8)((float)y + 1.732 * (float)cb);

	return (u32)( r << 16 | g << 8 | b );

}
