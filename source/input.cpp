/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * input.cpp
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogcsys.h>
#include <unistd.h>

#include "snes9x/port.h"
#include "snes9xgx.h"
#include "button_mapping.h"
#include "menu.h"
#include "video.h"
#include "input.h"
#include "libgui/Gui.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/controls.h"

#ifdef HW_RVL
#include "drivers/ogc/input/retrode.h"
#include "drivers/ogc/input/xbox360.h"
#include "drivers/ogc/input/hornet.h"
#include "drivers/ogc/input/mayflash.h"
#endif

#define ANALOG_SENSITIVITY 30

int playerMapping[4] = {0,1,2,3};

// hold superscope/mouse/justifier cursor positions
static int cursor_x[5] = {0,0,0,0,0};
static int cursor_y[5] = {0,0,0,0,0};

/****************************************************************************
 * Controller Functions
 *
 * The following map the Wii controls to the Snes9x controller system
 ***************************************************************************/
#define ASSIGN_BUTTON_TRUE( keycode, snescmd ) \
	  S9xMapButton( keycode, cmd = S9xGetCommandT(snescmd), true)

#define ASSIGN_BUTTON_FALSE( keycode, snescmd ) \
	  S9xMapButton( keycode, cmd = S9xGetCommandT(snescmd), false)

static int scopeTurbo = 0; // tracks whether superscope turbo is on or off
uint32_t btnmap[CTRL_BTN_MAPPINGS][GUI_HW_MAX][MAXJP]; // button mapping

void ResetControls(int consoleCtrl, int wiiCtrl)
{
	int i;
	/*** Gamecube controller Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_GAMECUBE))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_X;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_Y;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_TRIGGER_L;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_TRIGGER_R;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_TRIGGER_ZR; // Z button
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = INPUT_BTN_RIGHT;
	}

	/*** Wiimote Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_WIIMOTE))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_2;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_1;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_NONE;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_NONE;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_RIGHT;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = INPUT_BTN_DOWN;
	}

	/*** Classic Controller Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_CLASSIC))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_X;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_Y;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_TRIGGER_L;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_TRIGGER_R;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = INPUT_BTN_RIGHT;
	}

	/*** Wii U Pro Controller / Gamepad (DRC) ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && (wiiCtrl == GUI_HW_WUPC || wiiCtrl == GUI_HW_DRC)))
	{
        int hw = (wiiCtrl == GUI_HW_WUPC) ? GUI_HW_WUPC : GUI_HW_DRC;
		i=0;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_A;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_B;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_X;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_Y;
		btnmap[CTRL_PAD][hw][i++] = INPUT_TRIGGER_L;
		btnmap[CTRL_PAD][hw][i++] = INPUT_TRIGGER_R;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_PLUS;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_UP;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_DOWN;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_LEFT;
		btnmap[CTRL_PAD][hw][i++] = INPUT_BTN_RIGHT;
	}
		
	/*** Nunchuk + Wiimote Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_NUNCHUK))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_TRIGGER_L;  // C mapped to L
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_TRIGGER_ZL; // Z mapped to ZL
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_2;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_1;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = INPUT_BTN_RIGHT;
	}

	/*** Superscope (Map identical to generic UI masks) ***/
	if (consoleCtrl == -1 || consoleCtrl == CTRL_SCOPE) {
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_TRIGGER_ZR;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = INPUT_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_UP;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_DOWN;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = INPUT_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = INPUT_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = INPUT_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = INPUT_BTN_PLUS;
	}

	/*** Mouse & Justifier Mapping (Simplified identically to masks) ***/
    if (consoleCtrl == -1 || consoleCtrl == CTRL_MOUSE) {
        btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE][0] = INPUT_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE][1] = INPUT_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE][0] = INPUT_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE][1] = INPUT_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_CLASSIC][0] = INPUT_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_CLASSIC][1] = INPUT_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_WUPC][0] = INPUT_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_WUPC][1] = INPUT_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_DRC][0] = INPUT_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_DRC][1] = INPUT_BTN_B;
    }

    if (consoleCtrl == -1 || consoleCtrl == CTRL_JUST) {
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][0] = INPUT_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][1] = INPUT_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][2] = INPUT_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][0] = INPUT_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][1] = INPUT_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][2] = INPUT_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][0] = INPUT_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][1] = INPUT_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][2] = INPUT_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_WUPC][0] = INPUT_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_WUPC][1] = INPUT_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_WUPC][2] = INPUT_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_DRC][0] = INPUT_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_DRC][1] = INPUT_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_DRC][2] = INPUT_BTN_PLUS;
    }
}

