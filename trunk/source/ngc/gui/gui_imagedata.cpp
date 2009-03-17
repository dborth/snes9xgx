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
		IMGCTX ctx = PNGU_SelectImageFromBuffer(img);

		if(!ctx)
			return;

		int res = PNGU_GetImageProperties(ctx, &imgProp);

		if(res == PNGU_OK)
		{
			data = (u8 *)memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);

			if(data)
			{
				res = PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, data, 255);

				if(res == PNGU_OK)
				{
					width = imgProp.imgWidth;
					height = imgProp.imgHeight;
					int len = imgProp.imgWidth * imgProp.imgHeight * 4;
					DCFlushRange(data, len+len%32);
				}
				else
				{
					free(data);
					data = NULL;
				}
			}
		}
		PNGU_ReleaseImageContext (ctx);
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
