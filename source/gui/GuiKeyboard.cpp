/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiKeyboard.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

static char tmptxt[MAX_KEYBOARD_DISPLAY];

static char * GetDisplayText(char * t)
{
	if(!t)
		return NULL;

	snprintf(tmptxt, MAX_KEYBOARD_DISPLAY, "%s", t);
	return &tmptxt[0];
}

/**
 * Constructor for the GuiKeyboard class.
 */

GuiKeyboard::GuiKeyboard(char * t, u32 max)
{
	width = 540;
	height = 400;
	shift = 0;
	caps = 0;
	selectable = true;
	focus = 0; // allow focus
	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;
	snprintf(kbtextstr, 255, "%s", t);
	kbtextmaxlen = max;

	Key thekeys[4][11] = {
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
		{'0',')'},
		{'\0','\0'}
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
		{'p','P'},
		{'-','_'}
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
		{';',':'},
		{'\'','"'}
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
		{'/','?'},
		{'\0','\0'}
	}
	};
	memcpy(keys, thekeys, sizeof(thekeys));

	keyTextbox = new GuiImageData(keyboard_textbox_png);
	keyTextboxImg = new GuiImage(keyTextbox);
	keyTextboxImg->setAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keyTextboxImg->setPosition(0, 0);
	this->append(keyTextboxImg);

	kbText = new GuiText(GetDisplayText(kbtextstr), 22, (GXColor){0, 0, 0, 0xff});
	kbText->setAlignment(ALIGN_CENTRE, ALIGN_TOP);
	kbText->setPosition(0, 13);
	this->append(kbText);

	key = new GuiImageData(keyboard_key_png);
	keyOver = new GuiImageData(keyboard_key_over_png);
	keyMedium = new GuiImageData(keyboard_mediumkey_png);
	keyMediumOver = new GuiImageData(keyboard_mediumkey_over_png);
	keyLarge = new GuiImageData(keyboard_largekey_png);
	keyLargeOver = new GuiImageData(keyboard_largekey_over_png);

	keySoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	keySoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);

	trigA = new GuiTrigger;
	trigA->setSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, WIIDRC_BUTTON_A);
	trig2 = new GuiTrigger;
	trig2->setSimpleTrigger(-1, WPAD_BUTTON_2, 0, 0);

	keyBackImg = new GuiImage(keyMedium);
	keyBackOverImg = new GuiImage(keyMediumOver);
	keyBackText = new GuiText("Back", 22, (GXColor){0, 0, 0, 0xff});
	keyBack = new GuiButton(keyMedium->getWidth(), keyMedium->getHeight());
	keyBack->setImage(keyBackImg);
	keyBack->setImageOver(keyBackOverImg);
	keyBack->setLabel(keyBackText);
	keyBack->setSoundOver(keySoundOver);
	keyBack->setSoundClick(keySoundClick);
	keyBack->setTrigger(trigA);
	keyBack->setTrigger(trig2);
	keyBack->setPosition(10*42+40, 0*42+80);
	keyBack->setEffectGrow();
	this->append(keyBack);

	keyCapsImg = new GuiImage(keyMedium);
	keyCapsOverImg = new GuiImage(keyMediumOver);
	keyCapsText = new GuiText("Caps", 22, (GXColor){0, 0, 0, 0xff});
	keyCaps = new GuiButton(keyMedium->getWidth(), keyMedium->getHeight());
	keyCaps->setImage(keyCapsImg);
	keyCaps->setImageOver(keyCapsOverImg);
	keyCaps->setLabel(keyCapsText);
	keyCaps->setSoundOver(keySoundOver);
	keyCaps->setSoundClick(keySoundClick);
	keyCaps->setTrigger(trigA);
	keyCaps->setTrigger(trig2);
	keyCaps->setPosition(0, 2*42+80);
	keyCaps->setEffectGrow();
	this->append(keyCaps);

	keyShiftImg = new GuiImage(keyMedium);
	keyShiftOverImg = new GuiImage(keyMediumOver);
	keyShiftText = new GuiText("Shift", 22, (GXColor){0, 0, 0, 0xff});
	keyShift = new GuiButton(keyMedium->getWidth(), keyMedium->getHeight());
	keyShift->setImage(keyShiftImg);
	keyShift->setImageOver(keyShiftOverImg);
	keyShift->setLabel(keyShiftText);
	keyShift->setSoundOver(keySoundOver);
	keyShift->setSoundClick(keySoundClick);
	keyShift->setTrigger(trigA);
	keyShift->setTrigger(trig2);
	keyShift->setPosition(21, 3*42+80);
	keyShift->setEffectGrow();
	this->append(keyShift);

	keySpaceImg = new GuiImage(keyLarge);
	keySpaceOverImg = new GuiImage(keyLargeOver);
	keySpace = new GuiButton(keyLarge->getWidth(), keyLarge->getHeight());
	keySpace->setImage(keySpaceImg);
	keySpace->setImageOver(keySpaceOverImg);
	keySpace->setSoundOver(keySoundOver);
	keySpace->setSoundClick(keySoundClick);
	keySpace->setTrigger(trigA);
	keySpace->setTrigger(trig2);
	keySpace->setPosition(0, 4*42+80);
	keySpace->setAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keySpace->setEffectGrow();
	this->append(keySpace);

	char txt[2] = { 0, 0 };

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<11; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				txt[0] = keys[i][j].ch;
				keyImg[i][j] = new GuiImage(key);
				keyImgOver[i][j] = new GuiImage(keyOver);
				keyTxt[i][j] = new GuiText(txt, 22, (GXColor){0, 0, 0, 0xff});
				keyTxt[i][j]->setAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
				keyTxt[i][j]->setPosition(0, -8);
				keyBtn[i][j] = new GuiButton(key->getWidth(), key->getHeight());
				keyBtn[i][j]->setImage(keyImg[i][j]);
				keyBtn[i][j]->setImageOver(keyImgOver[i][j]);
				keyBtn[i][j]->setSoundOver(keySoundOver);
				keyBtn[i][j]->setSoundClick(keySoundClick);
				keyBtn[i][j]->setTrigger(trigA);
				keyBtn[i][j]->setTrigger(trig2);
				keyBtn[i][j]->setLabel(keyTxt[i][j]);
				keyBtn[i][j]->setPosition(j*42+21*i+40, i*42+80);
				keyBtn[i][j]->setEffectGrow();
				this->append(keyBtn[i][j]);
			}
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
	delete keySoundClick;
	delete trigA;
	delete trig2;

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<11; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				delete keyImg[i][j];
				delete keyImgOver[i][j];
				delete keyTxt[i][j];
				delete keyBtn[i][j];
			}
		}
	}
}

