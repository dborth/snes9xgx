/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiImage.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "../libgui/Gui.h"
/**
 * Constructor for the GuiImage class.
 */
GuiImage::GuiImage()
{
	image = NULL;
	width = 0;
	height = 0;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE_DATA;
}

GuiImage::GuiImage(GuiImageData * img)
{
	image = NULL;
	width = 0;
	height = 0;
	if(img)
	{
		image = img->getImage();
		width = img->getWidth();
		height = img->getHeight();
	}
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE_DATA;
}

GuiImage::GuiImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE_TEXTURE;
}

GuiImage::GuiImage(int w, int h, GXColor c)
{
	image = (u8 *)memalign (32, w * h << 2);
	width = w;
	height = h;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE_COLOR;

	if(!image)
		return;

	int x, y;

	for(y=0; y < h; ++y)
	{
		for(x=0; x < w; ++x)
		{
			this->setPixel(x, y, c);
		}
	}
	int len = w * h << 2;
	if(len%32) len += (32-len%32);
	DCFlushRange(image, len);
}

/**
 * Destructor for the GuiImage class.
 */
GuiImage::~GuiImage()
{
	if(imgType == IMAGE_COLOR && image)
		free(image);
}

u8 * GuiImage::getImage()
{
	return image;
}

void GuiImage::setImage(GuiImageData * img)
{
	image = NULL;
	width = 0;
	height = 0;
	if(img)
	{
		image = img->getImage();
		width = img->getWidth();
		height = img->getHeight();
	}
	imgType = IMAGE_DATA;
}

void GuiImage::setImage(u8 * img, int w, int h)
{
	image = img;
	width = w;
	height = h;
	imgType = IMAGE_TEXTURE;
}

void GuiImage::setAngle(float a)
{
	imageangle = a;
}

void GuiImage::setTile(int t)
{
	tile = t;
}

GXColor GuiImage::getPixel(int x, int y)
{
	if(!image || this->getWidth() <= 0 || x < 0 || y < 0)
		return (GXColor){0, 0, 0, 0};

	u32 offset = (((y >> 2)<<4)*this->getWidth()) + ((x >> 2)<<6) + (((y%4 << 2) + x%4 ) << 1);
	GXColor color;
	color.a = *(image+offset);
	color.r = *(image+offset+1);
	color.g = *(image+offset+32);
	color.b = *(image+offset+33);
	return color;
}

void GuiImage::setPixel(int x, int y, GXColor color)
{
	if(!image || this->getWidth() <= 0 || x < 0 || y < 0)
		return;

	u32 offset = (((y >> 2)<<4)*this->getWidth()) + ((x >> 2)<<6) + (((y%4 << 2) + x%4 ) << 1);
	*(image+offset) = color.a;
	*(image+offset+1) = color.r;
	*(image+offset+32) = color.g;
	*(image+offset+33) = color.b;
}

void GuiImage::setStripe(int s)
{
	stripe = s;
}

void GuiImage::colorStripe(int shift)
{
	GXColor color;
	int x, y=0;
	int alt = 0;
	
	int thisHeight =  this->getHeight();
	int thisWidth =  this->getWidth();

	for(; y < thisHeight; ++y)
	{
		if(y % 3 == 0)
			alt ^= 1;

		if(alt)
		{
			for(x=0; x < thisWidth; ++x)
			{
				color = getPixel(x, y);

				if(color.r < 255-shift)
					color.r += shift;
				else
					color.r = 255;
				if(color.g < 255-shift)
					color.g += shift;
				else
					color.g = 255;
				if(color.b < 255-shift)
					color.b += shift;
				else
					color.b = 255;

				color.a = 255;
				setPixel(x, y, color);
			}
		}
		else
		{
			for(x=0; x < thisWidth; ++x)
			{
				color = getPixel(x, y);

				if(color.r > shift)
					color.r -= shift;
				else
					color.r = 0;
				if(color.g > shift)
					color.g -= shift;
				else
					color.g = 0;
				if(color.b > shift)
					color.b -= shift;
				else
					color.b = 0;

				color.a = 255;
				setPixel(x, y, color);
			}
		}
	}
}

/**
 * Draw the button on screen
 */
void GuiImage::draw()
{
	if(!image || !this->isVisible() || tile == 0)
		return;

	float currScaleX = this->getScaleX();
	float currScaleY = this->getScaleY();
	int currLeft = this->getLeft();
	int thisTop = this->getTop();

	if(tile > 0)
	{
		int alpha = this->getAlpha();
		for(int i=0; i<tile; ++i)
		{
			Menu_DrawImg(currLeft+width*i, thisTop, width, height, image, imageangle, currScaleX, currScaleY, alpha);
		}
	}
	else
	{
		Menu_DrawImg(currLeft, thisTop, width, height, image, imageangle, currScaleX, currScaleY, this->getAlpha());
	}

	if(stripe > 0)
	{
		int thisHeight = this->getHeight();
		int thisWidth = this->getWidth();
		for(int y=0; y < thisHeight; y+=6)
			Menu_DrawRectangle(currLeft,thisTop+y,thisWidth,3,(GXColor){0, 0, 0, (u8)stripe},1);
	}
	this->updateEffects();
}
