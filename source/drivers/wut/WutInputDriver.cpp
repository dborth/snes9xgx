/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutInputDriver.cpp
 ***************************************************************************/
#include "WutInputDriver.h"
#include "../Platform.h"
#include "../InputController.h"
#include "../InputData.h"

#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <sysapp/switch.h>
#include <cmath>
#include <algorithm>

static uint8_t vpadRumblePattern[15] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

/****************************************************************************
 * Hardware Mapping Helpers
 ***************************************************************************/
static uint32_t MapVPADToGeneric(uint32_t vpad_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (vpad_btns & VPAD_BUTTON_A)       mask |= INPUT_BTN_A;
	if (vpad_btns & VPAD_BUTTON_B)       mask |= INPUT_BTN_B;
	if (vpad_btns & VPAD_BUTTON_X)       mask |= INPUT_BTN_X;
	if (vpad_btns & VPAD_BUTTON_Y)       mask |= INPUT_BTN_Y;
	if (vpad_btns & VPAD_BUTTON_UP)      mask |= INPUT_BTN_UP;
	if (vpad_btns & VPAD_BUTTON_DOWN)    mask |= INPUT_BTN_DOWN;
	if (vpad_btns & VPAD_BUTTON_LEFT)    mask |= INPUT_BTN_LEFT;
	if (vpad_btns & VPAD_BUTTON_RIGHT)   mask |= INPUT_BTN_RIGHT;
	if (vpad_btns & VPAD_BUTTON_PLUS)    mask |= INPUT_BTN_PLUS;
	if (vpad_btns & VPAD_BUTTON_MINUS)   mask |= INPUT_BTN_MINUS;
	if (vpad_btns & VPAD_BUTTON_HOME)    mask |= INPUT_BTN_HOME;
	if (vpad_btns & VPAD_BUTTON_L)       mask |= INPUT_TRIGGER_L;
	if (vpad_btns & VPAD_BUTTON_R)       mask |= INPUT_TRIGGER_R;
	if (vpad_btns & VPAD_BUTTON_ZL)      mask |= INPUT_TRIGGER_ZL;
	if (vpad_btns & VPAD_BUTTON_ZR)      mask |= INPUT_TRIGGER_ZR;
	if (vpad_btns & VPAD_BUTTON_STICK_L) mask |= INPUT_THUMB_L;
	if (vpad_btns & VPAD_BUTTON_STICK_R) mask |= INPUT_THUMB_R;
	return mask;
}

static uint32_t MapKPADCoreToGeneric(uint32_t kpad_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (kpad_btns & WPAD_BUTTON_A)     mask |= INPUT_BTN_A;
	if (kpad_btns & WPAD_BUTTON_B)     mask |= INPUT_BTN_B;
	if (kpad_btns & WPAD_BUTTON_1)     mask |= INPUT_BTN_1;
	if (kpad_btns & WPAD_BUTTON_2)     mask |= INPUT_BTN_2;
	if (kpad_btns & WPAD_BUTTON_UP)    mask |= INPUT_BTN_UP;
	if (kpad_btns & WPAD_BUTTON_DOWN)  mask |= INPUT_BTN_DOWN;
	if (kpad_btns & WPAD_BUTTON_LEFT)  mask |= INPUT_BTN_LEFT;
	if (kpad_btns & WPAD_BUTTON_RIGHT) mask |= INPUT_BTN_RIGHT;
	if (kpad_btns & WPAD_BUTTON_PLUS)  mask |= INPUT_BTN_PLUS;
	if (kpad_btns & WPAD_BUTTON_MINUS) mask |= INPUT_BTN_MINUS;
	if (kpad_btns & WPAD_BUTTON_HOME)  mask |= INPUT_BTN_HOME;
	return mask;
}

