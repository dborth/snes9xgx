/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiInputController.h
 *
 * Represents a single connected logical controller.
 * Handles device-specific translation (like sideways Wiimote mapping) 
 * and repeat-delay logic for UI navigation.
 ***************************************************************************/
#pragma once

#include "GuiInput.h"

class GuiInputController {
public:
	GuiInputController(int channel);
	~GuiInputController() = default;

	/**
	 * Updates the controller state. Called once per frame by the driver.
	 * @param data The raw, mapped inputs from the hardware.
	 * @param deltaTime Elapsed time since last frame in seconds.
	 */
	void update(const GuiInputPadData& data, float deltaTime);

	//! Configuration
	void setSideways(bool s) { sideways = s; }
	bool isSideways() const { return sideways; }
	int getChannel() const { return channel; }

	//! State Accessors
	const GuiInputPadData& getPadData() const { return currentData; }

	bool isPressed(uint32_t logicalButtonMask) const;
	bool isHeld(uint32_t logicalButtonMask) const;
	bool isPrimaryPressed() const;
	bool isSecondaryPressed() const;

	//! Navigation Helpers (Accounts for orientation and scroll delays)
	bool up() const;
	bool down() const;
	bool left() const;
	bool right() const;

private:
	int channel;
	bool sideways;
	GuiInputPadData currentData;

	// Analog stick deadzone
	const float STICK_DEADZONE = 0.2f;

	// Scrolling delay timers (in seconds)
	const float SCROLL_DELAY_INITIAL = 0.3f;
	const float SCROLL_DELAY_LOOP = 0.05f;

	float scrollTimer;

	// Internal helper to process directional holds and repeats
	bool processDirection(uint32_t logicalButtonMask, float stickAxis, bool isNegativeAxis) const;

	// Mutable state to allow the const navigation functions to reset the timer
	// when a valid scroll triggers. (A common pattern to keep accessors clean).
	mutable float internalScrollTimer;
};

extern GuiInputController* userInput[4];
extern int rumbleRequest[4];