void GuiKeyboard::update(GuiTrigger * t)
{
	if(_elements.size() == 0 || (state == STATE_DISABLED && parentElement))
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		_elements.at(i)->update(t);
	}

	bool update = false;

	if(keySpace->getState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) < kbtextmaxlen)
		{
			kbtextstr[strlen(kbtextstr)] = ' ';
			kbText->setText(kbtextstr);
		}
		keySpace->setState(STATE_SELECTED, t->chan);
	}
	else if(keyBack->getState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) > 0)
		{
			kbtextstr[strlen(kbtextstr)-1] = 0;
			kbText->setText(GetDisplayText(kbtextstr));
		}
		keyBack->setState(STATE_SELECTED, t->chan);
	}
	else if(keyShift->getState() == STATE_CLICKED)
	{
		shift ^= 1;
		keyShift->setState(STATE_SELECTED, t->chan);
		update = true;
	}
	else if(keyCaps->getState() == STATE_CLICKED)
	{
		caps ^= 1;
		keyCaps->setState(STATE_SELECTED, t->chan);
		update = true;
	}

	char txt[2] = { 0, 0 };

	startloop:

	for(int i=0; i<4; i++)
	{
		for(int j=0; j<11; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				if(update)
				{
					if(shift || caps)
						txt[0] = keys[i][j].chShift;
					else
						txt[0] = keys[i][j].ch;

					keyTxt[i][j]->setText(txt);
				}

				if(keyBtn[i][j]->getState() == STATE_CLICKED)
				{
					u32 len = strlen(kbtextstr);

					if(len < kbtextmaxlen-1)
					{
						if(shift || caps)
						{
							kbtextstr[len] = keys[i][j].chShift;
						}
						else
						{
							kbtextstr[len] = keys[i][j].ch;
						}
						kbtextstr[len+1] = '\0';
					}
					kbText->setText(GetDisplayText(kbtextstr));
					keyBtn[i][j]->setState(STATE_SELECTED, t->chan);

					if(shift)
					{
						shift ^= 1;
						update = true;
						goto startloop;
					}
				}
			}
		}
	}

	this->toggleFocus(t);

	if(focus) // only send actions to this window if it's in focus
	{
		// pad/joystick navigation
		if(t->right())
			this->moveSelectionHor(1);
		else if(t->left())
			this->moveSelectionHor(-1);
		else if(t->down())
			this->moveSelectionVert(1);
		else if(t->up())
			this->moveSelectionVert(-1);
	}
}
