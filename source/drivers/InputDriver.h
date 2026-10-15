/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * InputDriver.h
 *
 * Platform input backend GuiElements delegate to.
 * Exactly one driver implements this and assigns the single global instance.
 ***************************************************************************/
#pragma once

#include "InputData.h"

enum {
	WIIMOTE_ORIENTATION_VERTICAL = 0,
	WIIMOTE_ORIENTATION_HORIZONTAL,
	WIIMOTE_ORIENTATION_LENGTH
};

class InputDriver
{
	public:
		virtual ~InputDriver() = default;
		
		virtual void init() = 0;
		virtual void shutdown() = 0;
		
		//! Polls the hardware and dispatches InputPadData payloads
		virtual void update() = 0;
		
		//! Requests a rumble event on the specified controller channel
		virtual void setRumble(int channel, bool rumble) = 0;
		
		//! The USB controller to show on the controller status strip (none on platforms without USB controllers)
		virtual InputUsbStatus getUsbStatus() { return InputUsbStatus(); }

		void setRumbleEnabled(bool enabled) { rumbleEnabled = enabled; }
		bool isRumbleEnabled() const { return rumbleEnabled; }
		void setWiimoteOrientation(int orientation) { wiimoteOrientation = orientation; }
		int getWiimoteOrientation() const { return wiimoteOrientation; }
	
	protected:
		bool rumbleEnabled = true;
		int wiimoteOrientation = WIIMOTE_ORIENTATION_VERTICAL;
};
