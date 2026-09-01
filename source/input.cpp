/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * input.cpp
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogcsys.h>
#include <unistd.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include <ogc/lwp_watchdog.h>

#include "snes9x/port.h"
#include "snes9xgx.h"
#include "button_mapping.h"
#include "menu.h"
#include "video.h"
#include "input.h"
#include "libgui/Gui.h"

#include "drivers/ogc/wiidrc.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/controls.h"

#ifdef HW_RVL
#include "utils/retrode.h"
#include "utils/xbox360.h"
#include "utils/hornet.h"
#include "utils/mayflash.h"
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
uint32_t btnmap[4][6][12]; // button mapping

void ResetControls(int consoleCtrl, int wiiCtrl)
{
	int i;
	/*** Gamecube controller Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_GAMECUBE))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_X;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_Y;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_L;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_R;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_ZR; // Z button
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_GAMECUBE][i++] = GUI_BTN_RIGHT;
	}

	/*** Wiimote Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_WIIMOTE))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_2;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_1;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_NONE;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_NONE;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_RIGHT;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_WIIMOTE][i++] = GUI_BTN_DOWN;
	}

	/*** Classic Controller Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_CLASSIC))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_X;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_Y;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_TRIGGER_L;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_TRIGGER_R;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_CLASSIC][i++] = GUI_BTN_RIGHT;
	}

	/*** Wii U Pro Controller / Gamepad (DRC) ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && (wiiCtrl == GUI_HW_WUPC || wiiCtrl == GUI_HW_DRC)))
	{
        int hw = (wiiCtrl == GUI_HW_WUPC) ? GUI_HW_WUPC : GUI_HW_DRC;
		i=0;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_A;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_B;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_X;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_Y;
		btnmap[CTRL_PAD][hw][i++] = GUI_TRIGGER_L;
		btnmap[CTRL_PAD][hw][i++] = GUI_TRIGGER_R;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_PLUS;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_UP;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_DOWN;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_LEFT;
		btnmap[CTRL_PAD][hw][i++] = GUI_BTN_RIGHT;
	}
		
	/*** Nunchuk + Wiimote Padmap ***/
	if(consoleCtrl == -1 || (consoleCtrl == CTRL_PAD && wiiCtrl == GUI_HW_NUNCHUK))
	{
		i=0;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_A;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_B;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_L;  // C mapped to L
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_TRIGGER_ZL; // Z mapped to ZL
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_2;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_1;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_PLUS;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_UP;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_DOWN;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_LEFT;
		btnmap[CTRL_PAD][GUI_HW_NUNCHUK][i++] = GUI_BTN_RIGHT;
	}

	/*** Superscope (Map identical to generic UI masks) ***/
	if (consoleCtrl == -1 || consoleCtrl == CTRL_SCOPE) {
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_TRIGGER_ZR;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_GAMECUBE][i++] = GUI_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_UP;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_DOWN;
		btnmap[CTRL_SCOPE][GUI_HW_WIIMOTE][i++] = GUI_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_CLASSIC][i++] = GUI_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_WUPC][i++] = GUI_BTN_PLUS;
		i=0;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_B;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_A;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_MINUS;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_Y;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_X;
		btnmap[CTRL_SCOPE][GUI_HW_DRC][i++] = GUI_BTN_PLUS;
	}

	/*** Mouse & Justifier Mapping (Simplified identically to masks) ***/
    if (consoleCtrl == -1 || consoleCtrl == CTRL_MOUSE) {
        btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE][0] = GUI_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_GAMECUBE][1] = GUI_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE][0] = GUI_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_WIIMOTE][1] = GUI_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_CLASSIC][0] = GUI_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_CLASSIC][1] = GUI_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_WUPC][0] = GUI_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_WUPC][1] = GUI_BTN_B;
        btnmap[CTRL_MOUSE][GUI_HW_DRC][0] = GUI_BTN_A;
        btnmap[CTRL_MOUSE][GUI_HW_DRC][1] = GUI_BTN_B;
    }

    if (consoleCtrl == -1 || consoleCtrl == CTRL_JUST) {
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][0] = GUI_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][1] = GUI_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_GAMECUBE][2] = GUI_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][0] = GUI_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][1] = GUI_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_WIIMOTE][2] = GUI_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][0] = GUI_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][1] = GUI_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_CLASSIC][2] = GUI_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_WUPC][0] = GUI_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_WUPC][1] = GUI_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_WUPC][2] = GUI_BTN_PLUS;
        btnmap[CTRL_JUST][GUI_HW_DRC][0] = GUI_BTN_B;
        btnmap[CTRL_JUST][GUI_HW_DRC][1] = GUI_BTN_A;
        btnmap[CTRL_JUST][GUI_HW_DRC][2] = GUI_BTN_PLUS;
    }
}

