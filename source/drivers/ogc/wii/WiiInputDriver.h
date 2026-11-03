/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiInputDriver.h
 *
 * The Wii's input: everything on a GameCube (OgcInputDriver) plus the
 * Wiimote and its extensions, the Wii U GamePad (as a Wii U's vWii/Wii VC
 * sees it), the USB controllers, and the console's power/reset buttons.
 ***************************************************************************/
#pragma once

#include "../OgcInputDriver.h"
#include "input/WiiUsbPads.h"

class WiiInputDriver : public OgcInputDriver
{
	public:
		~WiiInputDriver() override;

		InputUsbStatus getUsbStatus() override { return usbPads.getStatus(); }
		const char * getUsbControllerInfo() override { return usbPads.getStatusText(); }

		//! Running as a Wii U Virtual Console title: the Wii U GamePad is the controller.
		//! Valid once init() has run.
		bool isWiiVC() const { return wiiVC; }

	protected:
		void initHardware() override;
		void scanHardware() override;
		void readChannel(int channel, InputPadData & pad) override;
		bool isSystemRumbleAllowed() const override;
		void driveRumble(int channel, bool on) override;

	private:
		void readWiimote(int channel, InputPadData & pad);
		void readGamePad(InputPadData & pad);

		WiiUsbPads usbPads;
		bool wiiVC = false;
};
