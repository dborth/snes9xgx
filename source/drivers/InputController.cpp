/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * InputController.cpp
 ***************************************************************************/

#include <cmath>
#include "Platform.h"
#include "InputController.h"

InputController* userInput[4] = {nullptr, nullptr, nullptr, nullptr};

void InitUserInputControllers()
{
	for(int i = 0; i < 4; i++)
	{
		if(!userInput[i])
			userInput[i] = new InputController(i);
	}
}

InputController::InputController(int ch) : 
	channel(ch),
	sideways(false),
	scrollTimer(0.0f),
	internalScrollTimer(0.0f)
{}

void InputController::update(const InputPadData& data, float deltaTime) {
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

bool InputController::processDirection(uint32_t logicalButtonMask, float stickAxis, bool isPositiveAxis) const {
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

bool InputController::isPrimaryPressed() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_2 : INPUT_BTN_A;
	return (currentData.buttons_d & targetBtn);
}

bool InputController::isSecondaryPressed() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_1 : INPUT_BTN_B;
	return (currentData.buttons_d & targetBtn);
}

bool InputController::isPressed(uint32_t logicalButtonMask) const {
	return (currentData.buttons_d & logicalButtonMask);
}

bool InputController::isHeld(uint32_t logicalButtonMask) const {
	return (currentData.buttons_h & logicalButtonMask);
}

bool InputController::up() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_RIGHT : INPUT_BTN_UP;
	return processDirection(targetBtn, currentData.stickY, true);
}

bool InputController::down() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_LEFT : INPUT_BTN_DOWN;
	return processDirection(targetBtn, currentData.stickY, false);
}

bool InputController::left() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_UP : INPUT_BTN_LEFT;
	return processDirection(targetBtn, currentData.stickX, false);
}

bool InputController::right() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_DOWN : INPUT_BTN_RIGHT;
	return processDirection(targetBtn, currentData.stickX, true);
}
