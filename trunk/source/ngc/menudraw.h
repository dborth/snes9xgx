/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 June 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * menudraw.h
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
#ifndef _NGCMENUDRAW_
#define _NGCMENUDRAW_

#include "filesel.h"

#define PAGESIZE 17 // max item listing on a screen

int FT_Init ();
void setfontsize (int pixelsize);
void setfontcolour (u8 r, u8 g, u8 b);
void DrawText (int x, int y, const char *text);
void unpackbackdrop ();
void Credits ();
void RomInfo ();
void WaitButtonA ();
int RunMenu (char items[][50], int maxitems, const char *title, int fontsize = 20, int x = -1);
void DrawMenu (char items[][50], const char *title, int maxitems, int selected, int fontsize = 20, int x = -1);
void ShowCheats (char items[][50], char itemvalues[][50], int maxitems, int offset, int selection);
void ShowFiles (FILEENTRIES filelist[], int maxfiles, int offset, int selection);

void WaitPrompt (const char *msg);
int WaitPromptChoice (const char *msg, const char* bmsg, const char* amsg);
void ShowAction (const char *msg);
void ShowProgress (const char *msg, int done, int total);
void DrawPolygon (int vertices, int *varray, u8 r, u8 g, u8 b);
void DrawLineFast( int startx, int endx, int y, u8 r, u8 g, u8 b );

extern int menu;

#endif
