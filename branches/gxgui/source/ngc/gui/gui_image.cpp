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
}

GuiImage::GuiImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imageangle = 0;
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

void GuiImage::SetAngle(float a)
{
	imageangle = a;
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

	GRRLIB_DrawImg(this->GetLeft(), this->GetTop(), width, height, image, imageangle, 1, 1, 255);
}
