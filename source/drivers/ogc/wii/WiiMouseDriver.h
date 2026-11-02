/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiMouseDriver.h
 *
 * USB mouse via libogc's usbmouse
 ***************************************************************************/
#pragma once

#include "../../MouseDriver.h"

class WiiMouseDriver : public MouseDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool poll(MouseEvent & out) override;
		bool isConnected() const override;

	private:
		bool initialized = false;
};
