/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerorder.h
 *
 * Which controller counts first when several are connected. The controller
 * strip (ControllerStatus) uses it to choose the icon for a channel, and the
 * mapping screen uses it to choose the hardware to start on, so the two always
 * agree on what "the controller in use" is.
 *
 * For a channel the order is: a USB controller (DualShock 4, Xbox 360, other),
 * the GamePad, then Pro Controller, Classic Controller, Nunchuk, Wiimote and
 * GameCube pad.
 ***************************************************************************/
#pragma once

#include <stddef.h>
#include "drivers/InputData.h"

namespace ControllerOrder
{

//!The first of `list` that this channel has connected, or INPUT_HW_MAX
inline int FirstConnected(const InputPadData& pad, const int * list, size_t count)
{
	for(size_t i = 0; i < count; i++)
		if(pad.hw_connected[list[i]])
			return list[i];
	return INPUT_HW_MAX;
}

//!The USB controller on this channel, in the order DualShock 4, Xbox 360, other; INPUT_HW_MAX if none
inline int UsbHardware(const InputPadData& pad)
{
	static const int order[] = { INPUT_HW_DS4, INPUT_HW_XBOX360, INPUT_HW_GENERIC_USB };
	return FirstConnected(pad, order, sizeof(order) / sizeof(order[0]));
}

//!The Wii or GameCube controller on this channel, in the order Pro Controller, Classic Controller, Nunchuk,
//!Wiimote, GameCube pad (the Classic, Nunchuk and Pro ride on a Wiimote, so they come before it); INPUT_HW_MAX if none
inline int ConsoleHardware(const InputPadData& pad)
{
	static const int order[] = { INPUT_HW_WUPC, INPUT_HW_CLASSIC, INPUT_HW_NUNCHUK, INPUT_HW_WIIMOTE, INPUT_HW_GAMECUBE };
	return FirstConnected(pad, order, sizeof(order) / sizeof(order[0]));
}

//!The controller in use on this channel: a USB controller if one is connected, else the GamePad if it is, else the
//!first console controller. INPUT_HW_MAX if there is none.
inline int InUse(const InputPadData& pad)
{
	const int usb = UsbHardware(pad);
	if(usb != INPUT_HW_MAX)
		return usb;
	if(pad.hw_connected[INPUT_HW_DRC])
		return INPUT_HW_DRC;
	return ConsoleHardware(pad);
}

}
