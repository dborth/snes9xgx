/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * michniewski August 2008
 * Tantric 2008-2023
 *
 * button_mapping.h
 *
 * Controller button mapping
 ***************************************************************************/

#ifndef BTN_MAP_H
#define BTN_MAP_H

const char ctrlrName[6][32] =
{ "GameCube Controller", "Wiimote", "Nunchuk + Wiimote", "Classic Controller", "Wii U Pro Controller", "Wii U Gamepad" };

typedef struct _btn_map {
	uint32_t btn;					// button 'id'
	char name[7];				// button name
} BtnMap;

typedef struct _ctrlr_map {
	uint16_t type;					// controller type
	int num_btns;				// number of buttons on the controller
	BtnMap map[15];				// controller button map
} CtrlrMap;

extern CtrlrMap ctrlr_def[6];

#endif
