/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_text.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include "snes9xGX.h"
#include "filelist.h"

static int currentSize = 0;

/**
 * Constructor for the GuiText class.
 */
GuiText::GuiText(const char * t, int s, GXColor c)
{
	text = NULL;
	size = s;
	color = c;
	style = FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE;

	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;

	if(t)
	{
		// this is temporary: removes - and '
		// because FreeType GX won't show them
		char newt[200];
		int i = -1;
		strcpy(newt, t);
		while(newt[++i] != 0)
			if(newt[i] == '-' || newt[i] == '\'')
				newt[i] = ' ';

		text = fontSystem->charToWideChar((char *)newt);
	}
}

/**
 * Destructor for the GuiText class.
 */
GuiText::~GuiText()
{
	if(text)
	{
		delete text;
		text = NULL;
	}
}

void GuiText::SetText(const char * t)
{
	if(text)
		delete text;

	text = NULL;

	if(t)
	{
		// this is temporary: removes - and '
		// because FreeType GX won't show them
		char newt[200];
		int i = -1;
		strcpy(newt, t);
		while(newt[++i] != 0)
			if(newt[i] == '-' || newt[i] == '\'')
				newt[i] = ' ';

		text = fontSystem->charToWideChar((char *)newt);
	}
}
void GuiText::SetSize(int s)
{
	size = s;
}
void GuiText::SetColor(GXColor c)
{
	color = c;
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

/**
 * Draw the text on screen
 */
void GuiText::Draw()
{
	if(!text)
		return;

	if(!this->IsVisible())
		return;

	if(size != currentSize)
	{
		fontSystem->loadFont(font_ttf, font_ttf_size, size);
		currentSize = size;
	}
	fontSystem->drawText(this->GetLeft(), this->GetTop(), text, color, style);
}
