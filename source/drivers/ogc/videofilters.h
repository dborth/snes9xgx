/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * videofilters.h
 * HQ2x, Scale2X, 2xBR, DDT filters
 * Original code from Michniewski, adapted from Snes9x Win32/MacOSX ports
 ****************************************************************************/
#ifndef _VIDEOFILTERS_H_
#define _VIDEOFILTERS_H_

#include <stdint.h>

enum UpscaleFilter {
	UPSCALE_NONE = 0,
	UPSCALE_2XBR,
	UPSCALE_DDT,
	UPSCALE_HQ2X,
	UPSCALE_HQ2XS,
	UPSCALE_HQ2XBOLD,
	UPSCALE_SCALE2X,
	UPSCALE_2XBRLV1,
	NUM_UPSCALE_FILTERS
};

typedef void (*TFilterMethod)(uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);

void SelectFilterMethod (int filterID);
const char* GetUpscaleFilterName (int filterID);
int GetFilterScale();

extern TFilterMethod FilterMethod;

#endif
