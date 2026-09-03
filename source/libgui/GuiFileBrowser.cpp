/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiFileBrowser.cpp
 ***************************************************************************/

#include "Gui.h"
#include "../filebrowser.h"

GuiFileBrowser::GuiFileBrowser(int w, int h)
{
	width = w;
	height = h;
	numEntries = 0;
	selectedItem = 0;
	selectable = true;
	listChanged = true; // trigger an initial list update
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, INPUT_BTN_A);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND::PCM);

	bgFileSelection = new GuiImageData(bg_game_selection_png);
	bgFileSelectionImg = new GuiImage(bgFileSelection);
	bgFileSelectionImg->setParent(this);
	bgFileSelectionImg->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

	bgFileSelectionEntry = new GuiImageData(bg_game_selection_entry_png);

	iconFolder = new GuiImageData(icon_folder_png);
	iconSD = new GuiImageData(icon_sd_png);
	iconUSB = new GuiImageData(icon_usb_png);
	iconDVD = new GuiImageData(icon_dvd_png);
	iconSMB = new GuiImageData(icon_smb_png);

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
	scrollbarBox = new GuiImageData(scrollbar_box_png);
	scrollbarBoxImg = new GuiImage(scrollbarBox);
	scrollbarBoxOver = new GuiImageData(scrollbar_box_over_png);
	scrollbarBoxOverImg = new GuiImage(scrollbarBoxOver);

	arrowUpBtn = new GuiButton(arrowUpImg->getWidth(), arrowUpImg->getHeight());
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpImg);
	arrowUpBtn->setImageOver(arrowUpOverImg);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setClickable(false);
	arrowUpBtn->setHoldable(true);
	arrowUpBtn->setTrigger(trigHeldA);
	arrowUpBtn->setSoundOver(btnSoundOver);
	arrowUpBtn->setSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(arrowDownImg->getWidth(), arrowDownImg->getHeight());
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownImg);
	arrowDownBtn->setImageOver(arrowDownOverImg);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setClickable(false);
	arrowDownBtn->setHoldable(true);
	arrowDownBtn->setTrigger(trigHeldA);
	arrowDownBtn->setSoundOver(btnSoundOver);
	arrowDownBtn->setSoundClick(btnSoundClick);

	scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->getWidth(), scrollbarBoxImg->getHeight());
	scrollbarBoxBtn->setParent(this);
	scrollbarBoxBtn->setImage(scrollbarBoxImg);
	scrollbarBoxBtn->setImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	scrollbarBoxBtn->setMinY(0);
	scrollbarBoxBtn->setMaxY(156);
	scrollbarBoxBtn->setSelectable(false);
	scrollbarBoxBtn->setClickable(false);
	scrollbarBoxBtn->setHoldable(true);
	scrollbarBoxBtn->setTrigger(trigHeldA);

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		fileListText[i] = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 0xff});
		fileListText[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		fileListText[i]->setPosition(5,0);
		fileListText[i]->setMaxWidth(295);

		fileListBg[i] = new GuiImage(bgFileSelectionEntry);
		fileListIcon[i] = nullptr;

		fileList[i] = new GuiButton(295, 26);
		fileList[i]->setParent(this);
		fileList[i]->setLabel(fileListText[i]);
		fileList[i]->setImageOver(fileListBg[i]);
		fileList[i]->setPosition(2,26*i+3);
		fileList[i]->setTrigger(trigA);
		fileList[i]->setSoundClick(btnSoundClick);
	}
}

GuiFileBrowser::~GuiFileBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;

	delete bgFileSelectionImg;
	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;

	delete bgFileSelection;
	delete bgFileSelectionEntry;
	delete iconFolder;
	delete iconSD;
	delete iconUSB;
	delete iconDVD;
	delete iconSMB;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;
	delete scrollbarBox;
	delete scrollbarBoxOver;

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigHeldA;
	delete trigA;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		delete fileListText[i];
		delete fileList[i];
		delete fileListBg[i];

		if(fileListIcon[i])
			delete fileListIcon[i];
	}
}

void GuiFileBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<FILE_PAGESIZE; i++)
		fileList[i]->resetState();

	if(f == 1)
		fileList[selectedItem]->setState(STATE::SELECTED);
}

void GuiFileBrowser::resetState()
{
	state = STATE::DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		fileList[i]->resetState();
	}
}

void GuiFileBrowser::triggerUpdate()
{
	int newIndex = browser.selIndex-browser.pageIndex;
	
	if(newIndex >= FILE_PAGESIZE)
		newIndex = FILE_PAGESIZE-1;
	else if(newIndex < 0)
		newIndex = 0;

	selectedItem = newIndex;
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiFileBrowser::draw()
{
	if(!this->isVisible())
		return;

	bgFileSelectionImg->draw();

	for(uint32_t i=0; i<FILE_PAGESIZE; ++i)
	{
		fileList[i]->draw();
	}

	scrollbarImg->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();
	scrollbarBoxBtn->draw();

	this->updateEffects();
}

void GuiFileBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int position = 0;
	int positionWiimote = 0;

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);
	scrollbarBoxBtn->update(controller);

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();
	// A InputController with no live signal this frame (e.g. a persistent
	// but currently-disconnected controller slot) must not be able to claim
	// or evict a selection just by taking its turn through this loop -- only
	// a channel that's genuinely doing something (pointing or pressing) gets
	// a say in who owns the selected slot.
	bool channelActive = pad.validPointer || pad.buttons_d != 0 || pad.buttons_h != 0;

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->getState() == STATE::HELD &&
		scrollbarBoxBtn->getStateChan() == currentChan &&
		pad.validPointer &&
		browser.numEntries > FILE_PAGESIZE)
	{
		scrollbarBoxBtn->setPosition(0,0);
		positionWiimote = pad.cursor_y - 60 - scrollbarBoxBtn->getTop();

		if(positionWiimote < scrollbarBoxBtn->getMinY())
			positionWiimote = scrollbarBoxBtn->getMinY();
		else if(positionWiimote > scrollbarBoxBtn->getMaxY())
			positionWiimote = scrollbarBoxBtn->getMaxY();

		browser.pageIndex = (positionWiimote * browser.numEntries)/156.0f - selectedItem;

		if(browser.pageIndex <= 0)
		{
			browser.pageIndex = 0;
		}
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
		}
		listChanged = true;
		focus = false;
	}

    // Evaluate simulated D-Pad actions via on-screen buttons
	bool simulateDown = arrowDownBtn->getState() == STATE::HELD && arrowDownBtn->getStateChan() == currentChan;
	bool simulateUp = arrowUpBtn->getState() == STATE::HELD && arrowUpBtn->getStateChan() == currentChan;

	if(simulateDown)
	{
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}
	else if(simulateUp)
	{
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(controller->right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > FILE_PAGESIZE)
		{
			browser.pageIndex += FILE_PAGESIZE;
			if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
			listChanged = true;
		}
	}
	else if(controller->left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= FILE_PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(controller->down() || simulateDown)
	{
		if(browser.pageIndex + selectedItem + 1 < browser.numEntries)
		{
			if(selectedItem == FILE_PAGESIZE-1)
			{
				// move list down by 1
				++browser.pageIndex;
				listChanged = true;
			}
			else if(fileList[selectedItem+1]->isVisible())
			{
				fileList[selectedItem]->resetState();
				fileList[++selectedItem]->setState(STATE::SELECTED, currentChan);
			}
		}
	}
	else if(controller->up() || simulateUp)
	{
		if(selectedItem == 0 &&	browser.pageIndex + selectedItem > 0)
		{
			// move list up by 1
			--browser.pageIndex;
			listChanged = true;
		}
		else if(selectedItem > 0)
		{
			fileList[selectedItem]->resetState();
			fileList[--selectedItem]->setState(STATE::SELECTED, currentChan);
		}
	}

	endNavigation:

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != browser.numEntries)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(fileList[i]->getState() == STATE::DISABLED)
					fileList[i]->setState(STATE::DEFAULT);

				fileList[i]->setVisible(true);

				fileListText[i]->setText(browserList[browser.pageIndex+i].displayname);

				if(fileListIcon[i])
				{
					delete fileListIcon[i];
					fileListIcon[i] = nullptr;
					fileListText[i]->setPosition(5,0);
				}

				switch(browserList[browser.pageIndex+i].icon)
				{
					case ICON_FOLDER:
						fileListIcon[i] = new GuiImage(iconFolder);
						break;
					case ICON_SD:
						fileListIcon[i] = new GuiImage(iconSD);
						break;
					case ICON_USB:
						fileListIcon[i] = new GuiImage(iconUSB);
						break;
					case ICON_DVD:
						fileListIcon[i] = new GuiImage(iconDVD);
						break;
					case ICON_SMB:
						fileListIcon[i] = new GuiImage(iconSMB);
						break;
				}
				fileList[i]->setIcon(fileListIcon[i]);
				if(fileListIcon[i] != nullptr)
					fileListText[i]->setPosition(30,0);
			}
			else
			{
				fileList[i]->setVisible(false);
				fileList[i]->setState(STATE::DISABLED);
			}
		}

		if(i != selectedItem && fileList[i]->getState() == STATE::SELECTED)
			fileList[i]->resetState();
		else if(focus && channelActive && i == selectedItem && fileList[i]->getState() == STATE::DEFAULT)
			fileList[selectedItem]->setState(STATE::SELECTED, currentChan);
		else if(focus && channelActive && i == selectedItem && fileList[i]->getState() == STATE::SELECTED &&
				fileList[i]->getStateChan() != -1 && fileList[i]->getStateChan() != currentChan)
			// Slot is already SELECTED but carries a stale channel from an earlier
			// selection (e.g. a reused slot after paging, or a preselected item that
			// was never actually hovered by the real channel yet). Without this,
			// currentChan == stateChan never holds and clicks are silently ignored
			// until the item happens to be navigated away from and back.
			fileList[i]->resetState();

		// Present a "no channel" (-1) identity to any item the cursor isn't
		// currently over. Without this, a stale stateChan left on a reused
		// list slot (e.g. after paging/navigating) can permanently block
		// clicks from the real channel until the item is re-hovered.
		if(pad.validPointer && !fileList[i]->isInside(pad.cursor_x, pad.cursor_y))
			controller->setChannel(-1);

		fileList[i]->update(controller);
		controller->setChannel(currentChan);

		if(fileList[i]->getState() == STATE::SELECTED)
		{
			selectedItem = i;
			browser.selIndex = browser.pageIndex + i;
		}

		if(selectedItem == i)
			fileListText[i]->setScroll(SCROLL::HORIZONTAL);
		else
			fileListText[i]->setScroll(SCROLL::NONE);
	}

	// update the location of the scroll box based on the position in the file list
	if(positionWiimote > 0)
	{
		position = positionWiimote; // follow wiimote cursor
		scrollbarBoxBtn->setPosition(0,position+36);
	}
	else if(listChanged || numEntries != browser.numEntries)
	{
		if(float((browser.pageIndex<<1))/(float(FILE_PAGESIZE)) < 1.0)
		{
			position = 0;
		}
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			position = 156;
		}
		else
		{
			position = 156 * (browser.pageIndex + FILE_PAGESIZE/2) / (float)browser.numEntries;
		}
		scrollbarBoxBtn->setPosition(0,position+36);
	}

	listChanged = false;
	numEntries = browser.numEntries;

	if(updateCB)
		updateCB(this);
}
