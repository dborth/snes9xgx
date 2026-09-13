/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutInputDriver.h
 ***************************************************************************/
#pragma once

#include "../InputDriver.h"
#include "../OneEuroFilter.h"

//!Wii U InputDriver: VPAD for the GamePad (stick, buttons, and touch,
//!channel 0 only) plus KPAD/WPAD for up to 4 Wiimotes/Nunchuks/Classic/
//!Pro Controllers. GamePad touch is mapped onto the unified cursor/button
//!fields; IR pointer position is smoothed to counter KPADReadEx sampling
//!faster/noisier than the UI update rate.
class WutInputDriver : public InputDriver {
	public:
		WutInputDriver();
		~WutInputDriver() override;

		void init() override;
		void shutdown() override;
		void update() override;
		void setRumble(int channel, bool rumble) override;

		void openHomeButtonOverlay();

	private:
		int rumbleCount[4];
		bool rumbleRequest[4];

		bool drcTouchedPrev;
		float drcLastTouchX;
		float drcLastTouchY;

		// IR pointer smoothing (per Wiimote channel). One Euro Filter adapts
		// its own smoothing strength to pointer speed each frame, so unlike
		// a fixed-alpha EMA there's no single constant to tune - see
		// OneEuroFilter.h for what minCutoff/beta below actually control.
		OneEuroFilter irFilterX[4];
		OneEuroFilter irFilterY[4];
		bool irSmoothInit[4];
};
