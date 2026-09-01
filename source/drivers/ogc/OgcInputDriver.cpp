/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcInputDriver.cpp
 * Hardware Translation Layer (Driver)
 * Wii/GameCube controller management and normalization
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <ogc/conf.h>

#include "OgcInputDriver.h"
#include "../Platform.h"
#include "wiidrc.h"
#include "../../libgui/GuiInputController.h"

static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/****************************************************************************
 * Hardware Mapping Helpers
 * Translates raw libogc hardware bits to our generic UI masks
 ***************************************************************************/
static uint32_t MapPADToGeneric(uint32_t pad_btns) {
	uint32_t mask = GUI_BTN_NONE;
	if (pad_btns & PAD_BUTTON_A)      mask |= GUI_BTN_A;
	if (pad_btns & PAD_BUTTON_B)      mask |= GUI_BTN_B;
	if (pad_btns & PAD_BUTTON_X)      mask |= GUI_BTN_X;
	if (pad_btns & PAD_BUTTON_Y)      mask |= GUI_BTN_Y;
	if (pad_btns & PAD_BUTTON_UP)     mask |= GUI_BTN_UP;
	if (pad_btns & PAD_BUTTON_DOWN)   mask |= GUI_BTN_DOWN;
	if (pad_btns & PAD_BUTTON_LEFT)   mask |= GUI_BTN_LEFT;
	if (pad_btns & PAD_BUTTON_RIGHT)  mask |= GUI_BTN_RIGHT;
	if (pad_btns & PAD_BUTTON_START)  mask |= GUI_BTN_PLUS;
	if (pad_btns & PAD_TRIGGER_L)     mask |= GUI_TRIGGER_L;
	if (pad_btns & PAD_TRIGGER_R)     mask |= GUI_TRIGGER_R;
	if (pad_btns & PAD_TRIGGER_Z)     mask |= GUI_TRIGGER_ZR;
	return mask;
}

#ifdef HW_RVL
static uint32_t MapWiimoteToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	if (wpad_btns & WPAD_BUTTON_A)     mask |= GUI_BTN_A;
	if (wpad_btns & WPAD_BUTTON_B)     mask |= GUI_BTN_B;
	if (wpad_btns & WPAD_BUTTON_1)     mask |= GUI_BTN_1;
	if (wpad_btns & WPAD_BUTTON_2)     mask |= GUI_BTN_2;
	if (wpad_btns & WPAD_BUTTON_UP)    mask |= GUI_BTN_UP;
	if (wpad_btns & WPAD_BUTTON_DOWN)  mask |= GUI_BTN_DOWN;
	if (wpad_btns & WPAD_BUTTON_LEFT)  mask |= GUI_BTN_LEFT;
	if (wpad_btns & WPAD_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (wpad_btns & WPAD_BUTTON_PLUS)  mask |= GUI_BTN_PLUS;
	if (wpad_btns & WPAD_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (wpad_btns & WPAD_BUTTON_HOME)  mask |= GUI_BTN_HOME;

	return mask;
}

static uint32_t MapNunchukToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	if (wpad_btns & WPAD_NUNCHUK_BUTTON_Z) mask |= GUI_TRIGGER_ZL;
	if (wpad_btns & WPAD_NUNCHUK_BUTTON_C) mask |= GUI_TRIGGER_L;

	return mask;
}

static uint32_t MapClassicToGeneric(uint32_t wpad_btns)
{
	uint32_t mask = GUI_BTN_NONE;

	// Classic Controller inputs (upper 16 bits)
	if (wpad_btns & WPAD_CLASSIC_BUTTON_A) mask |= GUI_BTN_A;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_B) mask |= GUI_BTN_B;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_X) mask |= GUI_BTN_X;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_Y) mask |= GUI_BTN_Y;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_UP) mask |= GUI_BTN_UP;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_DOWN) mask |= GUI_BTN_DOWN;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_LEFT) mask |= GUI_BTN_LEFT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_PLUS) mask |= GUI_BTN_PLUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_HOME) mask |= GUI_BTN_HOME;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_L) mask |= GUI_TRIGGER_L;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_FULL_R) mask |= GUI_TRIGGER_R;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZL) mask |= GUI_TRIGGER_ZL;
	if (wpad_btns & WPAD_CLASSIC_BUTTON_ZR) mask |= GUI_TRIGGER_ZR;

	return mask;
}

