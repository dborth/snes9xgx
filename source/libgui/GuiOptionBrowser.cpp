/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiOptionBrowser.cpp
 ***************************************************************************/

#include "Gui.h"

GuiOptionBrowser::GuiOptionBrowser(int w, int h, OptionList * l)
{
	width = w;
	height = h;
	options = l;
	selectable = true;
	listOffset = this->findMenuItem(-1, 1);
	listChanged = true; // trigger an initial list update
	selectedItem = 0;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND::PCM);

	bgOptions = new GuiImageData(bg_options_png);
	bgOptionsImg = new GuiImage(bgOptions);
	bgOptionsImg->setParent(this);
	bgOptionsImg->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

	bgOptionsEntry = new GuiImageData(bg_options_entry_png);

	scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
	scrollbarImg->setParent(this);
	scrollbarImg->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	scrollbarImg->setPosition(0, 30);

	arrowDown = new GuiImageData(scrollbar_arrowdown_png);
	arrowDownImg = new GuiImage(arrowDown);
	arrowDownOver = new GuiImageData(scrollbar_arrowdown_over_png);
	arrowDownOverImg = new GuiImage(arrowDownOver);
	arrowUp = new GuiImageData(scrollbar_arrowup_png);
	arrowUpImg = new GuiImage(arrowUp);
	arrowUpOver = new GuiImageData(scrollbar_arrowup_over_png);
	arrowUpOverImg = new GuiImage(arrowUpOver);

	arrowUpBtn = new GuiButton(arrowUpImg->getWidth(), arrowUpImg->getHeight());
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpImg);
	arrowUpBtn->setImageOver(arrowUpOverImg);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setTrigger(trigA);
	arrowUpBtn->setSoundOver(btnSoundOver);
	arrowUpBtn->setSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(arrowDownImg->getWidth(), arrowDownImg->getHeight());
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownImg);
	arrowDownBtn->setImageOver(arrowDownOverImg);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setTrigger(trigA);
	arrowDownBtn->setSoundOver(btnSoundOver);
	arrowDownBtn->setSoundClick(btnSoundClick);

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		optionTxt[i] = new GuiText(nullptr, 20, (GuiColor){0, 0, 0, 0xff});
		optionTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		optionTxt[i]->setPosition(8,0);
		optionTxt[i]->setMaxWidth(235);

		optionVal[i] = new GuiText(nullptr, 20, (GuiColor){0, 0, 0, 0xff});
		optionVal[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		optionVal[i]->setPosition(250,0);

		optionBg[i] = new GuiImage(bgOptionsEntry);

		optionBtn[i] = new GuiButton(512,30);
		optionBtn[i]->setParent(this);
		optionBtn[i]->setLabel(optionTxt[i], 0);
		optionBtn[i]->setLabel(optionVal[i], 1);
		optionBtn[i]->setImageOver(optionBg[i]);
		optionBtn[i]->setPosition(0,30*i+3);
		optionBtn[i]->setTrigger(trigA);
		optionBtn[i]->setSoundClick(btnSoundClick);
	}
}

GuiOptionBrowser::~GuiOptionBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;

	delete bgOptionsImg;
	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;

	delete bgOptions;
	delete bgOptionsEntry;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;

	delete trigA;
	delete btnSoundOver;
	delete btnSoundClick;

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		delete optionTxt[i];
		delete optionVal[i];
		delete optionBg[i];
		delete optionBtn[i];
	}
}

void GuiOptionBrowser::setCol1Position(int x)
{
	for(int i=0; i<OPTION_PAGESIZE; i++)
		optionTxt[i]->setPosition(x,0);
}

void GuiOptionBrowser::setCol2Position(int x)
{
	for(int i=0; i<OPTION_PAGESIZE; i++)
		optionVal[i]->setPosition(x,0);
}

void GuiOptionBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<OPTION_PAGESIZE; i++)
		optionBtn[i]->resetState();

	if(f == 1)
		optionBtn[selectedItem]->setState(STATE::SELECTED);
}

void GuiOptionBrowser::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		optionBtn[i]->resetState();
	}
}

int GuiOptionBrowser::getClickedOption()
{
	int found = -1;
	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		if(optionBtn[i]->getState() == STATE::CLICKED)
		{
			optionBtn[i]->setState(STATE::SELECTED);
			found = optionIndex[i];
			break;
		}
	}
	return found;
}

/****************************************************************************
 * FindMenuItem
 *
 * Help function to find the next visible menu item on the list
 ***************************************************************************/

int GuiOptionBrowser::findMenuItem(int currentItem, int direction)
{
	int nextItem = currentItem + direction;

	if(nextItem < 0 || nextItem >= options->length)
		return -1;

	if(strlen(options->name[nextItem]) > 0)
		return nextItem;
	else
		return findMenuItem(nextItem, direction);
}

