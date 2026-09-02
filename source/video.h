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

#include <unistd.h>

// Offset into buffer to allow a two pixel border around the whole rendered
// SNES image. This is a speed up hack to allow some of the image processing
// routines to access black pixel data outside the normal bounds of the buffer.
#define EXT_WIDTH (MAX_SNES_WIDTH + 4)
#define EXT_PITCH (EXT_WIDTH * 2)
#define EXT_HEIGHT (MAX_SNES_HEIGHT + 4)
#define EXT_OFFSET (EXT_PITCH * 2 + 2 * 2)

void AllocGfxMem();
void setGFX();
void ClearScreenshot();
void TakeScreenshot();

typedef struct
{
	uint8_t * buffer;
	int size;
	int width;
	int height;
	float scaleX;
	float scaleY;
	int xoffset;
	int yoffset;
} GameScreenPng;

extern GameScreenPng gameScreenPng;

#endif