static uint32_t MapWiiUGamepadToGeneric(uint32_t drc_btns) {
	uint32_t mask = GUI_BTN_NONE;
	if (drc_btns & WIIDRC_BUTTON_A) mask |= GUI_BTN_A;
	if (drc_btns & WIIDRC_BUTTON_B) mask |= GUI_BTN_B;
	if (drc_btns & WIIDRC_BUTTON_X) mask |= GUI_BTN_X;
	if (drc_btns & WIIDRC_BUTTON_Y) mask |= GUI_BTN_Y;
	if (drc_btns & WIIDRC_BUTTON_UP) mask |= GUI_BTN_UP;
	if (drc_btns & WIIDRC_BUTTON_DOWN) mask |= GUI_BTN_DOWN;
	if (drc_btns & WIIDRC_BUTTON_LEFT) mask |= GUI_BTN_LEFT;
	if (drc_btns & WIIDRC_BUTTON_RIGHT) mask |= GUI_BTN_RIGHT;
	if (drc_btns & WIIDRC_BUTTON_PLUS) mask |= GUI_BTN_PLUS;
	if (drc_btns & WIIDRC_BUTTON_MINUS) mask |= GUI_BTN_MINUS;
	if (drc_btns & WIIDRC_BUTTON_HOME) mask |= GUI_BTN_HOME;
	if (drc_btns & WIIDRC_BUTTON_L) mask |= GUI_TRIGGER_L;
	if (drc_btns & WIIDRC_BUTTON_R) mask |= GUI_TRIGGER_R;
	if (drc_btns & WIIDRC_BUTTON_ZL) mask |= GUI_TRIGGER_ZL;
	if (drc_btns & WIIDRC_BUTTON_ZR) mask |= GUI_TRIGGER_ZR;
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
		return clampf((float)offset / (float)(max - center), 0.0f, 1.0f);
	} else {
		return clampf((float)offset / (float)(center - min), -1.0f, 0.0f);
	}
}
#endif

OgcInputDriver::OgcInputDriver() {
	for (int i = 0; i < 4; i++) {
		rumbleCount[i] = 0;
		rumbleRequest[i] = false;
	}
}

OgcInputDriver::~OgcInputDriver() {
	shutdown();
}

void OgcInputDriver::init() {
	InitUserInputControllers();
}

void OgcInputDriver::shutdown() {
	for (int i = 0; i < 4; i++) {
		#ifdef HW_RVL
		WPAD_Rumble(i, 0);
		#endif
		PAD_ControlMotor(i, PAD_MOTOR_STOP);
		rumbleCount[i] = 0;
		rumbleRequest[i] = false;
	}
}

void OgcInputDriver::setRumble(int channel, bool rumble) {
	if (channel >= 0 && channel < 4) {
		rumbleRequest[channel] = rumble;
	}
}

