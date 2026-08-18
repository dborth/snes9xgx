/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiElement.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

/**
 * Constructor for the Object class.
 */
GuiElement::GuiElement()
{
	xoffset = 0;
	yoffset = 0;
	xmin = 0;
	xmax = 0;
	ymin = 0;
	ymax = 0;
	width = 0;
	height = 0;
	alpha = 255;
	xscale = 1;
	yscale = 1;
	state = STATE::DEFAULT;
	stateChan = -1;
	trigger[0] = nullptr;
	trigger[1] = nullptr;
	trigger[2] = nullptr;
	trigger[3] = nullptr;
	trigger[4] = nullptr;
	parentElement = nullptr;
	rumble = true;
	selectable = false;
	clickable = false;
	holdable = false;
	visible = true;
	focus = -1; // cannot be focused
	updateCB = nullptr;
	yoffsetDyn = 0;
	xoffsetDyn = 0;
	alphaDyn = -1;
	scaleDyn = 1;
	effects = 0;
	effectAmount = 0;
	effectTarget = 0;
	effectsOver = 0;
	effectAmountOver = 0;
	effectTargetOver = 0;

	// default alignment - align to top left
	alignmentVert = ALIGN_V::TOP;
	alignmentHor = ALIGN_H::LEFT;
}

/**
 * Destructor for the GuiElement class.
 */
GuiElement::~GuiElement()
{
}

void GuiElement::setParent(GuiElement * e)
{
	parentElement = e;
}

GuiElement * GuiElement::getParent()
{
	return parentElement;
}

int GuiElement::getLeft()
{
	int x = 0;
	int pWidth = 0;
	int pLeft = 0;

	if(parentElement)
	{
		pWidth = parentElement->getWidth();
		pLeft = parentElement->getLeft();
	}

	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
		pLeft += xoffsetDyn;

	switch(alignmentHor)
	{
		case ALIGN_H::LEFT:
			x = pLeft;
			break;
		case ALIGN_H::CENTRE:
			x = pLeft + pWidth/2.0 - (width*xscale)/2.0;
			break;
		case ALIGN_H::RIGHT:
			x = pLeft + pWidth - width*xscale;
			break;
	}
	x += (width*(xscale - 1))/2.0; // correct offset for scaled images
	return x + xoffset;
}

int GuiElement::getTop()
{
	int y = 0;
	int pHeight = 0;
	int pTop = 0;

	if(parentElement)
	{
		pHeight = parentElement->getHeight();
		pTop = parentElement->getTop();
	}

	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
		pTop += yoffsetDyn;

	switch(alignmentVert)
	{
		case ALIGN_V::TOP:
			y = pTop;
			break;
		case ALIGN_V::MIDDLE:
			y = pTop + pHeight/2.0 - (height*yscale)/2.0;
			break;
		case ALIGN_V::BOTTOM:
			y = pTop + pHeight - height*yscale;
			break;
	}
	y += (height*(yscale - 1))/2.0; // correct offset for scaled images
	return y + yoffset;
}

void GuiElement::setMinX(int x)
{
	xmin = x;
}

int GuiElement::getMinX()
{
	return xmin;
}

void GuiElement::setMaxX(int x)
{
	xmax = x;
}

int GuiElement::getMaxX()
{
	return xmax;
}

void GuiElement::setMinY(int y)
{
	ymin = y;
}

int GuiElement::getMinY()
{
	return ymin;
}

void GuiElement::setMaxY(int y)
{
	ymax = y;
}

int GuiElement::getMaxY()
{
	return ymax;
}

int GuiElement::getWidth()
{
	return width;
}

int GuiElement::getHeight()
{
	return height;
}

void GuiElement::setSize(int w, int h)
{

	width = w;
	height = h;
}

bool GuiElement::isVisible()
{
	return visible;
}

void GuiElement::setVisible(bool v)
{
	visible = v;
}

void GuiElement::setAlpha(int a)
{
	alpha = a;
}

int GuiElement::getAlpha()
{
	int a = alpha;

	if(alphaDyn >= 0)
		a = alphaDyn;

	if(parentElement)
		a *= float(parentElement->getAlpha())/255.0f;

	return a;
}

void GuiElement::setScale(float s)
{
	xscale = s;
	yscale = s;
}

void GuiElement::setScaleX(float s)
{
	xscale = s;
}

void GuiElement::setScaleY(float s)
{
	yscale = s;
}

void GuiElement::setScale(int mw, int mh)
{
	xscale = 1.0f;
	if(width > mw || height > mh)
	{
		if(width/(height*1.0) > mw/(mh*1.0))
			xscale = mw/(width*1.0);
		else
			xscale = mh/(height*1.0);
	}
	yscale = xscale;
}

float GuiElement::getScale()
{
	float s = xscale * scaleDyn;

	if(parentElement)
		s *= parentElement->getScale();

	return s;
}

float GuiElement::getScaleX()
{
	float s = xscale * scaleDyn;

	if(parentElement)
		s *= parentElement->getScale();

	return s;
}

float GuiElement::getScaleY()
{
	float s = yscale * scaleDyn;

	if(parentElement)
		s *= parentElement->getScaleY();

	return s;
}

STATE GuiElement::getState()
{
	return state;
}

int GuiElement::getStateChan()
{
	return stateChan;
}

void GuiElement::setState(STATE s, int c)
{
	state = s;
	stateChan = c;
}

