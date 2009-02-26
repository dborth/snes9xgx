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
	alpha = 255;
	scaleX = 1;
	scaleY = 1;
	tile = 0;
}

GuiImage::GuiImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imageangle = 0;
	alpha = 255;
	scaleX = 1;
	scaleY = 1;
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

void GuiImage::SetAlpha(int a)
{
	alpha = a;
}

void GuiImage::SetScale(float x, float y)
{
	scaleX = x;
	scaleY = y;
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

	if(tile > 0)
	{
		for(int i=0; i<tile; i++)
			Menu_DrawImg(this->GetLeft()+width*i, this->GetTop(), width, height, image, imageangle, scaleX, scaleY, alpha);
	}
	else
	{
		int left = this->GetLeft();

		// temporary (maybe), used to correct offset for scaled images
		if(scaleX != 1)
			left = left - width/2 + (width*scaleX)/2;

		Menu_DrawImg(left, this->GetTop(), width, height, image, imageangle, scaleX, scaleY, alpha);
	}
}
