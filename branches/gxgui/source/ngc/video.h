/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
  * Michniewski 2008
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
void StopGX();
void ResetVideo_Emu ();
void ResetVideo_Menu ();
void setGFX ();
void update_video (int width, int height);
void zoom (float speed);
void zoom_reset ();

extern int screenheight;
extern int screenwidth;
extern bool progressive;

#endif
