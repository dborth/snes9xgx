/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTrigger.h
 *
 * Menu input trigger management.
 * Acts as a generic UI condition matcher for GuiElements
 ***************************************************************************/
#pragma once

#include <cstdint>

enum class TRIGGER_TYPE {
	SIMPLE,
	HELD,
	BUTTON_ONLY,
	BUTTON_ONLY_IN_FOCUS
};

enum class TRIGGER_ACTION {
	NONE,      // Explicit button mask provided
	PRIMARY,   // Semantic Accept: A (Vertical) or 2 (Sideways)
	SECONDARY  // Semantic Cancel: B (Vertical) or 1 (Sideways)
};

class GuiTrigger {
public:
	GuiTrigger();
	~GuiTrigger() = default;

	//! Semantic Triggers
	// Automatically resolves to A/2 or B/1 based on controller orientation
	void setPrimaryTrigger(int ch = -1);
	void setSecondaryTrigger(int ch = -1);

	//! Sets a simple trigger. Requires: element is selected, and trigger button is pressed
	//!\param ch Controller channel number (-1 for any channel)
	//!\param buttonMask Logical GuiButton bitmask
	void setSimpleTrigger(int ch, uint32_t buttonMask);

	//! Sets a held trigger. Requires: element is selected, and trigger button is held
	//!\param ch Controller channel number (-1 for any channel)
	//!\param buttonMask Logical GuiButton bitmask
	void setHeldTrigger(int ch, uint32_t buttonMask);

	//! Sets a button-only trigger. Requires: Trigger button is pressed
	//!\param ch Controller channel number (-1 for any channel)
	//!\param buttonMask Logical GuiButton bitmask
	void setButtonOnlyTrigger(int ch, uint32_t buttonMask);

	//! Sets a button-only trigger. Requires: trigger button is pressed and parent window is in focus
	//!\param ch Controller channel number (-1 for any channel)
	//!\param buttonMask Logical GuiButton bitmask
	void setButtonOnlyInFocusTrigger(int ch, uint32_t buttonMask);

	//! Evaluation methods
	bool isClicked(const InputController* controller) const;
	bool isHeld(const InputController* controller) const;
	bool isReleased(const InputController* controller) const;

	//! Accessors
	TRIGGER_TYPE getType() const { return type; }
	int getChannel() const { return chan; }

private:
	TRIGGER_TYPE type;
	TRIGGER_ACTION action;
	int chan;
	uint32_t conditionMask;

	//! Dynamically calculates the required bitmask based on orientation
	uint32_t resolveMask(const InputController* controller) const;
};
