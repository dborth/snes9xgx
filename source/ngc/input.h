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

extern int rumbleRequest[4];
extern u32 btnmap[4][4][12];

void ResetControls();
void ShutoffRumble();
void DoRumble(int i);
s8 WPAD_Stick(u8 chan, u8 right, int axis);
void UpdateCursorPosition (int pad, int &pos_x, int &pos_y);
void decodepad (int pad);
void NGCReportButtons ();
void SetControllers ();
void SetDefaultButtonMap ();

#endif
