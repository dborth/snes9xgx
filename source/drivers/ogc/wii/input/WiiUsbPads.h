/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiUsbPads.h
 *
 * The third-party USB controllers (Retrode, Hornet, Mayflash, Xbox 360 and
 * DualShock 4, and the pen tablet) as one unit for WiiInputDriver. The device drivers themselves
 * (retrode.c, xbox360.c, ... and the shared discovery in usbinput.c) stay
 * plain C; this is the only place that knows their APIs, and where each
 * device's native buttons are mapped to the generic input masks.
 *
 * To add a controller: write its C driver (register it in usbinput.c), then
 * add its scan, read and status handling in WiiUsbPads.cpp.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../../../InputData.h"

class WiiUsbPads
{
	public:
		//! Discovers/attaches devices and latches their state. Once per frame, before read().
		void scan();

		//! Adds the state of every USB controller assigned to `channel` to `pad`,
		//! each in the slot of its own hardware profile.
		void read(int channel, InputPadData & pad);

		//! Adds the pen tablet to `pad` on channel 0, in the USB mouse's profile, and
		//! its pen as the pointer on a `width` x `height` screen while in range. Called after the
		//! Wiimote, so the pen takes the pointer over from the IR.
		void readPointer(int channel, int width, int height, InputPadData & pad);

		//! The controller to show on the status strip (see InputUsbStatus)
		InputUsbStatus getStatus() const;

		//! One line with the state of every driver, for the credits screen
		const char * getStatusText();

	private:
		void readGeneric(int channel, InputPadData & pad);

		// Which of the SNES-style adapters is connected, latched by scan()
		bool retrodeActive = false;
		bool hornetActive = false;
		bool mayflashActive = false;

		// The adapters report held state only: the previous frame's, for the down/up edges
		uint32_t prevGenericHeld[4] = { 0, 0, 0, 0 };

		char statusText[160] = { 0 };
};
