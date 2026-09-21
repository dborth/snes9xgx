/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2008-2026
 *
 * WutUpscaleFilters.h
 ****************************************************************************/
#pragma once

#include <stdint.h>

enum UpscaleFilter {
	UPSCALE_NONE = 0,
	UPSCALE_SCALEFX,
	NUM_UPSCALE_FILTERS
};

const char* GetUpscaleFilterName (int filterID);
