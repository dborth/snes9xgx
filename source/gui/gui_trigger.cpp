/****************************************************************************
 * libwiigui
 *
 * Tantric 2009
 *
 * gui_trigger.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include <ogc/lwp_watchdog.h>
#include <gctypes.h>

static u64 prev[4];
static u64 now[4];
static u32 delay[4];

/**
 * Constructor for the GuiTrigger class.
 */
GuiTrigger::GuiTrigger()
{
	chan = -1;
	memset(&wiidrcdata, 0, sizeof(GamePadData));
	memset(&wpaddata, 0, sizeof(WPADData));
	memset(&pad, 0, sizeof(PADData));
	wpad = &wpaddata;
}

/**
 * Destructor for the GuiTrigger class.
 */
GuiTrigger::~GuiTrigger()
{
}

/**
 * Sets a simple trigger. Requires:
 * - Element is selected
 * - Trigger button is pressed
 */
void GuiTrigger::SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns)
{
	type = TRIGGER_SIMPLE;
	chan = ch;
	wiidrcdata.btns_d = wiidrcbtns;
	wpaddata.btns_d = wiibtns;
	pad.btns_d = gcbtns;
}

/**
 * Sets a held trigger. Requires:
 * - Element is selected
 * - Trigger button is pressed and held
 */
void GuiTrigger::SetHeldTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns)
{
	type = TRIGGER_HELD;
	chan = ch;
	wiidrcdata.btns_h = wiidrcbtns;
	wpaddata.btns_h = wiibtns;
	pad.btns_h = gcbtns;
}

/**
 * Sets a button trigger. Requires:
 * - Trigger button is pressed
 */
void GuiTrigger::SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns)
{
	type = TRIGGER_BUTTON_ONLY;
	chan = ch;
	wiidrcdata.btns_d = wiidrcbtns;
	wpaddata.btns_d = wiibtns;
	pad.btns_d = gcbtns;
}

/**
 * Sets a button trigger. Requires:
 * - Trigger button is pressed
 * - Parent window is in focus
 */
void GuiTrigger::SetButtonOnlyInFocusTrigger(s32 ch, u32 wiibtns, u16 gcbtns, u16 wiidrcbtns)
{
	type = TRIGGER_BUTTON_ONLY_IN_FOCUS;
	chan = ch;
	wiidrcdata.btns_d = wiidrcbtns;
	wpaddata.btns_d = wiibtns;
	pad.btns_d = gcbtns;
}

/****************************************************************************
 * WPAD_Stick
 *
 * Get X/Y value from Wii Joystick (classic, nunchuk) input
 ***************************************************************************/

s8 GuiTrigger::WPAD_Stick(u8 stick, int axis)
{
	#ifdef HW_RVL
	struct joystick_t* js = NULL;

	switch (wpad->exp.type) {
		case WPAD_EXP_NUNCHUK:
			js = stick ? NULL : &wpad->exp.nunchuk.js;
			break;

		case WPAD_EXP_CLASSIC:
			js = stick ? &wpad->exp.classic.rjs : &wpad->exp.classic.ljs;
			break;

		default:
			break;
	}

	if (js) {
		int pos;
		int min;
		int max;
		int center;

		if(axis == 1) {
			pos = js->pos.y;
			min = js->min.y;
			max = js->max.y;
			center = js->center.y;
		}
		else {
			pos = js->pos.x;
			min = js->min.x;
			max = js->max.x;
			center = js->center.x;
		}

		// some 3rd party controllers return invalid analog sticks calibration data
		if ((min >= center) || (max <= center)) {
			// force default calibration settings
			min = 0;
			max = stick ? 32 : 64;
			center = stick ? 16 : 32;
		}

		if (pos > max) return 127;
		if (pos < min) return -128;

		pos -= center;

		if (pos > 0) {
			return (s8)(127.0 * ((float)pos / (float)(max - center)));
		}
		else {
			return (s8)(128.0 * ((float)pos / (float)(center - min)));
		}
	}
	#endif
	return 0;
}

s8 GuiTrigger::WPAD_StickX(u8 stick)
{
	return WPAD_Stick(stick, 0);
}

s8 GuiTrigger::WPAD_StickY(u8 stick)
{
	return WPAD_Stick(stick, 1);
}

bool GuiTrigger::Left()
{
	u32 wiibtn = GCSettings.WiimoteOrientation ? WPAD_BUTTON_UP : WPAD_BUTTON_LEFT;

	if((wpad->btns_d | wpad->btns_h) & (wiibtn | WPAD_CLASSIC_BUTTON_LEFT)
			|| (wiidrcdata.btns_d | wiidrcdata.btns_h) & WIIDRC_BUTTON_LEFT
			|| (pad.btns_d | pad.btns_h) & PAD_BUTTON_LEFT
			|| pad.stickX < -PADCAL
			|| WPAD_StickX(0) < -PADCAL
			|| wiidrcdata.stickX < -WIIDRCCAL)
	{
		if(wpad->btns_d & (wiibtn | WPAD_CLASSIC_BUTTON_LEFT)
			|| wiidrcdata.btns_d & WIIDRC_BUTTON_LEFT
			|| pad.btns_d & PAD_BUTTON_LEFT)
		{
			prev[chan] = gettime();
			delay[chan] = SCROLL_DELAY_INITIAL; // reset scroll delay
			return true;
		}

		now[chan] = gettime();

		if(diff_usec(prev[chan], now[chan]) > delay[chan])
		{
			prev[chan] = now[chan];
			
			if(delay[chan] == SCROLL_DELAY_INITIAL)
				delay[chan] = SCROLL_DELAY_LOOP;
			else if(delay[chan] > SCROLL_DELAY_DECREASE)
				delay[chan] -= SCROLL_DELAY_DECREASE;
			return true;
		}
	}
	return false;
}

