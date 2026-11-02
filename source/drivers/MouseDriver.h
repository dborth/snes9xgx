/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * MouseDriver.h
 *
 * Physical (USB) mouse input. The Wiimote IR pointer is NOT here: it is
 * part of the pad state (InputPadData::cursor_x/y).
 ***************************************************************************/
#pragma once

#include <stdint.h>

enum MouseButtons : uint8_t {
	MOUSE_BTN_LEFT   = (1 << 0),
	MOUSE_BTN_RIGHT  = (1 << 1),
	MOUSE_BTN_MIDDLE = (1 << 2)
};

struct MouseEvent
{
	int     dx, dy;    //!< relative motion since the previous event
	uint8_t buttons;   //!< MouseButtons bitmask currently held
};

//! Physical (USB) mouse. The base class is the driver for a platform that has
//! none (eg. GameCube): it is never connected and never has an event, so the
//! app can poll it without asking whether there is a mouse.
class MouseDriver
{
	public:
		virtual ~MouseDriver() = default;

		virtual void init() {}
		virtual void shutdown() {}

		//! Pops the next pending event into `out`. Returns false when the
		//! queue is empty. Call from the main thread, once per frame.
		virtual bool poll(MouseEvent & out) { (void)out; return false; }

		//! True while at least one mouse is attached (follows hot-plug)
		virtual bool isConnected() const { return false; }
};