/**
 * Draw the button on screen
 */
void GuiOptionBrowser::draw()
{
	if(!this->isVisible())
		return;

	bgOptionsImg->draw();

	int next = listOffset;

	for(int i=0; i<OPTION_PAGESIZE; ++i)
	{
		if(next >= 0)
		{
			optionBtn[i]->draw();
			next = this->findMenuItem(next, 1);
		}
		else
			break;
	}

	scrollbarImg->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();

	this->updateEffects();
}

void GuiOptionBrowser::triggerUpdate()
{
	listChanged = true;
}

void GuiOptionBrowser::resetText()
{
	int next = listOffset;

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		if(next >= 0)
		{
			optionBtn[i]->resetText();
			next = this->findMenuItem(next, 1);
		}
		else
			break;
	}
}

void GuiOptionBrowser::update(GuiInputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int next, prev;

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);

	next = listOffset;

	if(listChanged)
	{
		listChanged = false;
		for(int i=0; i<OPTION_PAGESIZE; ++i)
		{
			if(next >= 0)
			{
				if(optionBtn[i]->getState() == STATE::DISABLED)
				{
					optionBtn[i]->setVisible(true);
					optionBtn[i]->setState(STATE::DEFAULT);
				}

				optionTxt[i]->setText(options->name[next]);
				optionVal[i]->setText(options->value[next]);
				optionIndex[i] = next;
				next = this->findMenuItem(next, 1);
			}
			else
			{
				optionBtn[i]->setVisible(false);
				optionBtn[i]->setState(STATE::DISABLED);
			}
		}
	}

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();
	// A GuiInputController with no live signal this frame (e.g. a persistent
	// but currently-disconnected controller slot) must not be able to claim
	// or evict a selection just by taking its turn through this loop -- only
	// a channel that's genuinely doing something (pointing or pressing) gets
	// a say in who owns the selected slot.
	bool channelActive = pad.validPointer || pad.buttons_d != 0 || pad.buttons_h != 0;

	for(int i=0; i<OPTION_PAGESIZE; ++i)
	{
		if(i != selectedItem && optionBtn[i]->getState() == STATE::SELECTED)
			optionBtn[i]->resetState();
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::DEFAULT)
			optionBtn[selectedItem]->setState(STATE::SELECTED, currentChan);
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::SELECTED &&
			optionBtn[i]->getStateChan() != -1 && optionBtn[i]->getStateChan() != currentChan)
			// Slot is already SELECTED but carries a stale channel from an earlier
			// selection (e.g. a reused slot after paging, or a preselected item that
			// was never actually hovered by the real channel yet). Without this,
			// currentChan == stateChan never holds and clicks are silently ignored
			// until the item happens to be navigated away from and back.
			optionBtn[i]->resetState();

		// Present a "no channel" (-1) identity to any item the cursor isn't
		// currently over. Without this, a stale stateChan left on a reused
		// list slot (e.g. after paging/navigating) can permanently block
		// clicks from the real channel until the item is re-hovered.
		if(pad.validPointer && !optionBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
			controller->setChannel(-1);

		optionBtn[i]->update(controller);
		controller->setChannel(currentChan);

		if(optionBtn[i]->getState() == STATE::SELECTED)
			selectedItem = i;

		if(selectedItem == i) {
			optionTxt[i]->setScroll(SCROLL::HORIZONTAL);
			optionVal[i]->setScroll(SCROLL::HORIZONTAL);
		}
		else
		{
			optionTxt[i]->setScroll(SCROLL::NONE);
			optionVal[i]->setScroll(SCROLL::NONE);
		}
	}

	// pad/joystick navigation
	if(!focus)
		return; // skip navigation

	if(controller->down() || arrowDownBtn->getState() == STATE::CLICKED)
	{
		next = this->findMenuItem(optionIndex[selectedItem], 1);

		if(next >= 0)
		{
			if(selectedItem == OPTION_PAGESIZE-1)
			{
				// move list down by 1
				listOffset = this->findMenuItem(listOffset, 1);
				listChanged = true;
			}
			else if(optionBtn[selectedItem+1]->isVisible())
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem+1]->setState(STATE::SELECTED, controller->getChannel());
				++selectedItem;
			}
		}
		arrowDownBtn->resetState();
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		prev = this->findMenuItem(optionIndex[selectedItem], -1);

		if(prev >= 0)
		{
			if(selectedItem == 0)
			{
				// move list up by 1
				listOffset = prev;
				listChanged = true;
			}
			else
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem-1]->setState(STATE::SELECTED, controller->getChannel());
				--selectedItem;
			}
		}
		arrowUpBtn->resetState();
	}

	if(updateCB)
		updateCB(this);
}
