/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiFileBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "../filebrowser.h"
#include "Gui.h"

/**
 * Constructor for the GuiFileBrowser class.
 */
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
	trigA->setSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, WIIDRC_BUTTON_A);
	trig2 = new GuiTrigger;
	trig2->setSimpleTrigger(-1, WPAD_BUTTON_2, 0, 0);

	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, WIIDRC_BUTTON_A);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

	bgFileSelection = new GuiImageData(bg_game_selection_png);
	bgFileSelectionImg = new GuiImage(bgFileSelection);
	bgFileSelectionImg->setParent(this);
	bgFileSelectionImg->setAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgFileSelectionEntry = new GuiImageData(bg_game_selection_entry_png);

	iconFolder = new GuiImageData(icon_folder_png);
	iconSD = new GuiImageData(icon_sd_png);
	iconUSB = new GuiImageData(icon_usb_png);
	iconDVD = new GuiImageData(icon_dvd_png);
	iconSMB = new GuiImageData(icon_smb_png);

	scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
	scrollbarImg->setParent(this);
	scrollbarImg->setAlignment(ALIGN_RIGHT, ALIGN_TOP);
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
	arrowUpBtn->setAlignment(ALIGN_RIGHT, ALIGN_TOP);
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
	arrowDownBtn->setAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
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
	scrollbarBoxBtn->setAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->setMinY(0);
	scrollbarBoxBtn->setMaxY(156);
	scrollbarBoxBtn->setSelectable(false);
	scrollbarBoxBtn->setClickable(false);
	scrollbarBoxBtn->setHoldable(true);
	scrollbarBoxBtn->setTrigger(trigHeldA);

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		fileListText[i] = new GuiText(NULL, 20, (GXColor){0, 0, 0, 0xff});
		fileListText[i]->setAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		fileListText[i]->setPosition(5,0);
		fileListText[i]->setMaxWidth(295);

		fileListBg[i] = new GuiImage(bgFileSelectionEntry);
		fileListIcon[i] = NULL;

		fileList[i] = new GuiButton(295, 26);
		fileList[i]->setParent(this);
		fileList[i]->setLabel(fileListText[i]);
		fileList[i]->setImageOver(fileListBg[i]);
		fileList[i]->setPosition(2,26*i+3);
		fileList[i]->setTrigger(trigA);
		fileList[i]->setTrigger(trig2);
		fileList[i]->setSoundClick(btnSoundClick);
	}
}

/**
 * Destructor for the GuiFileBrowser class.
 */
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
	delete trig2;

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
		fileList[selectedItem]->setState(STATE_SELECTED);
}

void GuiFileBrowser::resetState()
{
	state = STATE_DEFAULT;
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

	for(u32 i=0; i<FILE_PAGESIZE; ++i)
	{
		fileList[i]->draw();
	}

	scrollbarImg->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();
	scrollbarBoxBtn->draw();

	this->updateEffects();
}

void GuiFileBrowser::drawTooltip()
{
}

void GuiFileBrowser::update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
		return;

	int position = 0;
	int positionWiimote = 0;

	arrowUpBtn->update(t);
	arrowDownBtn->update(t);
	scrollbarBoxBtn->update(t);

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->getState() == STATE_HELD &&
		scrollbarBoxBtn->getStateChan() == t->chan &&
		t->wpad->ir.valid &&
		browser.numEntries > FILE_PAGESIZE
		)
	{
		scrollbarBoxBtn->setPosition(0,0);
		positionWiimote = t->wpad->ir.y - 60 - scrollbarBoxBtn->getTop();

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

	if(arrowDownBtn->getState() == STATE_HELD && arrowDownBtn->getStateChan() == t->chan)
	{
		t->wpad->btns_d |= WPAD_BUTTON_DOWN;
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}
	else if(arrowUpBtn->getState() == STATE_HELD && arrowUpBtn->getStateChan() == t->chan)
	{
		t->wpad->btns_d |= WPAD_BUTTON_UP;
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(t->right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > FILE_PAGESIZE)
		{
			browser.pageIndex += FILE_PAGESIZE;
			if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
			listChanged = true;
		}
	}
	else if(t->left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= FILE_PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(t->down())
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
				fileList[++selectedItem]->setState(STATE_SELECTED, t->chan);
			}
		}
	}
	else if(t->up())
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
			fileList[--selectedItem]->setState(STATE_SELECTED, t->chan);
		}
	}

	endNavigation:

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != browser.numEntries)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(fileList[i]->getState() == STATE_DISABLED)
					fileList[i]->setState(STATE_DEFAULT);

				fileList[i]->setVisible(true);

				fileListText[i]->setText(browserList[browser.pageIndex+i].displayname);

				if(fileListIcon[i])
				{
					delete fileListIcon[i];
					fileListIcon[i] = NULL;
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
				if(fileListIcon[i] != NULL)
					fileListText[i]->setPosition(30,0);
			}
			else
			{
				fileList[i]->setVisible(false);
				fileList[i]->setState(STATE_DISABLED);
			}
		}

		if(i != selectedItem && fileList[i]->getState() == STATE_SELECTED)
			fileList[i]->resetState();
		else if(focus && i == selectedItem && fileList[i]->getState() == STATE_DEFAULT)
			fileList[selectedItem]->setState(STATE_SELECTED, t->chan);

		int currChan = t->chan;

		if(t->wpad->ir.valid && !fileList[i]->isInside(t->wpad->ir.x, t->wpad->ir.y))
			t->chan = -1;

		fileList[i]->update(t);
		t->chan = currChan;

		if(fileList[i]->getState() == STATE_SELECTED)
		{
			selectedItem = i;
			browser.selIndex = browser.pageIndex + i;
		}

		if(selectedItem == i)
			fileListText[i]->setScroll(SCROLL_HORIZONTAL);
		else
			fileListText[i]->setScroll(SCROLL_NONE);
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