static uint32_t MapKPADProToGeneric(uint32_t pro_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (pro_btns & WPAD_PRO_BUTTON_A)       mask |= INPUT_BTN_A;
	if (pro_btns & WPAD_PRO_BUTTON_B)       mask |= INPUT_BTN_B;
	if (pro_btns & WPAD_PRO_BUTTON_X)       mask |= INPUT_BTN_X;
	if (pro_btns & WPAD_PRO_BUTTON_Y)       mask |= INPUT_BTN_Y;
	if (pro_btns & WPAD_PRO_BUTTON_UP)      mask |= INPUT_BTN_UP;
	if (pro_btns & WPAD_PRO_BUTTON_DOWN)    mask |= INPUT_BTN_DOWN;
	if (pro_btns & WPAD_PRO_BUTTON_LEFT)    mask |= INPUT_BTN_LEFT;
	if (pro_btns & WPAD_PRO_BUTTON_RIGHT)   mask |= INPUT_BTN_RIGHT;
	if (pro_btns & WPAD_PRO_BUTTON_PLUS)    mask |= INPUT_BTN_PLUS;
	if (pro_btns & WPAD_PRO_BUTTON_MINUS)   mask |= INPUT_BTN_MINUS;
	if (pro_btns & WPAD_PRO_BUTTON_HOME)    mask |= INPUT_BTN_HOME;
	if (pro_btns & WPAD_PRO_TRIGGER_L)      mask |= INPUT_TRIGGER_L;
	if (pro_btns & WPAD_PRO_TRIGGER_R)      mask |= INPUT_TRIGGER_R;
	if (pro_btns & WPAD_PRO_TRIGGER_ZL)     mask |= INPUT_TRIGGER_ZL;
	if (pro_btns & WPAD_PRO_TRIGGER_ZR)     mask |= INPUT_TRIGGER_ZR;
	if (pro_btns & WPAD_PRO_BUTTON_STICK_L) mask |= INPUT_THUMB_L;
	if (pro_btns & WPAD_PRO_BUTTON_STICK_R) mask |= INPUT_THUMB_R;
	return mask;
}

static uint32_t MapKPADClassicToGeneric(uint32_t cls_btns) {
	uint32_t mask = INPUT_BTN_NONE;
	if (cls_btns & WPAD_CLASSIC_BUTTON_A)      mask |= INPUT_BTN_A;
	if (cls_btns & WPAD_CLASSIC_BUTTON_B)      mask |= INPUT_BTN_B;
	if (cls_btns & WPAD_CLASSIC_BUTTON_X)      mask |= INPUT_BTN_X;
	if (cls_btns & WPAD_CLASSIC_BUTTON_Y)      mask |= INPUT_BTN_Y;
	if (cls_btns & WPAD_CLASSIC_BUTTON_UP)     mask |= INPUT_BTN_UP;
	if (cls_btns & WPAD_CLASSIC_BUTTON_DOWN)   mask |= INPUT_BTN_DOWN;
	if (cls_btns & WPAD_CLASSIC_BUTTON_LEFT)   mask |= INPUT_BTN_LEFT;
	if (cls_btns & WPAD_CLASSIC_BUTTON_RIGHT)  mask |= INPUT_BTN_RIGHT;
	if (cls_btns & WPAD_CLASSIC_BUTTON_PLUS)   mask |= INPUT_BTN_PLUS;
	if (cls_btns & WPAD_CLASSIC_BUTTON_MINUS)  mask |= INPUT_BTN_MINUS;
	if (cls_btns & WPAD_CLASSIC_BUTTON_HOME)   mask |= INPUT_BTN_HOME;
	if (cls_btns & WPAD_CLASSIC_BUTTON_L)      mask |= INPUT_TRIGGER_L;
	if (cls_btns & WPAD_CLASSIC_BUTTON_R)      mask |= INPUT_TRIGGER_R;
	if (cls_btns & WPAD_CLASSIC_BUTTON_ZL)     mask |= INPUT_TRIGGER_ZL;
	if (cls_btns & WPAD_CLASSIC_BUTTON_ZR)     mask |= INPUT_TRIGGER_ZR;
	return mask;
}

static uint32_t MapKPADNunchukToGeneric(uint32_t wpad_btns) {
	uint32_t mask = INPUT_BTN_NONE;

	if (wpad_btns & WPAD_NUNCHUK_BUTTON_Z) mask |= INPUT_TRIGGER_ZL;
	if (wpad_btns & WPAD_NUNCHUK_BUTTON_C) mask |= INPUT_TRIGGER_L;

	return mask;
}

