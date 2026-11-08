/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiUsbPads.cpp
 ***************************************************************************/

#include <stdio.h>
#include <gccore.h>

#include "WiiUsbPads.h"
#include "usbinput.h"
#include "retrode.h"
#include "xbox360.h"
#include "hornet.h"
#include "mayflash.h"
#include "ds4.h"
#include "tablet.h"
#include "../../OgcInputUtil.h"

/****************************************************************************
 * Hardware Mapping Helpers
 * Each device's native buttons to our generic UI masks
 ***************************************************************************/
static uint32_t MapDS4ToGeneric(uint32_t ds4_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (ds4_btns & DS4_BUTTON_CIRCLE) mask |= INPUT_BTN_A;
	if (ds4_btns & DS4_BUTTON_CROSS) mask |= INPUT_BTN_B;
	if (ds4_btns & DS4_BUTTON_TRIANGLE) mask |= INPUT_BTN_X;
	if (ds4_btns & DS4_BUTTON_SQUARE) mask |= INPUT_BTN_Y;
	if (ds4_btns & DS4_BUTTON_UP) mask |= INPUT_BTN_UP;
	if (ds4_btns & DS4_BUTTON_DOWN) mask |= INPUT_BTN_DOWN;
	if (ds4_btns & DS4_BUTTON_LEFT) mask |= INPUT_BTN_LEFT;
	if (ds4_btns & DS4_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (ds4_btns & DS4_BUTTON_OPTIONS) mask |= INPUT_BTN_PLUS;
	if (ds4_btns & DS4_BUTTON_SHARE) mask |= INPUT_BTN_MINUS;
	if (ds4_btns & DS4_BUTTON_PS) mask |= INPUT_BTN_HOME;
	if (ds4_btns & DS4_BUTTON_L1) mask |= INPUT_TRIGGER_L;
	if (ds4_btns & DS4_BUTTON_R1) mask |= INPUT_TRIGGER_R;
	if (ds4_btns & DS4_BUTTON_L2) mask |= INPUT_TRIGGER_ZL;
	if (ds4_btns & DS4_BUTTON_R2) mask |= INPUT_TRIGGER_ZR;
	if (ds4_btns & DS4_BUTTON_L3) mask |= INPUT_THUMB_L;
	if (ds4_btns & DS4_BUTTON_R3) mask |= INPUT_THUMB_R;
	return mask;
}

static uint32_t MapXbox360ToGeneric(uint32_t xbox_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	// Face buttons keep their SNES position: Xbox B (right) = A, Xbox A (bottom) = B, etc.
	if (xbox_btns & XBOX360_BUTTON_B) mask |= INPUT_BTN_A;
	if (xbox_btns & XBOX360_BUTTON_A) mask |= INPUT_BTN_B;
	if (xbox_btns & XBOX360_BUTTON_Y) mask |= INPUT_BTN_X;
	if (xbox_btns & XBOX360_BUTTON_X) mask |= INPUT_BTN_Y;
	if (xbox_btns & XBOX360_BUTTON_UP) mask |= INPUT_BTN_UP;
	if (xbox_btns & XBOX360_BUTTON_DOWN) mask |= INPUT_BTN_DOWN;
	if (xbox_btns & XBOX360_BUTTON_LEFT) mask |= INPUT_BTN_LEFT;
	if (xbox_btns & XBOX360_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (xbox_btns & XBOX360_BUTTON_START) mask |= INPUT_BTN_PLUS;
	if (xbox_btns & XBOX360_BUTTON_BACK) mask |= INPUT_BTN_MINUS;
	if (xbox_btns & XBOX360_BUTTON_LB) mask |= INPUT_TRIGGER_L;
	if (xbox_btns & XBOX360_BUTTON_RB) mask |= INPUT_TRIGGER_R;
	if (xbox_btns & XBOX360_BUTTON_LT) mask |= INPUT_TRIGGER_ZL;
	if (xbox_btns & XBOX360_BUTTON_RT) mask |= INPUT_TRIGGER_ZR;
	if (xbox_btns & XBOX360_BUTTON_L3) mask |= INPUT_THUMB_L;
	if (xbox_btns & XBOX360_BUTTON_R3) mask |= INPUT_THUMB_R;
	return mask;
}

// Retrode, Hornet and Mayflash are SNES-layout pads, and their drivers report them in the PAD_*
// bit layout (A B X Y L R Start, with Select on the Z bit). This is the only place those bits are
// read as anything other than a real GameCube pad: Select goes to MINUS, not to the GC Z button.
static uint32_t MapGenericUsbToGeneric(uint32_t pad_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (pad_btns & PAD_BUTTON_A)      mask |= INPUT_BTN_A;
	if (pad_btns & PAD_BUTTON_B)      mask |= INPUT_BTN_B;
	if (pad_btns & PAD_BUTTON_X)      mask |= INPUT_BTN_X;
	if (pad_btns & PAD_BUTTON_Y)      mask |= INPUT_BTN_Y;
	if (pad_btns & PAD_BUTTON_UP)     mask |= INPUT_BTN_UP;
	if (pad_btns & PAD_BUTTON_DOWN)   mask |= INPUT_BTN_DOWN;
	if (pad_btns & PAD_BUTTON_LEFT)   mask |= INPUT_BTN_LEFT;
	if (pad_btns & PAD_BUTTON_RIGHT)  mask |= INPUT_BTN_RIGHT;
	if (pad_btns & PAD_BUTTON_START)  mask |= INPUT_BTN_PLUS;
	if (pad_btns & PAD_TRIGGER_Z)     mask |= INPUT_BTN_MINUS;
	if (pad_btns & PAD_TRIGGER_L)     mask |= INPUT_TRIGGER_L;
	if (pad_btns & PAD_TRIGGER_R)     mask |= INPUT_TRIGGER_R;
	return mask;
}

static uint32_t MapTabletToGeneric(uint32_t tablet_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (tablet_btns & TABLET_BUTTON_TIP) mask |= INPUT_BTN_A;
	if (tablet_btns & TABLET_BUTTON_BARREL) mask |= INPUT_BTN_B;
	if (tablet_btns & TABLET_BUTTON_BARREL2) mask |= INPUT_BTN_X;
	return mask;
}

static bool IsConnected(const UsbInputDriver & driver) {
	return UsbInput_GetState(&driver) == USBINPUT_STATE_CONNECTED;
}

/****************************************************************************
 * Scan
 ***************************************************************************/
void WiiUsbPads::scan() {
	// Discovery first: it attaches the devices the ScanPads() calls below read from
	UsbInput_Scan();
	Retrode_ScanPads();
	XBOX360_ScanPads();
	Hornet_ScanPads();
	Mayflash_ScanPads();
	DS4_ScanPads();
	Tablet_ScanPads();

	retrodeActive  = IsConnected(Retrode_UsbDriver);
	hornetActive   = IsConnected(Hornet_UsbDriver);
	mayflashActive = IsConnected(Mayflash_UsbDriver);
}

/****************************************************************************
 * Read
 ***************************************************************************/

// Generic USB adapters (Retrode, Hornet, Mayflash). They report held state only, so the
// down/up edges are derived here; menu triggers and button mapping need them.
// Retrode has 4 ports (2 SNES, 2 Genesis), Hornet is one pad, Mayflash has 2 ports.
// The three share one profile: a pad is the union of whatever is held on that channel.
void WiiUsbPads::readGeneric(int channel, InputPadData & pad) {
	bool active = retrodeActive || (hornetActive && channel == 0) || (mayflashActive && channel < 2);

	if (!active) {
		prevGenericHeld[channel] = 0;
		return;
	}

	uint32_t held = Retrode_ButtonsHeld(channel) | Hornet_ButtonsHeld(channel) | Mayflash_ButtonsHeld(channel);
	uint32_t down = held & ~prevGenericHeld[channel];
	uint32_t up = prevGenericHeld[channel] & ~held;
	prevGenericHeld[channel] = held;

	pad.hw_connected[INPUT_HW_GENERIC_USB] = true;
	pad.hw_buttons_d[INPUT_HW_GENERIC_USB] = MapGenericUsbToGeneric(down);
	pad.hw_buttons_h[INPUT_HW_GENERIC_USB] = MapGenericUsbToGeneric(held);
	pad.hw_buttons_r[INPUT_HW_GENERIC_USB] = MapGenericUsbToGeneric(up);
}

void WiiUsbPads::read(int channel, InputPadData & pad) {
	readGeneric(channel, pad);

	// Xbox 360 controller (one at a time, on whichever channel it is currently assigned to)
	if (XBOX360_Connected(channel)) {
		pad.hw_connected[INPUT_HW_XBOX360] = true;
		pad.hw_buttons_d[INPUT_HW_XBOX360] = MapXbox360ToGeneric(XBOX360_ButtonsDown(channel));
		pad.hw_buttons_h[INPUT_HW_XBOX360] = MapXbox360ToGeneric(XBOX360_ButtonsHeld(channel));
		pad.hw_buttons_r[INPUT_HW_XBOX360] = MapXbox360ToGeneric(XBOX360_ButtonsUp(channel));
		pad.hw_stickX[INPUT_HW_XBOX360] = OgcStickToFloat(XBOX360_lStickX(channel));
		pad.hw_stickY[INPUT_HW_XBOX360] = OgcStickToFloat(XBOX360_lStickY(channel));
		pad.hw_substickX[INPUT_HW_XBOX360] = OgcStickToFloat(XBOX360_rStickX(channel));
		pad.hw_substickY[INPUT_HW_XBOX360] = OgcStickToFloat(XBOX360_rStickY(channel));
	}

	// DualShock 4 (its driver's state is for the one controller, whichever channel it is on)
	if (DS4_Connected(channel)) {
		pad.hw_connected[INPUT_HW_DS4] = true;
		pad.hw_buttons_d[INPUT_HW_DS4] = MapDS4ToGeneric(DS4_ButtonsDown());
		pad.hw_buttons_h[INPUT_HW_DS4] = MapDS4ToGeneric(DS4_ButtonsHeld());
		pad.hw_buttons_r[INPUT_HW_DS4] = MapDS4ToGeneric(DS4_ButtonsUp());
		pad.hw_stickX[INPUT_HW_DS4] = OgcStickToFloat(DS4_lStickX());
		pad.hw_stickY[INPUT_HW_DS4] = OgcStickToFloat(DS4_lStickY());
		pad.hw_substickX[INPUT_HW_DS4] = OgcStickToFloat(DS4_rStickX());
		pad.hw_substickY[INPUT_HW_DS4] = OgcStickToFloat(DS4_rStickY());
	}
}

// USB pen tablet, on channel 0, in the USB mouse's hardware profile: the tip is the left button
// (A), the lower and upper side switches the right (B) and middle (X) ones. While the pen is in
// range it owns the pointer, the whole tablet mapped onto the screen.
void WiiUsbPads::readPointer(int channel, int width, int height, InputPadData & pad) {
	if (channel != 0 || !Tablet_Connected())
		return;

	pad.hw_connected[INPUT_HW_USBMOUSE] = true;
	pad.hw_buttons_d[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsDown());
	pad.hw_buttons_h[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsHeld());
	pad.hw_buttons_r[INPUT_HW_USBMOUSE] |= MapTabletToGeneric(Tablet_ButtonsUp());

	if (Tablet_InRange()) {
		pad.validPointer = true;
		pad.isTouch = false;
		pad.cursor_x = Tablet_X() * (width - 1);
		pad.cursor_y = Tablet_Y() * (height - 1);
		pad.cursor_angle = 0.0f;
	}
}

/****************************************************************************
 * Status
 ***************************************************************************/

// The status strip shows one icon for the USB controllers, so the drivers are grouped by the
// icon (hardware profile) they share. In priority order: DS4, Xbox 360, other.
struct StatusGroup {
	GuiHardwareProfile profile;
	const UsbInputDriver * const * drivers;
	int count;
};

static const UsbInputDriver * const ds4Drivers[] = { &DS4_UsbDriver };
static const UsbInputDriver * const xboxDrivers[] = { &XBOX360_UsbDriver };
static const UsbInputDriver * const genericDrivers[] = { &Retrode_UsbDriver, &Hornet_UsbDriver, &Mayflash_UsbDriver };

static const StatusGroup statusGroups[] = {
	{ INPUT_HW_DS4,         ds4Drivers,     sizeof(ds4Drivers) / sizeof(ds4Drivers[0]) },
	{ INPUT_HW_XBOX360,     xboxDrivers,    sizeof(xboxDrivers) / sizeof(xboxDrivers[0]) },
	{ INPUT_HW_GENERIC_USB, genericDrivers, sizeof(genericDrivers) / sizeof(genericDrivers[0]) },
};

static bool AnyInState(const StatusGroup & group, UsbInputState state) {
	for (int i = 0; i < group.count; i++) {
		if (UsbInput_GetState(group.drivers[i]) == state)
			return true;
	}
	return false;
}

InputUsbStatus WiiUsbPads::getStatus() const {
	InputUsbStatus status;

	// A connected controller wins over one that only needs a replug, whatever its priority
	const UsbInputState states[] = { USBINPUT_STATE_CONNECTED, USBINPUT_STATE_REPLUG_REQUIRED };
	for (UsbInputState state : states) {
		for (const StatusGroup & group : statusGroups) {
			if (AnyInState(group, state)) {
				status.profile = group.profile;
				status.replugRequired = (state == USBINPUT_STATE_REPLUG_REQUIRED);
				return status;
			}
		}
	}

	return status;
}

static const char * StateLabel(UsbInputState state) {
	switch (state) {
		case USBINPUT_STATE_CONNECTED:       return "connected";
		case USBINPUT_STATE_REPLUG_REQUIRED: return "please replug";
		default:                             return "not found";
	}
}

// Lists the drivers that have something plugged in; nothing at all when there is no USB controller
const char * WiiUsbPads::getStatusText() {
	size_t length = 0;
	statusText[0] = 0;

	for (int i = 0; i < UsbInput_DriverCount(); i++) {
		const UsbInputDriver * driver = UsbInput_DriverAt(i);
		UsbInputState state = UsbInput_GetState(driver);
		if (state == USBINPUT_STATE_NOT_FOUND)
			continue;

		int written = snprintf(statusText + length, sizeof(statusText) - length, "%s%s: %s",
			length ? ", " : "", driver->name, StateLabel(state));
		if (written < 0 || (size_t)written >= sizeof(statusText) - length) {
			statusText[length] = 0; // no room for this one: keep what fits whole
			break;
		}
		length += written;
	}

	return statusText;
}
