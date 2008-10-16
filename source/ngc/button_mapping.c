#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include "button_mapping.h"

/***
* Controller Button Descriptions:
* used for identifying which buttons have been pressed when configuring
* and for displaying the name of said button
***/
//CtrlrMap ctrlr_def[4];

CtrlrMap ctrlr_def[4] = { 
// Nunchuk btn def
CTRLR_NUNCHUK,	
13,
WPAD_BUTTON_DOWN, "DOWN",
WPAD_BUTTON_UP, "UP",
WPAD_BUTTON_LEFT, "LEFT",
WPAD_BUTTON_RIGHT, "RIGHT",
WPAD_BUTTON_A, "A",
WPAD_BUTTON_B, "B",
WPAD_BUTTON_1, "1",
WPAD_BUTTON_2, "2",
WPAD_BUTTON_PLUS, "PLUS",
WPAD_BUTTON_MINUS, "MINUS",
WPAD_BUTTON_HOME, "HOME",
WPAD_NUNCHUK_BUTTON_Z, "Z",
WPAD_NUNCHUK_BUTTON_C, "C", 
0, "", 
0, "", 
// Classic btn def
CTRLR_CLASSIC,
15,
WPAD_CLASSIC_BUTTON_DOWN, "DOWN",
WPAD_CLASSIC_BUTTON_UP, "UP",
WPAD_CLASSIC_BUTTON_LEFT, "LEFT",
WPAD_CLASSIC_BUTTON_RIGHT, "RIGHT",
WPAD_CLASSIC_BUTTON_A, "A",
WPAD_CLASSIC_BUTTON_B, "B",
WPAD_CLASSIC_BUTTON_X, "X",
WPAD_CLASSIC_BUTTON_Y, "Y",
WPAD_CLASSIC_BUTTON_PLUS, "PLUS",
WPAD_CLASSIC_BUTTON_MINUS, "MINUS",
WPAD_CLASSIC_BUTTON_HOME, "HOME",
WPAD_CLASSIC_BUTTON_FULL_L, "L TRIG",
WPAD_CLASSIC_BUTTON_FULL_R, "R TRIG",
WPAD_CLASSIC_BUTTON_ZL, "ZL",
WPAD_CLASSIC_BUTTON_ZR, "ZR",
// Gamecube controller btn  def
CTRLR_GCPAD,
13,
PAD_BUTTON_DOWN, "DOWN",
PAD_BUTTON_UP, "UP",
PAD_BUTTON_LEFT, "LEFT",
PAD_BUTTON_RIGHT, "RIGHT",
PAD_BUTTON_A, "A",
PAD_BUTTON_B, "B",
PAD_BUTTON_X, "X",
PAD_BUTTON_Y, "Y",
PAD_BUTTON_MENU, "MENU",
PAD_BUTTON_START, "START",
PAD_TRIGGER_L, "L TRIG",
PAD_TRIGGER_R, "R TRIG",
PAD_TRIGGER_Z, "Z",
0, "", 
0, "", 
// Wiimote btn def
CTRLR_WIIMOTE,
11,
WPAD_BUTTON_DOWN, "DOWN",
WPAD_BUTTON_UP, "UP",
WPAD_BUTTON_LEFT, "LEFT",
WPAD_BUTTON_RIGHT, "RIGHT",
WPAD_BUTTON_A, "A",
WPAD_BUTTON_B, "B",
WPAD_BUTTON_1, "1",
WPAD_BUTTON_2, "2",
WPAD_BUTTON_PLUS, "PLUS",
WPAD_BUTTON_MINUS, "MINUS",
WPAD_BUTTON_HOME, "HOME",
0, "",
0, "",
0, "",
0, ""
};

