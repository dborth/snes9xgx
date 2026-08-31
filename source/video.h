/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * video.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _GCVIDEOH_
#define _GCVIDEOH_

#include <ogcsys.h>
#include "libgui/Gui.h"

#include "snes9x/snes9x.h"

void AllocGfxMem();
void InitVideo ();
void ResetVideo_Emu();
void setGFX();
void update_video (int width, int height);
void ResetVideo_Menu();
void ClearScreenshot();
void TakeScreenshot();
void Menu_Render();

typedef struct
{
	u8 * buffer;
	int size;
	int width;
	int height;
	float scaleX;
	float scaleY;
	int xoffset;
	int yoffset;
} GameScreenPng;

extern GameScreenPng gameScreenPng;

extern GXRModeObj *vmode;
extern bool progressive;
extern u32 FrameTimer;
extern bool vmode_60hz;
extern uint32 prevRenderedFrameCount;
extern int CheckVideo;

#endif