static inline float clampf(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

/****************************************************************************
 * Hardware Mapping Helpers
 * Translates raw libogc hardware bits to our generic UI masks
 ***************************************************************************/
static uint32_t MapPADToGeneric(uint32_t pad_btns)
{
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

static uint32_t MapWiiUGamepadToGeneric(uint32_t drc_btns)
{
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
static float NormalizeWPADAnalog(int pos, int min, int max, int center)
{
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

/****************************************************************************
 * UpdatePads
 * Scans all controllers, combines states, and updates controllers
 ***************************************************************************/
void UpdatePads()
{
	#ifdef HW_RVL
	WiiDRC_ScanPads();
	Retrode_ScanPads();
	XBOX360_ScanPads();
	Hornet_ScanPads();
	Mayflash_ScanPads();
	WPAD_ScanPads();

	bool retrodeActive  = (Retrode_Status()[0]  == 'c');
	bool xboxActive     = (XBOX360_Status()[0]  == 'c');
	bool hornetActive   = (Hornet_Status()[0]   == 'c');
	bool mayflashActive = (Mayflash_Status()[0] == 'c');
	#endif

	uint32_t activeGamecubePads = PAD_ScanPads();

	float deltaTime = 1.0f / 60.0f;

	for(int i = 3; i >= 0; i--) {
		GuiInputPadData padData;

		// Process GameCube Controller & Third Party USB Adaptors
		bool gamecubeActive = (activeGamecubePads & (1 << i)) != 0;

		#ifdef HW_RVL
		if (retrodeActive) gamecubeActive = true;
		if (xboxActive) gamecubeActive = true;
		if (hornetActive && i == 0) gamecubeActive = true;
		if (mayflashActive && i < 2)  gamecubeActive = true;
		#endif

		if(gamecubeActive) {
			uint32_t padHeld = PAD_ButtonsHeld(i);
			#ifdef HW_RVL
			// Inject USB controllers into GameCube held state (since they emulate GC bitmasks)
			padHeld |= Retrode_ButtonsHeld(i) | XBOX360_ButtonsHeld(i) | Hornet_ButtonsHeld(i) | Mayflash_ButtonsHeld(i);
			#endif

			padData.hw_connected[GUI_HW_GAMECUBE] = true;
			padData.hw_buttons_d[GUI_HW_GAMECUBE] = MapPADToGeneric(PAD_ButtonsDown(i));
			padData.hw_buttons_h[GUI_HW_GAMECUBE] = MapPADToGeneric(padHeld);
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

		// 4. Merge into unified aggregate state for UI Elements
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
		}

		// Push the finalized, merged payload to the controller abstraction
		userInput[i]->update(padData, deltaTime);
	}
}

/****************************************************************************
 * SetupPads
 * Allocates controllers and initializes hardware
 ***************************************************************************/
static bool soundSync = false;

void SetupPads()
{
	soundSync = Settings.SoundSync;
	PAD_Init();

	#ifdef HW_RVL
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	#endif

	for(int i = 0; i < 4; i++) {
		userInput[i] = new GuiInputController(i);
	}
}

/****************************************************************************
 * UpdateCursorPosition
 *
 * Updates X/Y coordinates for Superscope/mouse/justifier position
 ***************************************************************************/
static void UpdateCursorPosition(int chan, int &pos_x, int &pos_y)
{
	if (!userInput[chan]) return;
	const GuiInputPadData& pad = userInput[chan]->getPadData();

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
	if (!userInput[chan]) return;
	const GuiInputPadData& pad = userInput[chan]->getPadData();
	int i, offset;

	float sensitivity = (float)ANALOG_SENSITIVITY / 128.0f;

	// Inject virtual buttons translated from analog sticks
	uint32_t virtual_jp = 0;
	if (pad.stickY > sensitivity) virtual_jp |= GUI_BTN_UP;
	else if (pad.stickY < -sensitivity) virtual_jp |= GUI_BTN_DOWN;
	if (pad.stickX < -sensitivity) virtual_jp |= GUI_BTN_LEFT;
	else if (pad.stickX > sensitivity) virtual_jp |= GUI_BTN_RIGHT;

	if (GCSettings.MapABXYRightStick)
	{
		if (pad.substickY > sensitivity) virtual_jp |= GUI_BTN_X;
		else if (pad.substickY < -sensitivity) virtual_jp |= GUI_BTN_B;
		if (pad.substickX < -sensitivity) virtual_jp |= GUI_BTN_Y;
		else if (pad.substickX > sensitivity) virtual_jp |= GUI_BTN_A;
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
	S9xReportButton(0x90, (pad.buttons_h & GUI_TRIGGER_ZR) != 0);
#endif
}

bool isMenuRequested()
{
	for(int i=0; i<4; i++)
	{
		if (!userInput[i]) continue;
		const GuiInputPadData& pad = userInput[i]->getPadData();

		bool rightStickLeft = (pad.substickX < -0.55f);
		bool homePressed = (pad.buttons_h & GUI_BTN_HOME);
		bool lPlusRPlusStart = (pad.buttons_h & GUI_TRIGGER_L) && (pad.buttons_h & GUI_TRIGGER_R) && (pad.buttons_h & GUI_BTN_PLUS);
		bool oneTwoPlus = (pad.buttons_h & GUI_BTN_1) && (pad.buttons_h & GUI_BTN_2) && (pad.buttons_h & GUI_BTN_PLUS);

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
	if (!userInput[0]) return false;
	const GuiInputPadData& pad = userInput[0]->getPadData();

	switch(GCSettings.TurboModeButton)
	{
		case TURBO_BUTTON_RSTICK:
			return (pad.substickX > 0.55f);
		case TURBO_BUTTON_A:
			return (pad.buttons_h & GUI_BTN_A);
		case TURBO_BUTTON_B:
			return (pad.buttons_h & GUI_BTN_B);
		case TURBO_BUTTON_X:
			return (pad.buttons_h & GUI_BTN_X);
		case TURBO_BUTTON_Y:
			return (pad.buttons_h & GUI_BTN_Y);
		case TURBO_BUTTON_L:
			return (pad.buttons_h & GUI_TRIGGER_L);
		case TURBO_BUTTON_R:
			return (pad.buttons_h & GUI_TRIGGER_R);
		case TURBO_BUTTON_ZL:
			return (pad.buttons_h & GUI_TRIGGER_ZL);
		case TURBO_BUTTON_ZR:
			return (pad.buttons_h & GUI_TRIGGER_ZR);
		case TURBO_BUTTON_Z: // GC Z fallback
			return (pad.buttons_h & GUI_TRIGGER_ZL);
		case TURBO_BUTTON_C: // Nunchuk C fallback
			return (pad.buttons_h & GUI_TRIGGER_L);
		case TURBO_BUTTON_1:
			return (pad.buttons_h & GUI_BTN_1);
		case TURBO_BUTTON_2:
			return (pad.buttons_h & GUI_BTN_2);
		case TURBO_BUTTON_PLUS:
			return (pad.buttons_h & GUI_BTN_PLUS);
		case TURBO_BUTTON_MINUS:
			return (pad.buttons_h & GUI_BTN_MINUS);
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

	UpdatePads();

	if (GCSettings.TurboModeEnabled)
	{
		Settings.TurboMode = IsTurboModeInputPressed();
	}
	
	if(Settings.TurboMode) {
		Settings.SoundSync = false;
	}
	else {
		Settings.SoundSync = soundSync;
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