/****************************************************************************
 * UpdateCursorPosition
 *
 * Updates X/Y coordinates for Superscope/mouse/justifier position
 ***************************************************************************/
static void UpdateCursorPosition(int chan, int &pos_x, int &pos_y)
{
	if (!controller[chan]) return;
	const InputPadData& pad = controller[chan]->getPadData();

	if (pad.validPointer)
	{
		pos_x = (int)((pad.cursor_x * 256.0f) / 640.0f);
		pos_y = (int)((pad.cursor_y * 224.0f) / 480.0f);
	}
	else
	{
		float sensitivity = (float)ANALOG_SENSITIVITY / 128.0f;
		if (std::abs(pad.stickX) > sensitivity) pos_x += (int)(pad.stickX * 6.4f);
		if (std::abs(pad.stickY) > sensitivity) pos_y -= (int)(pad.stickY * 6.4f);
	}

	if (pos_x > 256) pos_x = 256;
	if (pos_x < 0) pos_x = 0;
	if (pos_y > 224) pos_y = 224;
	if (pos_y < 0) pos_y = 0;
}

/****************************************************************************
 * decodepad
 *
 * Reads the changes (buttons pressed, etc) from a controller and reports
 * these changes to Snes9x
 ***************************************************************************/
static void decodepad (int chan, int emuChan)
{
	if (!controller[chan]) return;
	const InputPadData& pad = controller[chan]->getPadData();
	int i, offset;

	float sensitivity = (float)ANALOG_SENSITIVITY / 128.0f;

	// Inject virtual buttons translated from analog sticks
	uint32_t virtual_jp = 0;
	if (pad.stickY > sensitivity) virtual_jp |= INPUT_BTN_UP;
	else if (pad.stickY < -sensitivity) virtual_jp |= INPUT_BTN_DOWN;
	if (pad.stickX < -sensitivity) virtual_jp |= INPUT_BTN_LEFT;
	else if (pad.stickX > sensitivity) virtual_jp |= INPUT_BTN_RIGHT;

	if (GCSettings.MapABXYRightStick)
	{
		if (pad.substickY > sensitivity) virtual_jp |= INPUT_BTN_X;
		else if (pad.substickY < -sensitivity) virtual_jp |= INPUT_BTN_B;
		if (pad.substickX < -sensitivity) virtual_jp |= INPUT_BTN_Y;
		else if (pad.substickX > sensitivity) virtual_jp |= INPUT_BTN_A;
	}

	offset = ((emuChan + 1) << 4);

	/*** Report pressed buttons (gamepads) ***/
	for (i = 0; i < 12; i++)
	{
		bool button_pressed = false;

		// Check if ANY connected hardware matches the mapping
		for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++)
		{
			if (!pad.hw_connected[hw]) continue;
			uint32_t mapped_btn = btnmap[CTRL_PAD][hw][i];

			if ((pad.hw_buttons_h[hw] & mapped_btn) || (virtual_jp & mapped_btn)) {
				button_pressed = true;
				break;
			}
		}

		S9xReportButton(offset + i, button_pressed);
	}

	/*** Superscope ***/
	if (Settings.SuperScopeMaster && emuChan == 0) // report only once
	{
		offset = 0x50;
		for (i = 0; i < 6; i++)
		{
			bool button_pressed = false;
			for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++) {
				if (!pad.hw_connected[hw]) continue;
				if (pad.hw_buttons_h[hw] & btnmap[CTRL_SCOPE][hw][i]) {
					button_pressed = true;
					break;
				}
			}

			if (button_pressed)
			{
				if(i == 3 || i == 4) // turbo
				{
					if((i == 3 && scopeTurbo == 1) || (i == 4 && scopeTurbo == 0)) {
						S9xReportButton(offset + i, false);
					} else {
						scopeTurbo = 4-i;
						S9xReportButton(offset + i, true);
					}
				}
				else S9xReportButton(offset + i, true);
			}
			else S9xReportButton(offset + i, false);
		}
		offset = 0x80;
		UpdateCursorPosition(emuChan, cursor_x[0], cursor_y[0]);
		S9xReportPointer(offset, (uint16_t) cursor_x[0], (uint16_t) cursor_y[0]);
	}
	/*** Mouse ***/
	else if (Settings.MouseMaster && emuChan < 2)
	{
		offset = 0x60 + (2 * emuChan);
		for (i = 0; i < 2; i++)
		{
			bool button_pressed = false;
			for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++) {
				if (!pad.hw_connected[hw]) continue;
				if (pad.hw_buttons_h[hw] & btnmap[CTRL_MOUSE][hw][i]) {
					button_pressed = true; break;
				}
			}
			S9xReportButton(offset + i, button_pressed);
		}
		offset = 0x81;
		UpdateCursorPosition(emuChan, cursor_x[1 + emuChan], cursor_y[1 + emuChan]);
		S9xReportPointer(offset + emuChan, (uint16_t) cursor_x[1 + emuChan], (uint16_t) cursor_y[1 + emuChan]);
	}
	/*** Justifier ***/
	else if (Settings.JustifierMaster && emuChan < 2)
	{
		offset = 0x70 + (3 * emuChan);
		for (i = 0; i < 3; i++)
		{
			bool button_pressed = false;
			for (uint32_t hw = 0; hw < GUI_HW_MAX; hw++) {
				if (!pad.hw_connected[hw]) continue;
				if (pad.hw_buttons_h[hw] & btnmap[CTRL_JUST][hw][i]) {
					button_pressed = true; break;
				}
			}
			S9xReportButton(offset + i, button_pressed);
		}
		offset = 0x83;
		UpdateCursorPosition(emuChan, cursor_x[3 + emuChan], cursor_y[3 + emuChan]);
		S9xReportPointer(offset + emuChan, (uint16_t) cursor_x[3 + emuChan], (uint16_t) cursor_y[3 + emuChan]);
	}

