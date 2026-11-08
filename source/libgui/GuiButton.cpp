/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiButton.cpp
 ***************************************************************************/

#include <algorithm>
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

	soundOver = GuiSound::getDefaultOver();
	soundHold = nullptr;
	soundClick = GuiSound::getDefaultClick();
	selectable = true;
	holdable = false;
	clickable = true;
}

GuiButton::~GuiButton()
{
}

void GuiButton::setImage(GuiElement* img)
{
	image = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageOver(GuiElement* img)
{
	imageOver = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageHold(GuiElement* img)
{
	imageHold = img;
	if(img) img->setParent(this);
}
void GuiButton::setImageClick(GuiElement* img)
{
	imageClick = img;
	if(img) img->setParent(this);
}
void GuiButton::setIcon(GuiElement* img)
{
	icon = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconOver(GuiElement* img)
{
	iconOver = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconHold(GuiElement* img)
{
	iconHold = img;
	if(img) img->setParent(this);
}
void GuiButton::setIconClick(GuiElement* img)
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
void GuiButton::setState(STATE s, int c)
{
	GuiElement::setState(s, c);

	GuiElement * visuals[] = { image, imageOver, imageHold, imageClick, icon, iconOver, iconHold, iconClick };
	for(GuiElement * e : visuals)
		if(e) e->setState(s, c);
}

void GuiButton::resetState()
{
	GuiElement::resetState();

	GuiElement * visuals[] = { image, imageOver, imageHold, imageClick, icon, iconOver, iconHold, iconClick };
	for(GuiElement * e : visuals)
		if(e) e->setState(state, stateChan);
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
//! How far the button can widen from its designed width `baseW`, in total (see setAutoFit())
int GuiButton::getFitRoom(int baseW)
{
	if(!parentElement)
		return 0;

	// where the button is at its designed width, however far it has been stretched
	const ALIGN_H align = getAlignmentHor();
	int left = getLeft();
	int right = left + width;
	if(align == ALIGN_H::LEFT)
		right = left + baseW;
	else if(align == ALIGN_H::RIGHT)
		left = right - baseW;
	else
	{
		left = (left + right) / 2 - baseW / 2;
		right = left + baseW;
	}

	const int parentLeft = parentElement->getLeft();
	const int parentRight = parentLeft + parentElement->getWidth();
	const int middle = (parentLeft + parentRight) / 2;
	int roomL = left - parentLeft;
	int roomR = parentRight - right;
	if(align == ALIGN_H::LEFT)
		roomR = std::min(roomR, middle - FIT_GAP / 2 - right);
	else if(align == ALIGN_H::RIGHT)
		roomL = std::min(roomL, left - (middle + FIT_GAP / 2));

	GuiWindow * window = parentElement->getAsWindow();
	if(window)
	{
		const int top = getTop();
		const int bottom = top + getHeight();

		for(uint32_t i = 0; i < window->getSize(); i++)
		{
			GuiElement * e = window->getGuiElementAt(i);
			if(!e || e == this || !e->isVisible() || !e->isSelectable())
				continue;

			const int eTop = e->getTop();
			if(eTop >= bottom || eTop + e->getHeight() <= top)
				continue; // not in the same row

			const int eLeft = e->getLeft();
			const int eRight = eLeft + e->getWidth();
			if(eRight <= left)
				roomL = std::min(roomL, (left - eRight - FIT_GAP) / 2);
			else if(eLeft >= right)
				roomR = std::min(roomR, (eLeft - right - FIT_GAP) / 2);
		}
	}

	roomL = std::max(roomL, 0);
	roomR = std::max(roomR, 0);
	if(align == ALIGN_H::LEFT)
		return roomR;
	if(align == ALIGN_H::RIGHT)
		return roomL;
	return 2 * std::min(roomL, roomR);
}

//! Stretches the button to fit its label (see setAutoFit())
void GuiButton::fitToLabel()
{
	GuiText * t = label[0];
	if(!t || !image)
		return;

	int inset = image->getLabelInset();
	if(inset < 0)
		return;

	// nothing to do until the label changes
	if(fitBaseW >= 0 && t->getFontSize() == fitTextFont && t->getTextWidth() == fitTextW)
		return;

	if(fitBaseW < 0)
	{
		fitBaseW = width;
		fitBaseFont = t->getFontSize();
	}
	else
		t->setFontSize(fitBaseFont);

	// The label is centered, so an icon beside it needs the same room at the right. An icon is beside the label when
	// it is all in the left third of the button; a bigger one is a backdrop and the label is drawn over it.
	if(icon)
	{
		const int iconRight = icon->getLeft() - getLeft() + icon->getWidth();
		if(icon->getAlignmentHor() == ALIGN_H::LEFT && iconRight <= fitBaseW / 3)
			inset = std::max(inset, iconRight + FIT_GAP / 2);
	}
	inset = std::min(inset, fitBaseW / 3); // always leave room for the label

	const bool wrapped = t->isWrapped();
	// wrapping is the way to fit more words, so a label that wraps widens the button only for a word that does not fit
	const int need = (wrapped ? t->getLongestWordWidth() : t->getTextWidth()) + 2 * inset;
	const int oldW = width;
	int w = std::max(fitBaseW, need);
	const int maxW = fitBaseW + getFitRoom(fitBaseW);

	if(w > maxW)
	{
		w = maxW;
		if(!wrapped)
		{
			const int minFont = fitBaseFont * FIT_MIN_FONT_PERCENT / 100;
			int s = fitBaseFont;
			while(s > minFont && t->getTextWidth() > w - 2 * inset)
				t->setFontSize(--s);
		}
	}

	const int inner = w - 2 * inset;
	if(wrapped)
		t->setWrap(true, inner);
	else
		t->setMaxWidth(t->getTextWidth() > inner ? inner : 0);

	if(w != oldW)
	{
		GuiElement * images[] = { image, imageOver, imageHold, imageClick };
		for(GuiElement * e : images)
		{
			if(e && e->getWidth() == oldW)
				e->setSize(w, e->getHeight());
		}
		setSize(w, getHeight());
	}

	fitTextFont = t->getFontSize();
	fitTextW = t->getTextWidth();
}

void GuiButton::draw()
{
	if(!this->isVisible())
		return;

	if(autoFit)
		fitToLabel();

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

void GuiButton::update(InputController * controller)
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
