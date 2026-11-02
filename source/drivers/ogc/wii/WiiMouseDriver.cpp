/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiMouseDriver.cpp
 ***************************************************************************/
#include <gctypes.h>
#include <ogc/usbmouse.h>

#include "WiiMouseDriver.h"

void WiiMouseDriver::init()
{
	if (!initialized)
		initialized = (MOUSE_Init() >= 0);
}

void WiiMouseDriver::shutdown()
{
	if (initialized)
		MOUSE_Deinit();

	initialized = false;
}

bool WiiMouseDriver::isConnected() const
{
	return initialized && MOUSE_IsConnected();
}

bool WiiMouseDriver::poll(MouseEvent & out)
{
	mouse_event me;

	if (initialized && MOUSE_GetEvent(&me)) {
		out.dx = me.rx;
		out.dy = me.ry;
		out.buttons = 0;
		if (me.button & 0x1) out.buttons |= MOUSE_BTN_LEFT;
		if (me.button & 0x2) out.buttons |= MOUSE_BTN_RIGHT;
		if (me.button & 0x4) out.buttons |= MOUSE_BTN_MIDDLE;
		return true;
	}

	return false;
}
