/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_savebrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include "filebrowser.h"

/**
 * Constructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::GuiSaveBrowser(int w, int h, SaveList * s)
{
	width = w;
	height = h;
	saves = s;
	selectable = true;
	listOffset = 0;
	selectedItem = 0;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	gameSave = new GuiImageData(button_gamesave_png);
	gameSaveOver = new GuiImageData(button_gamesave_over_png);
	gameSaveBlank = new GuiImageData(button_gamesave_blank_png);

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
	arrowUpBtn->SetTrigger(trigA);

	arrowDownBtn = new GuiButton(arrowDownImg->GetWidth(), arrowDownImg->GetHeight());
	arrowDownBtn->SetParent(this);
	arrowDownBtn->SetImage(arrowDownImg);
	arrowDownBtn->SetImageOver(arrowDownOverImg);
	arrowDownBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	arrowDownBtn->SetSelectable(false);
	arrowDownBtn->SetTrigger(trigA);

	scrollbarBoxBtn = new GuiButton(scrollbarBoxImg->GetWidth(), scrollbarBoxImg->GetHeight());
	scrollbarBoxBtn->SetParent(this);
	scrollbarBoxBtn->SetImage(scrollbarBoxImg);
	scrollbarBoxBtn->SetImageOver(scrollbarBoxOverImg);
	scrollbarBoxBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	scrollbarBoxBtn->SetSelectable(false);

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveDateTime[i] = new GuiText(NULL, 22, (GXColor){0, 0, 0, 0xff});
		saveDateTime[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
		saveDateTime[i]->SetPosition(80,5);

		saveType[i] = new GuiText(NULL, 22, (GXColor){0, 0, 0, 0xff});
		saveType[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
		saveType[i]->SetPosition(80,50);

		saveBgImg[i] = new GuiImage(gameSave);
		saveBgOverImg[i] = new GuiImage(gameSaveOver);
		savePreviewImg[i] = new GuiImage(gameSaveBlank);

		saveBtn[i] = new GuiButton(saveBgImg[i]->GetWidth(),saveBgImg[i]->GetHeight());
		saveBtn[i]->SetParent(this);
		saveBtn[i]->SetLabel(saveDateTime[i]);
		saveBtn[i]->SetLabel2(saveType[i]);
		saveBtn[i]->SetImage(saveBgImg[i]);
		saveBtn[i]->SetImageOver(saveBgOverImg[i]);
		saveBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
		saveBtn[i]->SetPosition(247*(i % 2),87*(i/2));
		saveBtn[i]->SetTrigger(trigA);
	}
}

/**
 * Destructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::~GuiSaveBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;

	delete scrollbarImg;
	delete arrowDownImg;
	delete arrowDownOverImg;
	delete arrowUpImg;
	delete arrowUpOverImg;
	delete scrollbarBoxImg;
	delete scrollbarBoxOverImg;

	delete gameSave;
	delete gameSaveOver;
	delete gameSaveBlank;
	delete scrollbar;
	delete arrowDown;
	delete arrowDownOver;
	delete arrowUp;
	delete arrowUpOver;
	delete scrollbarBox;
	delete scrollbarBoxOver;

	delete trigA;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		delete saveBtn[i];
		delete saveDateTime[i];
		delete saveType[i];
		delete saveBgImg[i];
		delete saveBgOverImg[i];
		delete savePreviewImg[i];
	}
}

void GuiSaveBrowser::SetFocus(int f)
{
	focus = f;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->ResetState();

	if(f == 1)
		saveBtn[selectedItem]->SetState(STATE_SELECTED);
}

void GuiSaveBrowser::ResetState()
{
	state = STATE_DEFAULT;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->ResetState();
	}
}

int GuiSaveBrowser::GetClickedSave()
{
	int found = -1;
	for(int i=0; i<SAVELISTSIZE; i++)
	{
		if(saveBtn[i]->GetState() == STATE_CLICKED)
		{
			saveBtn[i]->SetState(STATE_SELECTED);
			found = i;
			break;
		}
	}
	return found;
}

/**
 * Draw the button on screen
 */
