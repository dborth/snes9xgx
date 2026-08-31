/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiButton.cpp
 ***************************************************************************/

#include "Gui.h"

GuiButton::GuiButton(int w, int h)
{
	width = w;
	height = h;
	image = nullptr;
	imageOver = nullptr;
	imageHold = nullptr;
	imageClick = nullptr;
	icon = nullptr;
	iconOver = nullptr;
	iconHold = nullptr;
	iconClick = nullptr;

	for(int i=0; i < MAX_BTN_LABELS; i++)
	{
		label[i] = nullptr;
		labelOver[i] = nullptr;
		labelHold[i] = nullptr;
		labelClick[i] = nullptr;
	}

	soundOver = nullptr;
	soundHold = nullptr;
	soundClick = nullptr;
	selectable = true;
	holdable = false;
	clickable = true;
}

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
	if(n >= MAX_BTN_LABELS) return;
	label[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelOver(GuiText* txt, int n)
{
	if(n >= MAX_BTN_LABELS) return;
	labelOver[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelHold(GuiText* txt, int n)
{
	if(n >= MAX_BTN_LABELS) return;
	labelHold[n] = txt;
	if(txt) txt->setParent(this);
}
void GuiButton::setLabelClick(GuiText* txt, int n)
{
	if(n >= MAX_BTN_LABELS) return;
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

/**
 * Draw the button on screen
 */
void GuiButton::draw()
{
	if(!this->isVisible())
		return;

	if(state == STATE::SELECTED || state == STATE::HELD)
	{
		if(imageOver)
			imageOver->draw();
		else if(image) // draw image
			image->draw();

		if(iconOver)
			iconOver->draw();
		else if(icon) // draw icon
			icon->draw();

		for(int i=0; i < MAX_BTN_LABELS; i++) {
			if(labelOver[i])
				labelOver[i]->draw();
			else if(label[i])
				label[i]->draw();
		}
	}
	else
	{
		if(image) // draw image
			image->draw();
		if(icon) // draw icon
			icon->draw();

		for(int i=0; i < MAX_BTN_LABELS; i++) {
			if(label[i])
				label[i]->draw();
		}
	}

	this->updateEffects();
}

void GuiButton::resetText()
{
	for(int i=0; i<MAX_BTN_LABELS; i++)
	{
		if(label[i])
			label[i]->resetText();
		if(labelOver[i])
			labelOver[i]->resetText();
	}
}

void GuiButton::update(GuiInputController * controller)
{
	if(state == STATE::CLICKED || state == STATE::DISABLED || !controller)
		return;
	else if(parentElement && parentElement->getState() == STATE::DISABLED)
		return;

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();

	// cursor
	if(pad.validPointer && currentChan >= 0)
	{
		if(this->isInside(pad.cursor_x, pad.cursor_y))
		{
			if(state == STATE::DEFAULT) // we weren't on the button before!
			{
				this->setState(STATE::SELECTED, currentChan);

				if(this->isRumble())
					platform->getInput()->setRumble(currentChan, true);

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
			if(state == STATE::SELECTED && (stateChan == currentChan || stateChan == -1))
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

	// button triggers
	if(this->isClickable())
	{
		for(int i=0; i<MAX_TRIGGERS; i++)
		{
			if(trigger[i] && trigger[i]->isClicked(controller))
			{
				if(currentChan == stateChan || stateChan == -1)
				{
					if(state == STATE::SELECTED)
					{
						if(!pad.validPointer || this->isInside(pad.cursor_x, pad.cursor_y))
						{
							this->setState(STATE::CLICKED, currentChan);
							if(soundClick)
								soundClick->play();
						}
					}
					else if(trigger[i]->getType() == TRIGGER_TYPE::BUTTON_ONLY)
					{
						this->setState(STATE::CLICKED, currentChan);
					}
					else if(trigger[i]->getType() == TRIGGER_TYPE::BUTTON_ONLY_IN_FOCUS &&
							parentElement->isFocused())
					{
						this->setState(STATE::CLICKED, currentChan);
					}
				}
			}
		}
	}

	if(this->isHoldable())
	{
		bool held = false;

		for(int i=0; i<MAX_TRIGGERS; i++)
		{
			if(trigger[i])
			{
				// Evaluate transition to CLICKED via held trigger types
				if(trigger[i]->isClicked(controller))
				{
					if(trigger[i]->getType() == TRIGGER_TYPE::HELD && state == STATE::SELECTED &&
						(currentChan == stateChan || stateChan == -1))
					{
						this->setState(STATE::CLICKED, currentChan);
					}
				}

				// Evaluate sustained hold
				if(trigger[i]->isHeld(controller))
				{
					if(trigger[i]->getType() == TRIGGER_TYPE::HELD)
						held = true;
				}
			}
		}

		if(!held && state == STATE::HELD && stateChan == currentChan)
		{
			this->resetState();
		}
		else if(held && state == STATE::CLICKED && stateChan == currentChan)
		{
			this->setState(STATE::HELD, currentChan);
		}
	}

	if(updateCB)
		updateCB(this);
}
