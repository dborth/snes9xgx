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
}

GuiImage::GuiImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imageangle = 0;
	alpha = 255;
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

/**
 * Draw the button on screen
 */
void GuiImage::Draw()
{
	if(!image)
		return;

	if(!this->IsVisible())
		return;

	// temporary (kind of), used to correct offset for scaled images
	int theleft = this->GetLeft() - width/2 + (width*scaleX)/2;

	if(scaleX == 1)
		GRRLIB_DrawImg(this->GetLeft(), this->GetTop(), width, height, image, imageangle, scaleX, scaleY, alpha);
	else
		GRRLIB_DrawImg(theleft, this->GetTop(), width, height, image, imageangle, scaleX, scaleY, alpha);
}
