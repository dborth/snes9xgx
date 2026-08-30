/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImage.cpp
 ***************************************************************************/

#include "Gui.h"
#include <malloc.h>

GuiImage::GuiImage()
{
	texture = nullptr;
	ownsTexture = false;
	width = 0;
	height = 0;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE::TEXTURE;
}

GuiImage::GuiImage(GuiImageData * img)
{
	texture = nullptr;
	width = 0;
	height = 0;

	if(img)
	{
		texture = img->getTexture();
		width = img->getWidth();
		height = img->getHeight();
	}

	ownsTexture = false;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE::TEXTURE;
}

GuiImage::GuiImage(uint8_t * tex, int w, int h)
{
	texture = tex;
	ownsTexture = false;
	width = w;
	height = h;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE::TEXTURE;
}

GuiImage::GuiImage(int w, int h, PixelColor c)
{
	texture = nullptr;
	ownsTexture = true;
	width = w;
	height = h;
	imageangle = 0;
	tile = -1;
	stripe = 0;
	imgType = IMAGE::COLOR;
	baseColor = c;
}

GuiImage::~GuiImage()
{
	if(ownsTexture && texture)
	{
		destroyTexture(texture);
		texture = nullptr;
	}
}

void GuiImage::setImage(GuiImageData * img)
{
	if(ownsTexture && texture)
	{
		destroyTexture(texture);
		texture = nullptr;
	}

	texture = nullptr;
	ownsTexture = false;
	width = 0;
	height = 0;
	if(img)
	{
		texture = img->getTexture();
		width = img->getWidth();
		height = img->getHeight();
	}
	imgType = IMAGE::TEXTURE;
}

void GuiImage::setImage(uint8_t * img, int w, int h)
{
	if(ownsTexture && texture)
	{
		destroyTexture(texture);
		texture = nullptr;
	}

	if(img) {
		texture = createTexture(w, h);
		loadTextureData(texture, img, w, h);
		ownsTexture = true;
		width = w;
		height = h;
	}
	else {
		texture = nullptr;
		ownsTexture = false;
		width = 0;
		height = 0;
	}

	imgType = IMAGE::TEXTURE;
}

void GuiImage::setTexture(uint8_t * tex, int w, int h)
{
	if(ownsTexture && texture)
	{
		destroyTexture(texture);
		texture = nullptr;
	}

	texture = tex;
	ownsTexture = false;
	width = w;
	height = h;
	imgType = IMAGE::TEXTURE;
}

void GuiImage::setAngle(float a)
{
	imageangle = a;
}

void GuiImage::setTile(int t)
{
	tile = t;
}

void GuiImage::setStripe(int s)
{
	stripe = s;
}

void GuiImage::draw()
{
	if(!this->isVisible() || tile == 0)
		return;

	float currScaleX = this->getScaleX();
	float currScaleY = this->getScaleY();
	int currLeft = this->getLeft();
	int thisTop = this->getTop();
	int alpha = this->getAlpha();

	if(imgType == IMAGE::COLOR)
	{
		PixelColor c = baseColor;
		c.a = alpha;
		Menu_DrawRectangle(currLeft, thisTop, width, height, c);
	}
	else if(texture)
	{
		if(tile > 0)
		{
			for(int i=0; i<tile; ++i)
				Menu_DrawImg(texture, currLeft+width*i, thisTop, width, height, imageangle, currScaleX, currScaleY, alpha);
		}
		else
		{
			Menu_DrawImg(texture, currLeft, thisTop, width, height, imageangle, currScaleX, currScaleY, alpha);
		}
	}

	if(stripe > 0)
	{
		int thisHeight = this->getHeight();
		int thisWidth = this->getWidth();
		for(int y=0; y < thisHeight; y+=6)
			Menu_DrawRectangle(currLeft, thisTop+y, thisWidth, 3, (PixelColor){0, 0, 0, (uint8_t)stripe});
	}

	this->updateEffects();
}
