/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiShape.cpp
 ***************************************************************************/

#include "Gui.h"

GuiShape::GuiShape(int w, int h) :
	kind(SHAPE::RECTANGLE), corners(0.0f), transition(0.12f),
	blend(1.0f), started(false), dirty(true), meshWidth(-1), meshHeight(-1), meshFeather(-1.0f)
{
	width = w;
	height = h;
}

void GuiShape::setShape(SHAPE s)
{
	kind = s;
	dirty = true;
}

void GuiShape::setCorners(const GuiCorners& c)
{
	corners = c;
	dirty = true;
}

void GuiShape::setCapsule()
{
	setCorners(GuiCorners(100000.0f)); // clamped to half the shorter side
}

void GuiShape::setStyle(const GuiShapeStyle& s)
{
	style = s;
	started = false; // draw the new look at once, without fading from the old one
}

void GuiShape::setPaint(const GuiPaint& p)
{
	setStyle(GuiShapeStyle(p));
}

void GuiShape::setTransition(float seconds)
{
	transition = seconds < 0.0f ? 0.0f : seconds;
}

static SHAPE_STATE shapeStateFor(STATE s)
{
	switch(s)
	{
		case STATE::SELECTED:
		case STATE::HELD:     return SHAPE_STATE::FOCUSED;
		case STATE::CLICKED:  return SHAPE_STATE::PRESSED;
		case STATE::DISABLED: return SHAPE_STATE::DISABLED;
		default:              return SHAPE_STATE::NORMAL;
	}
}

void GuiShape::draw()
{
	if(!this->isVisible())
		return;

	const int a = this->getAlpha();
	if(a > 0 && width > 0 && height > 0)
	{
		VideoDriver * video = platform->getVideo();

		// choose the paint for the current state, blending toward it when it changes
		const GuiPaint& want = style.forState(shapeStateFor(state));
		if(!started)
		{
			from = to = shown = want;
			blend = 1.0f;
			started = true;
			dirty = true;
		}
		else if(want != to)
		{
			from = shown;
			to = want;
			blend = transition > 0.0f ? 0.0f : 1.0f;
			if(blend >= 1.0f)
				shown = to;
		}
		if(blend < 1.0f)
		{
			blend += video->getDeltaTime() / transition;
			if(blend >= 1.0f)
			{
				blend = 1.0f;
				shown = to;
			}
			else
			{
				const float eased = blend * blend * (3.0f - 2.0f * blend);
				shown = GuiPaint::mix(from, to, eased);
			}
			dirty = true;
		}

		// rebuild the geometry when anything it depends on changed
		const float scale = video->getUIScale();
		const float feather = scale > 0.0f ? 1.0f / scale : 1.0f;
		if(dirty || meshWidth != width || meshHeight != height || meshFeather != feather)
		{
			mesh = GuiMesh(feather);
			GuiRect r = { 0.0f, 0.0f, (float)width, (float)height };
			GuiAddPaintedShape(mesh, kind, r, corners, shown);
			meshWidth = width;
			meshHeight = height;
			meshFeather = feather;
			dirty = false;
		}

		// place it like an image: scaled about its own center, at the element's position
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