#ifdef HW_RVL
	// screenshot (temp)
	S9xReportButton(0x90, (pad.buttons_h & INPUT_TRIGGER_ZR) != 0);
#endif
}

bool isMenuRequested()
{
	for(int i=0; i<4; i++)
	{
		if (!controller[i]) continue;
		const InputPadData& pad = controller[i]->getPadData();

		bool rightStickLeft = (pad.substickX < -0.55f);
		bool homePressed = (pad.buttons_h & INPUT_BTN_HOME);
		bool lPlusRPlusStart = (pad.buttons_h & INPUT_TRIGGER_L) && (pad.buttons_h & INPUT_TRIGGER_R) && (pad.buttons_h & INPUT_BTN_PLUS);
		bool oneTwoPlus = (pad.buttons_h & INPUT_BTN_1) && (pad.buttons_h & INPUT_BTN_2) && (pad.buttons_h & INPUT_BTN_PLUS);

		if (GCSettings.GamepadMenuToggle == GAMEPAD_MENU_TOGGLE_HOME_RIGHTSTICK)
		{
			if (rightStickLeft || homePressed) return true;
		}
		else if (GCSettings.GamepadMenuToggle == GAMEPAD_MENU_TOGGLE_LRSTART_12PLUS)
		{
			if (lPlusRPlusStart || oneTwoPlus) return true;
		}
		else // All toggle options enabled
		{
			if (rightStickLeft || homePressed || lPlusRPlusStart || oneTwoPlus) return true;
		}
	}
	return false;
}

