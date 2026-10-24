/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiKeyboard.cpp
 ***************************************************************************/

#include "libgui/Gui.h"
#include "GuiKeyboard.h"

#define KB_FONTSIZE 22

// The keys are drawn from the theme, in three widths. Text field sits at the top.
#define KB_KEY_HEIGHT		40
#define KB_KEY_WIDTH		40
#define KB_MEDIUM_WIDTH		80
#define KB_LARGE_WIDTH		200
#define KB_KEY_RADIUS		10.0f
#define KB_TEXTBOX_WIDTH	400
#define KB_TEXTBOX_Y		10
#define KB_TEXT_Y		23		// text inside the field (KB_TEXTBOX_Y + 13)

//! A key face of the given width: light and outlined at rest, tinted with an accent
//! outline and glow when the pointer is on it, deeper while pressed.
static GuiShape * newKeyShape(int w)
{
	const GuiTheme& theme = GuiTheme::current();
	PixelColor glow = theme.accent;
	glow.a = 110;

	GuiShape * s = new GuiShape(w, KB_KEY_HEIGHT);
	s->setCorners(GuiCorners(KB_KEY_RADIUS));
	s->setStyle(GuiShapeStyle(GuiPaint().gradient(theme.surfaceRaised, theme.surface).stroke(1.5f, theme.outlineStrong).shadow(4.0f, theme.shadow, 0.0f, 1.0f))
		.onFocus(GuiPaint().gradient(theme.surfaceRaised, theme.highlight).stroke(2.5f, theme.accent).glow(10.0f, glow, 1.0f))
		.onPress(GuiPaint().gradient(theme.highlight, theme.outline).stroke(2.0f, theme.accentDark)));
	return s;
}

static char tmptxt[MAX_KEYBOARD_DISPLAY+1];

static const char * GetDisplayText(const char * t)
{
	if(!t)
		return nullptr;

	strncpy(tmptxt, t, MAX_KEYBOARD_DISPLAY);
	tmptxt[MAX_KEYBOARD_DISPLAY] = '\0';
	return &tmptxt[0];
}

GuiKeyboard::GuiKeyboard(char * t, uint32_t max)
{
	width = 540;
	height = 400;
	shift = 0;
	caps = 0;
	selectable = true;
	focus = 0; // allow focus
	alignmentHor = ALIGN_H::CENTRE;
	alignmentVert = ALIGN_V::MIDDLE;
	snprintf(kbtextstr, 255, "%s", t);
	kbtextmaxlen = max;

	Key thekeys[KB_ROWS][KB_COLUMNS] = {
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

	const GuiTheme& theme = GuiTheme::current();

	// the text being edited: a white capsule field with a firm outline
	keyTextboxShape = new GuiShape(KB_TEXTBOX_WIDTH, KB_KEY_HEIGHT);
	keyTextboxShape->setCapsule();
	keyTextboxShape->setPaint(GuiPaint().color(theme.surfaceRaised).stroke(2.0f, theme.accentDark));
	keyTextboxShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	keyTextboxShape->setPosition(0, KB_TEXTBOX_Y);
	this->append(keyTextboxShape);

	kbText = new GuiText(GetDisplayText(kbtextstr), KB_FONTSIZE, theme.text);
	kbText->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	kbText->setPosition(0, KB_TEXT_Y);
	this->append(kbText);

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	keyBackImg = newKeyShape(KB_MEDIUM_WIDTH);
	keyBackText = new GuiText("Back", KB_FONTSIZE, theme.text);
	keyBack = new GuiButton(KB_MEDIUM_WIDTH, KB_KEY_HEIGHT);
	keyBack->setImage(keyBackImg);
	keyBack->setLabel(keyBackText);
	keyBack->setTrigger(trigA);
	keyBack->setPosition(10*42+44, 0*42+80);
	keyBack->setEffectGrow();
	this->append(keyBack);

	keyCapsImg = newKeyShape(KB_MEDIUM_WIDTH);
	keyCapsText = new GuiText("Caps", KB_FONTSIZE, theme.text);
	keyCaps = new GuiButton(KB_MEDIUM_WIDTH, KB_KEY_HEIGHT);
	keyCaps->setImage(keyCapsImg);
	keyCaps->setLabel(keyCapsText);
	keyCaps->setTrigger(trigA);
	keyCaps->setPosition(0, 2*42+80);
	keyCaps->setEffectGrow();
	this->append(keyCaps);

	keyShiftImg = newKeyShape(KB_MEDIUM_WIDTH);
	keyShiftText = new GuiText("Shift", KB_FONTSIZE, theme.text);
	keyShift = new GuiButton(KB_MEDIUM_WIDTH, KB_KEY_HEIGHT);
	keyShift->setImage(keyShiftImg);
	keyShift->setLabel(keyShiftText);
	keyShift->setTrigger(trigA);
	keyShift->setPosition(21, 3*42+80);
	keyShift->setEffectGrow();
	this->append(keyShift);

	keySpaceImg = newKeyShape(KB_LARGE_WIDTH);
	keySpace = new GuiButton(KB_LARGE_WIDTH, KB_KEY_HEIGHT);
	keySpace->setImage(keySpaceImg);
	keySpace->setTrigger(trigA);
	keySpace->setPosition(0, 4*42+80);
	keySpace->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	keySpace->setEffectGrow();
	this->append(keySpace);

	char txt[2] = { 0, 0 };

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				txt[0] = keys[i][j].ch;
				keyImg[i][j] = newKeyShape(KB_KEY_WIDTH);
				keyTxt[i][j] = new GuiText(txt, KB_FONTSIZE, theme.text);
				keyTxt[i][j]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
				keyTxt[i][j]->setPosition(0, -8);
				keyBtn[i][j] = new GuiButton(KB_KEY_WIDTH, KB_KEY_HEIGHT);
				keyBtn[i][j]->setImage(keyImg[i][j]);
				keyBtn[i][j]->setTrigger(trigA);
				keyBtn[i][j]->setLabel(keyTxt[i][j]);
				keyBtn[i][j]->setPosition(j*42+21*i+42, i*42+80);
				keyBtn[i][j]->setEffectGrow();
				this->append(keyBtn[i][j]);
			}
		}
	}
}

