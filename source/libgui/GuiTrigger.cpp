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

bool GuiTrigger::isClicked(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	return (controller->getPadData().buttons_d & resolveMask(controller)) != 0;
}

bool GuiTrigger::isHeld(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	return (controller->getPadData().buttons_h & resolveMask(controller)) != 0;
}

bool GuiTrigger::isReleased(const InputController* controller) const {
	if (!controller || (chan != -1 && controller->getChannel() != chan)) {
		return false;
	}
	return (controller->getPadData().buttons_r & resolveMask(controller)) != 0;
}
