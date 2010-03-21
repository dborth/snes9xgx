/****************************************************************************
 * libwiigui
 *
 * Tantric 2009
 *
 * gui_text.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include "../utils/gettext.h"

static GXColor presetColor = (GXColor){255, 255, 255, 255};
static int currentSize = 0;
static int presetSize = 0;
static int presetMaxWidth = 0;
static int presetAlignmentHor = 0;
static int presetAlignmentVert = 0;
static u16 presetStyle = 0;

#define TEXT_SCROLL_DELAY			8
#define	TEXT_SCROLL_INITIAL_DELAY	6

/**
 * Constructor for the GuiText class.
 */
GuiText::GuiText(const char * t, int s, GXColor c)
{
	origText = NULL;
	text = NULL;
	size = s;
	color = c;
	alpha = c.a;
	style = FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE;
	maxWidth = 0;
	wrap = false;
	textDyn = NULL;
	textScroll = SCROLL_NONE;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;

	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;

	if(t)
	{
		origText = strdup(t);
		text = charToWideChar(gettext(t));
	}
}

/**
 * Constructor for the GuiText class, uses presets
 */
GuiText::GuiText(const char * t)
{
	origText = NULL;
	text = NULL;
	size = presetSize;
	color = presetColor;
	alpha = presetColor.a;
	style = presetStyle;
	maxWidth = presetMaxWidth;
	wrap = false;
	textDyn = NULL;
	textScroll = SCROLL_NONE;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;

	alignmentHor = presetAlignmentHor;
	alignmentVert = presetAlignmentVert;

	if(t)
	{
		origText = strdup(t);
		text = charToWideChar(gettext(t));
	}
}

/**
 * Destructor for the GuiText class.
 */
GuiText::~GuiText()
{
	if(origText)
		free(origText);
	if(text)
		delete[] text;
	if(textDyn)
		delete[] textDyn;
}

void GuiText::SetText(const char * t)
{
	if(origText)
		free(origText);
	if(text)
		delete[] text;
	if(textDyn)
		delete[] textDyn;

	origText = NULL;
	text = NULL;
	textDyn = NULL;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;

	if(t)
	{
		origText = strdup(t);
		text = charToWideChar(gettext(t));
	}
}

void GuiText::SetPresets(int sz, GXColor c, int w, u16 s, int h, int v)
{
	presetSize = sz;
	presetColor = c;
	presetStyle = s;
	presetMaxWidth = w;
	presetAlignmentHor = h;
	presetAlignmentVert = v;
}

void GuiText::SetFontSize(int s)
{
	size = s;
}

void GuiText::SetMaxWidth(int width)
{
	maxWidth = width;
}

void GuiText::SetWrap(bool w, int width)
{
	wrap = w;
	maxWidth = width;
}

void GuiText::SetScroll(int s)
{
	if(textScroll == s)
		return;

	if(textDyn)
	{
		delete[] textDyn;
		textDyn = NULL;
	}
	textScroll = s;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
}

void GuiText::SetColor(GXColor c)
{
	color = c;
	alpha = c.a;
}

void GuiText::SetStyle(u16 s)
{
	style = s;
}

void GuiText::SetAlignment(int hor, int vert)
{
	style = 0;

	switch(hor)
	{
		case ALIGN_LEFT:
			style |= FTGX_JUSTIFY_LEFT;
			break;
		case ALIGN_RIGHT:
			style |= FTGX_JUSTIFY_RIGHT;
			break;
		default:
			style |= FTGX_JUSTIFY_CENTER;
			break;
	}
	switch(vert)
	{
		case ALIGN_TOP:
			style |= FTGX_ALIGN_TOP;
			break;
		case ALIGN_BOTTOM:
			style |= FTGX_ALIGN_BOTTOM;
			break;
		default:
			style |= FTGX_ALIGN_MIDDLE;
			break;
	}

	alignmentHor = hor;
	alignmentVert = vert;
}

void GuiText::ResetText()
{
	if(!origText)
		return;
	if(text)
		delete[] text;

	text = charToWideChar(gettext(origText));
}

