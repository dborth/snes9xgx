/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiArrow.cpp
 ***************************************************************************/

#include "Gui.h"

GuiArrow::GuiArrow(ARROW dir, int w, int h, PixelColor rest, PixelColor lit) :
	direction(dir), restColor(rest), litColor(lit), meshColor(rest),
	meshWidth(-1), meshHeight(-1), meshFeather(-1.0f)
{
	width = w;
	height = h;
}

void GuiArrow::draw()
{
	if(!this->isVisible())
		return;

	const int a = this->getAlpha();
	if(a > 0 && width > 0 && height > 0)
	{
		VideoDriver * video = platform->getVideo();
		const bool isLit = state == STATE::SELECTED || state == STATE::HELD || state == STATE::CLICKED;
		const PixelColor want = isLit ? litColor : restColor;

		const float scale = video->getUIScale();
		const float feather = scale > 0.0f ? 1.0f / scale : 1.0f;
		if(meshWidth != width || meshHeight != height || meshFeather != feather ||
		   meshColor.r != want.r || meshColor.g != want.g || meshColor.b != want.b || meshColor.a != want.a)
		{
			const float w = (float)width, h = (float)height;
			std::vector<GuiPoint> pts;
			switch(direction)
			{
				case ARROW::UP:    pts = { { w * 0.5f, 0.0f }, { w, h }, { 0.0f, h } }; break;
				case ARROW::DOWN:  pts = { { 0.0f, 0.0f }, { w, 0.0f }, { w * 0.5f, h } }; break;
				case ARROW::LEFT:  pts = { { 0.0f, h * 0.5f }, { w, 0.0f }, { w, h } }; break;
				case ARROW::RIGHT: pts = { { 0.0f, 0.0f }, { w, h * 0.5f }, { 0.0f, h } }; break;
			}
			mesh = GuiMesh(feather);
			mesh.addPolygon(pts, GuiFill::solid(want));
			meshWidth = width;
			meshHeight = height;
			meshFeather = feather;
			meshColor = want;
		}

		const float sx = this->getScaleX();
		const float sy = this->getScaleY();
		const float cx = this->getLeft() + width * 0.5f;
		const float cy = this->getTop() + height * 0.5f;
		const float hw = width * 0.5f;
		const float hh = height * 0.5f;
		const std::vector<MeshVertex>& src = mesh.vertices();

		scratch.resize(src.size());
		for(size_t i = 0; i < src.size(); i++)
		{
			scratch[i].x = cx + (src[i].x - hw) * sx;
			scratch[i].y = cy + (src[i].y - hh) * sy;
			scratch[i].c = src[i].c;
			if(a < 255)
				scratch[i].c.a = (uint8_t)((src[i].c.a * a + 127) / 255);
		}
		if(!scratch.empty())
			platform->getVideo()->getImageRenderer()->drawMesh(scratch.data(), scratch.size());
	}

	this->updateEffects();
}
