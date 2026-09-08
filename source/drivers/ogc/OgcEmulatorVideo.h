/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * OgcEmulatorVideo.h
 ***************************************************************************/
#pragma once

#include <gccore.h>
#include <stdint.h>
#include "../EmulatorVideoDriver.h"

class OgcVideoDriver;

class OgcEmulatorVideo : public EmulatorVideoDriver
{
	public:
		OgcEmulatorVideo() : videoDriver(nullptr) {}

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(int width, int height) override;
		void readFrameRGB24(uint8_t* dst) override;
		void forceVideoUpdate() override;

	private:
		void configureOriginalModeTables(GXRModeObj* baseMode);
		void initScanlineTexture();
		void setupScanlineFilterTEV();
		bool shouldApplyScanlines();
		void drawInit();
		void drawSquare();
		void resetFbWidth(int width, GXRModeObj *rmode);
		void untileRGB5A3ToRGB24(const void * tiledTexture, int width, int height, uint8_t* dst);
		OgcVideoDriver* videoDriver;
};
