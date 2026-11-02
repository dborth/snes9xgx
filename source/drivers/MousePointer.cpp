/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * MousePointer.cpp
 ***************************************************************************/
#include "MousePointer.h"

void MousePointer::update(MouseDriver* mouse, float dt, int width, int height, State & out)
{
	out = State();
	out.connected = mouse->isConnected();

	if (!out.connected) {
		// Start again from the middle of the screen, and not owning the pointer,
		// when the next mouse is plugged in
		started = false;
		held = 0;
		idle = kIdleSeconds;
		return;
	}

	if (!started) {
		x = width * 0.5f;
		y = height * 0.5f;
		started = true;
	}

	bool activity = false;
	uint8_t seen = 0;      // every button that was down at any event this frame
	uint8_t latest = held; // the buttons held at the last event
	MouseEvent me;

	while (mouse->poll(me)) {
		x += me.dx * kSpeed;
		y += me.dy * kSpeed;
		seen |= me.buttons;
		latest = me.buttons;

		if (me.dx || me.dy)
			activity = true;
	}

	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x > width - 1) x = (float)(width - 1);
	if (y > height - 1) y = (float)(height - 1);

	// A press and release inside one frame is still seen, as a one frame hold
	uint8_t pressed = seen & ~held;
	uint8_t now = latest | pressed;

	if (now != held)
		activity = true;

	out.down = pressed;
	out.held = now;
	out.up = held & ~now;
	held = now;

	idle = activity ? 0.0f : idle + dt;

	out.active = (idle < kIdleSeconds);
	out.x = x;
	out.y = y;
}
