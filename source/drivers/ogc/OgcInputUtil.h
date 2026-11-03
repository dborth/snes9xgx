/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcInputUtil.h
 *
 * Small helpers shared by the GameCube/Wii input code (OgcInputDriver, the
 * Wii extension drivers and the USB controller adapters).
 ***************************************************************************/
#pragma once

static inline float OgcClampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

//! A signed 8-bit stick axis (-128..127, as libogc and the USB drivers report them) as -1..1
static inline float OgcStickToFloat(int v) {
	return OgcClampf((float)v / 128.0f, -1.0f, 1.0f);
}
