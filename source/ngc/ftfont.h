/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Screen Font Driver
 *
 * Uses libfreetype 2.2.1 compiled for GC with TTF support only.
 * TTF only reduces the library by some 900kbytes!
 *
 * Visit - http://www.freetype.org !
 *
 * **WARNING***
 *
 * ONLY USE GUARANTEED PATENT FREE FONTS.
 * THOSE IN YOUR WINDOWS\FONTS DIRECTORY ARE COPYRIGHT
 * AND MAY NOT BE DISTRIBUTED!
 *
 * softdev July 2006
 * crunchy2 June 2007
 ****************************************************************************/
#ifndef _TTFFONTS_
#define _TTFFONTS_

int FT_Init ();
void setfontsize (int pixelsize);
void setfontcolour (u8 r, u8 g, u8 b);
void DrawText (int x, int y, char *text);
void unpackbackdrop ();
void Credits ();
void RomInfo ();
void WaitButtonA ();
int RunMenu (char items[][50], int maxitems, char *title, int fontsize = 20, int x = -1);
void DrawMenu (char items[][50], char *title, int maxitems, int selected, int fontsize = 20, int x = -1);
void WaitPrompt (char *msg);
int WaitPromptChoice (char *msg, char* bmsg, char* amsg);
void ShowAction (char *msg);
void ShowProgress (char *msg, int done, int total);
void DrawPolygon (int vertices, int *varray, u8 r, u8 g, u8 b);
void DrawLineFast( int startx, int endx, int y, u8 r, u8 g, u8 b );

#endif
