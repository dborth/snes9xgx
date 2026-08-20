/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiSaveBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "../filebrowser.h"
#include "Gui.h"

/**
 * Constructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::GuiSaveBrowser(int w, int h, SaveList * s, int a)
{
	width = w;
	height = h;
	saves = s;
	action = a;
	selectable = true;

	if(action == 0) // load
		listOffset = 0;
	else if(action == 2) // delete SRAM / State
		listOffset = 0;
	else
		listOffset = -2; // save - reserve -2 & -1 for new slots

	selectedItem = 0;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND::PCM);

	gameSave = new GuiImageData(button_gamesave_png);
	gameSaveOver = new GuiImageData(button_gamesave_over_png);
	gameSaveBlank = new GuiImageData(button_gamesave_blank_png);

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

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveDate[i] = new GuiText(nullptr, 18, (GuiColor){0, 0, 0, 0xff});
		saveDate[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveDate[i]->setPosition(80,5);
		saveTime[i] = new GuiText(nullptr, 18, (GuiColor){0, 0, 0, 0xff});
		saveTime[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveTime[i]->setPosition(80,27);

		saveType[i] = new GuiText(nullptr, 18, (GuiColor){0, 0, 0, 0xff});
		saveType[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveType[i]->setPosition(80,50);

		saveBgImg[i] = new GuiImage(gameSave);
		saveBgOverImg[i] = new GuiImage(gameSaveOver);
		savePreviewImg[i] = new GuiImage(gameSaveBlank);
		savePreviewImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		savePreviewImg[i]->setPosition(5,0);

		saveBtn[i] = new GuiButton(saveBgImg[i]->getWidth(),saveBgImg[i]->getHeight());
		saveBtn[i]->setParent(this);
		saveBtn[i]->setLabel(saveDate[i], 0);
		saveBtn[i]->setLabel(saveTime[i], 1);
		saveBtn[i]->setLabel(saveType[i], 2);
		saveBtn[i]->setImage(saveBgImg[i]);
		saveBtn[i]->setImageOver(saveBgOverImg[i]);
		saveBtn[i]->setIcon(savePreviewImg[i]);
		saveBtn[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveBtn[i]->setPosition(257*(i % 2),87*(i>>1));
		saveBtn[i]->setTrigger(trigA);
		saveBtn[i]->setState(STATE::DISABLED);
		saveBtn[i]->setEffectGrow();
		saveBtn[i]->setVisible(false);
		saveBtn[i]->setSoundOver(btnSoundOver);
		saveBtn[i]->setSoundClick(btnSoundClick);
		saveBtnLastOver[i] = false;
	}
	saveBtn[0]->setState(STATE::SELECTED, -1);
	saveBtn[0]->setVisible(true);
}

/**
 * Destructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::~GuiSaveBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;

	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;

	delete gameSave;
	delete gameSaveOver;
	delete gameSaveBlank;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigA;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		delete saveBtn[i];
		delete saveDate[i];
		delete saveTime[i];
		delete saveType[i];
		delete saveBgImg[i];
		delete saveBgOverImg[i];
		delete savePreviewImg[i];
	}
}

void GuiSaveBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->resetState();

	if(f == 1 && selectedItem >= 0)
		saveBtn[selectedItem]->setState(STATE::SELECTED);
}

void GuiSaveBrowser::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->resetState();
	}
}

int GuiSaveBrowser::getClickedSave()
{
	int found = -3;
	for(int i=0; i<SAVELISTSIZE; i++)
	{
		if(saveBtn[i]->getState() == STATE::CLICKED)
		{
			saveBtn[i]->setState(STATE::SELECTED);
			found = listOffset+i;
			break;
		}
	}
	return found;
}

/**
 * Draw the button on screen
 */
void GuiSaveBrowser::draw()
{
	if(!this->isVisible())
		return;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->draw();

	scrollbarImg->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();

	this->updateEffects();
}