void GuiElement::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}
}

void GuiElement::setClickable(bool c)
{
	clickable = c;
}

void GuiElement::setSelectable(bool s)
{
	selectable = s;
}

void GuiElement::setHoldable(bool d)
{
	holdable = d;
}

bool GuiElement::isSelectable()
{
	if(state == STATE::DISABLED || state == STATE::CLICKED)
		return false;
	else
		return selectable;
}

bool GuiElement::isClickable()
{
	if(state == STATE::DISABLED ||
		state == STATE::CLICKED ||
		state == STATE::HELD)
		return false;
	else
		return clickable;
}

bool GuiElement::isHoldable()
{
	if(state == STATE::DISABLED)
		return false;
	else
		return holdable;
}

void GuiElement::setFocus(int f)
{
	focus = f;
}

int GuiElement::isFocused()
{
	return focus;
}

void GuiElement::setTrigger(GuiTrigger * t)
{
	if(!trigger[0])
		trigger[0] = t;
	else if(!trigger[1])
		trigger[1] = t;
	else if(!trigger[2])
		trigger[2] = t;
	else if(!trigger[3])
		trigger[3] = t;
	else if(!trigger[4])
		trigger[4] = t;
	else // all were assigned, so we'll just overwrite the first one
		trigger[0] = t;
}

void GuiElement::setTrigger(u8 i, GuiTrigger * t)
{
	trigger[i] = t;
}

bool GuiElement::isRumble()
{
	return rumble;
}

void GuiElement::setRumble(bool r)
{
	rumble = r;
}

int GuiElement::getEffect()
{
	return effects;
}

void GuiElement::setEffect(int eff, int amount, int target)
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
	if(eff & EFFECT_FADE)
	{
		if(amount > 0)
			alphaDyn = 0;
		else if(amount < 0)
			alphaDyn = alpha;
	}

	effects |= eff;
	effectAmount = amount;
	effectTarget = target;
}

void GuiElement::setEffectOnOver(int eff, int amount, int target)
{
	effectsOver |= eff;
	effectAmountOver = amount;
	effectTargetOver = target;
}

void GuiElement::setEffectGrow()
{
	setEffectOnOver(EFFECT_SCALE, 4, 110);
}

void GuiElement::updateEffects()
{
	if(effects & (EFFECT_SLIDE_IN | EFFECT_SLIDE_OUT))
	{
		if(effects & EFFECT_SLIDE_IN)
		{
			if(effects & EFFECT_SLIDE_LEFT)
			{
				xoffsetDyn += effectAmount;

				if(xoffsetDyn >= 0)
				{
					xoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_RIGHT)
			{
				xoffsetDyn -= effectAmount;

				if(xoffsetDyn <= 0)
				{
					xoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_TOP)
			{
				yoffsetDyn += effectAmount;

				if(yoffsetDyn >= 0)
				{
					yoffsetDyn = 0;
					effects = 0;
				}
			}
			else if(effects & EFFECT_SLIDE_BOTTOM)
			{
				yoffsetDyn -= effectAmount;

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
				xoffsetDyn -= effectAmount;

				if(xoffsetDyn <= -screenwidth)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_RIGHT)
			{
				xoffsetDyn += effectAmount;

				if(xoffsetDyn >= screenwidth)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_TOP)
			{
				yoffsetDyn -= effectAmount;

				if(yoffsetDyn <= -screenheight)
					effects = 0; // shut off effect
			}
			else if(effects & EFFECT_SLIDE_BOTTOM)
			{
				yoffsetDyn += effectAmount;

				if(yoffsetDyn >= screenheight)
					effects = 0; // shut off effect
			}
		}
	}
	if(effects & EFFECT_FADE)
	{
		alphaDyn += effectAmount;

		if(effectAmount < 0 && alphaDyn <= 0)
		{
			alphaDyn = 0;
			effects = 0; // shut off effect
		}
		else if(effectAmount > 0 && alphaDyn >= alpha)
		{
			alphaDyn = alpha;
			effects = 0; // shut off effect
		}
	}
	if(effects & EFFECT_SCALE)
	{
		scaleDyn += f32(effectAmount)*0.01f;
		f32 effTar100 = f32(effectTarget)*0.01f;

		if((effectAmount < 0 && scaleDyn <= effTar100)
			|| (effectAmount > 0 && scaleDyn >= effTar100))
		{
			scaleDyn = effTar100;
			effects = 0; // shut off effect
		}
	}
}

void GuiElement::update(GuiTrigger * t)
{
	if(updateCB)
		updateCB(this);
}

void GuiElement::setUpdateCallback(UpdateCallback u)
{
	updateCB = u;
}

void GuiElement::setPosition(int xoff, int yoff)
{
	xoffset = xoff;
	yoffset = yoff;
}

void GuiElement::setAlignment(ALIGN_H hor, ALIGN_V vert)
{
	alignmentHor = hor;
	alignmentVert = vert;
}

int GuiElement::getSelected()
{
	return -1;
}

void GuiElement::resetText()
{
}

void GuiElement::draw()
{
}

void GuiElement::drawTooltip()
{
}

bool GuiElement::isInside(int x, int y)
{
	if(unsigned(x - this->getLeft())  < unsigned(width)
	&& unsigned(y - this->getTop())  < unsigned(height))
		return true;
	return false;
}
