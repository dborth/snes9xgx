/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Michniewski 2008
 *
 * filter.h
 *
 * Filters Header File
 ****************************************************************************/
#ifndef _FILTER_H_
#define _FILTER_H_

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "snes9x/snes9x.h"
 
enum RenderFilter{
	FILTER_NONE = 0,

	FILTER_HQ2X,
	FILTER_HQ2XS,
	FILTER_HQ2XBOLD,
    FILTER_SCALE2X,
	FILTER_TVMODE,

	NUM_FILTERS
};

typedef void (*TFilterMethod)(uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);

extern TFilterMethod FilterMethod;

extern unsigned char * filtermem;

void SelectFilterMethod ();
const char* GetFilterName (RenderFilter filterID);
int GetFilterScale(RenderFilter filterID);
void InitLUTs();

#endif

