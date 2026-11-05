/****************************************************************************
 * Snes9x GX
 *
 * michniewski August 2008
 * Daryl Borth 2008-2026
 *
 * button_mapping.h
 *
 * Controller button mapping
 ***************************************************************************/

#ifndef BTN_MAP_H
#define BTN_MAP_H

#include "drivers/InputData.h"

// indexed by GuiHardwareProfile. The size is deduced so the check below catches a profile without a name.
const char ctrlrName[][32] =
{ "GameCube Controller", "Wiimote", "Nunchuk + Wiimote", "Classic Controller", "Wii U Pro Controller", "Wii U Gamepad", "DualShock 4", "Xbox 360 Controller", "USB Controller", "USB Mouse" };
static_assert(sizeof(ctrlrName) / sizeof(ctrlrName[0]) == INPUT_HW_MAX, "ctrlrName needs one entry per GuiHardwareProfile");

typedef struct _btn_map {
	uint32_t btn;					// button 'id'
	char name[9];				// button name
} BtnMap;

typedef struct _ctrlr_map {
	uint16_t type;					// controller type
	int num_btns;				// number of buttons on the controller
	BtnMap map[15];				// controller button map
} CtrlrMap;

extern CtrlrMap ctrlr_def[INPUT_HW_MAX];

#endif
