/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * video.cpp
 ***************************************************************************/

#include <unistd.h>
#include <malloc.h>

#include "memmanager.h"
#include "video.h"
#include "fileop.h"
#include "utils/pngcodec.h"

#include "drivers/Platform.h"
#include "drivers/ogc/videofilters.h"

#include "snes9x/memmap.h"

#define SNES9XGFX_SIZE 		(EXT_PITCH*EXT_HEIGHT)

static unsigned char * snes9xgfx = nullptr;
GameScreenPng gameScreenPng;

/****************************************************************************
 * AllocGfxMem
 ***************************************************************************/
void AllocGfxMem()
{
	snes9xgfx = (unsigned char *)memalign(32, SNES9XGFX_SIZE);
	memset(snes9xgfx, 0, SNES9XGFX_SIZE);

	GFX.Pitch = EXT_PITCH;
	GFX.Screen = (uint16_t*)(snes9xgfx + EXT_OFFSET);
}

/****************************************************************************
 * setGFX
 *
 * Setup the global GFX information for Snes9x
 ***************************************************************************/
void setGFX()
{
	GFX.Pitch = EXT_PITCH;
}

void ClearScreenshot()
{
	if(gameScreenPng.buffer) {
		extmem_free(gameScreenPng.buffer);
		gameScreenPng.buffer = nullptr;
	}

	gameScreenPng.size = 0;
}

/****************************************************************************
 * TakeScreenshot
 *
 * Copies the current emulator frame into a PNG buffer
 ***************************************************************************/
void TakeScreenshot()
{
	AllocSaveBuffer();
	platform->getVideo()->getEmulatorVideo()->readFrameRGB24(savebuffer);
	uint32_t size = 0;
	gameScreenPng.buffer = EncodePNGFromRGB24(gameScreenPng.width, gameScreenPng.height, savebuffer, 0, &size);
	gameScreenPng.size = (int) size;
	FreeSaveBuffer();
}
