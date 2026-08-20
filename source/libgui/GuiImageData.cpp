/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImageData.cpp
 ***************************************************************************/

#include "Gui.h"

GuiImageData::GuiImageData(const uint8_t * i, int maxw, int maxh)
{
	data = nullptr;
	width = 0;
	height = 0;

	if(i)
		data = DecodePNG(i, &width, &height, data, maxw, maxh);
}

GuiImageData::~GuiImageData()
{
	if(data)
	{
		free(data);
		data = nullptr;
	}
}

uint8_t * GuiImageData::getImage()
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
