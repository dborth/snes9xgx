/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_imagedata.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

/**
 * Constructor for the GuiImageData class.
 */
GuiImageData::GuiImageData(const u8 * img)
{
	if(img == NULL)
	{
		data = NULL;
		width = 0;
		height = 0;
	}
	else
	{
		PNGUPROP imgProp;
		IMGCTX ctx;

		ctx = PNGU_SelectImageFromBuffer(img);
		PNGU_GetImageProperties (ctx, &imgProp);
		width = imgProp.imgWidth;
		height = imgProp.imgHeight;
		data = (u8 *)memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
		PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, data, 255);
		PNGU_ReleaseImageContext (ctx);
		DCFlushRange (data, imgProp.imgWidth * imgProp.imgHeight * 4);
	}
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

u8 * GuiImageData::GetImage()
{
	return data;
}

int GuiImageData::GetWidth()
{
	return width;
}

int GuiImageData::GetHeight()
{
	return height;
}
