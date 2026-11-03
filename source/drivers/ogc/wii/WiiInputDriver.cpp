/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2009-2026
 * WiiInputDriver.cpp
 *
 * Wiimote/extension management and normalization, the Wii U GamePad
 * (libwiidrc), and the Wii-only parts of the GameCube driver's frame
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>

#include "WiiInputDriver.h"
#include "WiiPlatform.h"
#include "input/wiidrc.h"
#include "../OgcInputUtil.h"
#include "../../Platform.h"
#include "../../InputController.h"

extern "C" {
s32 __STM_Close();
s32 __STM_Init();
}

static void WiimotePowerButtonCallback(s32) { NotifyWiiShutdownRequested(); }

WiiInputDriver::~WiiInputDriver() {
	// The base destructor's own shutdown() can no longer reach driveRumble() below
	shutdown();
}

void WiiInputDriver::initHardware() {
	// Wii Power/Reset buttons
	__STM_Close();
	__STM_Init();
	SYS_SetPowerCallback(NotifyWiiShutdownRequested);

	WiiDRC_Init();
	wiiVC = WiiDRC_Inited();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	WPAD_SetPowerButtonCallback(WiimotePowerButtonCallback);
}

void WiiInputDriver::scanHardware() {
	usbPads.scan();
	WiiDRC_ScanPads();
	WPAD_ScanPads();
}

bool WiiInputDriver::isSystemRumbleAllowed() const {
	return CONF_GetPadMotorMode() != 0;
}

void WiiInputDriver::driveRumble(int channel, bool on) {
	WPAD_Rumble(channel, on ? 1 : 0);
}

/****************************************************************************
 * Hardware Mapping Helpers
 * Translates raw libogc hardware bits to our generic UI masks
 ***************************************************************************/
static uint32_t MapWiimoteToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = INPUT_BTN_NONE;

	if (wpad_btns & WPAD_BUTTON_A)     mask |= INPUT_BTN_A;
	if (wpad_btns & WPAD_BUTTON_B)     mask |= INPUT_BTN_B;
	if (wpad_btns & WPAD_BUTTON_1)     mask |= INPUT_BTN_1;
	if (wpad_btns & WPAD_BUTTON_2)     mask |= INPUT_BTN_2;
	if (wpad_btns & WPAD_BUTTON_UP)    mask |= INPUT_BTN_UP;
	if (wpad_btns & WPAD_BUTTON_DOWN)  mask |= INPUT_BTN_DOWN;
	if (wpad_btns & WPAD_BUTTON_LEFT)  mask |= INPUT_BTN_LEFT;
	if (wpad_btns & WPAD_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (wpad_btns & WPAD_BUTTON_PLUS)  mask |= INPUT_BTN_PLUS;
	if (wpad_btns & WPAD_BUTTON_MINUS) mask |= INPUT_BTN_MINUS;
	if (wpad_btns & WPAD_BUTTON_HOME)  mask |= INPUT_BTN_HOME;

	return mask;
}

static uint32_t MapNunchukToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = INPUT_BTN_NONE;

	if (wpad_btns & WPAD_NUNCHUK_BUTTON_Z) mask |= INPUT_TRIGGER_ZL;
	if (wpad_btns & WPAD_NUNCHUK_BUTTON_C) mask |= INPUT_TRIGGER_L;

	return mask;
}

static uint32_t MapClassicToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = INPUT_BTN_NONE;

	// Classic Controller inputs (upper 16 bits)
	if (wpad_btns & WPAD_CLASSIC_BUTTON_A) mask |= INPUT_BTN_A;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_B) mask |= INPUT_BTN_B;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_X) mask |= INPUT_BTN_X;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_Y) mask |= INPUT_BTN_Y;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_UP) mask |= INPUT_BTN_UP;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_DOWN) mask |= INPUT_BTN_DOWN;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_LEFT) mask |= INPUT_BTN_LEFT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_PLUS) mask |= INPUT_BTN_PLUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_MINUS) mask |= INPUT_BTN_MINUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_HOME) mask |= INPUT_BTN_HOME;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_L) mask |= INPUT_TRIGGER_L;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_R) mask |= INPUT_TRIGGER_R;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZL) mask |= INPUT_TRIGGER_ZL;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZR) mask |= INPUT_TRIGGER_ZR;

	return mask;
}

