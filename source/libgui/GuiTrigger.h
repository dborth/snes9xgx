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

//!How a GuiTrigger matches input: simple, held, button-only, or button-only-in-focus.
//!\ingroup grp_core
enum class TRIGGER_TYPE {
	SIMPLE,
	HELD,
	BUTTON_ONLY,
	BUTTON_ONLY_IN_FOCUS
};

//!Whether a GuiTrigger uses an explicit button mask or a semantic primary (Accept) / secondary (Cancel) action.
//!\ingroup grp_core
enum class TRIGGER_ACTION {
	NONE,      // Explicit button mask provided
	PRIMARY,   // Semantic Accept: A (Vertical) or 2 (Sideways)
	SECONDARY  // Semantic Cancel: B (Vertical) or 1 (Sideways)
};

//!Input-condition matcher attached to a GuiElement (via setSimpleTrigger()
//!etc.) and evaluated against an InputController - the isClicked()/
//!isHeld()/isReleased() methods GuiElement::update() calls each frame.
//!\ingroup grp_core
class GuiTrigger {
public:
	GuiTrigger();
	~GuiTrigger() = default;

	//! Semantic Triggers
	// Automatically resolves to A/2 or B/1 based on controller orientation
	void setPrimaryTrigger(int ch = -1);
	//!\param ch Controller channel number (-1 for any channel)
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

	//!\return true if this trigger's condition is currently met on controller
	bool isClicked(const InputController* controller) const;
	//!\return true if this trigger's condition is currently met on controller
	bool isHeld(const InputController* controller) const;
	//!\return true if this trigger's condition was met on controller last frame but not this frame
	bool isReleased(const InputController* controller) const;

	//! Accessors
	TRIGGER_TYPE getType() const { return type; }
	//!\return the channel this trigger was configured for (-1 for any channel)
	int getChannel() const { return chan; }

private:
	TRIGGER_TYPE type;
	TRIGGER_ACTION action;
	int chan;
	uint32_t conditionMask;

	//! Dynamically calculates the required bitmask based on orientation
	uint32_t resolveMask(const InputController* controller) const;
};