/*
// Nunchuk btn def
ctrlr_def[0].type = CTRLR_NUNCHUK;
ctrlr_def[0].num_btns = 13;
ctrlr_def[0].map[] = { WPAD_BUTTON_DOWN, "DOWN",
WPAD_BUTTON_UP, "UP",
WPAD_BUTTON_LEFT, "LEFT",
WPAD_BUTTON_RIGHT, "RIGHT",
WPAD_BUTTON_A, "A",
WPAD_BUTTON_B, "B",
WPAD_BUTTON_1, "1",
WPAD_BUTTON_2, "2",
WPAD_BUTTON_PLUS, "PLUS",
WPAD_BUTTON_MINUS, "MINUS",
WPAD_BUTTON_HOME, "HOME",
WPAD_NUNCHUK_BUTTON_Z, "Z",
WPAD_NUNCHUK_BUTTON_C, "C" 
};
// Classic btn def
ctrlr_def[1].type = CTRLR_CLASSIC;
ctrlr_def[1].num_btns = 15;
ctrlr_def[1].map[] = { WPAD_CLASSIC_BUTTON_DOWN, "DOWN",
WPAD_CLASSIC_BUTTON_UP, "UP",
WPAD_CLASSIC_BUTTON_LEFT, "LEFT",
WPAD_CLASSIC_BUTTON_RIGHT, "RIGHT",
WPAD_CLASSIC_BUTTON_A, "A",
WPAD_CLASSIC_BUTTON_B, "B",
WPAD_CLASSIC_BUTTON_X, "X",
WPAD_CLASSIC_BUTTON_Y, "Y",
WPAD_CLASSIC_BUTTON_PLUS, "PLUS",
WPAD_CLASSIC_BUTTON_MINUS, "MINUS",
WPAD_CLASSIC_BUTTON_HOME, "HOME",
WPAD_CLASSIC_BUTTON_FULL_L, "L TRIG",
WPAD_CLASSIC_BUTTON_FULL_R, "R TRIG",
WPAD_CLASSIC_BUTTON_ZL, "ZL",
WPAD_CLASSIC_BUTTON_ZR, "ZR",
};
// Gamecube controller btn  def
ctrlr_def[2].type = CTRLR_GCPAD;
ctrlr_def[2].num_btns = 13;
ctrlr_def[2].map[] = { PAD_BUTTON_DOWN, "DOWN",
PAD_BUTTON_UP, "UP",
PAD_BUTTON_LEFT, "LEFT",
PAD_BUTTON_RIGHT, "RIGHT",
PAD_BUTTON_A, "A",
PAD_BUTTON_B, "B",
PAD_BUTTON_X, "X",
PAD_BUTTON_Y, "Y",
PAD_BUTTON_MENU, "MENU",
PAD_BUTTON_START, "START",
PAD_BUTTON_L, "L TRIG",
PAD_BUTTON_R, "R TRIG",
PAD_BUTTON_Z, "Z",
};
// Wiimote btn def
ctrlr_def[3].type = CTRLR_WIIMOTE;
ctrlr_def[3].num_btns = 11;
ctrlr_def[3].map[] = { WPAD_BUTTON_DOWN, "DOWN",
WPAD_BUTTON_UP, "UP",
WPAD_BUTTON_LEFT, "LEFT",
WPAD_BUTTON_RIGHT, "RIGHT",
WPAD_BUTTON_A, "A",
WPAD_BUTTON_B, "B",
WPAD_BUTTON_1, "1",
WPAD_BUTTON_2, "2",
WPAD_BUTTON_PLUS, "PLUS",
WPAD_BUTTON_MINUS, "MINUS",
WPAD_BUTTON_HOME, "HOME"
};
// end buttonmaps
*/

/***
* Default controller maps
* button press on left, and corresponding snes button on right
* arguably some data is unnecessary here but lets stick to one struct type ok?

CtrlrMap defaultmap[4];
// Nunchuk Default
defaultmap[0].type = CTRLR_NUNCHUK;
defaultmap[0].num_btns = 12;
defaultmap[0].map[] = { WPAD_BUTTON_A, "A",
	WPAD_BUTTON_B, "B",
	WPAD_NUNCHUK_BUTTON_C, "X",
	WPAD_NUNCHUK_BUTTON_Z, "Y",
	WPAD_BUTTON_MINUS, "L",
	WPAD_BUTTON_PLUS, "R",
	WPAD_BUTTON_2, "SELECT",
	WPAD_BUTTON_1, "START",
	WPAD_BUTTON_UP, "UP",
	WPAD_BUTTON_DOWN, "DOWN",
	WPAD_BUTTON_LEFT, "LEFT",
	WPAD_BUTTON_RIGHT, "RIGHT"
};
// Classic Default
defaultmap[1].type = CTRLR_CLASSIC;
defaultmap[1].num_btns = 12;
defaultmap[1].map[] = { WPAD_CLASSIC_BUTTON_A, "A",
	WPAD_CLASSIC_BUTTON_B, "B",
	WPAD_CLASSIC_BUTTON_Y, "X",
	WPAD_CLASSIC_BUTTON_X, "Y",
	WPAD_CLASSIC_BUTTON_FULL_L, "L",
	WPAD_CLASSIC_BUTTON_FULL_R, "R",
	WPAD_CLASSIC_BUTTON_MINUS, "SELECT",
	WPAD_CLASSIC_BUTTON_PLUS, "START",
	WPAD_CLASSIC_BUTTON_UP, "UP",
	WPAD_CLASSIC_BUTTON_DOWN, "DOWN",
	WPAD_CLASSIC_BUTTON_LEFT, "LEFT",
	WPAD_CLASSIC_BUTTON_RIGHT, "RIGHT"
};
// Gamecube Controller Default
defaultmap[2].type = CTRLR_GCPAD;
defaultmap[2].num_btns = 12;
defaultmap[2].map[] = {PAD_BUTTON_A, "A",
	PAD_BUTTON_B, "B",
	PAD_BUTTON_X, "X",
	PAD_BUTTON_Y, "Y",
	PAD_TRIGGER_L, "L",
	PAD_TRIGGER_R, "R",
	PAD_TRIGGER_Z, "SELECT",
	PAD_BUTTON_START, "START",
	PAD_BUTTON_UP, "UP",
	PAD_BUTTON_DOWN, "DOWN",
	PAD_BUTTON_LEFT, "LEFT",
	PAD_BUTTON_RIGHT, "RIGHT"
};
// Wiimote Default
defaultmap[3].type = CTRLR_WIIMOTE;
defaultmap[3].num_btns = 12;
defaultmap[3].map[] = { WPAD_BUTTON_B, "A",
	WPAD_BUTTON_2, "B",
	WPAD_BUTTON_1, "X",
	WPAD_BUTTON_A, "Y",
	0x0000, "L",
	0x0000, "R",
	WPAD_BUTTON_MINUS, "SELECT",
	WPAD_BUTTON_PLUS, "START",
	WPAD_BUTTON_RIGHT, "UP",
	WPAD_BUTTON_LEFT, "DOWN",
	WPAD_BUTTON_UP, "LEFT",
	WPAD_BUTTON_DOWN, "RIGHT"
};
// end default padmaps
***/



// eof