void GuiSaveBrowser::update(GuiInputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int i, len;
	char savetext[50];

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);

	// pad/joystick navigation
	if(!focus)
		goto endNavigation; // skip navigation

	if(selectedItem < 0) selectedItem = 0;

	if(controller->right())
	{
		if(selectedItem == SAVELISTSIZE-1)
		{
			if(listOffset + SAVELISTSIZE < saves->length)
			{
				// move list down by 1 row
				listOffset += 2;
				selectedItem -= 1;
			}
		}
		else if(saveBtn[selectedItem+1]->isVisible())
		{
			saveBtn[selectedItem]->resetState();
			saveBtn[selectedItem+1]->setState(STATE::SELECTED, controller->getChannel());
			selectedItem += 1;
		}
	}
	else if(controller->left())
	{
		if(selectedItem == 0)
		{
			if((listOffset - 2 >= 0 && action == 0) ||
				(listOffset >= 0 && action == 1) ||
				(listOffset - 2 >= 0 && action == 2))
			{
				// move list up by 1
				listOffset -= 2;
				selectedItem = SAVELISTSIZE-1;
			}
		}
		else
		{
			if(saveBtn[selectedItem-1]->isVisible())
			{
				selectedItem -= 1;
			}
		}
	}
	else if(controller->down() || arrowDownBtn->getState() == STATE::CLICKED)
	{
		if(selectedItem >= SAVELISTSIZE-2)
		{
			if(listOffset + SAVELISTSIZE + 1 < saves->length)
			{
				listOffset += 2;
			}
			else if(listOffset + SAVELISTSIZE < saves->length)
			{
				listOffset += 2;

				if(selectedItem == SAVELISTSIZE-1)
					selectedItem -= 1;
			}
		}
		else if(saveBtn[selectedItem+2]->isVisible())
		{
			selectedItem += 2;
		}
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		if(selectedItem < 2)
		{
			if((listOffset - 2 >= 0 && action == 0) ||
				(listOffset >= 0 && action == 1) ||
				(listOffset - 2 >= 0 && action == 2))
			{
				// move list up by 1
				listOffset -= 2;
			}
		}
		else
		{
			if(saveBtn[selectedItem-2]->isVisible())
			{
				selectedItem -= 2;
			}
		}
	}

	endNavigation:

	if(arrowDownBtn->getState() == STATE::CLICKED)
		arrowDownBtn->resetState();

	if(arrowUpBtn->getState() == STATE::CLICKED)
		arrowUpBtn->resetState();

	for(i=0; i<SAVELISTSIZE; i++)
	{
		if(listOffset+i < 0 && action == 1)
		{
			saveDate[0]->setText(nullptr);
			saveTime[0]->setText("New");
			saveType[0]->setText("State");
			savePreviewImg[0]->setImage(gameSaveBlank);
			saveBtn[0]->setVisible(true);

			if(saveBtn[0]->getState() == STATE::DISABLED)
				saveBtn[0]->setState(STATE::DEFAULT);

			if (GCSettings.HideSRAMSaving == 0)
			{
				saveDate[1]->setText(nullptr);
				saveTime[1]->setText("New");
				saveType[1]->setText("SRAM");
				savePreviewImg[1]->setImage(gameSaveBlank);
				saveBtn[1]->setVisible(true);

				if(saveBtn[1]->getState() == STATE::DISABLED)
					saveBtn[1]->setState(STATE::DEFAULT);
			}
		}
		else if(listOffset+i < saves->length)
		{
			if(saveBtn[i]->getState() == STATE::DISABLED || !saveBtn[i]->isVisible())
			{
				saveBtn[i]->setVisible(true);
				saveBtn[i]->setState(STATE::DEFAULT);
			}

			saveDate[i]->setText(saves->date[listOffset+i]);
			saveTime[i]->setText(saves->time[listOffset+i]);

			if(saves->type[listOffset+i] == FILE_SRAM)
				sprintf(savetext, "SRAM");
			else
				sprintf(savetext, "State");

			len = strlen(saves->filename[listOffset+i]);
			if(len > 10 &&
				((saves->filename[listOffset+i][len-8] == 'A' &&
				saves->filename[listOffset+i][len-7] == 'u' &&
				saves->filename[listOffset+i][len-6] == 't' &&
				saves->filename[listOffset+i][len-5] == 'o') ||
				saves->filename[listOffset+i][len-5] == '0')
				)
			{
				strcat(savetext, " (Auto)");
			}
			saveType[i]->setText(savetext);

			if(saves->previewImg[listOffset+i] != nullptr)
				savePreviewImg[i]->setImage(saves->previewImg[listOffset+i]);
			else
				savePreviewImg[i]->setImage(gameSaveBlank);
		}
		else
		{
			saveBtn[i]->setVisible(false);
			saveBtn[i]->setState(STATE::DISABLED);
		}

		if(i != selectedItem && saveBtn[i]->getState() == STATE::SELECTED)
			saveBtn[i]->resetState();
		else if(focus && i == selectedItem && saveBtn[i]->getState() == STATE::DEFAULT)
			saveBtn[selectedItem]->setState(STATE::SELECTED, controller->getChannel());

		auto pad = controller->getPadData();

		if(pad.validPointer)
		{
			if(!saveBtnLastOver[i] && saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
				saveBtn[i]->resetState();
			saveBtnLastOver[i] = saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y);
		}

		saveBtn[i]->update(controller);

		if(saveBtn[i]->getState() == STATE::SELECTED)
			selectedItem = i;
	}

	if(updateCB)
		updateCB(this);
}
