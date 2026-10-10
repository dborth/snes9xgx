/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTrigger.cpp
 ***************************************************************************/

#include "Gui.h"

GuiTrigger::GuiTrigger() :
	type(TRIGGER_TYPE::SIMPLE),
	action(TRIGGER_ACTION::NONE),
	chan(-1),
	conditionMask(INPUT_BTN_NONE)
{}

void GuiTrigger::setPrimaryTrigger(int ch) {
	type = TRIGGER_TYPE::SIMPLE;
	action = TRIGGER_ACTION::PRIMARY;
	chan = ch;
	conditionMask = INPUT_BTN_NONE; // Handled dynamically in resolveMask
}

void GuiTrigger::setSecondaryTrigger(int ch) {
	type = TRIGGER_TYPE::BUTTON_ONLY;
	action = TRIGGER_ACTION::SECONDARY;
	chan = ch;
	conditionMask = INPUT_BTN_NONE; // Handled dynamically in resolveMask
}

void GuiTrigger::setSimpleTrigger(int ch, uint32_t buttonMask) {
	type = TRIGGER_TYPE::SIMPLE;
	action = TRIGGER_ACTION::NONE;
	chan = ch;
	conditionMask = buttonMask;
}

void GuiTrigger::setHeldTrigger(int ch, uint32_t buttonMask) {
	type = TRIGGER_TYPE::HELD;
	action = TRIGGER_ACTION::NONE;
	chan = ch;
	conditionMask = buttonMask;
}

void GuiTrigger::setButtonOnlyTrigger(int ch, uint32_t buttonMask) {
	type = TRIGGER_TYPE::BUTTON_ONLY;
	action = TRIGGER_ACTION::NONE;
	chan = ch;
	conditionMask = buttonMask;
}

void GuiTrigger::setButtonOnlyInFocusTrigger(int ch, uint32_t buttonMask) {
	type = TRIGGER_TYPE::BUTTON_ONLY_IN_FOCUS;
	action = TRIGGER_ACTION::NONE;
	chan = ch;
	conditionMask = buttonMask;
}

uint32_t GuiTrigger::resolveMask(const InputController* controller) const {
	if (action == TRIGGER_ACTION::PRIMARY) {
		return controller->isSideways() ? INPUT_BTN_2 : INPUT_BTN_A;
	}
	else if (action == TRIGGER_ACTION::SECONDARY) {
		return controller->isSideways() ? INPUT_BTN_1 : INPUT_BTN_B;
	}

	return conditionMask; // Fallback to explicit mask for non-semantic triggers
}

// A sideways Wiimote swaps A/B for 2/1, but only the Wiimote is held that way
static uint32_t UnrotatedHwMask(TRIGGER_ACTION action, const InputController* c)
{
	if (!c->isSideways())
		return INPUT_BTN_NONE;
	if (action == TRIGGER_ACTION::PRIMARY)
		return INPUT_BTN_A;
	if (action == TRIGGER_ACTION::SECONDARY)
		return INPUT_BTN_B;
	return INPUT_BTN_NONE;
}

bool GuiTrigger::isClicked(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	const InputPadData& d = controller->getPadData();
	return ((d.buttons_d & resolveMask(controller)) |
	        (d.hw_buttons_d[INPUT_HW_DRC] & UnrotatedHwMask(action, controller))) != 0;
}

bool GuiTrigger::isHeld(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	const InputPadData& d = controller->getPadData();
	return ((d.buttons_h & resolveMask(controller)) |
	        (d.hw_buttons_h[INPUT_HW_DRC] & UnrotatedHwMask(action, controller))) != 0;
}

bool GuiTrigger::isReleased(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	const InputPadData& d = controller->getPadData();
	return ((d.buttons_r & resolveMask(controller)) |
	        (d.hw_buttons_r[INPUT_HW_DRC] & UnrotatedHwMask(action, controller))) != 0;
}
