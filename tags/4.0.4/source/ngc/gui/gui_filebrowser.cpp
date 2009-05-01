/****************************************************************************
 * libwiigui
 *
 * Tantric 2009
 *
 * gui_filebrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include "filebrowser.h"

/**
 * Constructor for the GuiFileBrowser class.
 */
GuiFileBrowser::GuiFileBrowser(int w, int h)
{
	width = w;
	height = h;
	selectedItem = 0;
	selectable = true;
	listChanged = true; // trigger an initial list update
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	if(GCSettings.WiimoteOrientation)
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	trigHeldA = new GuiTrigger;
	trigHeldA->SetHeldTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

	bgGameSelection = new GuiImageData(bg_game_selection_png);
	bgGameSelectionImg = new GuiImage(bgGameSelection);
	bgGameSelectionImg->SetParent(this);
	bgGameSelectionImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgGameSelectionEntry = new GuiImageData(bg_game_selection_entry_png);
	gameFolder = new GuiImageData(folder_png);

	scrollbar = new GuiImageData(scrollbar_png);
	scrollbarImg = new GuiImage(scrollbar);
	scrollbarImg->SetParent(this);
	scrollbarImg->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarImg->SetPosition(0, 30);

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

	arrowUpBtn = new GuiButton(arrowUpImg->GetWidth(), arrowUpImg->GetHeight());
	arrowUpBtn->SetParent(this);
	arrowUpBtn->SetImage(arrowUpImg);
	arrowUpBtn->SetImageOver(arrowUpOverImg);
	arrowUpBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	arrowUpBtn->SetSelectable(false);
	arrowUpBtn->SetClickable(false);
	arrowUpBtn->SetHoldable(true);
	arrowUpBtn->SetTrigger(trigHeldA);
	arrowUpBtn->SetSoundOver(btnSoundOver);
	arrowUpBtn->SetSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(arrowDownImg->GetWidth(), arrowDownImg->GetHeight());
	arrowDownBtn->SetParent(this);
	arrowDownBtn->SetImage(arrowDownImg);
	arrowDownBtn->SetImageOver(arrowDownOverImg);
	arrowDownBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	arrowDownBtn->SetSelectable(false);
	arrowDownBtn->SetClickable(false);
	arrowDownBtn->SetHoldable(true);
	arrowDownBtn->SetTrigger(trigHeldA);
	arrowDownBtn->SetSoundOver(btnSoundOver);
	arrowDownBtn->SetSoundClick(btnSoundClick);

	scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->GetWidth(), scrollbarBoxImg->GetHeight());
	scrollbarBoxBtn->SetParent(this);
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetMinY(0);
	scrollbarBoxBtn->SetMaxY(136);
	scrollbarBoxBtn->SetSelectable(false);
	scrollbarBoxBtn->SetClickable(false);
	scrollbarBoxBtn->SetHoldable(true);
	scrollbarBoxBtn->SetTrigger(trigHeldA);

	for(int i=0; i<PAGESIZE; i++)
	{
		gameListText[i] = new GuiText("Game",22, (GXColor){0, 0, 0, 0xff});
		gameListText[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		gameListText[i]->SetPosition(5,0);

		gameListBg[i] = new GuiImage(bgGameSelectionEntry);
		gameListFolder[i] = new GuiImage(gameFolder);

		gameList[i] = new GuiButton(380, 30);
		gameList[i]->SetParent(this);
		gameList[i]->SetLabel(gameListText[i]);
		gameList[i]->SetImageOver(gameListBg[i]);
		gameList[i]->SetPosition(2,30*i+3);
		gameList[i]->SetTrigger(trigA);
		gameList[i]->SetSoundClick(btnSoundClick);
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

	delete bgGameSelectionImg;
	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;

	delete bgGameSelection;
	delete bgGameSelectionEntry;
	delete gameFolder;
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

	for(int i=0; i<PAGESIZE; i++)
	{
		delete gameListText[i];
		delete gameList[i];
		delete gameListBg[i];
		delete gameListFolder[i];
	}
}

void GuiFileBrowser::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<PAGESIZE; i++)
		gameList[i]->ResetState();

	if(f == 1)
		gameList[selectedItem]->SetState(STATE_SELECTED);
}

void GuiFileBrowser::ResetState()
{
	state = STATE_DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<PAGESIZE; i++)
	{
		gameList[i]->ResetState();
	}
}

