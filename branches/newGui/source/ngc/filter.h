/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
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

#include "snes9x.h"
 
enum RenderFilter{
	FILTER_NONE = 0,

	FILTER_HQ2X,
	FILTER_HQ2XS,
	FILTER_HQ2XBOLD,

	NUM_FILTERS
};

typedef void (*TFilterMethod)(uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);

extern TFilterMethod FilterMethod;
extern TFilterMethod FilterMethodHiRes;

extern void * filtermem;

//
// Prototypes
//
void SelectFilterMethod ();
void RenderPlain (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
void SelectFilterMethod ();
TFilterMethod FilterToMethod (RenderFilter filterID);
const char* GetFilterName (RenderFilter filterID);
bool GetFilterHiResSupport (RenderFilter filterID);
int GetFilterScale(RenderFilter filterID);
template<int GuiScale> void RenderHQ2X (uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
void InitLUTs();

#endif

