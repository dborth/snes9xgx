/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_image.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
/**
 * Constructor for the GuiImage class.
 */

GuiImage::GuiImage(GuiImageData * img)
{
	image = img->GetImage();
	width = img->GetWidth();
	height = img->GetHeight();
	imageangle = 0;
	tile = 0;
}

GuiImage::GuiImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imageangle = 0;
	tile = 0;
}

/**
 * Destructor for the GuiImage class.
 */
GuiImage::~GuiImage()
{
}

u8 * GuiImage::GetImage()
{
	return image;
}

void GuiImage::SetImage(GuiImageData * img)
{
	image = img->GetImage();
	width = img->GetWidth();
	height = img->GetHeight();
}

void GuiImage::SetImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
}

void GuiImage::SetAngle(float a)
{
	imageangle = a;
}

void GuiImage::SetTile(int t)
{
	tile = t;
}

/**
 * Draw the button on screen
 */
void GuiImage::Draw()
{
	if(!image)
		return;

	if(!this->IsVisible())
		return;

	float currScale = this->GetScale();
	int currLeft = this->GetLeft();

	if(tile > 0)
	{
		for(int i=0; i<tile; i++)
			Menu_DrawImg(currLeft+width*i, this->GetTop(), width, height, image, imageangle, currScale, currScale, this->GetAlpha());
	}
	else
	{
		// temporary (maybe), used to correct offset for scaled images
		if(scale != 1)
			currLeft = currLeft - width/2 + (width*scale)/2;

		Menu_DrawImg(currLeft, this->GetTop(), width, height, image, imageangle, currScale, currScale, this->GetAlpha());
	}

	this->UpdateEffects();
}