GuiKeyboard::~GuiKeyboard()
{
	delete kbText;
	delete keyTextboxShape;
	delete keyCapsText;
	delete keyCapsImg;
	delete keyCaps;
	delete keyShiftText;
	delete keyShiftImg;
	delete keyShift;
	delete keyBackText;
	delete keyBackImg;
	delete keyBack;
	delete keySpaceImg;
	delete keySpace;
	delete trigA;

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				delete keyImg[i][j];
				delete keyTxt[i][j];
				delete keyBtn[i][j];
			}
		}
	}
}

void GuiKeyboard::update(InputController * c)
{
	if(_elements.size() == 0 || (state == STATE::DISABLED && parentElement))
		return;

	for (uint8_t i = 0; i < _elements.size(); i++)
	{
		_elements.at(i)->update(c);
	}

	bool update = false;

	if(keySpace->getState() == STATE::CLICKED)
	{
		size_t len = strlen(kbtextstr);
		if(len < kbtextmaxlen-1)
		{
			kbtextstr[len] = ' ';
			kbtextstr[len+1] = '\0';
			kbText->setText(kbtextstr);
		}
		keySpace->setState(STATE::SELECTED, c->getChannel());
	}
	else if(keyBack->getState() == STATE::CLICKED)
	{
		size_t len = strlen(kbtextstr);
		if(len > 0)
		{
			kbtextstr[len-1] = '\0';
			kbText->setText(GetDisplayText(kbtextstr));
		}
		keyBack->setState(STATE::SELECTED, c->getChannel());
	}
	else if(keyShift->getState() == STATE::CLICKED)
	{
		shift ^= 1;
		keyShift->setState(STATE::SELECTED, c->getChannel());
		update = true;
	}
	else if(keyCaps->getState() == STATE::CLICKED)
	{
		caps ^= 1;
		keyCaps->setState(STATE::SELECTED, c->getChannel());
		update = true;
	}

	char txt[2] = { 0, 0 };

	startloop:

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
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

				if(keyBtn[i][j]->getState() == STATE::CLICKED)
				{
					size_t len = strlen(kbtextstr);

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
					keyBtn[i][j]->setState(STATE::SELECTED, c->getChannel());

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

	this->toggleFocus(c);

	if(focus) // only send actions to this window if it's in focus
	{
		// pad/joystick navigation
		if(c->right())
			this->moveSelectionHor(1);
		else if(c->left())
			this->moveSelectionHor(-1);
		else if(c->down())
			this->moveSelectionVert(1);
		else if(c->up())
			this->moveSelectionVert(-1);
	}
}