bool IsTurboModeInputPressed()
{
	if (!controller[0]) return false;
	const InputPadData& pad = controller[0]->getPadData();

	switch(GCSettings.TurboModeButton)
	{
		case TURBO_BUTTON_RSTICK:
			return (pad.substickX > 0.55f);
		case TURBO_BUTTON_A:
			return (pad.buttons_h & INPUT_BTN_A);
		case TURBO_BUTTON_B:
			return (pad.buttons_h & INPUT_BTN_B);
		case TURBO_BUTTON_X:
			return (pad.buttons_h & INPUT_BTN_X);
		case TURBO_BUTTON_Y:
			return (pad.buttons_h & INPUT_BTN_Y);
		case TURBO_BUTTON_L:
			return (pad.buttons_h & INPUT_TRIGGER_L);
		case TURBO_BUTTON_R:
			return (pad.buttons_h & INPUT_TRIGGER_R);
		case TURBO_BUTTON_ZL:
			return (pad.buttons_h & INPUT_TRIGGER_ZL);
		case TURBO_BUTTON_ZR:
			return (pad.buttons_h & INPUT_TRIGGER_ZR);
		case TURBO_BUTTON_Z: // GC Z fallback
			return (pad.buttons_h & INPUT_TRIGGER_ZL);
		case TURBO_BUTTON_C: // Nunchuk C fallback
			return (pad.buttons_h & INPUT_TRIGGER_L);
		case TURBO_BUTTON_1:
			return (pad.buttons_h & INPUT_BTN_1);
		case TURBO_BUTTON_2:
			return (pad.buttons_h & INPUT_BTN_2);
		case TURBO_BUTTON_PLUS:
			return (pad.buttons_h & INPUT_BTN_PLUS);
		case TURBO_BUTTON_MINUS:
			return (pad.buttons_h & INPUT_BTN_MINUS);
		default:
			return false;
	}
}

static bool buttonsReported = false;

void ClearButtonsReported () {
	buttonsReported = false;
}

/****************************************************************************
 * ReportButtons
 *
 * Called on each rendered frame
 * Our way of putting controller input into Snes9x
 ***************************************************************************/
void ReportButtons ()
{
	if(buttonsReported)
		return;

	buttonsReported = true;

	int i;

	platform->getInput()->update();

	if (GCSettings.TurboModeEnabled)
	{
		Settings.TurboMode = IsTurboModeInputPressed();
	}
	
	if(Settings.TurboMode) {
		Settings.SoundSync = false;
	}
	else {
		Settings.SoundSync = true;
	}

	/* Check for menu:
	 * CStick left
	 * OR "L+R+START" (eg. Homebrew/Adapted SNES controllers)
	 * OR "Home" on the wiimote or classic controller
	 * OR Left on classic right analog stick
	 */
	if(isMenuRequested())
		MenuRequested = true; // go to the menu

	int numControllers = (Settings.MultiPlayer5Master == true ? 4 : 2);

	for (i = 0; i < 4; i++) {
		if(playerMapping[i] < numControllers) {
			decodepad (i, playerMapping[i]);
		}
	}
}

