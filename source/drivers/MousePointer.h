/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * MousePointer.h
 *
 * Turns the relative events of a MouseDriver into what a pad driver reports
 * for a pointer: a cursor position on the screen and button edges. The
 * Wiimote IR and touch pointers come in the pad state already; this is the
 * same thing for a USB mouse.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "MouseDriver.h"
#include "InputData.h"

class MousePointer
{
	public:
		struct State
		{
			bool connected = false; //!< a mouse is attached
			bool active = false;    //!< moved or clicked recently: it owns the pointer
			float x = 0, y = 0;     //!< position on the screen, in pixels
			uint8_t down = 0;       //!< MouseButtons pressed this frame
			uint8_t held = 0;       //!< MouseButtons held this frame (a press is held for at least one frame)
			uint8_t up = 0;         //!< MouseButtons released this frame
		};

		//! Drains the mouse's pending events, once per frame. `dt` is the frame
		//! time in seconds, `width`/`height` the screen the pointer stays on.
		void update(MouseDriver* mouse, float dt, int width, int height, State & out);

		//! The same, for a pad driver: reports the mouse into `pad` (channel 0): its own hardware
		//! profile for the buttons, and the pointer (like the Wiimote IR) while it is being used.
		//! Nothing is reported when no mouse is connected.
		void apply(MouseDriver* mouse, float dt, int width, int height, InputPadData & pad);

	private:
		//! Counts of motion per pixel of pointer movement
		static constexpr float kSpeed = 1.0f;
		//! A mouse that has not moved or clicked for this long gives the pointer
		//! back (eg. to the Wiimote IR)
		static constexpr float kIdleSeconds = 3.0f;

		bool started = false;
		float x = 0, y = 0;
		uint8_t held = 0;
		float idle = kIdleSeconds;
};
