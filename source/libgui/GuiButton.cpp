/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiButton.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"
/**
 * Constructor for the GuiButton class.
 */

GuiButton::GuiButton(int w, int h)
{
	width = w;
	height = h;
	image = NULL;
	imageOver = NULL;
	imageHold = NULL;
	imageClick = NULL;
	icon = NULL;
	iconOver = NULL;
	iconHold = NULL;
	iconClick = NULL;

	for(int i=0; i < 3; i++)
	{
		label[i] = NULL;
		labelOver[i] = NULL;
		labelHold[i] = NULL;
		labelClick[i] = NULL;
	}

	soundOver = NULL;
	soundHold = NULL;
	soundClick = NULL;
	tooltip = NULL;
	selectable = true;
	holdable = false;
	clickable = true;
}

/**
 * Destructor for the GuiButton class.
 */
GuiButton::~GuiButton()
{
}

void GuiButton::setImage(GuiImage* img)
{
	image = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageOver(GuiImage* img)
{
	imageOver = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageHold(GuiImage* img)
{
	imageHold = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageClick(GuiImage* img)
{
	imageClick = img;
	if(img) img->setParent(this);
}
void GuiButton::setIcon(GuiImage* img)
{
	icon = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconOver(GuiImage* img)
{
	iconOver = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconHold(GuiImage* img)
{
	iconHold = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconClick(GuiImage* img)
{
	iconClick = img;
	if(img) img->setParent(this);
}
void GuiButton::setLabel(GuiText* txt, int n)
{
	label[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelOver(GuiText* txt, int n)
{
	labelOver[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelHold(GuiText* txt, int n)
{
	labelHold[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelClick(GuiText* txt, int n)
{
	labelClick[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setSoundOver(GuiSound * snd)
{
	soundOver = snd;
}
void GuiButton::setSoundHold(GuiSound * snd)
{
	soundHold = snd;
}
void GuiButton::setSoundClick(GuiSound * snd)
{
	soundClick = snd;
}
void GuiButton::setTooltip(GuiTooltip* t)
{
	tooltip = t;
	if(t)
		tooltip->setParent(this);
}

/**
 * Draw the button on screen
 */
void GuiButton::draw()
{
	if(!this->isVisible())
		return;

	if(state == STATE_SELECTED || state == STATE_HELD)
	{
		if(imageOver)
			imageOver->draw();
		else if(image) // draw image
			image->draw();

		if(iconOver)
			iconOver->draw();
		else if(icon) // draw icon
			icon->draw();

		// draw text
		if(labelOver[0])
			labelOver[0]->draw();
		else if(label[0])
			label[0]->draw();
			
		if(labelOver[1])
			labelOver[1]->draw();
		else if(label[1])
			label[1]->draw();
			
		if(labelOver[2])
			labelOver[2]->draw();
		else if(label[2])
			label[2]->draw();
	}
	else
	{
		if(image) // draw image
			image->draw();
		if(icon) // draw icon
			icon->draw();

		// draw text
		if(label[0])
			label[0]->draw();
		if(label[1])
			label[1]->draw();
		if(label[2])
			label[2]->draw();
	}

	this->updateEffects();
}

void GuiButton::drawTooltip()
{
	if(tooltip)
		tooltip->drawTooltip();
}

void GuiButton::resetText()
{
	for(int i=0; i<3; i++)
	{
		if(label[i])
			label[i]->resetText();
		if(labelOver[i])
			labelOver[i]->resetText();
	}
	if(tooltip)
		tooltip->resetText();
}

void GuiButton::update(GuiTrigger * t)
{
	if(state == STATE_CLICKED || state == STATE_DISABLED || !t)
		return;
	else if(parentElement && parentElement->getState() == STATE_DISABLED)
		return;

	#ifdef HW_RVL
	// cursor
	if(t->wpad->ir.valid && t->chan >= 0)
	{
		if(this->isInside(t->wpad->ir.x, t->wpad->ir.y))
		{
			if(state == STATE_DEFAULT) // we weren't on the button before!
			{
				this->setState(STATE_SELECTED, t->chan);

				if(this->isRumble())
					rumbleRequest[t->chan] = 1;

				if(soundOver)
					soundOver->play();

				if(effectsOver && !effects)
				{
					// initiate effects
					effects = effectsOver;
					effectAmount = effectAmountOver;
					effectTarget = effectTargetOver;
				}
			}
		}
		else
		{
			if(state == STATE_SELECTED && (stateChan == t->chan || stateChan == -1))
				this->resetState();

			if(effectTarget == effectTargetOver && effectAmount == effectAmountOver)
			{
				// initiate effects (in reverse)
				effects = effectsOver;
				effectAmount = -effectAmountOver;
				effectTarget = 100;
			}
		}
	}
	#endif

	// button triggers
	if(this->isClickable())
	{
		s32 wm_btns, wm_btns_trig, cc_btns, cc_btns_trig, wiidrc_btns, wiidrc_btns_trig;
		for(int i=0; i<5; i++)
		{
			if(trigger[i] && (trigger[i]->chan == -1 || trigger[i]->chan == t->chan))
			{
				// higher 16 bits only (wiimote)
				wm_btns = t->wpad->btns_d << 16;
				wm_btns_trig = trigger[i]->wpad->btns_d << 16;

				// lower 16 bits only (classic controller)
				cc_btns = t->wpad->btns_d >> 16;
				cc_btns_trig = trigger[i]->wpad->btns_d >> 16;

				// Wii U Gamepad
				wiidrc_btns = t->wiidrcdata.btns_d;
				wiidrc_btns_trig = trigger[i]->wiidrcdata.btns_d;

				if(
					(t->wpad->btns_d > 0 &&
					(wm_btns == wm_btns_trig ||
					(cc_btns == cc_btns_trig && t->wpad->exp.type == EXP_CLASSIC))) ||
					(t->pad.btns_d == trigger[i]->pad.btns_d && t->pad.btns_d > 0) ||
					(wiidrc_btns == wiidrc_btns_trig && wiidrc_btns > 0))
				{
					if(t->chan == stateChan || stateChan == -1)
					{
						if(state == STATE_SELECTED)
						{
							if(!t->wpad->ir.valid ||	this->isInside(t->wpad->ir.x, t->wpad->ir.y))
							{
								this->setState(STATE_CLICKED, t->chan);

								if(soundClick)
									soundClick->play();
							}
						}
						else if(trigger[i]->type == TRIGGER_BUTTON_ONLY)
						{
							this->setState(STATE_CLICKED, t->chan);
						}
						else if(trigger[i]->type == TRIGGER_BUTTON_ONLY_IN_FOCUS &&
								parentElement->isFocused())
						{
							this->setState(STATE_CLICKED, t->chan);
						}
					}
				}
			}
		}
	}

	if(this->isHoldable())
	{
		bool held = false;
		s32 wm_btns, wm_btns_h, wm_btns_trig, cc_btns, cc_btns_h, cc_btns_trig, wiidrc_btns, wiidrc_btns_h, wiidrc_btns_trig;

		for(int i=0; i<5; i++)
		{
			if(trigger[i] && (trigger[i]->chan == -1 || trigger[i]->chan == t->chan))
			{
				// higher 16 bits only (wiimote)
				wm_btns = t->wpad->btns_d << 16;
				wm_btns_h = t->wpad->btns_h << 16;
				wm_btns_trig = trigger[i]->wpad->btns_h << 16;

				// lower 16 bits only (classic controller)
				cc_btns = t->wpad->btns_d >> 16;
				cc_btns_h = t->wpad->btns_h >> 16;
				cc_btns_trig = trigger[i]->wpad->btns_h >> 16;

				// Wii U Gamepad
				wiidrc_btns = t->wiidrcdata.btns_d;
				wiidrc_btns_h = t->wiidrcdata.btns_h;
				wiidrc_btns_trig = trigger[i]->wiidrcdata.btns_h;

				if(
					(t->wpad->btns_d > 0 &&
					(wm_btns == wm_btns_trig ||
					(cc_btns == cc_btns_trig && t->wpad->exp.type == EXP_CLASSIC))) ||
					(t->pad.btns_d == trigger[i]->pad.btns_h && t->pad.btns_d > 0) ||
					(wiidrc_btns == wiidrc_btns_trig && wiidrc_btns > 0))
				{
					if(trigger[i]->type == TRIGGER_HELD && state == STATE_SELECTED &&
						(t->chan == stateChan || stateChan == -1))
						this->setState(STATE_CLICKED, t->chan);
				}

				if(
					(t->wpad->btns_h > 0 &&
					(wm_btns_h == wm_btns_trig ||
					(cc_btns_h == cc_btns_trig && t->wpad->exp.type == EXP_CLASSIC))) ||
					(t->pad.btns_h == trigger[i]->pad.btns_h && t->pad.btns_h > 0) ||
					(wiidrc_btns_h == wiidrc_btns_trig && wiidrc_btns_h > 0))
				{
					if(trigger[i]->type == TRIGGER_HELD)
						held = true;
				}

				if(!held && state == STATE_HELD && stateChan == t->chan)
				{
					this->resetState();
				}
				else if(held && state == STATE_CLICKED && stateChan == t->chan)
				{
					this->setState(STATE_HELD, t->chan);
				}
			}
		}
	}

	if(updateCB)
		updateCB(this);
}
