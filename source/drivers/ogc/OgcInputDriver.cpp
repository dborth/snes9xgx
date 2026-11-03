/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2009-2026
 * OgcInputDriver.cpp
 *
 * GameCube controller management and normalization, and the per-frame
 * machinery shared with the Wii driver (see WiiInputDriver)
 ***************************************************************************/

#include <stdint.h>
#include <cmath>
#include <ogcsys.h>
#include <ogc/pad.h>

#include "OgcInputDriver.h"
#include "OgcInputUtil.h"
#include "../Platform.h"
#include "../InputController.h"

OgcInputDriver::OgcInputDriver() {
	for (int i = 0; i < 4; i++) {
		rumbleRequest[i] = false;
		menuRumbleFrames[i] = 0;
		menuRumbleGapFrames[i] = 0;
	}
}

OgcInputDriver::~OgcInputDriver() {
	shutdown();
}

void OgcInputDriver::init() {
	PAD_Init();
	initHardware();
	InitUserInputControllers();
}

void OgcInputDriver::stopAllRumble() {
	for (int i = 0; i < 4; i++) {
		driveRumble(i, false);
		PAD_ControlMotor(i, PAD_MOTOR_STOP);
		rumbleRequest[i] = false;
		menuRumbleFrames[i] = 0;
		menuRumbleGapFrames[i] = 0;
	}
}

void OgcInputDriver::shutdown() {
	stopAllRumble();
}

void OgcInputDriver::setRumble(int channel, bool rumble) {
	if (channel >= 0 && channel < 4) {
		rumbleRequest[channel] = rumble;
	}
}

/****************************************************************************
 * Hardware Mapping Helpers
 * Translates raw libogc hardware bits to our generic UI masks
 ***************************************************************************/
static uint32_t MapPADToGeneric(uint32_t pad_btns) {
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
	if (pad_btns & PAD_TRIGGER_L)     mask |= INPUT_TRIGGER_L;
	if (pad_btns & PAD_TRIGGER_R)     mask |= INPUT_TRIGGER_R;
	if (pad_btns & PAD_TRIGGER_Z)     mask |= INPUT_TRIGGER_ZR;
	return mask;
}

void OgcInputDriver::update() {
	scanHardware();

	u32 activeGamecubePads = PAD_ScanPads();
	bool allowRumble = isRumbleEnabled() && isSystemRumbleAllowed();

	for(int i = 3; i >= 0; i--) {
		InputPadData padData;

		// GameCube controller (real pads only: USB controllers have profiles of their own)
		bool gamecubeActive = (activeGamecubePads & (1 << i)) != 0;

		if(gamecubeActive) {
			padData.hw_connected[INPUT_HW_GAMECUBE] = true;
			padData.hw_buttons_d[INPUT_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsDown(i));
			padData.hw_buttons_h[INPUT_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsHeld(i));
			padData.hw_buttons_r[INPUT_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsUp(i));
			padData.hw_stickX[INPUT_HW_GAMECUBE] = OgcStickToFloat(PAD_StickX(i));
			padData.hw_stickY[INPUT_HW_GAMECUBE] = OgcStickToFloat(PAD_StickY(i));
			padData.hw_substickX[INPUT_HW_GAMECUBE] = OgcStickToFloat(PAD_SubStickX(i));
			padData.hw_substickY[INPUT_HW_GAMECUBE] = OgcStickToFloat(PAD_SubStickY(i));
		}

		// Whatever else this console has (Wiimotes, USB controllers, ...)
		readChannel(i, padData);

		// USB mouse, on channel 0: its own hardware profile for the buttons, and the pointer like the
		// Wiimote IR's while it is being used. A platform without a mouse reports not connected.
		if (i == 0)
			mousePointer.apply(platform->getMouse(), platform->getVideo()->getDeltaTime(),
				platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight(), padData);

		// Merge into unified aggregate state for UI Elements
		for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++) {
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
		controller[i]->update(padData, platform->getVideo()->getDeltaTime());
		
		// Menu (hover) rumble: a short tick with an enforced silent gap afterward
		static constexpr int kMenuRumbleOnFrames = 2; // ~33ms motor-on burst
		static constexpr int kMenuRumbleGapFrames = 6; // ~100ms enforced silence after a tick

		if (rumbleRequest[i]) {
			rumbleRequest[i] = false;
			if (menuRumbleFrames[i] == 0 && menuRumbleGapFrames[i] == 0) {
				menuRumbleFrames[i] = kMenuRumbleOnFrames;
			}
		}

		bool wantRumble = menuRumbleFrames[i] > 0;
		bool doRumble = wantRumble && allowRumble;

		if (menuRumbleFrames[i] > 0) {
			menuRumbleFrames[i]--;
			if (menuRumbleFrames[i] == 0) menuRumbleGapFrames[i] = kMenuRumbleGapFrames;
		} else if (menuRumbleGapFrames[i] > 0) {
			menuRumbleGapFrames[i]--;
		}

		driveRumble(i, doRumble);

		if (gamecubeActive || !doRumble) {
			PAD_ControlMotor(i, doRumble ? PAD_MOTOR_RUMBLE : PAD_MOTOR_STOP);
		}
	}
}