void GuiSaveBrowser::Draw()
{
	if(!this->IsVisible())
		return;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->Draw();

	scrollbarImg->Draw();
	arrowUpBtn->Draw();
	arrowDownBtn->Draw();
	scrollbarBoxBtn->Draw();
}

void GuiSaveBrowser::Update(GuiTrigger * t)
{
	// update the location of the scroll box based on the position in the option list
	int position = 144*(listOffset) / saves->length;
	scrollbarBoxBtn->SetPosition(0,position+36);

	arrowUpBtn->Update(t);
	arrowDownBtn->Update(t);
	scrollbarBoxBtn->Update(t);

	// pad/joystick navigation
	if(!focus)
		goto endNavigation; // skip navigation

	if(t->wpad.btns_d & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)
			|| t->pad.btns_d & PAD_BUTTON_RIGHT)
	{
		if(selectedItem == SAVELISTSIZE-1)
		{
			if(listOffset + SAVELISTSIZE < saves->length)
			{
				// move list down by 1 row
				listOffset += 2;
				selectedItem += 1;
			}
		}
		else if(saveBtn[selectedItem+1]->IsVisible())
		{
			saveBtn[selectedItem]->ResetState();
			saveBtn[selectedItem+1]->SetState(STATE_SELECTED);
			selectedItem += 1;
		}
	}
	else if(t->wpad.btns_d & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)
		|| t->pad.btns_d & PAD_BUTTON_LEFT)
	{
		if(selectedItem == 0)
		{
			if(listOffset - 2 >= 0)
			{
				// move list up by 1
				listOffset -= 2;
				selectedItem -= 1;
			}
		}
		else
		{
			saveBtn[selectedItem]->ResetState();
			saveBtn[selectedItem-1]->SetState(STATE_SELECTED);
			selectedItem -= 1;
		}
	}
	else if(t->wpad.btns_d & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)
		|| t->pad.btns_d & PAD_BUTTON_DOWN)
	{
		if(selectedItem >= SAVELISTSIZE-2)
		{
			if(listOffset + SAVELISTSIZE + 1 < saves->length)
			{
				listOffset += 2;
				selectedItem += 2;
			}
			else if(listOffset + SAVELISTSIZE < saves->length)
			{
				listOffset += 2;
				selectedItem += 1;
			}
		}
		else if(saveBtn[selectedItem+2]->IsVisible())
		{
			saveBtn[selectedItem]->ResetState();
			saveBtn[selectedItem+2]->SetState(STATE_SELECTED);
			selectedItem += 2;
		}
	}
	else if(t->wpad.btns_d & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)
		|| t->pad.btns_d & PAD_BUTTON_UP)
	{
		if(selectedItem < 2)
		{
			if(listOffset - 2 >= 0)
			{
				// move list up by 1
				listOffset -= 2;
				selectedItem -= 2;
			}
		}
		else
		{
			saveBtn[selectedItem]->ResetState();
			saveBtn[selectedItem-2]->SetState(STATE_SELECTED);
			selectedItem -= 2;
		}
	}

endNavigation:

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		if(listOffset+i < saves->length)
		{
			if(saveBtn[i]->GetState() == STATE_DISABLED)
			{
				saveBtn[i]->SetVisible(true);
				saveBtn[i]->ResetState();
			}

			saveDateTime[i]->SetText(saves->datetime[listOffset+i]);

			if(saves->type[listOffset+i] == FILE_SRAM)
				saveType[i]->SetText("SRAM");
			else
				saveType[i]->SetText("Snapshot");
		}
		else
		{
			saveBtn[i]->SetVisible(false);
			saveBtn[i]->SetState(STATE_DISABLED);
		}

		saveBtn[i]->Update(t);

		if(saveBtn[i]->GetState() == STATE_SELECTED)
		{
			selectedItem = i;
		}
	}
}
