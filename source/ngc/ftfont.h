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
void DrawText (int x, int y, char *text);
void legal ();
void highlight (int on);
void WaitButtonA ();
void setfontcolour (u8 r, u8 g, u8 b);
int RunMenu (char items[][20], int maxitems, char *title);
void WaitPrompt (char *msg);
int WaitPromptChoice (char *msg, char* bmsg, char* amsg);
void ShowAction (char *msg);
void ShowProgress (char *msg, int done, int total);
void DrawPolygon (int vertices, int *varray, u8 r, u8 g, u8 b);
void DrawLineFast( int startx, int endx, int y, u8 r, u8 g, u8 b );

#endif
