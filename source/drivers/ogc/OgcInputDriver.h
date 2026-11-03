/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcInputDriver.h
 *
 * Input for every libogc console. This is the GameCube driver, and the base
 * of the Wii one: the Wii has everything a GameCube has (the controller
 * ports and rumble), so WiiInputDriver derives from this class and adds its
 * own hardware through the protected hooks below, rather than this file
 * switching on HW_RVL.
 ***************************************************************************/
#pragma once
#include "../InputDriver.h"
#include "../MousePointer.h"

class OgcInputDriver : public InputDriver
{
	public:
		OgcInputDriver();
		~OgcInputDriver() override;

		void init() override;
		void shutdown() override;
		void update() override;
		void setRumble(int channel, bool rumble) override;

	protected:
		void stopAllRumble() override;

		//! Extension points for a console with more input hardware than GameCube pads.
		//! The defaults do nothing, which is exactly a GameCube.

		//! Called from init() after the GameCube pads are up
		virtual void initHardware() {}

		//! Called once per frame before the channels are read: poll the extra hardware
		virtual void scanHardware() {}

		//! Called once per channel per frame: add the extra hardware's state for `channel`
		//! to `pad` (each device in the slot of its own hardware profile)
		virtual void readChannel(int channel, InputPadData & pad) { (void)channel; (void)pad; }

		//! False if the console's own settings disable controller rumble
		virtual bool isSystemRumbleAllowed() const { return true; }

		//! Drives the rumble motor of any controller other than the GameCube pad on
		//! `channel`; called every frame (and with false on shutdown)
		virtual void driveRumble(int channel, bool on) { (void)channel; (void)on; }

	private:
		bool rumbleRequest[4];
    int menuRumbleFrames[4];     // frames left in the current menu "tick" (0 = idle)
    int menuRumbleGapFrames[4];  // frames left in the enforced silent gap after a tick
    MousePointer mousePointer;   // the USB mouse as a pointer on channel 0 (see Platform::getMouse())
};
