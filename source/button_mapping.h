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

const char ctrlrName[7][32] =
{ "GameCube Controller", "Wiimote", "Nunchuk + Wiimote", "Classic Controller", "Wii U Pro Controller", "Wii U Gamepad", "DualShock 4" };

typedef struct _btn_map {
	uint32_t btn;					// button 'id'
	char name[9];				// button name
} BtnMap;

typedef struct _ctrlr_map {
	uint16_t type;					// controller type
	int num_btns;				// number of buttons on the controller
	BtnMap map[15];				// controller button map
} CtrlrMap;

extern CtrlrMap ctrlr_def[7];

#endif
