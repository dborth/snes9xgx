/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>
#include <wiiuse/wpad.h>

#define PI 				3.14159265f
#define PADCAL			50
#define MAXJP 			12 // # of mappable controller buttons

enum
{
	TRIGGER_SIMPLE,
	TRIGGER_HELD,
	TRIGGER_BUTTON_ONLY,
	TRIGGER_BUTTON_ONLY_IN_FOCUS
};

typedef struct _paddata {
	u16 btns_d;
	u16 btns_u;
	u16 btns_h;
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u8 triggerL;
	u8 triggerR;
} PADData;

class GuiTrigger
{
	public:
		GuiTrigger();
		~GuiTrigger();
		void SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetHeldTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetButtonOnlyInFocusTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		s8 WPAD_Stick(u8 right, int axis);
		bool Left();
		bool Right();
		bool Up();
		bool Down();

		u8 type;
		s32 chan;
		WPADData wpad;
		PADData pad;
};

extern GuiTrigger userInput[4];
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