void SetControllers()
{
	if (Settings.MultiPlayer5Master == true)
	{
		S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
		S9xSetController (1, CTL_MP5, 1, 2, 3, -1);
	}
	else if (Settings.SuperScopeMaster == true)
	{
		S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
		S9xSetController (1, CTL_SUPERSCOPE, 0, 0, 0, 0);
	}
	else if (Settings.MouseMaster == true)
	{
		if (GCSettings.Controller == CTRL_MOUSE)
		{
			S9xSetController (0, CTL_MOUSE, 0, 0, 0, 0);
			S9xSetController (1, CTL_JOYPAD, 1, 0, 0, 0);
		}
		else if (GCSettings.Controller == CTRL_MOUSE_PORT2)
		{
			S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
			S9xSetController (1, CTL_MOUSE, 1, 0, 0, 0);
		}
		else if (GCSettings.Controller == CTRL_MOUSE_BOTH_PORTS)
		{
			S9xSetController (0, CTL_MOUSE, 0, 0, 0, 0);
			S9xSetController (1, CTL_MOUSE, 1, 0, 0, 0);
		}	
	}
	else if (Settings.JustifierMaster == true)
	{
		S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
		S9xSetController(1, CTL_JUSTIFIER, 1, 0, 0, 0);
	}
	else
	{
		// Plugin 2 Joypads by default
		S9xSetController (0, CTL_JOYPAD, 0, 0, 0, 0);
		S9xSetController (1, CTL_JOYPAD, 1, 0, 0, 0);
	}
}

/****************************************************************************
 * Set the default mapping
 ***************************************************************************/
void SetDefaultButtonMap ()
{
	int maxcode = 0x10;
	s9xcommand_t cmd;

	/*** Joypad 1 ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 A");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 B");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 X");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Y");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 R");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Start");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Select");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Up");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Down");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Left");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad1 Right");

	maxcode = 0x20;
	/*** Joypad 2 ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 A");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 B");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 X");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Y");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 R");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Start");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Select");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Up");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Down");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Left");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad2 Right");

	maxcode = 0x30;
	/*** Joypad 3 ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 A");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 B");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 X");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Y");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 R");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Start");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Select");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Up");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Down");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Left");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad3 Right");

	maxcode = 0x40;
	/*** Joypad 4 ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 A");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 B");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 X");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Y");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 R");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Start");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Select");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Up");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Down");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Left");
	ASSIGN_BUTTON_FALSE (maxcode++, "Joypad4 Right");

	maxcode = 0x50;
	/*** Superscope ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Fire");
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope AimOffscreen");
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Cursor");
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope ToggleTurbo");
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope ToggleTurbo");
	ASSIGN_BUTTON_FALSE (maxcode++, "Superscope Pause");

	maxcode = 0x60;
	/*** Mouse ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Mouse1 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Mouse1 R");
	ASSIGN_BUTTON_FALSE (maxcode++, "Mouse2 L");
	ASSIGN_BUTTON_FALSE (maxcode++, "Mouse2 R");

	maxcode = 0x70;
	/*** Justifier ***/
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier1 Trigger");
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier1 AimOffscreen");
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier1 Start");
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier2 Trigger");
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier2 AimOffscreen");
	ASSIGN_BUTTON_FALSE (maxcode++, "Justifier2 Start");

	maxcode = 0x80;
	S9xMapPointer(maxcode++, S9xGetCommandT("Pointer Superscope"), false);
	S9xMapPointer(maxcode++, S9xGetCommandT("Pointer Mouse1"), false);
	S9xMapPointer(maxcode++, S9xGetCommandT("Pointer Mouse2"), false);
	S9xMapPointer(maxcode++, S9xGetCommandT("Pointer Justifier1"), false);
	S9xMapPointer(maxcode++, S9xGetCommandT("Pointer Justifier2"), false);

	maxcode = 0x90;
	//ASSIGN_BUTTON_FALSE (maxcode++, "Screenshot");

	SetControllers();
}

#ifdef HW_RVL
char* GetUSBControllerInfo()
{
    static char info[100];
    snprintf(info, 100, "Retrode: %s, XBOX360: %s, Hornet: %s, Mayflash: %s", Retrode_Status(), XBOX360_Status(), Hornet_Status(), Mayflash_Status());
    return info;
}
#endif
