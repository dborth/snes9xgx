/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_keyboard.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
/**
 * Constructor for the GuiKeyboard class.
 */

GuiKeyboard::GuiKeyboard(char * t)
{
	width = 540;
	height = 400;
	shift = 0;
	caps = 0;
	selectable = true;
	focus = 0; // allow focus
	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;
	strncpy(kbtextstr, t, 100);
	kbtextstr[100] = 0;

	Key thekeys[4][10] = {
	{
		{'1','!'},
		{'2','@'},
		{'3','#'},
		{'4','$'},
		{'5','%'},
		{'6','^'},
		{'7','&'},
		{'8','*'},
		{'9','('},
		{'0',')'}
	},
	{
		{'q','Q'},
		{'w','W'},
		{'e','E'},
		{'r','R'},
		{'t','T'},
		{'y','Y'},
		{'u','U'},
		{'i','I'},
		{'o','O'},
		{'p','P'}
	},
	{
		{'a','A'},
		{'s','S'},
		{'d','D'},
		{'f','F'},
		{'g','G'},
		{'h','H'},
		{'j','J'},
		{'k','K'},
		{'l','L'},
		{':',';'}
	},

	{
		{'z','Z'},
		{'x','X'},
		{'c','C'},
		{'v','V'},
		{'b','B'},
		{'n','N'},
		{'m','M'},
		{',','<'},
		{'.','>'},
		{'/','?'}
	}
	};
	memcpy(keys, thekeys, sizeof(thekeys));

	keyTextbox = new GuiImageData(keyboard_textbox_png);
	keyTextboxImg = new GuiImage(keyTextbox);
	keyTextboxImg->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keyTextboxImg->SetPosition(0, 0);
	this->Append(keyTextboxImg);

	kbText = new GuiText(kbtextstr, 22, (GXColor){0, 0, 0, 0xff});
	kbText->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	kbText->SetPosition(0, 13);
	this->Append(kbText);

	key = new GuiImageData(keyboard_key_png);
	keyOver = new GuiImageData(keyboard_key_over_png);
	keyMedium = new GuiImageData(keyboard_mediumkey_png);
	keyMediumOver = new GuiImageData(keyboard_mediumkey_over_png);
	keyLarge = new GuiImageData(keyboard_largekey_png);
	keyLargeOver = new GuiImageData(keyboard_largekey_over_png);

	keySoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	trigA = new GuiTrigger;

	if(GCSettings.WiimoteOrientation)
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	else
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	keyBackImg = new GuiImage(keyMedium);
	keyBackOverImg = new GuiImage(keyMediumOver);
	keyBackText = new GuiText("Back", 22, (GXColor){0, 0, 0, 0xff});
	keyBack = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyBack->SetImage(keyBackImg);
	keyBack->SetImageOver(keyBackOverImg);
	keyBack->SetLabel(keyBackText);
	keyBack->SetSoundOver(keySoundOver);
	keyBack->SetTrigger(trigA);
	keyBack->SetPosition(10*42+40, 0*42+80);
	keyBack->SetEffectGrow();
	this->Append(keyBack);

	keyCapsImg = new GuiImage(keyMedium);
	keyCapsOverImg = new GuiImage(keyMediumOver);
	keyCapsText = new GuiText("Caps", 22, (GXColor){0, 0, 0, 0xff});
	keyCaps = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyCaps->SetImage(keyCapsImg);
	keyCaps->SetImageOver(keyCapsOverImg);
	keyCaps->SetLabel(keyCapsText);
	keyCaps->SetSoundOver(keySoundOver);
	keyCaps->SetTrigger(trigA);
	keyCaps->SetPosition(0, 2*42+80);
	keyCaps->SetEffectGrow();
	this->Append(keyCaps);

	keyShiftImg = new GuiImage(keyMedium);
	keyShiftOverImg = new GuiImage(keyMediumOver);
	keyShiftText = new GuiText("Shift", 22, (GXColor){0, 0, 0, 0xff});
	keyShift = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyShift->SetImage(keyShiftImg);
	keyShift->SetImageOver(keyShiftOverImg);
	keyShift->SetLabel(keyShiftText);
	keyShift->SetSoundOver(keySoundOver);
	keyShift->SetTrigger(trigA);
	keyShift->SetPosition(21, 3*42+80);
	keyShift->SetEffectGrow();
	this->Append(keyShift);

	keySpaceImg = new GuiImage(keyLarge);
	keySpaceOverImg = new GuiImage(keyLargeOver);
	keySpace = new GuiButton(keyLarge->GetWidth(), keyLarge->GetHeight());
	keySpace->SetImage(keySpaceImg);
	keySpace->SetImageOver(keySpaceOverImg);
	keySpace->SetSoundOver(keySoundOver);
	keySpace->SetTrigger(trigA);
	keySpace->SetPosition(0, 4*42+80);
	keySpace->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keySpace->SetEffectGrow();
	this->Append(keySpace);

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<10; j++)
		{
			keyImg[i][j] = new GuiImage(key);
			keyImgOver[i][j] = new GuiImage(keyOver);
			keyTxt[i][j] = new GuiText(NULL, 22, (GXColor){0, 0, 0, 0xff});
			keyTxt[i][j]->SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
			keyTxt[i][j]->SetPosition(0, -10);
			keyBtn[i][j] = new GuiButton(key->GetWidth(), key->GetHeight());
			keyBtn[i][j]->SetImage(keyImg[i][j]);
			keyBtn[i][j]->SetImageOver(keyImgOver[i][j]);
			keyBtn[i][j]->SetSoundOver(keySoundOver);
			keyBtn[i][j]->SetTrigger(trigA);
			keyBtn[i][j]->SetLabel(keyTxt[i][j]);
			keyBtn[i][j]->SetPosition(j*42+21*i+40, i*42+80);
			keyBtn[i][j]->SetEffectGrow();
			this->Append(keyBtn[i][j]);
		}
	}
}

