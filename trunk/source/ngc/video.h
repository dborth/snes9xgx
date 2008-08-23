/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Video
 *
 * This is a modified renderer from the Genesis Plus Project.
 * Well - you didn't expect me to write another one did ya ? -;)
 *
 * softdev July 2006
 ****************************************************************************/
#ifndef _GCVIDEOH_

#define _GCVIDEOH_
//#include <gccore.h>
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

#endif