// One Euro Filter tuning for the Wiimote IR pointer (see OneEuroFilter.h).
// KPAD delivers samples much faster than our ~60Hz update rate, so without
// smoothing the raw per-sample noise (and the coarser stride of only
// consuming one sample per frame) reads as a jerky cursor compared to
// libogc's WPAD IR handling on Wii, which is already smoothed internally.
//
// minCutoff: smoothing strength when the pointer is essentially still.
// Lower = less jitter at rest but more lag when motion starts. Chosen to
// noticeably calm hand-tremor-level jitter without feeling "stuck".
// beta: how quickly smoothing backs off as speed increases, in units of
// screen-pixels/sec of underlying signal speed. Tuned so a deliberate,
// fast pointer swipe across the screen tracks with negligible lag while a
// small idle wobble is still heavily damped.
static constexpr float IR_MIN_CUTOFF = 0.8f;
static constexpr float IR_BETA = 0.015f;

WutInputDriver::WutInputDriver() : drcTouchedPrev(false), drcLastTouchX(0.0f), drcLastTouchY(0.0f) {
	for (int i = 0; i < 4; i++) {
		rumbleCount[i] = 0;
		rumbleRequest[i] = false;
		irFilterX[i].setParams(IR_MIN_CUTOFF, IR_BETA);
		irFilterY[i].setParams(IR_MIN_CUTOFF, IR_BETA);
		irSmoothInit[i] = false;
	}
}

// KPADReadEx can return multiple buffered samples per call (newest to oldest).
// We only need a small buffer - just enough to detect/skip a KPAD_ERROR_NO_SAMPLES
// frame without discarding real motion.
static constexpr uint32_t KPAD_SAMPLE_BUFFER = 4;

WutInputDriver::~WutInputDriver() {
	shutdown();
}

void WutInputDriver::init() {
	KPADInit();
	VPADInit();
	OSEnableHomeButtonMenu(FALSE);
	InitUserInputControllers();
}

void WutInputDriver::shutdown() {
	for (int i = 0; i < 4; i++) {
		WPADControlMotor((WPADChan)i, FALSE);
		rumbleCount[i] = 0;
		rumbleRequest[i] = false;
	}
	VPADStopMotor(VPAD_CHAN_0);

	// Restore the system default before handing control back to the OS/loader.
	OSEnableHomeButtonMenu(TRUE);
}

void WutInputDriver::openHomeButtonOverlay() {
	_SYSSwitchToHBMWithMode(0);
}

void WutInputDriver::setRumble(int channel, bool rumble) {
	if (channel >= 0 && channel < 4) {
		rumbleRequest[channel] = rumble;
	}
}

