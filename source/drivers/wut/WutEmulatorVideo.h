/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * WutEmulatorVideo.h
 *
 * EmulatorVideoDriver implementation for Wii U: uploads the raw SNES
 * framebuffer into a linear GX2 texture and draws it with the shared
 * Texture2DShader.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "WutVideoDriver.h"
#include "../EmulatorVideoDriver.h"

class WutVideoDriver;

class WutEmulatorVideo : public EmulatorVideoDriver
{
	public:
		WutEmulatorVideo();
		~WutEmulatorVideo() override;

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(int width, int height) override;
		void readFrameRGB24(uint8_t* dst) override;
		void forceVideoUpdate() override;
		bool mapPointerToFrame(float canvasX, float canvasY, bool onGamePad, int* frameX, int* frameY) override;

	private:
		void rebuildTexture(int width, int height);
		void destroyTexture();
		void uploadFrame();
		void drawQuad();

		WutVideoDriver* videoDriver;

		GX2Texture* texture;
		GX2Sampler sampler;

		int vwidth, vheight;
		int oldvwidth, oldvheight;
		int checkVideo;
		uint32_t prevRenderedFrameCount;

		// Where the game quad is drawn: top-left x/y and size w/h in the pixels of each render target
		// Scaling/filtering uses it too (source-to-output scale = size / vwidth, vheight).
		struct TargetPlacement { float x, y, w, h; };
		TargetPlacement placement[OUTPUT_TARGET_COUNT];

		// The TV placement in UI-canvas pixels (top-left x/y, size w/h), derived
		// by resetVideo(). Not used for drawing - only the menu's game screenshot
		// background (gameScreenPng) needs it.
		float quadX, quadY, quadWidth, quadHeight;
};
		TargetPlacement placement[OUTPUT_TARGET_COUNT];
};
