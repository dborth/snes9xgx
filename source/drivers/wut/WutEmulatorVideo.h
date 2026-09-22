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

		// On-screen placement of the game quad, in design-canvas pixels
		// (top-left x/y, size w/h) - recomputed by resetVideo(). This is the
		// source of truth for the zoom/shift/aspect settings, and the metrics
		// the menu's game screenshot background (gameScreenPng) is drawn with.
		float quadX, quadY, quadWidth, quadHeight;

		// The same quad in physical pixels of each render target (top-left
		// x/y, size w/h), derived from the canvas placement above by the
		// canvas-to-target stretch. This is what actually gets drawn, and what
		// scaling/filtering needs (source-to-output scale = size / vwidth,vheight).
		struct TargetPlacement { float x, y, w, h; };
		TargetPlacement placement[OUTPUT_TARGET_COUNT];
};
