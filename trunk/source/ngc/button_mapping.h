/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * michniewski August 2008
 *
 * button_mapping.h
 *
 * Controller button mapping
 ***************************************************************************/

#ifndef BTN_MAP_H
#define BTN_MAP_H

enum {
	CTRLR_NONE = -1,
	CTRLR_NUNCHUK,
	CTRLR_CLASSIC,
	CTRLR_GCPAD,
	CTRLR_WIIMOTE,
	CTRLR_SNES = 7	// give some other value for the snes padmap
};

typedef struct _btn_map {
	u32 btn;					// button 'id'
	char* name;					// button name
} BtnMap;

typedef struct _ctrlr_map {
	u16 type;					// controller type
	int num_btns;				// number of buttons on the controller
	BtnMap map[15];		// controller button map
} CtrlrMap;

// externs:

extern CtrlrMap ctrlr_def[4];

#endif
