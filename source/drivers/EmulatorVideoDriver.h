/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorVideoDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "VideoDriver.h"

class EmulatorVideoDriver
{
	public:
		virtual ~EmulatorVideoDriver() = default;

		virtual void init(VideoDriver* videoDriver) = 0;
		virtual void resetVideo() = 0;
		virtual void presentFrame(int width, int height) = 0;
		virtual void readFrameRGB24(uint8_t* dst) = 0;
};