void WutInputDriver::update() {
	float screenWidth = (float)platform->getVideo()->getScreenWidth();
	float screenHeight = (float)platform->getVideo()->getScreenHeight();

	for (int i = 3; i >= 0; i--) {
		InputPadData padData;

		// VPAD Processing (GamePad, Channel 0 Only)
		if (i == 0) {
			VPADStatus vpadStatus;
			VPADReadError vpadError;
			VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

			if (vpadError == VPAD_READ_SUCCESS || vpadError == VPAD_READ_NO_SAMPLES) {
				padData.hw_connected[INPUT_HW_DRC] = true;
				padData.battery_level = vpadStatus.battery * 25; // normalize to 0-100

				padData.hw_buttons_d[INPUT_HW_DRC] = MapVPADToGeneric(vpadStatus.trigger);
				padData.hw_buttons_h[INPUT_HW_DRC] = MapVPADToGeneric(vpadStatus.hold);
				padData.hw_buttons_r[INPUT_HW_DRC] = MapVPADToGeneric(vpadStatus.release);

				padData.hw_stickX[INPUT_HW_DRC] = clampf(vpadStatus.leftStick.x, -1.0f, 1.0f);
				padData.hw_stickY[INPUT_HW_DRC] = clampf(vpadStatus.leftStick.y, -1.0f, 1.0f);
				padData.hw_substickX[INPUT_HW_DRC] = clampf(vpadStatus.rightStick.x, -1.0f, 1.0f);
				padData.hw_substickY[INPUT_HW_DRC] = clampf(vpadStatus.rightStick.y, -1.0f, 1.0f);

				padData.hw_gforceX[INPUT_HW_DRC] = vpadStatus.accelorometer.acc.x;
				padData.hw_gforceY[INPUT_HW_DRC] = vpadStatus.accelorometer.acc.y;
				padData.hw_gforceZ[INPUT_HW_DRC] = vpadStatus.accelorometer.acc.z;
				padData.hw_pitch[INPUT_HW_DRC] = vpadStatus.angle.x;
				padData.hw_roll[INPUT_HW_DRC]  = vpadStatus.angle.y;
				padData.hw_yaw[INPUT_HW_DRC]   = vpadStatus.angle.z;

				// Touch Screen & Pointer Coordinates Mapping
				bool drcTouched = (vpadStatus.tpNormal.touched != 0);

				if (drcTouched) {
					VPADTouchData calib;
					VPADGetTPCalibratedPoint(VPAD_CHAN_0, &calib, &vpadStatus.tpNormal);

					// DRC native touch space is 1280x720 -> scale to virtual video bounds
					float posX = clampf((float)calib.x * (screenWidth / 1280.0f), 0.0f, screenWidth);
					float posY = clampf((float)calib.y * (screenHeight / 720.0f), 0.0f, screenHeight);

					padData.isTouch = true;
					padData.validPointer = true;
					padData.cursor_x = posX;
					padData.cursor_y = posY;

					drcLastTouchX = posX;
					drcLastTouchY = posY;
				}

				// Touch lifecycle button synthesis (INPUT_BTN_A)
				if (drcTouched && !drcTouchedPrev) {
					// Touch Down
					padData.hw_buttons_d[INPUT_HW_DRC] |= INPUT_BTN_A;
					padData.hw_buttons_h[INPUT_HW_DRC] |= INPUT_BTN_A;
				} else if (drcTouched && drcTouchedPrev) {
					// Touch Held
					padData.hw_buttons_h[INPUT_HW_DRC] |= INPUT_BTN_A;
				} else if (!drcTouched && drcTouchedPrev) {
					// Touch Released
					padData.hw_buttons_r[INPUT_HW_DRC] |= INPUT_BTN_A;

					// Preserve pointer position on release frame
					padData.isTouch = true;
					padData.validPointer = true;
					padData.cursor_x = drcLastTouchX;
					padData.cursor_y = drcLastTouchY;
				}

				drcTouchedPrev = drcTouched;
			}
		}

		// KPAD Processing (Wiimotes, Extensions & Pro Controllers)
		KPADStatus kpadStatusBuf[KPAD_SAMPLE_BUFFER];
		KPADError kpadError = KPAD_ERROR_UNINITIALIZED;
		uint32_t kpadRead = KPADReadEx((KPADChan)i, kpadStatusBuf, KPAD_SAMPLE_BUFFER, &kpadError);

		// KPADReadEx can return count > 0 on a frame with no new report (stale/
		// cached data) - the error code, not the count, is what tells us the
		// sample is actually fresh. Consuming it unconditionally is what was
		// producing the periodic "skips a beat" stutter. Samples are ordered
		// newest-to-oldest, so index 0 is the one we want.
		if (kpadRead > 0 && kpadError == KPAD_ERROR_OK) {
			KPADStatus& kpadStatus = kpadStatusBuf[0];

			padData.hw_connected[INPUT_HW_WIIMOTE] = true;
			padData.battery_level = WPADGetBatteryLevel((WPADChan)i) * 25; // normalize to 0-100 range
			
			padData.hw_gforceX[INPUT_HW_WIIMOTE] = kpadStatus.acc.x;
			padData.hw_gforceY[INPUT_HW_WIIMOTE] = kpadStatus.acc.y;
			padData.hw_gforceZ[INPUT_HW_WIIMOTE] = kpadStatus.acc.z;
			padData.hw_pitch[INPUT_HW_WIIMOTE] = kpadStatus.angle.x;
			padData.hw_roll[INPUT_HW_WIIMOTE]  = kpadStatus.angle.y;

			if (kpadStatus.extensionType == WPAD_EXT_PRO_CONTROLLER) {
				padData.hw_connected[INPUT_HW_WUPC] = true;
				padData.hw_buttons_d[INPUT_HW_WUPC] = MapKPADProToGeneric(kpadStatus.pro.trigger);
				padData.hw_buttons_h[INPUT_HW_WUPC] = MapKPADProToGeneric(kpadStatus.pro.hold);
				padData.hw_buttons_r[INPUT_HW_WUPC] = MapKPADProToGeneric(kpadStatus.pro.release);

				padData.hw_stickX[INPUT_HW_WUPC] = clampf(kpadStatus.pro.leftStick.x, -1.0f, 1.0f);
				padData.hw_stickY[INPUT_HW_WUPC] = clampf(kpadStatus.pro.leftStick.y, -1.0f, 1.0f);
				padData.hw_substickX[INPUT_HW_WUPC] = clampf(kpadStatus.pro.rightStick.x, -1.0f, 1.0f);
				padData.hw_substickY[INPUT_HW_WUPC] = clampf(kpadStatus.pro.rightStick.y, -1.0f, 1.0f);

				controller[i]->setSideways(false);
			}
			else if (kpadStatus.extensionType == WPAD_EXT_CLASSIC || kpadStatus.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
				padData.hw_connected[INPUT_HW_CLASSIC] = true;
				padData.hw_buttons_d[INPUT_HW_CLASSIC] = MapKPADClassicToGeneric(kpadStatus.classic.trigger);
				padData.hw_buttons_h[INPUT_HW_CLASSIC] = MapKPADClassicToGeneric(kpadStatus.classic.hold);
				padData.hw_buttons_r[INPUT_HW_CLASSIC] = MapKPADClassicToGeneric(kpadStatus.classic.release);

				padData.hw_stickX[INPUT_HW_CLASSIC] = clampf(kpadStatus.classic.leftStick.x, -1.0f, 1.0f);
				padData.hw_stickY[INPUT_HW_CLASSIC] = clampf(kpadStatus.classic.leftStick.y, -1.0f, 1.0f);
				padData.hw_substickX[INPUT_HW_CLASSIC] = clampf(kpadStatus.classic.rightStick.x, -1.0f, 1.0f);
				padData.hw_substickY[INPUT_HW_CLASSIC] = clampf(kpadStatus.classic.rightStick.y, -1.0f, 1.0f);

				controller[i]->setSideways(false);
			}
			else {
				// Core Wiimote or Wiimote + Nunchuk
				padData.hw_buttons_d[INPUT_HW_WIIMOTE] = MapKPADCoreToGeneric(kpadStatus.trigger);
				padData.hw_buttons_h[INPUT_HW_WIIMOTE] = MapKPADCoreToGeneric(kpadStatus.hold);
				padData.hw_buttons_r[INPUT_HW_WIIMOTE] = MapKPADCoreToGeneric(kpadStatus.release);


				// Map IR pointer if active and DRC touch is not currently taking priority
				if (kpadStatus.posValid && !padData.validPointer) {
					float rawX = clampf((kpadStatus.pos.x * 0.5f + 0.5f) * screenWidth, 0.0f, screenWidth);
					float rawY = clampf((kpadStatus.pos.y * 0.5f + 0.5f) * screenHeight, 0.0f, screenHeight);

					float deltaTime = platform->getVideo()->getDeltaTime();
					float smoothX, smoothY;

					if (!irSmoothInit[i]) {
						// First valid sample after acquiring (or re-acquiring) the sensor
						// bar - snap straight to it instead of smoothing from a stale/zero
						// position, which would otherwise show up as a visible snap-drag.
						irFilterX[i].reset();
						irFilterY[i].reset();
						smoothX = irFilterX[i].filter(rawX, deltaTime);
						smoothY = irFilterY[i].filter(rawY, deltaTime);
						irSmoothInit[i] = true;
					} else {
						smoothX = irFilterX[i].filter(rawX, deltaTime);
						smoothY = irFilterY[i].filter(rawY, deltaTime);
					}

					padData.validPointer = true;
					padData.isTouch = false;
					padData.cursor_x = smoothX;
					padData.cursor_y = smoothY;
					padData.cursor_angle = kpadStatus.angle.y;
				} else if (!kpadStatus.posValid) {
					// Sensor bar tracking lost - reset the filter so we don't drag the
					// cursor toward a stale point when it's reacquired.
					irSmoothInit[i] = false;
				}

				if (kpadStatus.extensionType == WPAD_EXT_NUNCHUK || kpadStatus.extensionType == WPAD_EXT_MPLUS_NUNCHUK) {
					padData.hw_connected[INPUT_HW_NUNCHUK] = true;

					padData.hw_buttons_d[INPUT_HW_NUNCHUK] = MapKPADNunchukToGeneric(kpadStatus.nunchuk.trigger);
					padData.hw_buttons_h[INPUT_HW_NUNCHUK] = MapKPADNunchukToGeneric(kpadStatus.nunchuk.hold);
					padData.hw_buttons_r[INPUT_HW_NUNCHUK] = MapKPADNunchukToGeneric(kpadStatus.nunchuk.release);

					padData.hw_stickX[INPUT_HW_NUNCHUK] = clampf(kpadStatus.nunchuk.stick.x, -1.0f, 1.0f);
					padData.hw_stickY[INPUT_HW_NUNCHUK] = clampf(kpadStatus.nunchuk.stick.y, -1.0f, 1.0f);

					padData.hw_gforceX[INPUT_HW_NUNCHUK] = kpadStatus.nunchuk.acc.x;
					padData.hw_gforceY[INPUT_HW_NUNCHUK] = kpadStatus.nunchuk.acc.y;
					padData.hw_gforceZ[INPUT_HW_NUNCHUK] = kpadStatus.nunchuk.acc.z;

					controller[i]->setSideways(false);
				} else {
					// Sideways Wiimote auto-detection when no extension is connected
					controller[i]->setSideways(std::abs(kpadStatus.acc.x) > std::abs(kpadStatus.acc.y));
				}
			}
		} else {
			// No fresh KPAD sample this frame (disconnected, no controller, or a
			// genuine KPAD_ERROR_NO_SAMPLES tick) - reset the IR filter so a later
			// reconnect doesn't drag the cursor from a stale position.
			irSmoothInit[i] = false;
		}

		// Merge Aggregate State
		for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++) {
			if (!padData.hw_connected[hw]) continue;

			padData.buttons_d |= padData.hw_buttons_d[hw];
			padData.buttons_h |= padData.hw_buttons_h[hw];
			padData.buttons_r |= padData.hw_buttons_r[hw];

			if (std::abs(padData.hw_stickX[hw]) > std::abs(padData.stickX))          padData.stickX = padData.hw_stickX[hw];
			if (std::abs(padData.hw_stickY[hw]) > std::abs(padData.stickY))          padData.stickY = padData.hw_stickY[hw];
			if (std::abs(padData.hw_substickX[hw]) > std::abs(padData.substickX))    padData.substickX = padData.hw_substickX[hw];
			if (std::abs(padData.hw_substickY[hw]) > std::abs(padData.substickY))    padData.substickY = padData.hw_substickY[hw];
			if (std::abs(padData.hw_gforceX[hw]) > std::abs(padData.gforceX))        padData.gforceX = padData.hw_gforceX[hw];
			if (std::abs(padData.hw_gforceY[hw]) > std::abs(padData.gforceY))        padData.gforceY = padData.hw_gforceY[hw];
			if (std::abs(padData.hw_gforceZ[hw]) > std::abs(padData.gforceZ))        padData.gforceZ = padData.hw_gforceZ[hw];
			if (std::abs(padData.hw_pitch[hw]) > std::abs(padData.pitch))            padData.pitch = padData.hw_pitch[hw];
			if (std::abs(padData.hw_roll[hw]) > std::abs(padData.roll))              padData.roll = padData.hw_roll[hw];
			if (std::abs(padData.hw_yaw[hw]) > std::abs(padData.yaw))                padData.yaw = padData.hw_yaw[hw];
		}

		// Update logical controller state
		controller[i]->update(padData, platform->getVideo()->getDeltaTime());

		// Rumble Lifecycle Management
		if (rumbleRequest[i] && rumbleCount[i] < 3) {
			if (padData.hw_connected[INPUT_HW_WIIMOTE] || padData.hw_connected[INPUT_HW_WUPC]) {
				WPADControlMotor((WPADChan)i, TRUE);
			}
			if (i == 0 && padData.hw_connected[INPUT_HW_DRC]) {
				VPADControlMotor(VPAD_CHAN_0, vpadRumblePattern, sizeof(vpadRumblePattern));
			}
			rumbleCount[i]++;
		} else if (rumbleRequest[i]) {
			rumbleCount[i] = 12;
			rumbleRequest[i] = false;
		} else {
			if (rumbleCount[i]) rumbleCount[i]--;
			if (padData.hw_connected[INPUT_HW_WIIMOTE] || padData.hw_connected[INPUT_HW_WUPC]) {
				WPADControlMotor((WPADChan)i, FALSE);
			}
			if (i == 0) {
				VPADStopMotor(VPAD_CHAN_0);
			}
		}
	}
}