/**
 * Draw the text on screen
 */
void GuiText::Draw()
{
	if(!text)
		return;

	if(!this->IsVisible())
		return;

	GXColor c = color;
	c.a = this->GetAlpha();

	int newSize = size*this->GetScale();

	if(newSize > MAX_FONT_SIZE)
		newSize = MAX_FONT_SIZE;

	if(newSize != currentSize)
	{
		ChangeFontSize(newSize);
		if(!fontSystem[newSize])
			fontSystem[newSize] = new FreeTypeGX(newSize);
		currentSize = newSize;
	}
	
	u8 maxChar;

	if(maxWidth == 0)
	{
		fontSystem[currentSize]->drawText(this->GetLeft(), this->GetTop(), text, c, style);
		goto done;
	}

	maxChar = int((float((maxWidth<<1))) / (float(newSize))); // approximate

	if(wrap)
	{
		int lineheight = newSize + 6;
		int txtlen = wcslen(text);
		int i = 0;
		int ch = 0;
		int linenum = 0;
		int lastSpace = -1;
		int lastSpaceIndex = -1;
		wchar_t * textrow[20];

		while(ch < txtlen)
		{
			if(i == 0)
				textrow[linenum] = new wchar_t[txtlen + 1];

			textrow[linenum][i] = text[ch];
			textrow[linenum][i+1] = 0;

			if(text[ch] == ' ' || ch == txtlen-1)
			{
				if(wcslen(textrow[linenum]) >= maxChar)
				{
					if(lastSpace >= 0)
					{
						textrow[linenum][lastSpaceIndex] = 0; // discard space, and everything after
						ch = lastSpace; // go backwards to the last space
						lastSpace = -1; // we have used this space
						lastSpaceIndex = -1;
					}
					++linenum;
					i = -1;
				}
				else if(ch == txtlen-1)
				{
					++linenum;
				}
			}
			if(text[ch] == ' ' && i >= 0)
			{
				lastSpace = ch;
				lastSpaceIndex = i;
			}
			++ch;
			++i;
		}

		int voffset = 0;

		if(alignmentVert == ALIGN_MIDDLE)
			voffset = (lineheight >> 1) * (1-linenum);

		int left = this->GetLeft();
		int top  = this->GetTop() + voffset;

		for(i=0; i < linenum; ++i)
		{
			fontSystem[currentSize]->drawText(left, top+i*lineheight, textrow[i], c, style);
			delete[] textrow[i];
		}
		goto done;
	}

	if(textScroll == SCROLL_HORIZONTAL)
	{
		char *tmpText = strdup(gettext(origText));
		char *tmpText2 = strdup(tmpText);
		int textlen = strlen(tmpText);

		if(textlen > maxChar && (FrameTimer % textScrollDelay == 0))
		{
			if(textScrollInitialDelay)
			{
				--textScrollInitialDelay;
			}
			else
			{
				++textScrollPos;
				if(textScrollPos > textlen-1)
				{
					textScrollPos = 0;
					textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
				}

				strncpy(tmpText, &tmpText2[textScrollPos], maxChar-1);
				tmpText[maxChar-1] = 0;

				int dynlen = strlen(tmpText);

				if(dynlen+2 < maxChar)
				{
					tmpText[dynlen] = ' ';
					tmpText[dynlen+1] = ' ';
					strncat(&tmpText[dynlen+2], tmpText2, maxChar - dynlen - 2);
				}
				if(textDyn) delete[] textDyn;
				textDyn = charToWideChar(tmpText);
			}
		}
		free(tmpText);
		free(tmpText2);
	}

	if(!textDyn)
	{
		char *tmpText = strdup(gettext(origText));
		if(strlen(tmpText) > maxChar)
			tmpText[maxChar] = 0;
		textDyn = charToWideChar(tmpText);
		free(tmpText);
	}

	fontSystem[currentSize]->drawText(this->GetLeft(), this->GetTop(), textDyn, c, style);
done:
	this->UpdateEffects();
}
