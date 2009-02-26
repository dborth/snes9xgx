/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_element.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

/**
 * Constructor for the Object class.
 */
GuiElement::GuiElement()
{
	xoffset = 0;
	yoffset = 0;
	width = 0;
	height = 0;
	state = STATE_DEFAULT;
	trigger[0] = NULL;
	trigger[1] = NULL;
	parentElement = NULL;
	selectable = false;
	clickable = false;
	visible = true;
	focus = -1; // cannot be focused
	updateCB = NULL;
	yoffsetDyn = 0;
	xoffsetDyn = 0;
	effects = 0;

	// default alignment - align to top left
	alignmentVert = ALIGN_TOP;
	alignmentHor = ALIGN_LEFT;
}

/**
 * Destructor for the GuiElement class.
 */
GuiElement::~GuiElement()
{
}

void GuiElement::SetParent(GuiElement * e)
{
	parentElement = e;
}
/**
 * Get the left position of the GuiElement.
 * @see SetLeft()
 * @return Left position in pixel.
 */
int GuiElement::GetLeft()
{
	int x = 0;
	int pWidth = 0;
	int pLeft = 0;

	if(parentElement)
	{
		pWidth = parentElement->GetWidth();
		pLeft = parentElement->GetLeft();
	}

	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
		pLeft += xoffsetDyn;

	switch(alignmentHor)
	{
		case ALIGN_LEFT:
			x = pLeft;
			break;
		case ALIGN_CENTRE:
			x = pLeft + (pWidth/2) - (width/2);
			break;
		case ALIGN_RIGHT:
			x = pLeft + pWidth - width;
			break;
	}
	return x + xoffset;
}

/**
 * Get the top position of the GuiElement.
 * @see SetTop()
 * @return Top position in pixel.
 */
int GuiElement::GetTop()
{
	int y = 0;
	int pHeight = 0;
	int pTop = 0;

	if(parentElement)
	{
		pHeight = parentElement->GetHeight();
		pTop = parentElement->GetTop();
	}

	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
		pTop += yoffsetDyn;

	switch(alignmentVert)
	{
		case ALIGN_TOP:
			y = pTop;
			break;
		case ALIGN_MIDDLE:
			y = pTop + (pHeight/2) - (height/2);
			break;
		case ALIGN_BOTTOM:
			y = pTop + pHeight - height;
			break;
	}
	return y + yoffset;
}

/**
 * Get the width of the GuiElement.
 * @see SetWidth()
 * @return Width of the GuiElement.
 */
int GuiElement::GetWidth()
{
	return width;
}

/**
 * Get the height of the GuiElement.
 * @see SetHeight()
 * @return Height of the GuiElement.
 */
int GuiElement::GetHeight()
{
	return height;
}

/**
 * Set the width and height of the GuiElement.
 * @param[in] Width Width in pixel.
 * @param[in] Height Height in pixel.
 * @see SetWidth()
 * @see SetHeight()
 */
void GuiElement::SetSize(int w, int h)
{

	width = w;
	height = h;
}

/**
 * Get visible.
 * @see SetVisible()
 * @return true if visible, false otherwise.
 */
bool GuiElement::IsVisible()
{
	return visible;
}

/**
 * Set visible.
 * @param[in] Visible Set to true to show GuiElement.
 * @see IsVisible()
 */
void GuiElement::SetVisible(bool v)
{
	visible = v;
}

int GuiElement::GetState()
{
	return state;
}

void GuiElement::SetState(int s)
{
	state = s;
}

void GuiElement::ResetState()
{
	if(state != STATE_DISABLED)
		state = STATE_DEFAULT;
}

void GuiElement::SetClickable(bool c)
{
	clickable = c;
}

void GuiElement::SetSelectable(bool s)
{
	selectable = s;
}

bool GuiElement::IsSelectable()
{
	if(state == STATE_DISABLED || state == STATE_CLICKED)
		return false;
	else
		return selectable;
}

bool GuiElement::IsClickable()
{
	if(state == STATE_DISABLED || state == STATE_CLICKED)
		return false;
	else
		return clickable;
}

void GuiElement::SetFocus(int f)
{
	focus = f;
}

int GuiElement::IsFocused()
{
	return focus;
}

void GuiElement::SetTrigger(GuiTrigger * t)
{
	if(!trigger[0])
		trigger[0] = t;
	else if(!trigger[1])
		trigger[1] = t;
	else // both were assigned, so we'll just overwrite the first one
		trigger[0] = t;
}

void GuiElement::SetTrigger(u8 i, GuiTrigger * t)
{
	trigger[i] = t;
}

int GuiElement::GetEffect()
{
	return effects;
}

void GuiElement::SetEffect(int eff)
{
	if(eff & EFFECT_SLIDE_IN)
	{
		// these calculations overcompensate a little
		if(eff & EFFECT_SLIDE_TOP)
			yoffsetDyn = -screenheight;
		else if(eff & EFFECT_SLIDE_LEFT)
			xoffsetDyn = -screenwidth;
		else if(eff & EFFECT_SLIDE_BOTTOM)
			yoffsetDyn = screenheight;
		else if(eff & EFFECT_SLIDE_RIGHT)
			xoffsetDyn = screenwidth;
	}
	effects |= eff;
}



void GuiElement::UpdateEffects()
{
	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
	{
		if(effects & EFFECT_SLIDE_IN)
		{
			if(effects & EFFECT_SLIDE_LEFT)
			{
				xoffsetDyn += 50;

				if(xoffsetDyn >= 0)
				{
					xoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_RIGHT)
			{
				xoffsetDyn -= 50;

				if(xoffsetDyn <= 0)
				{
					xoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_TOP)
			{
				yoffsetDyn += 50;

				if(yoffsetDyn >= 0)
				{
					yoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_BOTTOM)
			{
				yoffsetDyn -= 50;

				if(yoffsetDyn <= 0)
				{
					yoffsetDyn = 0;
					effects = 0;
				}
			}
		}
		else
		{
			if(effects & EFFECT_SLIDE_LEFT)
			{
				xoffsetDyn -= 50;

				if(xoffsetDyn < -screenwidth)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_RIGHT)
			{
				xoffsetDyn += 50;

				if(xoffsetDyn > screenwidth)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_TOP)
			{
				yoffsetDyn -= 50;

				if(yoffsetDyn < -screenheight)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_BOTTOM)
			{
				yoffsetDyn += 50;

				if(yoffsetDyn > screenheight)
					effects = 0; // shut off effect
			}
		}
	}
}

void GuiElement::Update(GuiTrigger * t)
{
	if(updateCB)
		updateCB(this);
}

void GuiElement::SetUpdateCallback(UpdateCallback u)
{
	updateCB = u;
}

void GuiElement::SetPosition(int xoff, int yoff)
{
	xoffset = xoff;
	yoffset = yoff;
}

void GuiElement::SetAlignment(int hor, int vert)
{
	alignmentHor = hor;
	alignmentVert = vert;
}

int GuiElement::GetSelected()
{
	return -1;
}

/**
 * Draw an element on screen.
 */
void GuiElement::Draw()
{
}

/**
 * Check if a position is inside the GuiElement.
 * @param[in] x X position in pixel.
 * @param[in] y Y position in pixel.
 */
bool GuiElement::IsInside(int x, int y)
{
	if(x > this->GetLeft() && x < (this->GetLeft()+width)
	&& y > this->GetTop() && y < (this->GetTop()+height))
		return true;
	return false;
}
