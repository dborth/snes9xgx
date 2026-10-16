/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiText.cpp
 ***************************************************************************/

#include "Gui.h"

GuiTextTranslator* textTranslator = new GuiTextTranslator();
static PixelColor presetColor = (PixelColor){255, 255, 255, 255};
static int currentSize = 0;
static int presetSize = 0;
static int presetMaxWidth = 0;
static ALIGN_H presetAlignmentHor = ALIGN_H::LEFT;
static ALIGN_V presetAlignmentVert = ALIGN_V::TOP;
static uint16_t presetStyle = 0;

#define TEXT_SCROLL_DELAY			8
#define	TEXT_SCROLL_INITIAL_DELAY	6

/**
 * Constructor for the GuiText class.
 */
GuiText::GuiText(const char * t, int s, PixelColor c)
{
	origText = nullptr;
	text = nullptr;
	size = s;
	color = c;
	alpha = c.a;
	style = GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_MIDDLE;
	maxWidth = 0;
	wrap = false;
	textDynNum = 0;
	textScroll = SCROLL::NONE;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
	measuredW = measuredH = 0;
	measuredSize = fullWidthSize = -1;
	measuredGen = fullWidthGen = 0;
	fullWidth = 0;

	alignmentHor = ALIGN_H::CENTRE;
	alignmentVert = ALIGN_V::MIDDLE;

	if(t)
	{
		origText = strdup(t);
		text = getText(t);
	}

	for(int i=0; i < 20; i++)
		textDyn[i] = nullptr;
}

/**
 * Constructor for the GuiText class, uses presets
 */
GuiText::GuiText(const char * t)
{
	origText = nullptr;
	text = nullptr;
	size = presetSize;
	color = presetColor;
	alpha = presetColor.a;
	style = presetStyle;
	maxWidth = presetMaxWidth;
	wrap = false;
	textDynNum = 0;
	textScroll = SCROLL::NONE;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
	measuredW = measuredH = 0;
	measuredSize = fullWidthSize = -1;
	measuredGen = fullWidthGen = 0;
	fullWidth = 0;

	alignmentHor = presetAlignmentHor;
	alignmentVert = presetAlignmentVert;

	if(t)
	{
		origText = strdup(t);
		text = getText(t);
	}

	for(int i=0; i < 20; i++)
		textDyn[i] = nullptr;
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

	if(textDynNum > 0)
	{
		for(int i=0; i < textDynNum; i++)
			if(textDyn[i])
				delete[] textDyn[i];
	}
}

wchar_t* GuiText::getText(const char *t) const {
	return GuiTextRenderer::charToWideChar(textTranslator->getText(t));
}

void GuiText::setText(const char * t)
{
	if(origText)
		free(origText);
	if(text)
		delete[] text;

	if(textDynNum > 0)
	{
		for(int i=0; i < textDynNum; i++)
			if(textDyn[i])
				delete[] textDyn[i];
	}

	origText = nullptr;
	text = nullptr;
	textDynNum = 0;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	this->forgetMeasurements();

	if(t)
	{
		origText = strdup(t);
		text = getText(t);
	}
}

void GuiText::setWText(wchar_t * t)
{
	if(origText)
		free(origText);
	if(text)
		delete[] text;

	if(textDynNum > 0)
	{
		for(int i=0; i < textDynNum; i++)
			if(textDyn[i])
				delete[] textDyn[i];
	}

	origText = nullptr;
	text = nullptr;
	textDynNum = 0;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	this->forgetMeasurements();

	if(t)
		text = wcsdup(t);
}

int GuiText::getLength()
{
	if(!text)
		return 0;

	return wcslen(text);
}

void GuiText::setPresets(int sz, PixelColor c, int w, uint16_t s, ALIGN_H h, ALIGN_V v)
{
	presetSize = sz;
	presetColor = c;
	presetStyle = s;
	presetMaxWidth = w;
	presetAlignmentHor = h;
	presetAlignmentVert = v;
}

void GuiText::setFontSize(int s)
{
	size = s;
}

//! Measures the text at `size` unless that has already been done for this text, size and font
void GuiText::measure()
{
	if(measuredSize == size && measuredGen == GuiTextRenderer::getMetricsGeneration())
		return;

	fontSystem->setPixelSize(size);
	measuredW = fontSystem->getWidth(text);
	measuredH = fontSystem->getHeight(text);
	measuredSize = size;
	measuredGen = GuiTextRenderer::getMetricsGeneration();
}

void GuiText::forgetMeasurements()
{
	measuredSize = -1;
	fullWidthSize = -1;
}

//! The width of the whole text at a font size, measured on the first ask. The font must be at that size.
int GuiText::getFullWidth(int atSize)
{
	if(fullWidthSize != atSize || fullWidthGen != GuiTextRenderer::getMetricsGeneration())
	{
		fullWidth = fontSystem->getWidth(text);
		fullWidthSize = atSize;
		fullWidthGen = GuiTextRenderer::getMetricsGeneration();
	}
	return fullWidth;
}

void GuiText::setMaxWidth(int width)
{
	maxWidth = width;

	for(int i=0; i < textDynNum; i++)
	{
		if(textDyn[i])
		{
			delete[] textDyn[i];
			textDyn[i] = nullptr;
		}
	}

	textDynNum = 0;
}

int GuiText::getTextWidth()
{
	if(!text)
		return 0;

	this->measure();
	return measuredW;
}

void GuiText::setWrap(bool w, int width)
{
	wrap = w;
	maxWidth = width;

	for(int i=0; i < textDynNum; i++)
	{
		if(textDyn[i])
		{
			delete[] textDyn[i];
			textDyn[i] = nullptr;
		}
	}

	textDynNum = 0;
}

