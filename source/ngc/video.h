/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 *
 * video.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _GCVIDEOH_

#define _GCVIDEOH_

#include <ogcsys.h>

#include "snes9x.h"

void InitGCVideo ();
void ResetVideo_Emu ();
void ResetVideo_Menu ();
void setGFX ();
void update_video (int width, int height);
void clearscreen (int colour = COLOR_BLACK);
void showscreen ();
void zoom (float speed);
void zoom_reset ();

extern bool progressive;

#ifdef _DEBUG_VIDEO
 // log stuff
extern FILE * debughandle;
#endif

#endif
