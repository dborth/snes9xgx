/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2026
 *
 * filter.h
 * HQ2x, Scale2X, 2xBR, DDT filters
 * Original code from Michniewski, adapted from Snes9x Win32/MacOSX ports
 ****************************************************************************/
#ifndef _FILTER_H_
#define _FILTER_H_

#include <stdint.h>

enum RenderFilter {
	FILTER_NONE = 0,
	FILTER_HQ2X,
	FILTER_HQ2XS,
	FILTER_HQ2XBOLD,
	FILTER_SCALE2X,
	FILTER_SCANLINES,
	FILTER_2XBR,
	FILTER_2XBRLV1,
	FILTER_DDT,
	NUM_FILTERS
};

typedef void (*TFilterMethod)(uint8_t *srcPtr, uint32_t srcPitch, uint8_t *dstPtr, uint32_t dstPitch, int width, int height);

void SelectFilterMethod (int filterID);
const char* GetFilterName (int filterID);
int GetFilterScale();

extern TFilterMethod FilterMethod;

#endif