static uint32_t MapWiiUGamepadToGeneric(uint32_t drc_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (drc_btns & WIIDRC_BUTTON_A) mask |= INPUT_BTN_A;
	if (drc_btns & WIIDRC_BUTTON_B) mask |= INPUT_BTN_B;
	if (drc_btns & WIIDRC_BUTTON_X) mask |= INPUT_BTN_X;
	if (drc_btns & WIIDRC_BUTTON_Y) mask |= INPUT_BTN_Y;
	if (drc_btns & WIIDRC_BUTTON_UP) mask |= INPUT_BTN_UP;
	if (drc_btns & WIIDRC_BUTTON_DOWN) mask |= INPUT_BTN_DOWN;
	if (drc_btns & WIIDRC_BUTTON_LEFT) mask |= INPUT_BTN_LEFT;
	if (drc_btns & WIIDRC_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (drc_btns & WIIDRC_BUTTON_PLUS) mask |= INPUT_BTN_PLUS;
	if (drc_btns & WIIDRC_BUTTON_MINUS) mask |= INPUT_BTN_MINUS;
	if (drc_btns & WIIDRC_BUTTON_HOME) mask |= INPUT_BTN_HOME;
	if (drc_btns & WIIDRC_BUTTON_L) mask |= INPUT_TRIGGER_L;
	if (drc_btns & WIIDRC_BUTTON_R) mask |= INPUT_TRIGGER_R;
	if (drc_btns & WIIDRC_BUTTON_ZL) mask |= INPUT_TRIGGER_ZL;
	if (drc_btns & WIIDRC_BUTTON_ZR) mask |= INPUT_TRIGGER_ZR;
	return mask;
}

/****************************************************************************
 * Analog Normalization Helpers
 ***************************************************************************/
static float NormalizeWPADAnalog(int pos, int min, int max, int center) {
	if (min == max) return 0.0f;

	// Handle broken 3rd party controller calibration data
	if ((min >= center) || (max <= center)) {
		min = 0; max = 64; center = 32; // Generic fallback
	}
	int offset = pos - center;
	if (offset > 0) {
		return OgcClampf((float)offset / (float)(max - center), 0.0f, 1.0f);
	} else {
		return OgcClampf((float)offset / (float)(center - min), -1.0f, 0.0f);
	}
}

/****************************************************************************
 * Per-channel state
 ***************************************************************************/
void WiiInputDriver::readChannel(int channel, InputPadData & pad) {
	usbPads.read(channel, pad);
	readWiimote(channel, pad);
	usbPads.readPointer(channel, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight(), pad);

	// The GamePad is one controller, on channel 0
	if (channel == 0)
		readGamePad(pad);
}

// The Wiimote and whatever is plugged into it (Nunchuk, Classic Controller, Wii U Pro Controller)
void WiiInputDriver::readWiimote(int channel, InputPadData & padData) {
	uint32_t exp_type = WPAD_EXP_NONE;

	if (WPAD_Probe(channel, &exp_type) != WPAD_ERR_NONE)
		return;

	WPADData* wpad = WPAD_Data(channel);

	// Always process base Wiimote
	padData.hw_connected[INPUT_HW_WIIMOTE] = true;
	padData.battery_level = wpad->battery_level;
	padData.hw_battery_level[INPUT_HW_WIIMOTE] = wpad->battery_level;

	padData.hw_gforceX[INPUT_HW_WIIMOTE] = wpad->gforce.x;
	padData.hw_gforceY[INPUT_HW_WIIMOTE] = wpad->gforce.y;
	padData.hw_gforceZ[INPUT_HW_WIIMOTE] = wpad->gforce.z;
	padData.hw_pitch[INPUT_HW_WIIMOTE]   = wpad->orient.pitch;
	padData.hw_roll[INPUT_HW_WIIMOTE]    = wpad->orient.roll;
	padData.hw_yaw[INPUT_HW_WIIMOTE]     = wpad->orient.yaw;

	// A Wii U Pro Controller reports as a Classic Controller, but is not plugged into the Wiimote
	const bool isWUPC = (exp_type == WPAD_EXP_CLASSIC && wpad->exp.classic.type == 2);

	if (!isWUPC) {
		// The Wiimote itself: its buttons and IR pointer, whatever is plugged into it
		padData.hw_buttons_d[INPUT_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_d);
		padData.hw_buttons_h[INPUT_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_h);
		padData.hw_buttons_r[INPUT_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_u);

		if (wpad->ir.valid) {
			padData.validPointer = true;
			padData.isTouch = false;
			padData.cursor_x = wpad->ir.x;
			padData.cursor_y = wpad->ir.y;
			padData.cursor_angle = wpad->ir.angle;
		}

		// With anything plugged in it is held vertically, never sideways
		controller[channel]->setSideways(exp_type == WPAD_EXP_NONE && getWiimoteOrientation() == WIIMOTE_ORIENTATION_HORIZONTAL);
	}
	else {
		controller[channel]->setSideways(false);
	}

	if (exp_type == WPAD_EXP_NUNCHUK) {
		padData.hw_connected[INPUT_HW_NUNCHUK] = true;
		padData.hw_buttons_d[INPUT_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_d);
		padData.hw_buttons_h[INPUT_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_h);
		padData.hw_buttons_r[INPUT_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_u);
		joystick_t* js = &wpad->exp.nunchuk.js;
		padData.hw_stickX[INPUT_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.x, js->min.x, js->max.x, js->center.x);
		padData.hw_stickY[INPUT_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.y, js->min.y, js->max.y, js->center.y);

		padData.hw_gforceX[INPUT_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.x;
		padData.hw_gforceY[INPUT_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.y;
		padData.hw_gforceZ[INPUT_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.z;
		padData.hw_pitch[INPUT_HW_NUNCHUK]   = wpad->exp.nunchuk.orient.pitch;
		padData.hw_roll[INPUT_HW_NUNCHUK]    = wpad->exp.nunchuk.orient.roll;
		padData.hw_yaw[INPUT_HW_NUNCHUK]     = wpad->exp.nunchuk.orient.yaw;
	}
	else if (exp_type == WPAD_EXP_CLASSIC) {
		int hw = isWUPC ? INPUT_HW_WUPC : INPUT_HW_CLASSIC;

		padData.hw_connected[hw] = true;
		padData.hw_buttons_d[hw] = MapClassicToGeneric(wpad->btns_d);
		padData.hw_buttons_h[hw] = MapClassicToGeneric(wpad->btns_h);
		padData.hw_buttons_r[hw] = MapClassicToGeneric(wpad->btns_u);

		joystick_t* ljs = &wpad->exp.classic.ljs;
		joystick_t* rjs = &wpad->exp.classic.rjs;
		padData.hw_stickX[hw] = NormalizeWPADAnalog(ljs->pos.x, ljs->min.x, ljs->max.x, ljs->center.x);
		padData.hw_stickY[hw] = NormalizeWPADAnalog(ljs->pos.y, ljs->min.y, ljs->max.y, ljs->center.y);
		padData.hw_substickX[hw] = NormalizeWPADAnalog(rjs->pos.x, rjs->min.x, rjs->max.x, rjs->center.x);
		padData.hw_substickY[hw] = NormalizeWPADAnalog(rjs->pos.y, rjs->min.y, rjs->max.y, rjs->center.y);
	}
}

// Wii U GamePad, read over the network by libwiidrc
void WiiInputDriver::readGamePad(InputPadData & padData) {
	if (!WiiDRC_Inited() || !WiiDRC_Connected())
		return;

	padData.hw_connected[INPUT_HW_DRC] = true;
	DrcBatteryFromRaw(WiiDRC_Data()->battery, padData.hw_battery_level[INPUT_HW_DRC], padData.hw_charging[INPUT_HW_DRC]);
	padData.hw_buttons_d[INPUT_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsDown());
	padData.hw_buttons_h[INPUT_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsHeld());
	padData.hw_buttons_r[INPUT_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsUp());
	padData.hw_stickX[INPUT_HW_DRC] = OgcStickToFloat(WiiDRC_lStickX());
	padData.hw_stickY[INPUT_HW_DRC] = OgcStickToFloat(WiiDRC_lStickY());
	padData.hw_substickX[INPUT_HW_DRC] = OgcStickToFloat(WiiDRC_rStickX());
	padData.hw_substickY[INPUT_HW_DRC] = OgcStickToFloat(WiiDRC_rStickY());
}
