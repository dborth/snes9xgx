/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2008-2026
 *
 * WutUpscaleFilters.cpp
 ****************************************************************************/

#include "WutUpscaleFilters.h"

const char* GetUpscaleFilterName (int filterID)
{
	switch(filterID)
	{
		case UPSCALE_NONE: return "None";
		case UPSCALE_SCALEFX: return "ScaleFX";
		default: return "Unknown";
	}
}