bool GuiTrigger::Right()
{
	u32 wiibtn = GCSettings.WiimoteOrientation ? WPAD_BUTTON_DOWN : WPAD_BUTTON_RIGHT;

	if((wpad->btns_d | wpad->btns_h) & (wiibtn | WPAD_CLASSIC_BUTTON_RIGHT)
			|| (wiidrcdata.btns_d | wiidrcdata.btns_h) & WIIDRC_BUTTON_RIGHT
			|| (pad.btns_d | pad.btns_h) & PAD_BUTTON_RIGHT
			|| pad.stickX > PADCAL
			|| WPAD_StickX(0) > PADCAL
			|| wiidrcdata.stickX > WIIDRCCAL)
	{
		if(wpad->btns_d & (wiibtn | WPAD_CLASSIC_BUTTON_RIGHT)
			|| wiidrcdata.btns_d & WIIDRC_BUTTON_RIGHT
			|| pad.btns_d & PAD_BUTTON_RIGHT)
		{
			prev[chan] = gettime();
			delay[chan] = SCROLL_DELAY_INITIAL; // reset scroll delay
			return true;
		}

		now[chan] = gettime();

		if(diff_usec(prev[chan], now[chan]) > delay[chan])
		{
			prev[chan] = now[chan];
			
			if(delay[chan] == SCROLL_DELAY_INITIAL)
				delay[chan] = SCROLL_DELAY_LOOP;
			else if(delay[chan] > SCROLL_DELAY_DECREASE)
				delay[chan] -= SCROLL_DELAY_DECREASE;
			return true;
		}
	}
	return false;
}

bool GuiTrigger::Up()
{
	u32 wiibtn = GCSettings.WiimoteOrientation ? WPAD_BUTTON_RIGHT : WPAD_BUTTON_UP;

	if((wpad->btns_d | wpad->btns_h) & (wiibtn | WPAD_CLASSIC_BUTTON_UP)
			|| (wiidrcdata.btns_d | wiidrcdata.btns_h) & WIIDRC_BUTTON_UP
			|| (pad.btns_d | pad.btns_h) & PAD_BUTTON_UP
			|| pad.stickY > PADCAL
			|| WPAD_StickY(0) > PADCAL
			|| wiidrcdata.stickY > WIIDRCCAL)
	{
		if(wpad->btns_d & (wiibtn | WPAD_CLASSIC_BUTTON_UP)
			|| wiidrcdata.btns_d & WIIDRC_BUTTON_UP
			|| pad.btns_d & PAD_BUTTON_UP)
		{
			prev[chan] = gettime();
			delay[chan] = SCROLL_DELAY_INITIAL; // reset scroll delay
			return true;
		}

		now[chan] = gettime();

		if(diff_usec(prev[chan], now[chan]) > delay[chan])
		{
			prev[chan] = now[chan];
			
			if(delay[chan] == SCROLL_DELAY_INITIAL)
				delay[chan] = SCROLL_DELAY_LOOP;
			else if(delay[chan] > SCROLL_DELAY_DECREASE)
				delay[chan] -= SCROLL_DELAY_DECREASE;
			return true;
		}
	}
	return false;
}

bool GuiTrigger::Down()
{
	u32 wiibtn = GCSettings.WiimoteOrientation ? WPAD_BUTTON_LEFT : WPAD_BUTTON_DOWN;

	if((wpad->btns_d | wpad->btns_h) & (wiibtn | WPAD_CLASSIC_BUTTON_DOWN)
			|| (wiidrcdata.btns_d | wiidrcdata.btns_h) & WIIDRC_BUTTON_DOWN
			|| (pad.btns_d | pad.btns_h) & PAD_BUTTON_DOWN
			|| pad.stickY < -PADCAL
			|| WPAD_StickY(0) < -PADCAL
			|| wiidrcdata.stickY < -WIIDRCCAL)
	{
		if(wpad->btns_d & (wiibtn | WPAD_CLASSIC_BUTTON_DOWN)
			|| wiidrcdata.btns_d & WIIDRC_BUTTON_DOWN
			|| pad.btns_d & PAD_BUTTON_DOWN)
		{
			prev[chan] = gettime();
			delay[chan] = SCROLL_DELAY_INITIAL; // reset scroll delay
			return true;
		}

		now[chan] = gettime();

		if(diff_usec(prev[chan], now[chan]) > delay[chan])
		{
			prev[chan] = now[chan];
			
			if(delay[chan] == SCROLL_DELAY_INITIAL)
				delay[chan] = SCROLL_DELAY_LOOP;
			else if(delay[chan] > SCROLL_DELAY_DECREASE)
				delay[chan] -= SCROLL_DELAY_DECREASE;
			return true;
		}
	}
	return false;
}
