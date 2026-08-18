/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiOptionBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

/**
 * Constructor for the GuiOptionBrowser class.
 */
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
	trigA->setSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, WIIDRC_BUTTON_A);
	trig2 = new GuiTrigger;
	trig2->setSimpleTrigger(-1, WPAD_BUTTON_2, 0, 0);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

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
		optionTxt[i] = new GuiText(nullptr, 20, (GXColor){0, 0, 0, 0xff});
		optionTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		optionTxt[i]->setPosition(8,0);
		optionTxt[i]->setMaxWidth(235);

		optionVal[i] = new GuiText(nullptr, 20, (GXColor){0, 0, 0, 0xff});
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
		optionBtn[i]->setTrigger(trig2);
		optionBtn[i]->setSoundClick(btnSoundClick);
	}
}

/**
 * Destructor for the GuiOptionBrowser class.
 */
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
	delete trig2;
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

void GuiOptionBrowser::update(GuiTrigger * t)
{
	if(state == STATE::DISABLED || !t)
		return;

	int next, prev;

	arrowUpBtn->update(t);
	arrowDownBtn->update(t);

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

	for(int i=0; i<OPTION_PAGESIZE; ++i)
	{
		if(i != selectedItem && optionBtn[i]->getState() == STATE::SELECTED)
			optionBtn[i]->resetState();
		else if(focus && i == selectedItem && optionBtn[i]->getState() == STATE::DEFAULT)
			optionBtn[selectedItem]->setState(STATE::SELECTED, t->chan);

		int currChan = t->chan;

		if(t->wpad->ir.valid && !optionBtn[i]->isInside(t->wpad->ir.x, t->wpad->ir.y))
			t->chan = -1;

		optionBtn[i]->update(t);
		t->chan = currChan;

		if(optionBtn[i]->getState() == STATE::SELECTED)
		{
			selectedItem = i;
		}

		if(selectedItem == i)
			optionTxt[i]->setScroll(SCROLL::HORIZONTAL);
		else
			optionTxt[i]->setScroll(SCROLL::NONE);
	}

	// pad/joystick navigation
	if(!focus)
		return; // skip navigation

	if(t->down() || arrowDownBtn->getState() == STATE::CLICKED)
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
				optionBtn[selectedItem+1]->setState(STATE::SELECTED, t->chan);
				++selectedItem;
			}
		}
		arrowDownBtn->resetState();
	}
	else if(t->up() || arrowUpBtn->getState() == STATE::CLICKED)
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
				optionBtn[selectedItem-1]->setState(STATE::SELECTED, t->chan);
				--selectedItem;
			}
		}
		arrowUpBtn->resetState();
	}

	if(updateCB)
		updateCB(this);
}
