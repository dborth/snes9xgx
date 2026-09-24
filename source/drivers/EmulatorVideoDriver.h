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

		// Force the next presentFrame() to rebuild scaling/texture state
		// (eg. after a ROM load, before any frame has been rendered yet)
		virtual void forceVideoUpdate() = 0;

		// Maps a UI-canvas pointer position (IR pointer / touch, in the same canvas coordinates
		// as InputPadData::cursor_x/y) to a position in the SNES coordinate space
		// Returns false until the placement is known (before the first resetVideo/presentFrame).
		virtual bool mapPointerToFrame(float canvasX, float canvasY, int* frameX, int* frameY)
		{
			(void)canvasX; (void)canvasY; (void)frameX; (void)frameY;
			return false;
		}
};
