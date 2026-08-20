/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * michniewski August 2008
 * Tantric 2008-2023
 *
 * button_mapping.cpp
 *
 * Controller button mapping
 ***************************************************************************/

#include "libgui/Gui.h"
#include "button_mapping.h"

/****************************************************************************
 * Controller Button Descriptions:
 * used for identifying which buttons have been pressed when configuring
 * and for displaying the name of said button
 ***************************************************************************/

CtrlrMap ctrlr_def[6] = {
// Gamecube controller btn def
{
	GUI_HW_GAMECUBE,
	13,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_X, "X"},
		{GUI_BTN_Y, "Y"},
		{GUI_BTN_PLUS, "START"},
		{GUI_BTN_PLUS, "START"},
		{GUI_TRIGGER_L, "L"},
		{GUI_TRIGGER_R, "R"},
		{GUI_TRIGGER_ZL, "Z"},
		{GUI_BTN_NONE, ""},
		{GUI_BTN_NONE, ""}
	}
},
// Wiimote btn def
{
	GUI_HW_WIIMOTE,
	11,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_1, "1"},
		{GUI_BTN_2, "2"},
		{GUI_BTN_PLUS, "PLUS"},
		{GUI_BTN_MINUS, "MINUS"},
		{GUI_BTN_HOME, "HOME"},
		{GUI_BTN_NONE, ""},
		{GUI_BTN_NONE, ""},
		{GUI_BTN_NONE, ""},
		{GUI_BTN_NONE, ""}
	}
},
// Nunchuk btn def
{
	GUI_HW_NUNCHUK,
	13,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_1, "1"},
		{GUI_BTN_2, "2"},
		{GUI_BTN_PLUS, "PLUS"},
		{GUI_BTN_MINUS, "MINUS"},
		{GUI_BTN_HOME, "HOME"},
		{GUI_TRIGGER_ZL, "Z"},
		{GUI_TRIGGER_L, "C"},
		{GUI_BTN_NONE, ""},
		{GUI_BTN_NONE, ""}
	}
},
// Classic btn def
{
	GUI_HW_CLASSIC,
	15,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_X, "X"},
		{GUI_BTN_Y, "Y"},
		{GUI_BTN_PLUS, "PLUS"},
		{GUI_BTN_MINUS, "MINUS"},
		{GUI_BTN_HOME, "HOME"},
		{GUI_TRIGGER_L, "L TRIG"},
		{GUI_TRIGGER_R, "R TRIG"},
		{GUI_TRIGGER_ZL, "ZL"},
		{GUI_TRIGGER_ZR, "ZR"}
	}
},
// Wii U Pro controller
{
	GUI_HW_WUPC,
	15,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_X, "X"},
		{GUI_BTN_Y, "Y"},
		{GUI_BTN_PLUS, "PLUS"},
		{GUI_BTN_MINUS, "MINUS"},
		{GUI_BTN_HOME, "HOME"},
		{GUI_TRIGGER_L, "L"},
		{GUI_TRIGGER_R, "R"},
		{GUI_TRIGGER_ZL, "ZL"},
		{GUI_TRIGGER_ZR, "ZR"}
	}
},
// Wii U Gamepad btn def
{
	GUI_HW_DRC,
	15,
	{
		{GUI_BTN_DOWN, "DOWN"},
		{GUI_BTN_UP, "UP"},
		{GUI_BTN_LEFT, "LEFT"},
		{GUI_BTN_RIGHT, "RIGHT"},
		{GUI_BTN_A, "A"},
		{GUI_BTN_B, "B"},
		{GUI_BTN_X, "X"},
		{GUI_BTN_Y, "Y"},
		{GUI_BTN_PLUS, "PLUS"},
		{GUI_BTN_MINUS, "MINUS"},
		{GUI_BTN_HOME, "HOME"},
		{GUI_TRIGGER_L, "L"},
		{GUI_TRIGGER_R, "R"},
		{GUI_TRIGGER_ZL, "ZL"},
		{GUI_TRIGGER_ZR, "ZR"}
	}
}
};

