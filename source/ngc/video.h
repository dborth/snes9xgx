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
#include "snes9x.h"

void InitGCVideo ();
void clearscreen ();
void showscreen ();
void setGFX ();
void update_video (int width, int height);
void zoom (float speed);

#endif
