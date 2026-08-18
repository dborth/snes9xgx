/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiImageData.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

/**
 * Constructor for the GuiImageData class.
 */
GuiImageData::GuiImageData(const u8 * i, int maxw, int maxh)
{
	data = NULL;
	width = 0;
	height = 0;

	if(i)
		data = DecodePNG(i, &width, &height, data, maxw, maxh);
}

/**
 * Destructor for the GuiImageData class.
 */
GuiImageData::~GuiImageData()
{
	if(data)
	{
		free(data);
		data = NULL;
	}
}

u8 * GuiImageData::getImage()
{
	return data;
}

int GuiImageData::getWidth()
{
	return width;
}

int GuiImageData::getHeight()
{
	return height;
}
