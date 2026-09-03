/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * InputController.h
 *
 * Represents a single connected logical controller.
 * Handles device-specific translation (like sideways Wiimote mapping)
 * and repeat-delay logic for UI navigation.
 ***************************************************************************/
#pragma once

#include "InputData.h"

class InputController {
public:
	InputController(int channel);
	~InputController() = default;

	/**
	 * Updates the controller state. Called once per frame by the driver.
	 * @param data The raw, mapped inputs from the hardware.
	 * @param deltaTime Elapsed time since last frame in seconds.
	 */
	void update(const InputPadData& data, float deltaTime);

	//! Configuration
	void setSideways(bool s) { sideways = s; }
	bool isSideways() const { return sideways; }
	int getChannel() const { return channel; }

	//! Temporarily overrides the channel this controller reports via getChannel().
	//! Used by list-based elements (e.g. GuiFileBrowser) to present a "no channel"
	//! (-1) identity to items the cursor isn't currently over, so a stale
	//! stateChan left on a reused slot can't block clicks from the real channel.
	//! Callers MUST restore the original value (see getChannel()) after the
	//! element update() call this wraps.
	void setChannel(int c) { channel = c; }

	//! State Accessors
	const InputPadData& getPadData() const { return currentData; }

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
	InputPadData currentData;

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

extern InputController* userInput[4];

void InitUserInputControllers();
