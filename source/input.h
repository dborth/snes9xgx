/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * input.h
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include "drivers/InputData.h"
#include "snes9xgx.h"

#define MAXJP 			12 // # of mappable controller buttons

extern uint32_t btnmap[CTRL_BTN_MAPPINGS][GUI_HW_MAX][MAXJP];
extern int playerMapping[4];

void ResetControls(int cc = -1, int wc = -1);
void ReportButtons ();
void ClearButtonsReported ();
void SetControllers ();
void SetDefaultButtonMap ();
bool isMenuRequested();
#ifdef HW_RVL
char* GetUSBControllerInfo();
#endif

#endif
