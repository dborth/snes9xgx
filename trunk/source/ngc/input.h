
#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>

#define PI 				3.14159265f
#define PADCAL			50

extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];
extern unsigned int gcscopemap[];
extern unsigned int wmscopemap[];
extern unsigned int gcmousemap[];
extern unsigned int wmmousemap[];
extern unsigned int gcjustmap[];
extern unsigned int wmjustmap[];

START_EXTERN_C

s8 WPAD_StickX(u8 chan,u8 right);
s8 WPAD_StickY(u8 chan, u8 right);

void UpdateCursorPosition (int pad, int &pos_x, int &pos_y);
void decodepad (int pad);
void NGCReportButtons ();
void SetControllers ();
void SetDefaultButtonMap ();

//extern u32 wpad_get_analogues(int pad, float* mag1, u16* ang1, float* mag2, u16* ang2);

END_EXTERN_C

#endif
