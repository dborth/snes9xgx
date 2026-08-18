#ifndef GUIELEMENT_H
#define GUIELEMENT_H

#include "gui.h"

typedef void (*UpdateCallback)(void * e);

#define EFFECT_SLIDE_TOP			1
#define EFFECT_SLIDE_BOTTOM			2
#define EFFECT_SLIDE_RIGHT			4
#define EFFECT_SLIDE_LEFT			8
#define EFFECT_SLIDE_IN				16
#define EFFECT_SLIDE_OUT			32
#define EFFECT_FADE					64
#define EFFECT_SCALE				128
#define EFFECT_COLOR_TRANSITION		256

//!Primary GUI class. Most other classes inherit from this class.
class GuiElement
{
	public:
		//!Constructor
		GuiElement();
		//!Destructor
		virtual ~GuiElement();
		//!Set the element's parent
		//!\param e Pointer to parent element
		void setParent(GuiElement * e);
		//!Gets the element's parent
		//!\return Pointer to parent element
		GuiElement * getParent();
		//!Gets the current leftmost coordinate of the element
		//!Considers horizontal alignment, x offset, width, and parent element's GetLeft() / GetWidth() values
		//!\return left coordinate
		int getLeft();
		//!Gets the current topmost coordinate of the element
		//!Considers vertical alignment, y offset, height, and parent element's GetTop() / GetHeight() values
		//!\return top coordinate
		int getTop();
		//!Sets the minimum y offset of the element
		//!\param y Y offset
		void setMinY(int y);
		//!Gets the minimum y offset of the element
		//!\return Minimum Y offset
		int getMinY();
		//!Sets the maximum y offset of the element
		//!\param y Y offset
		void setMaxY(int y);
		//!Gets the maximum y offset of the element
		//!\return Maximum Y offset
		int getMaxY();
		//!Sets the minimum x offset of the element
		//!\param x X offset
		void setMinX(int x);
		//!Gets the minimum x offset of the element
		//!\return Minimum X offset
		int getMinX();
		//!Sets the maximum x offset of the element
		//!\param x X offset
		void setMaxX(int x);
		//!Gets the maximum x offset of the element
		//!\return Maximum X offset
		int getMaxX();
		//!Gets the current width of the element. Does not currently consider the scale
		//!\return width
		int getWidth();
		//!Gets the height of the element. Does not currently consider the scale
		//!\return height
		int getHeight();
		//!Sets the size (width/height) of the element
		//!\param w Width of element
		//!\param h Height of element
		void setSize(int w, int h);
		//!Checks whether or not the element is visible
		//!\return true if visible, false otherwise
		bool isVisible();
		//!Checks whether or not the element is selectable
		//!\return true if selectable, false otherwise
		bool isSelectable();
		//!Checks whether or not the element is clickable
		//!\return true if clickable, false otherwise
		bool isClickable();
		//!Checks whether or not the element is holdable
		//!\return true if holdable, false otherwise
		bool isHoldable();
		//!Sets whether or not the element is selectable
		//!\param s Selectable
		void setSelectable(bool s);
		//!Sets whether or not the element is clickable
		//!\param c Clickable
		void setClickable(bool c);
		//!Sets whether or not the element is holdable
		//!\param h Holdable
		void setHoldable(bool h);
		//!Gets the element's current state
		//!\return state
		int getState();
		//!Gets the controller channel that last changed the element's state
		//!\return Channel number (0-3, -1 = no channel)
		int getStateChan();
		//!Sets the element's alpha value
		//!\param a alpha value
		void setAlpha(int a);
		//!Gets the element's alpha value
		//!Considers alpha, alphaDyn, and the parent element's GetAlpha() value
		//!\return alpha
		int getAlpha();
		//!Sets the element's x and y scale
		//!\param s scale (1 is 100%)
		void setScale(float s);
		//!Sets the element's x scale
		//!\param s scale (1 is 100%)
		void setScaleX(float s);
		//!Sets the element's y scale
		//!\param s scale (1 is 100%)
		void setScaleY(float s);
		//!Sets the element's x and y scale, using the provided max width/height
		//!\param w Maximum width
		//!\param h Maximum height
		void setScale(int w, int h);
		//!Gets the element's current scale
		//!Considers scale, scaleDyn, and the parent element's GetScale() value
		float getScale();
		//!Gets the element's current x scale
		//!Considers scale, scaleDyn, and the parent element's GetScale() value
		float getScaleX();
		//!Gets the element's current y scale
		//!Considers scale, scaleDyn, and the parent element's GetScale() value
		float getScaleY();
		//!Set a new GuiTrigger for the element
		//!\param t Pointer to GuiTrigger
		void setTrigger(GuiTrigger * t);
		//!\overload
		//!\param i Index of trigger array to set
		//!\param t Pointer to GuiTrigger
		void setTrigger(u8 i, GuiTrigger * t);
		//!Checks whether rumble was requested by the element
		//!\return true is rumble was requested, false otherwise
		bool isRumble();
		//!Sets whether or not the element is requesting a rumble event
		//!\param r true if requesting rumble, false if not
		void setRumble(bool r);
		//!Set an effect for the element
		//!\param e Effect to enable
		//!\param a Amount of the effect (usage varies on effect)
		//!\param t Target amount of the effect (usage varies on effect)
		void setEffect(int e, int a, int t=0);
		//!Sets an effect to be enabled on wiimote cursor over
		//!\param e Effect to enable
		//!\param a Amount of the effect (usage varies on effect)
		//!\param t Target amount of the effect (usage varies on effect)
		void setEffectOnOver(int e, int a, int t=0);
		//!Shortcut to SetEffectOnOver(EFFECT_SCALE, 4, 110)
		void setEffectGrow();
		//!Gets the current element effects
		//!\return element effects
		int getEffect();
		//!Checks whether the specified coordinates are within the element's boundaries
		//!\param x X coordinate
		//!\param y Y coordinate
		//!\return true if contained within, false otherwise
		bool isInside(int x, int y);
		//!Sets the element's position
		//!\param x X coordinate
		//!\param y Y coordinate
		void setPosition(int x, int y);
		//!Updates the element's effects (dynamic values)
		//!Called by Draw(), used for animation purposes
		void updateEffects();
		//!Sets a function to called after after Update()
		//!Callback function can be used to response to changes in the state of the element, and/or update the element's attributes
		void setUpdateCallback(UpdateCallback u);
		//!Checks whether the element is in focus
		//!\return true if element is in focus, false otherwise
		int isFocused();
		//!Sets the element's visibility
		//!\param v Visibility (true = visible)
		virtual void setVisible(bool v);
		//!Sets the element's focus
		//!\param f Focus (true = in focus)
		virtual void setFocus(int f);
		//!Sets the element's state
		//!\param s State (STATE_DEFAULT, STATE_SELECTED, STATE_CLICKED, STATE_DISABLED)
		//!\param c Controller channel (0-3, -1 = none)
		virtual void setState(int s, int c = -1);
		//!Resets the element's state to STATE_DEFAULT
		virtual void resetState();
		//!Gets whether or not the element is in STATE_SELECTED
		//!\return true if selected, false otherwise
		virtual int getSelected();
		//!Sets the element's alignment respective to its parent element
		//!\param hor Horizontal alignment (ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTRE)
		//!\param vert Vertical alignment (ALIGN_TOP, ALIGN_BOTTOM, ALIGN_MIDDLE)
		virtual void setAlignment(int hor, int vert);
		//!Called when the language has changed, to obtain new text values for all text elements
		virtual void resetText();
		//!Called constantly to allow the element to respond to the current input data
		//!\param t Pointer to a GuiTrigger, containing the current input data from PAD/WPAD
		virtual void update(GuiTrigger * t);
		//!Called constantly to redraw the element
		virtual void draw();
		//!Called constantly to redraw the element's tooltip
		virtual void drawTooltip();
	protected:
		GuiTrigger * trigger[5]; //!< GuiTriggers (input actions) that this element responds to
		UpdateCallback updateCB; //!< Callback function to call when this element is updated
		GuiElement * parentElement; //!< Parent element
		int focus; //!< Element focus (-1 = focus disabled, 0 = not focused, 1 = focused)
		int width; //!< Element width
		int height; //!< Element height
		int xoffset; //!< Element X offset
		int yoffset; //!< Element Y offset
		int ymin; //!< Element's min Y offset allowed
		int ymax; //!< Element's max Y offset allowed
		int xmin; //!< Element's min X offset allowed
		int xmax; //!< Element's max X offset allowed
		int xoffsetDyn; //!< Element X offset, dynamic (added to xoffset value for animation effects)
		int yoffsetDyn; //!< Element Y offset, dynamic (added to yoffset value for animation effects)
		int alpha; //!< Element alpha value (0-255)
		int alphaDyn; //!< Element alpha, dynamic (multiplied by alpha value for blending/fading effects)
		f32 xscale; //!< Element X scale (1 = 100%)
		f32 yscale; //!< Element Y scale (1 = 100%)
		f32 scaleDyn; //!< Element scale, dynamic (multiplied by alpha value for blending/fading effects)
		int effects; //!< Currently enabled effect(s). 0 when no effects are enabled
		int effectAmount; //!< Effect amount. Used by different effects for different purposes
		int effectTarget; //!< Effect target amount. Used by different effects for different purposes
		int effectsOver; //!< Effects to enable when wiimote cursor is over this element. Copied to effects variable on over event
		int effectAmountOver; //!< EffectAmount to set when wiimote cursor is over this element
		int effectTargetOver; //!< EffectTarget to set when wiimote cursor is over this element
		int alignmentHor; //!< Horizontal element alignment, respective to parent element (LEFT, RIGHT, CENTRE)
		int alignmentVert; //!< Horizontal element alignment, respective to parent element (TOP, BOTTOM, MIDDLE)
		int state; //!< Element state (DEFAULT, SELECTED, CLICKED, DISABLED)
		int stateChan; //!< Which controller channel is responsible for the last change in state
		bool selectable; //!< Whether or not this element selectable (can change to SELECTED state)
		bool clickable; //!< Whether or not this element is clickable (can change to CLICKED state)
		bool holdable; //!< Whether or not this element is holdable (can change to HELD state)
		bool visible; //!< Visibility of the element. If false, Draw() is skipped
		bool rumble; //!< Wiimote rumble (on/off) - set to on when this element requests a rumble event
};

#endif // GUIELEMENT_H
