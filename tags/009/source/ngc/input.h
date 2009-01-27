/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric September 2008
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>

#define PI 				3.14159265f
#define PADCAL			50
#define MAXJP 			12 // # of mappable controller buttons

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

void ResetControls();
s8 WPAD_Stick(u8 chan,u8 right, int axis);
void UpdateCursorPosition (int pad, int &pos_x, int &pos_y);
void decodepad (int pad);
void NGCReportButtons ();
void SetControllers ();
void SetDefaultButtonMap ();

#endif
