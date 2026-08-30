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
void StopGX();
void ResetVideo_Emu();
void setGFX();
void update_video (int width, int height);
void ResetVideo_Menu();
void ClearScreenshot();
void TakeScreenshot();
void Menu_Render();
void Menu_DrawImg(u8 data[], f32 xpos, f32 ypos, u16 width, u16 height, f32 degrees, f32 scaleX, f32 scaleY, u8 alphaF );
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, PixelColor color);

void* createTexture(int width, int height);
void loadTextureData(void* texture, const uint8_t* rgba, int width, int height);
void destroyTexture(void * texture);

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
extern int screenheight;
extern int screenwidth;
extern bool progressive;
extern u32 FrameTimer;
extern bool vmode_60hz;
extern uint32 prevRenderedFrameCount;
extern int CheckVideo;

#endif
