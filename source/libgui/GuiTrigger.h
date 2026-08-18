#ifndef GUITRIGGER_H
#define GUITRIGGER_H

#include "Gui.h"

#define SCROLL_DELAY_INITIAL	200000
#define SCROLL_DELAY_LOOP		30000
#define SCROLL_DELAY_DECREASE	300

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

typedef struct _gamepaddata {
	u16 btns_d;
	u16 btns_u;
	u16 btns_h;
	s16 stickX;
	s16 stickY;
	s16 substickX;
	s16 substickY;
} GamePadData;

//!Menu input trigger management. Determine if action is neccessary based on input data by comparing controller input data to a specific trigger element.
class GuiTrigger
{
	public:
		//!Constructor
		GuiTrigger();
		//!Destructor
		~GuiTrigger();
		//!Sets a simple trigger. Requires: element is selected, and trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		//!\param wiidrcbtns Wii U Gamepad trigger button(s)
		void setSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns);
		//!Sets a held trigger. Requires: element is selected, and trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		//!\param wiidrcbtns Wii U Gamepad trigger button(s)
		void setHeldTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns);
		//!Sets a button-only trigger. Requires: Trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		//!\param wiidrcbtns Wii U Gamepad trigger button(s)
		void setButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns);
		//!Sets a button-only trigger. Requires: trigger button is pressed and parent window of element is in focus
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		//!\param wiidrcbtns Wii U Gamepad trigger button(s)
		void setButtonOnlyInFocusTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns);
		//!Get X or Y value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\param axis Controller stick axis (x-axis = 0, y-axis = 1)
		//!\return Stick value
		s8 WPAD_Stick(u8 stick, int axis);
		//!Get X value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\return Stick value
		s8 WPAD_StickX(u8 stick);
		//!Get Y value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\return Stick value
		s8 WPAD_StickY(u8 stick);
		//!Move menu selection left (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved left, false otherwise
		bool left();
		//!Move menu selection right (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved right, false otherwise
		bool right();
		//!Move menu selection up (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved up, false otherwise
		bool up();
		//!Move menu selection down (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved down, false otherwise
		bool down();

		WPADData wpaddata; //!< Wii controller trigger data
		PADData pad; //!< GameCube controller trigger data
		GamePadData wiidrcdata; //!< Wii U Gamepad trigger data
		WPADData * wpad; //!< Wii controller trigger
		s32 chan; //!< Trigger controller channel (0-3, -1 for all)
		u8 type; //!< trigger type (TRIGGER_SIMPLE,	TRIGGER_HELD, TRIGGER_BUTTON_ONLY, TRIGGER_BUTTON_ONLY_IN_FOCUS)
};

extern GuiTrigger userInput[4];

#endif // GUITRIGGER_H
