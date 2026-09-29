/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * InputController.cpp
 ***************************************************************************/

#include <cmath>
#include "InputController.h"

InputController* controller[4] = {nullptr, nullptr, nullptr, nullptr};

void InitUserInputControllers()
{
	for(int i = 0; i < 4; i++)
	{
		if(!controller[i])
			controller[i] = new InputController(i);
	}
}

InputController::InputController(int ch) : 
	channel(ch),
	sideways(false),
	scrollTimer(0.0f),
	lastDeltaTime(1.0f / 60.0f),
	stickDir(StickDir::None),
	prevStickDir(StickDir::None),
	stickEdge(false),
	internalScrollTimer(0.0f),
	holdDuration(0.0f)
{}

InputController::StickDir InputController::resolveStickDir(float x, float y, StickDir current) const {
	const float ax = std::abs(x);
	const float ay = std::abs(y);

	// Pick the dominant axis. Once a direction is held, the other axis has to
	// clearly lead before it takes over, so a wobbling stick doesn't flip back and forth.
	bool horizontal = ax > ay;
	if (current == StickDir::Left || current == StickDir::Right)
		horizontal = (ax * STICK_AXIS_SWITCH_BIAS >= ay);
	else if (current == StickDir::Up || current == StickDir::Down)
		horizontal = (ax > ay * STICK_AXIS_SWITCH_BIAS);

	StickDir candidate;
	float magnitude;
	if (horizontal) {
		candidate = (x > 0.0f) ? StickDir::Right : StickDir::Left;
		magnitude = ax;
	} else {
		candidate = (y > 0.0f) ? StickDir::Up : StickDir::Down;
		magnitude = ay;
	}

	// Staying in the current direction only needs the (lower) release threshold;
	// entering a new one has to clear the press threshold.
	if (candidate == current)
		return (magnitude >= STICK_RELEASE_THRESHOLD) ? candidate : StickDir::None;

	return (magnitude >= STICK_PRESS_THRESHOLD) ? candidate : StickDir::None;
}

void InputController::update(const InputPadData& data, float deltaTime) {
	currentData = data;
	lastDeltaTime = deltaTime;

	// Advance the scroll timer
	internalScrollTimer += deltaTime;

	// Turn the analog stick into a digital direction, remembering the edge so
	// a fresh push fires immediately just like a D-pad press does.
	prevStickDir = stickDir;
	stickDir = resolveStickDir(currentData.stickX, currentData.stickY, stickDir);
	stickEdge = (stickDir != StickDir::None && stickDir != prevStickDir);

	const uint32_t dirMask = INPUT_BTN_UP | INPUT_BTN_DOWN | INPUT_BTN_LEFT | INPUT_BTN_RIGHT;
	const bool dirButtonPressed = (currentData.buttons_d & dirMask) != 0;
	const bool anyDirectionHeld = (currentData.buttons_h & dirMask) != 0 || stickDir != StickDir::None;

	if (!anyDirectionHeld) {
		// Nothing directional held - reset both timers completely
		internalScrollTimer = 0.0f;
		holdDuration = 0.0f;
	} else if (dirButtonPressed || stickEdge) {
		// A new direction was just pressed: restart the initial delay and the
		// acceleration ramp, rather than inheriting them from the previous hold.
		internalScrollTimer = 0.0f;
		holdDuration = 0.0f;
	} else {
		holdDuration += deltaTime;
	}
}

bool InputController::processDirection(uint32_t logicalButtonMask, StickDir stickTarget) const {
	const bool buttonPressed = (currentData.buttons_d & logicalButtonMask) != 0;
	const bool buttonHeld = (currentData.buttons_h & logicalButtonMask) != 0;
	const bool stickPressed = stickEdge && stickDir == stickTarget;
	const bool stickHeld = (stickDir == stickTarget);

	// Initial press fires immediately (button or stick)
	if (buttonPressed || stickPressed) {
		internalScrollTimer = 0.0f; // Reset timer on fresh press
		return true;
	}

	// If it's held down (or stick pushed), evaluate the repeat delay
	if (buttonHeld || stickHeld) {
		// Buttons win if both are active; the stick curve only applies when the stick alone is driving.
		const bool analog = !buttonHeld;
		const float initialDelay = analog ? STICK_SCROLL_DELAY_INITIAL : SCROLL_DELAY_INITIAL;
		const float loopStart = analog ? STICK_SCROLL_DELAY_LOOP_START : SCROLL_DELAY_LOOP_START;
		const float loopMin = analog ? STICK_SCROLL_DELAY_LOOP_MIN : SCROLL_DELAY_LOOP_MIN;
		const float rampTime = analog ? STICK_SCROLL_ACCEL_RAMP_TIME : SCROLL_ACCEL_RAMP_TIME;

		if (internalScrollTimer >= initialDelay) {
			// Accelerate the repeat rate the longer this has been continuously
			// held, ramping from loopStart down to loopMin over rampTime seconds.
			float heldPastInitial = holdDuration - initialDelay;
			float t = heldPastInitial / rampTime;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
			float loopDelay = loopStart + (loopMin - loopStart) * t;

			// The timer keeps accumulating while a direction is held but nobody consumes it
			// (window not focused, directory loading, modal open). Bound the overshoot to one
			// frame so that doesn't turn into a burst of every-frame repeats once it is consumed.
			if (internalScrollTimer > initialDelay + lastDeltaTime)
				internalScrollTimer = initialDelay + lastDeltaTime;

			// Re-trigger and step back the timer by the loop amount so it triggers again soon
			internalScrollTimer -= loopDelay;
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
	return processDirection(targetBtn, StickDir::Up);
}

bool InputController::down() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_LEFT : INPUT_BTN_DOWN;
	return processDirection(targetBtn, StickDir::Down);
}

bool InputController::left() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_UP : INPUT_BTN_LEFT;
	return processDirection(targetBtn, StickDir::Left);
}

bool InputController::right() const {
	uint32_t targetBtn = sideways ? INPUT_BTN_DOWN : INPUT_BTN_RIGHT;
	return processDirection(targetBtn, StickDir::Right);
}
