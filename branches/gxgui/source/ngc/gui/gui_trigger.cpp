/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_trigger.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

static int scrollDelay = 0;

/**
 * Constructor for the GuiTrigger class.
 */
GuiTrigger::GuiTrigger()
{
	chan = -1;
	memset(&wpad, 0, sizeof(WPADData));
	memset(&pad, 0, sizeof(PADData));
}

/**
 * Destructor for the GuiTrigger class.
 */
GuiTrigger::~GuiTrigger()
{
}

/**
 * Sets a simple trigger - eg: Button selected, A button pressed, on all channels
 */
void GuiTrigger::SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns)
{
	type = TRIGGER_SIMPLE;
	chan = ch;
	wpad.btns_d = wiibtns;
	pad.btns_d = gcbtns;
}

/**
 * Sets a button trigger - eg: A button pressed and button NOT selected, on all channels
 */
void GuiTrigger::SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns)
{
	type = TRIGGER_BUTTON_ONLY;
	chan = ch;
	wpad.btns_d = wiibtns;
	pad.btns_d = gcbtns;
}

/****************************************************************************
 * WPAD_Stick
 *
 * Get X/Y value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 GuiTrigger::WPAD_Stick(u8 right, int axis)
{
	float mag = 0.0;
	float ang = 0.0;

	switch (wpad.exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (right == 0)
			{
				mag = wpad.exp.nunchuk.js.mag;
				ang = wpad.exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (right == 0)
			{
				mag = wpad.exp.classic.ljs.mag;
				ang = wpad.exp.classic.ljs.ang;
			}
			else
			{
				mag = wpad.exp.classic.rjs.mag;
				ang = wpad.exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate x/y value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val;

	if(axis == 0) // x-axis
		val = mag * sin((PI * ang)/180.0f);
	else // y-axis
		val = mag * cos((PI * ang)/180.0f);

	return (s8)(val * 128.0f);
}

bool GuiTrigger::Left()
{
	if((wpad.btns_d | wpad.btns_h) & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)
			|| pad.btns_d & PAD_BUTTON_LEFT
			|| pad.stickX < -PADCAL
			|| WPAD_Stick(0,0) < -PADCAL)
	{
		if(wpad.btns_d & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)
			|| pad.btns_d & PAD_BUTTON_LEFT)
		{
			scrollDelay = SCROLL_INITIAL_DELAY; // reset scroll delay.
			return true;
		}
		else if(scrollDelay == 0)
		{
			scrollDelay = SCROLL_LOOP_DELAY;
			return true;
		}
		else
		{
			scrollDelay--;
		}
	}
	return false;
}

bool GuiTrigger::Right()
{
	if((wpad.btns_d | wpad.btns_h) & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)
			|| pad.btns_d & PAD_BUTTON_RIGHT
			|| pad.stickX > PADCAL
			|| WPAD_Stick(0,0) > PADCAL)
	{
		if(wpad.btns_d & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)
			|| pad.btns_d & PAD_BUTTON_RIGHT)
		{
			scrollDelay = SCROLL_INITIAL_DELAY; // reset scroll delay.
			return true;
		}
		else if(scrollDelay == 0)
		{
			scrollDelay = SCROLL_LOOP_DELAY;
			return true;
		}
		else
		{
			scrollDelay--;
		}
	}
	return false;
}

bool GuiTrigger::Up()
{
	if((wpad.btns_d | wpad.btns_h) & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)
			|| pad.btns_d & PAD_BUTTON_UP
			|| pad.stickX > PADCAL
			|| WPAD_Stick(0,1) > PADCAL)
	{
		if(wpad.btns_d & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)
			|| pad.btns_d & PAD_BUTTON_UP)
		{
			scrollDelay = SCROLL_INITIAL_DELAY; // reset scroll delay.
			return true;
		}
		else if(scrollDelay == 0)
		{
			scrollDelay = SCROLL_LOOP_DELAY;
			return true;
		}
		else
		{
			scrollDelay--;
		}
	}
	return false;
}

bool GuiTrigger::Down()
{
	if((wpad.btns_d | wpad.btns_h) & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)
			|| (pad.btns_d | pad.btns_h) & PAD_BUTTON_DOWN
			|| pad.stickX < -PADCAL
			|| WPAD_Stick(0,1) < -PADCAL)
	{
		if(wpad.btns_d & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)
			|| pad.btns_d & PAD_BUTTON_DOWN)
		{
			scrollDelay = SCROLL_INITIAL_DELAY; // reset scroll delay.
			return true;
		}
		else if(scrollDelay == 0)
		{
			scrollDelay = SCROLL_LOOP_DELAY;
			return true;
		}
		else
		{
			scrollDelay--;
		}
	}
	return false;
}