void GuiText::setScroll(SCROLL s)
{
	if(textScroll == s)
		return;

	for(int i=0; i < textDynNum; i++)
	{
		if(textDyn[i])
		{
			delete[] textDyn[i];
			textDyn[i] = nullptr;
		}
	}

	textDynNum = 0;

	textScroll = s;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
}

void GuiText::setColor(PixelColor c)
{
	color = c;
	alpha = c.a;
}

void GuiText::setStyle(uint16_t s)
{
	style = s;
}

void GuiText::setAlignment(ALIGN_H hor, ALIGN_V vert)
{
	style = 0;

	switch(hor)
	{
		case ALIGN_H::LEFT:
			style |= GUI_TEXT_JUSTIFY_LEFT;
			break;
		case ALIGN_H::RIGHT:
			style |= GUI_TEXT_JUSTIFY_RIGHT;
			break;
		default:
			style |= GUI_TEXT_JUSTIFY_CENTER;
			break;
	}
	switch(vert)
	{
		case ALIGN_V::TOP:
			style |= GUI_TEXT_ALIGN_TOP;
			break;
		case ALIGN_V::BOTTOM:
			style |= GUI_TEXT_ALIGN_BOTTOM;
			break;
		default:
			style |= GUI_TEXT_ALIGN_MIDDLE;
			break;
	}

	alignmentHor = hor;
	alignmentVert = vert;
}

void GuiText::resetText()
{
	if(!origText)
		return;
	if(text)
		delete[] text;

	text = getText(origText);
	this->forgetMeasurements();

	for(int i=0; i < textDynNum; i++)
	{
		if(textDyn[i])
		{
			delete[] textDyn[i];
			textDyn[i] = nullptr;
		}
	}

	textDynNum = 0;
	currentSize = 0;
}

/**
 * Returns true once every textScrollDelay frame timer ticks.
 */
bool GuiText::scrollStepDue()
{
	const uint32_t now = platform->getVideo()->getFrameTimer();

	// Timer was reset: resync instead of stalling
	if(now < textScrollLastTick)
		textScrollLastTick = now;

	if(now - textScrollLastTick < (uint32_t)textScrollDelay)
		return false;

	textScrollLastTick = now;
	return true;
}

/**
 * Draw the text on screen
 */
void GuiText::draw()
{
	if(!text)
		return;

	if(!this->isVisible())
		return;

	PixelColor c = color;
	c.a = this->getAlpha();

	int newSize = size*this->getScale();

	fontSystem->setPixelSize(newSize);

	if(maxWidth == 0)
	{
		fontSystem->drawText(this->getLeft(), this->getTop(), text, c, style);
		this->updateEffects();
		return;
	}

	uint32_t textlen = wcslen(text);

	if(wrap)
	{
		if(textDynNum == 0)
		{
			int n = 0;
			uint32_t ch = 0;
			int linenum = 0;
			int lastSpace = -1;
			int lastSpaceIndex = -1;

			while(ch < textlen && linenum < 20)
			{
				if(n == 0)
					textDyn[linenum] = new wchar_t[textlen + 1];

				textDyn[linenum][n] = text[ch];
				textDyn[linenum][n+1] = 0;

				if(text[ch] == ' ' || ch == textlen-1)
				{
					if(fontSystem->getWidth(textDyn[linenum]) > maxWidth)
					{
						if(lastSpace >= 0)
						{
							textDyn[linenum][lastSpaceIndex] = 0; // discard space, and everything after
							ch = lastSpace; // go backwards to the last space
							lastSpace = -1; // we have used this space
							lastSpaceIndex = -1;
						}
						++linenum;
						n = -1;
					}
					else if(ch == textlen-1)
					{
						++linenum;
					}
				}
				if(text[ch] == ' ' && n >= 0)
				{
					lastSpace = ch;
					lastSpaceIndex = n;
				}
				++ch;
				++n;
			}
			textDynNum = linenum;
		}

		int lineheight = newSize + 6;
		int voffset = 0;

		if(alignmentVert == ALIGN_V::MIDDLE)
			voffset = (lineheight >> 1) * (1-textDynNum);

		int left = this->getLeft();
		int top  = this->getTop() + voffset;

		for(int i=0; i < textDynNum; ++i)
			fontSystem->drawText(left, top+i*lineheight, textDyn[i], c, style);
	}
	else
	{
		if(textDynNum == 0)
		{
			textDynNum = 1;
			textDyn[0] = wcsdup(text);

			// the longest start of the text that fits, found in one pass
			textDyn[0][fontSystem->fitCount(textDyn[0], maxWidth)] = 0;
		}

		if(textScroll == SCROLL::HORIZONTAL)
		{
			if(getFullWidth(newSize) > maxWidth && scrollStepDue())
			{
				if(textScrollInitialDelay)
				{
					--textScrollInitialDelay;
				}
				else
				{
					++textScrollPos;
					if((uint32_t)textScrollPos > textlen-1)
					{
						textScrollPos = 0;
						textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
					}

					fontSystem->scrollWindow(text, textlen, textScrollPos, maxWidth, textDyn[0]);
				}
			}
		}
		fontSystem->drawText(this->getLeft(), this->getTop(), textDyn[0], c, style);
	}
	this->updateEffects();
}

void GuiText::getNaturalSize(int& w, int& h)
{
	if(!text)
	{
		w = 0;
		h = 0;
		return;
	}

	this->measure();
	w = measuredW;
	h = measuredH;
	if(maxWidth > 0 && w > maxWidth)
		w = maxWidth;
}
