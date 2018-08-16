/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * michniewski August 2008
 * Tantric 2008-2010
 *
 * button_mapping.h
 *
 * Controller button mapping
 ***************************************************************************/

#ifndef BTN_MAP_H
#define BTN_MAP_H

enum {
	CTRLR_NONE = -1,
	CTRLR_GCPAD,
	CTRLR_WIIMOTE,
	CTRLR_NUNCHUK,
	CTRLR_CLASSIC,
	CTRLR_WUPC
};

const char ctrlrName[5][32] =
{ "GameCube Controller", "Wiimote", "Nunchuk + Wiimote", "Classic Controller", "Wii U Pro Controller" };

typedef struct _btn_map {
	u32 btn;					// button 'id'
	char* name;					// button name
} BtnMap;

typedef struct _ctrlr_map {
	u16 type;					// controller type
	int num_btns;				// number of buttons on the controller
	BtnMap map[15];		// controller button map
} CtrlrMap;

extern CtrlrMap ctrlr_def[5];

#endif