void GuiFileBrowser::TriggerUpdate()
{
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiFileBrowser::Draw()
{
	if(!this->IsVisible())
		return;

	bgGameSelectionImg->Draw();

	for(int i=0; i<PAGESIZE; i++)
	{
		gameList[i]->Draw();
	}

	scrollbarImg->Draw();
	arrowUpBtn->Draw();
	arrowDownBtn->Draw();
	scrollbarBoxBtn->Draw();

	this->UpdateEffects();
}

void GuiFileBrowser::Update(GuiTrigger * t)
{
	if(state == STATE_DISABLED || !t)
		return;

	int position = 0;
	int positionWiimote = 0;

	arrowUpBtn->Update(t);
	arrowDownBtn->Update(t);
	scrollbarBoxBtn->Update(t);

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->GetState() == STATE_HELD &&
		scrollbarBoxBtn->GetStateChan() == t->chan &&
		t->wpad.ir.valid &&
		browser.numEntries > PAGESIZE
		)
	{
		scrollbarBoxBtn->SetPosition(0,0);
		positionWiimote = t->wpad.ir.y - 60 - scrollbarBoxBtn->GetTop();

		if(positionWiimote < scrollbarBoxBtn->GetMinY())
			positionWiimote = scrollbarBoxBtn->GetMinY();
		else if(positionWiimote > scrollbarBoxBtn->GetMaxY())
			positionWiimote = scrollbarBoxBtn->GetMaxY();

		browser.pageIndex = (positionWiimote * browser.numEntries)/136.0 - selectedItem;

		if(browser.pageIndex <= 0)
		{
			browser.pageIndex = 0;
			selectedItem = 0;
		}
		else if(browser.pageIndex+PAGESIZE >= browser.numEntries)
		{
			browser.pageIndex = browser.numEntries-PAGESIZE;
			selectedItem = PAGESIZE-1;
		}
		listChanged = true;
		focus = false;
	}

	if(arrowDownBtn->GetState() == STATE_HELD && arrowDownBtn->GetStateChan() == t->chan)
	{
		t->wpad.btns_h |= WPAD_BUTTON_DOWN;
		if(!this->IsFocused())
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
	}
	else if(arrowUpBtn->GetState() == STATE_HELD && arrowUpBtn->GetStateChan() == t->chan)
	{
		t->wpad.btns_h |= WPAD_BUTTON_UP;
		if(!this->IsFocused())
			((GuiWindow *)this->GetParent())->ChangeFocus(this);
	}

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(t->Right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > PAGESIZE)
		{
			browser.pageIndex += PAGESIZE;
			if(browser.pageIndex+PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-PAGESIZE;
			listChanged = true;
		}
	}
	else if(t->Left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(t->Down())
	{
		if(browser.pageIndex + selectedItem + 1 < browser.numEntries)
		{
			if(selectedItem == PAGESIZE-1)
			{
				// move list down by 1
				browser.pageIndex++;
				listChanged = true;
			}
			else if(gameList[selectedItem+1]->IsVisible())
			{
				gameList[selectedItem]->ResetState();
				gameList[++selectedItem]->SetState(STATE_SELECTED, t->chan);
			}
		}
	}
	else if(t->Up())
	{
		if(selectedItem == 0 &&	browser.pageIndex + selectedItem > 0)
		{
			// move list up by 1
			browser.pageIndex--;
			listChanged = true;
		}
		else if(selectedItem > 0)
		{
			gameList[selectedItem]->ResetState();
			gameList[--selectedItem]->SetState(STATE_SELECTED, t->chan);
		}
	}

	endNavigation:

	for(int i=0; i<PAGESIZE; i++)
	{
		if(listChanged)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(gameList[i]->GetState() == STATE_DISABLED)
					gameList[i]->SetState(STATE_DEFAULT);

				gameList[i]->SetVisible(true);

				gameListText[i]->SetText(browserList[browser.pageIndex+i].displayname);

				if(browserList[browser.pageIndex+i].isdir) // directory
				{
					gameList[i]->SetIcon(gameListFolder[i]);
					gameListText[i]->SetPosition(30,0);
				}
				else
				{
					gameList[i]->SetIcon(NULL);
					gameListText[i]->SetPosition(10,0);
				}
			}
			else
			{
				gameList[i]->SetVisible(false);
				gameList[i]->SetState(STATE_DISABLED);
			}
		}

		if(focus)
		{
			if(i != selectedItem && gameList[i]->GetState() == STATE_SELECTED)
				gameList[i]->ResetState();
			else if(i == selectedItem && gameList[i]->GetState() == STATE_DEFAULT)
				gameList[selectedItem]->SetState(STATE_SELECTED, t->chan);
		}

		gameList[i]->Update(t);

		if(gameList[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
			browser.selIndex = browser.pageIndex + i;
		}
	}

	// update the location of the scroll box based on the position in the file list
	if(positionWiimote > 0)
		position = positionWiimote; // follow wiimote cursor
	else
		position = 136*(browser.pageIndex + selectedItem) / browser.numEntries;

	scrollbarBoxBtn->SetPosition(0,position+36);

	listChanged = false;

	if(updateCB)
		updateCB(this);
}
