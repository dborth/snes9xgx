/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiInputController.cpp
 ***************************************************************************/

#include <cmath>
#include "Gui.h"

GuiInputController* userInput[4] = {nullptr, nullptr, nullptr, nullptr};

void InitUserInputControllers()
{
	for(int i = 0; i < 4; i++)
	{
		if(!userInput[i])
			userInput[i] = new GuiInputController(i);
	}
}

GuiInputController::GuiInputController(int ch) : 
	channel(ch),
	sideways(false),
	scrollTimer(0.0f),
	internalScrollTimer(0.0f)
{}

void GuiInputController::update(const GuiInputPadData& data, float deltaTime) {
	currentData = data;

	// Advance the scroll timer
	internalScrollTimer += deltaTime;

	// If no directional inputs are held, reset the scroll timer completely
	if (currentData.buttons_h == 0 &&
		std::abs(currentData.stickX) < STICK_DEADZONE &&
		std::abs(currentData.stickY) < STICK_DEADZONE) {
		internalScrollTimer = 0.0f;
	}
}

bool GuiInputController::processDirection(uint32_t logicalButtonMask, float stickAxis, bool isPositiveAxis) const {
	bool isPressedDown = (currentData.buttons_d & logicalButtonMask);
	bool isHeld = (currentData.buttons_h & logicalButtonMask);
	bool isStickActive = isPositiveAxis ? (stickAxis > STICK_DEADZONE) : (stickAxis < -STICK_DEADZONE);

	// Initial press fires immediately
	if (isPressedDown) {
		internalScrollTimer = 0.0f; // Reset timer on fresh press
		return true;
	}

	// If it's held down (or stick pushed), evaluate the repeat delay
	if (isHeld || isStickActive) {
		if (internalScrollTimer >= SCROLL_DELAY_INITIAL) {
			// Re-trigger and step back the timer by the loop amount so it triggers again soon
			internalScrollTimer -= SCROLL_DELAY_LOOP;
			return true;
		}
	}

	return false;
}

bool GuiInputController::isPrimaryPressed() const {
	uint32_t targetBtn = sideways ? GUI_BTN_2 : GUI_BTN_A;
	return (currentData.buttons_d & targetBtn);
}

bool GuiInputController::isSecondaryPressed() const {
	uint32_t targetBtn = sideways ? GUI_BTN_1 : GUI_BTN_B;
	return (currentData.buttons_d & targetBtn);
}

bool GuiInputController::isPressed(uint32_t logicalButtonMask) const {
	return (currentData.buttons_d & logicalButtonMask);
}

bool GuiInputController::isHeld(uint32_t logicalButtonMask) const {
	return (currentData.buttons_h & logicalButtonMask);
}

bool GuiInputController::up() const {
	uint32_t targetBtn = sideways ? GUI_BTN_RIGHT : GUI_BTN_UP;
	return processDirection(targetBtn, currentData.stickY, true);
}

bool GuiInputController::down() const {
	uint32_t targetBtn = sideways ? GUI_BTN_LEFT : GUI_BTN_DOWN;
	return processDirection(targetBtn, currentData.stickY, false);
}

bool GuiInputController::left() const {
	uint32_t targetBtn = sideways ? GUI_BTN_UP : GUI_BTN_LEFT;
	return processDirection(targetBtn, currentData.stickX, false);
}

bool GuiInputController::right() const {
	uint32_t targetBtn = sideways ? GUI_BTN_DOWN : GUI_BTN_RIGHT;
	return processDirection(targetBtn, currentData.stickX, true);
}