void OgcInputDriver::update(float deltaTime) {
	#ifdef HW_RVL
	WPAD_ScanPads();
	WiiDRC_ScanPads();
	bool systemRumbleAllowed = (CONF_GetPadMotorMode() != 0);
	#else
	bool systemRumbleAllowed = true;
	#endif

	u32 activeGamecubePads = PAD_ScanPads();
	bool allowRumble = isRumbleEnabled() && systemRumbleAllowed;

	for(int i = 3; i >= 0; i--) {
		GuiInputPadData padData;

		bool gamecubeActive = (activeGamecubePads & (1 << i)) != 0;

		if(gamecubeActive) {
			padData.hw_connected[GUI_HW_GAMECUBE] = true;
			padData.hw_buttons_d[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsDown(i));
			padData.hw_buttons_h[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsHeld(i));
			padData.hw_buttons_r[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsUp(i));
			padData.hw_stickX[GUI_HW_GAMECUBE] = clampf((float)PAD_StickX(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_stickY[GUI_HW_GAMECUBE] = clampf((float)PAD_StickY(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_substickX[GUI_HW_GAMECUBE] = clampf((float)PAD_SubStickX(i) / 128.0f, -1.0f, 1.0f);
			padData.hw_substickY[GUI_HW_GAMECUBE] = clampf((float)PAD_SubStickY(i) / 128.0f, -1.0f, 1.0f);
		}

		#ifdef HW_RVL
		// Process Wiimote and Extensions
		uint32_t exp_type = WPAD_EXP_NONE;

		if (WPAD_Probe(i, &exp_type) == WPAD_ERR_NONE) {
			WPADData* wpad = WPAD_Data(i);

			// Always process base Wiimote
			padData.hw_connected[GUI_HW_WIIMOTE] = true;
			padData.battery_level = wpad->battery_level;
			
			padData.hw_gforceX[GUI_HW_WIIMOTE] = wpad->gforce.x;
			padData.hw_gforceY[GUI_HW_WIIMOTE] = wpad->gforce.y;
			padData.hw_gforceZ[GUI_HW_WIIMOTE] = wpad->gforce.z;
			padData.hw_pitch[GUI_HW_WIIMOTE]   = wpad->orient.pitch;
			padData.hw_roll[GUI_HW_WIIMOTE]    = wpad->orient.roll;
			padData.hw_yaw[GUI_HW_WIIMOTE]     = wpad->orient.yaw;

			if (exp_type == WPAD_EXP_NONE) {
				padData.hw_buttons_d[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_u);

				if (wpad->ir.valid) {
					padData.validPointer = true;
					padData.isTouch = false;
					padData.cursor_x = wpad->ir.x;
					padData.cursor_y = wpad->ir.y;
					padData.cursor_angle = wpad->ir.angle;
				}

				userInput[i]->setSideways(fabs(wpad->gforce.x) > fabs(wpad->gforce.y));
			}
			else if (exp_type == WPAD_EXP_NUNCHUK) {
				padData.hw_buttons_d[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_WIIMOTE] = MapWiimoteToGeneric(wpad->btns_u);

				padData.hw_connected[GUI_HW_NUNCHUK] = true;
				padData.hw_buttons_d[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_d);
				padData.hw_buttons_h[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_h);
				padData.hw_buttons_r[GUI_HW_NUNCHUK] = MapNunchukToGeneric(wpad->btns_u);
				joystick_t* js = &wpad->exp.nunchuk.js;
				padData.hw_stickX[GUI_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.x, js->min.x, js->max.x, js->center.x);
				padData.hw_stickY[GUI_HW_NUNCHUK] = NormalizeWPADAnalog(js->pos.y, js->min.y, js->max.y, js->center.y);
				
				padData.hw_gforceX[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.x;
				padData.hw_gforceY[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.y;
				padData.hw_gforceZ[GUI_HW_NUNCHUK] = wpad->exp.nunchuk.gforce.z;
				padData.hw_pitch[GUI_HW_NUNCHUK]   = wpad->exp.nunchuk.orient.pitch;
				padData.hw_roll[GUI_HW_NUNCHUK]    = wpad->exp.nunchuk.orient.roll;
				padData.hw_yaw[GUI_HW_NUNCHUK]     = wpad->exp.nunchuk.orient.yaw;
				
				userInput[i]->setSideways(false);
			}
			else if (exp_type == WPAD_EXP_CLASSIC) {
				bool isWUPC = (wpad->exp.classic.type == 2);
				int hw = isWUPC ? GUI_HW_WUPC : GUI_HW_CLASSIC;

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
				userInput[i]->setSideways(false);
			}
			else {
				userInput[i]->setSideways(false);
			}
		}

		// Process Wii U Gamepad
		if(i == 0 && WiiDRC_Inited() && WiiDRC_Connected()) {
			padData.hw_connected[GUI_HW_DRC] = true;
			padData.hw_buttons_d[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsDown());
			padData.hw_buttons_h[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsHeld());
			padData.hw_buttons_r[GUI_HW_DRC] = MapWiiUGamepadToGeneric(WiiDRC_ButtonsUp());
			padData.hw_stickX[GUI_HW_DRC] = clampf((float)WiiDRC_lStickX() / 128.0f, -1.0f, 1.0f);
			padData.hw_stickY[GUI_HW_DRC] = clampf((float)WiiDRC_lStickY() / 128.0f, -1.0f, 1.0f);
			padData.hw_substickX[GUI_HW_DRC] = clampf((float)WiiDRC_rStickX() / 128.0f, -1.0f, 1.0f);
			padData.hw_substickY[GUI_HW_DRC] = clampf((float)WiiDRC_rStickY() / 128.0f, -1.0f, 1.0f);
		}
		#endif

		// Merge into unified aggregate state for UI Elements
		for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++) {
			if (!padData.hw_connected[hw])
				continue;

			padData.buttons_d |= padData.hw_buttons_d[hw];
			padData.buttons_h |= padData.hw_buttons_h[hw];
			padData.buttons_r |= padData.hw_buttons_r[hw];

			if (std::abs(padData.hw_stickX[hw]) > std::abs(padData.stickX)) padData.stickX = padData.hw_stickX[hw];
			if (std::abs(padData.hw_stickY[hw]) > std::abs(padData.stickY)) padData.stickY = padData.hw_stickY[hw];
			if (std::abs(padData.hw_substickX[hw]) > std::abs(padData.substickX)) padData.substickX = padData.hw_substickX[hw];
			if (std::abs(padData.hw_substickY[hw]) > std::abs(padData.substickY)) padData.substickY = padData.hw_substickY[hw];
			if (std::abs(padData.hw_gforceX[hw]) > std::abs(padData.gforceX)) padData.gforceX = padData.hw_gforceX[hw];
			if (std::abs(padData.hw_gforceY[hw]) > std::abs(padData.gforceY)) padData.gforceY = padData.hw_gforceY[hw];
			if (std::abs(padData.hw_gforceZ[hw]) > std::abs(padData.gforceZ)) padData.gforceZ = padData.hw_gforceZ[hw];
			if (std::abs(padData.hw_pitch[hw]) > std::abs(padData.pitch)) padData.pitch = padData.hw_pitch[hw];
			if (std::abs(padData.hw_roll[hw]) > std::abs(padData.roll)) padData.roll = padData.hw_roll[hw];
			if (std::abs(padData.hw_yaw[hw]) > std::abs(padData.yaw)) padData.yaw = padData.hw_yaw[hw];
		}

		// Push the finalized, merged payload to the controller abstraction
		userInput[i]->update(padData, deltaTime);
		
		bool doRumble = rumbleRequest[i] && allowRumble;

		if (doRumble && rumbleCount[i] < 3) {
			#ifdef HW_RVL
			WPAD_Rumble(i, 1);
			#endif

			if (gamecubeActive) {
				PAD_ControlMotor(i, PAD_MOTOR_RUMBLE);
			}

			rumbleCount[i]++;
		} else if (doRumble) {
			rumbleCount[i] = 12;
			rumbleRequest[i] = false;
		} else {
			if (rumbleCount[i]) rumbleCount[i]--;

			#ifdef HW_RVL
			WPAD_Rumble(i, 0);
			#endif

			PAD_ControlMotor(i, PAD_MOTOR_STOP);
			rumbleRequest[i] = false; // ensure flag clears if toggled off mid-rumble
		}
	}
}
