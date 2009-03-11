/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/





#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif
#include <stdio.h>

#ifndef __WIN32__
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <string.h>
#include <fcntl.h>

#include <pngu/pngu.h>
#include <png.h>
#include "snes9xGX.h"
#include "fileop.h"
#include "filebrowser.h"

#include "snes9x.h"
#include "memmap.h"
#include "display.h"
#include "gfx.h"
#include "ppu.h"
#include "screenshot.h"

#define ShowFailure() S9xSetInfoString("Failed to take screenshot.")

#if 0
	//
	// Need png.c from libpng to access the follow functions (and the code in s9xdoscreenshot)
	//
	
	// Constants
	#define PNGU_SOURCE_BUFFER			1
	#define PNGU_SOURCE_DEVICE			2

	//extern struct _IMGCTX;

	extern "C" {
	// Prototypes of helper functions
	int pngu_info (IMGCTX ctx);
	//int pngu_decode (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, PNGU_u32 stripAlpha);
	void pngu_free_info (IMGCTX ctx);
	//void pngu_read_data_from_buffer (png_structp png_ptr, png_bytep data, png_size_t length);
	void pngu_write_data_to_buffer (png_structp png_ptr, png_bytep data, png_size_t length);
	void pngu_flush_data_to_buffer (png_structp png_ptr);
	//int pngu_clamp (int value, int min, int max);
	}
#endif

bool8 S9xDoScreenshot(int width, int height)
{

	{
		static char str [64];
		sprintf(str, "Saving screenshot");
		S9xSetInfoString(str);
		return true;
	}
	
#if 0	// disable screenshots for now. Code is not quite right.

	IMGCTX ctx;
	png_color_8 sig_bit;
	uint16 *screen = GFX.Screen;	// source
	int method = GCSettings.SaveMethod;
	
	// Erase from the context any info
	pngu_free_info (ctx);
	ctx->propRead = 0;
	
	// Set some defaults
	ctx->source = PNGU_SOURCE_DEVICE; // or PNGU_SOURCE_BUFFER;
	if ( !MakeFilePath(ctx->filename, FILE_SCREEN, method) )
		return FALSE;
	
	// init destination file
	if (ctx->source == PNGU_SOURCE_BUFFER);	
	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Open file
		if (!(ctx->fd = fopen (ctx->filename, "wb")))
			return PNGU_CANT_OPEN_FILE;
	}
	else
		return PNGU_NO_FILE_SELECTED;
		
	// Allocation of libpng structs
	ctx->png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!(ctx->png_ptr))
	{
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        return PNGU_LIB_ERROR;
	}

    ctx->info_ptr = png_create_info_struct (ctx->png_ptr);
    if (!(ctx->info_ptr))
    {
		png_destroy_write_struct (&(ctx->png_ptr), (png_infopp)NULL);
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        return PNGU_LIB_ERROR;
    }

	if (ctx->source == PNGU_SOURCE_BUFFER)
	{
		// Installation of our custom data writer function
		ctx->cursor = 0;
		png_set_write_fn (ctx->png_ptr, ctx, pngu_write_data_to_buffer, pngu_flush_data_to_buffer);
	}
	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Default data writer uses function fwrite, so it needs to use our FILE*
		png_init_io (ctx->png_ptr, ctx->fd);
	}

	// Setup output file properties
	png_set_IHDR (ctx->png_ptr, ctx->info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, 
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// 5 bits per color
	sig_bit.red=5;
	sig_bit.green=5;
	sig_bit.blue=5;
	png_set_sBIT(ctx->png_ptr, ctx->info_ptr, &sig_bit);
	png_set_shift(ctx->png_ptr, &sig_bit);

	png_write_info(ctx->png_ptr, ctx->info_ptr);

	png_set_packing(ctx->png_ptr);

	*(ctx->row_pointers) = new png_byte [png_get_rowbytes(ctx->png_ptr, ctx->info_ptr)];	// alloc mem

	for(int y=0; y<height; y++, screen+=GFX.RealPPL)
	{
		png_byte *rowpix = *(ctx->row_pointers);
		for(int x=0; x<width; x++)
		{
			uint32 r, g, b;
			DECOMPOSE_PIXEL(screen[x], r, g, b);
			*(rowpix++) = r;
			*(rowpix++) = g;
			*(rowpix++) = b;
		}
		png_write_row(ctx->png_ptr, *(ctx->row_pointers));
	}

    delete [] ctx->row_pointers;

	// Tell libpng we have no more data to write
	png_write_end (ctx->png_ptr, (png_infop) NULL);

	// Free resources
	free (ctx->img_data);
	free (ctx->row_pointers);
	png_destroy_write_struct (&(ctx->png_ptr), &(ctx->info_ptr));
	if (ctx->source == PNGU_SOURCE_DEVICE)
		fclose (ctx->fd);
	
	
	//fprintf(stderr, "%s saved.\n", fname);

	{
		static char str [64];
		sprintf(str, "Saved screenshot");
		S9xSetInfoString(str);
	}
	

	return TRUE;
#else
	Settings.TakeScreenshot=FALSE;
	fprintf(stderr, "Screenshot support not available (libpng was not found at build time)\n");
	return FALSE;
#endif
}