/**
 * Destructor for the GuiKeyboard class.
 */
GuiKeyboard::~GuiKeyboard()
{
	delete kbText;
	delete keyTextbox;
	delete keyTextboxImg;
	delete keyCapsText;
	delete keyCapsImg;
	delete keyCapsOverImg;
	delete keyCaps;
	delete keyShiftText;
	delete keyShiftImg;
	delete keyShiftOverImg;
	delete keyShift;
	delete keyBackText;
	delete keyBackImg;
	delete keyBackOverImg;
	delete keyBack;
	delete keySpaceImg;
	delete keySpaceOverImg;
	delete keySpace;
	delete key;
	delete keyOver;
	delete keyMedium;
	delete keyMediumOver;
	delete keyLarge;
	delete keyLargeOver;
	delete keySoundOver;
	delete trigA;

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<10; j++)
		{
			delete keyImg[i][j];
			delete keyImgOver[i][j];
			delete keyTxt[i][j];
			delete keyBtn[i][j];
		}
	}
}

void GuiKeyboard::Update(GuiTrigger * t)
{
	if(_elements.size() == 0 || (state == STATE_DISABLED && parentElement))
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try	{ _elements.at(i)->Update(t); }
		catch (exception& e) { }
	}

	if(keySpace->GetState() == STATE_CLICKED)
	{
		kbtextstr[strlen(kbtextstr)] = ' ';
		kbText->SetText(kbtextstr);
		keySpace->SetState(STATE_SELECTED);
	}
	else if(keyBack->GetState() == STATE_CLICKED)
	{
		kbtextstr[strlen(kbtextstr)-1] = 0;
		kbText->SetText(kbtextstr);
		keyBack->SetState(STATE_SELECTED);
	}
	else if(keyShift->GetState() == STATE_CLICKED)
	{
		shift ^= 1;
		keyShift->SetState(STATE_SELECTED);
	}
	else if(keyCaps->GetState() == STATE_CLICKED)
	{
		caps ^= 1;
		keyCaps->SetState(STATE_SELECTED);
	}

	char txt[2] = { 0, 0 };

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<10; j++)
		{
			if(shift || caps)
				txt[0] = keys[i][j].chShift;
			else
				txt[0] = keys[i][j].ch;

			keyTxt[i][j]->SetText(txt);

			if(keyBtn[i][j]->GetState() == STATE_CLICKED)
			{
				if(shift || caps)
				{
					kbtextstr[strlen(kbtextstr)] = keys[i][j].chShift;
					if(shift) shift ^= 1;
				}
				else
					kbtextstr[strlen(kbtextstr)] = keys[i][j].ch;

				kbText->SetText(kbtextstr);
				keyBtn[i][j]->SetState(STATE_SELECTED);
			}
		}
	}

	this->ToggleFocus(t);

	if(focus) // only send actions to this window if it's in focus
	{
		// pad/joystick navigation
		if(t->Right())
			this->MoveSelectionHor(1);
		else if(t->Left())
			this->MoveSelectionHor(-1);
		else if(t->Down())
			this->MoveSelectionVert(1);
		else if(t->Up())
			this->MoveSelectionVert(-1);
	}
}
