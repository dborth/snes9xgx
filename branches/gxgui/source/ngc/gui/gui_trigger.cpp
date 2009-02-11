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
/**
 * Constructor for the GuiTrigger class.
 */
GuiTrigger::GuiTrigger()
{
	chan = -1;
	memset(&wpad, 0, sizeof(WPADData));
	memset(&pad, 0, sizeof(PADStatus));
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
	pad.button = gcbtns;
}

/**
 * Sets a button trigger - eg: A button pressed and button NOT selected, on all channels
 */
void GuiTrigger::SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns)
{
	type = TRIGGER_BUTTON_ONLY;
	chan = ch;
	wpad.btns_d = wiibtns;
	pad.button = gcbtns;
}
