/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
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

	//! In the menu the left analog stick moves the pointer, like a mouse, instead of stepping
	//! through the elements; the D-pad still does that. Off by default: the game reads the same
	//! pad state, and must not see a stick-driven cursor. See SetStickPointerEnabled().
	void setStickPointer(bool enabled);

	//! Temporarily overrides the channel this controller reports via getChannel().
	//! Used by list-based elements (e.g. GuiGameBrowser) to present a "no channel"
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

	// Analog stick -> digital direction conversion
	const float STICK_PRESS_THRESHOLD = 0.39f; // 50/128, same engage point as 5.0.2 (PADCAL)
	const float STICK_RELEASE_THRESHOLD = 0.20f;
	const float STICK_AXIS_SWITCH_BIAS = 1.25f; // the other axis must lead by this factor to take over

	enum class StickDir : uint8_t { None, Up, Down, Left, Right };

	// Scrolling delay timers (in seconds)

	// D-pad / buttons
	const float SCROLL_DELAY_INITIAL = 0.2f;
	const float SCROLL_DELAY_LOOP_START = 0.08f; // ~12.5 rows/sec right after the initial delay
	const float SCROLL_DELAY_LOOP_MIN = 0.012f;  // ~83 rows/sec once fully ramped up
	const float SCROLL_ACCEL_RAMP_TIME = 1.0f;   // seconds of continuous holding to reach max speed

	// Analog stick: a gentler curve, since a stick is harder to release
	// precisely than a D-pad button and has no click to tell you how far it went.
	const float STICK_SCROLL_DELAY_INITIAL = 0.3f;
	const float STICK_SCROLL_DELAY_LOOP_START = 0.12f; // ~8 rows/sec right after the initial delay
	const float STICK_SCROLL_DELAY_LOOP_MIN = 0.04f;   // ~25 rows/sec once fully ramped up
	const float STICK_SCROLL_ACCEL_RAMP_TIME = 1.5f;

	float scrollTimer;
	float lastDeltaTime; // frame time from the most recent update(), used to bound the repeat timer

	// Stick direction state, advanced once per frame in update()
	StickDir stickDir;      // digital direction the stick is currently holding
	StickDir prevStickDir;
	bool stickEdge;         // true only on the frame stickDir changed to a new direction

	StickDir resolveStickDir(float x, float y, StickDir current) const;

	// Left stick as a pointer (see setStickPointer())
	static constexpr float STICK_POINTER_DEADZONE = 0.15f;
	static constexpr float STICK_POINTER_SPEED = 480.0f;  // pixels per second at full deflection (the canvas is 640 wide)
	static constexpr float STICK_POINTER_IDLE_SECONDS = 3.0f; // how long it stays on screen once the stick is let go
	void updateStickPointer(float deltaTime);

	bool stickPointerEnabled;
	bool stickPointerStarted;  // x/y hold a position
	bool stickPointerActive;   // the stick has moved the pointer, and has not been idle for long
	bool stickPointerHeld;     // the stick was pushed on the previous frame
	float stickPointerX, stickPointerY;
	float stickPointerIdle;

	// Internal helper to process directional holds and repeats
	bool processDirection(uint32_t logicalButtonMask, StickDir stickTarget) const;

	// Mutable state to allow the const navigation functions to reset the timer
	// when a valid scroll triggers. (A common pattern to keep accessors clean).
	mutable float internalScrollTimer;

	// How long a direction has been continuously held (or the stick pushed into a direction)
	float holdDuration;
};

extern InputController* controller[4];

void InitUserInputControllers();

//! Turns the left-stick pointer on (menu) or off (game) for every controller
void SetStickPointerEnabled(bool enabled);
