/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiShape.cpp
 ***************************************************************************/

#include "Gui.h"

// A change of state blends through this many looks. Each is a mesh the cache can share between
// every shape making the same change, so only the first one to fade pays to build them.
static const int kBlendSteps = 4;

GuiShape::GuiShape(int w, int h) :
	kind(SHAPE::RECTANGLE), corners(0.0f), transition(0.12f),
	blend(1.0f), blendStep(kBlendSteps), started(false), dirty(true), meshWidth(-1), meshHeight(-1), meshFeather(-1.0f),
	mesh(nullptr)
{
	width = w;
	height = h;
}

GuiShape::~GuiShape()
{
	GuiMeshCache::release(mesh);
	mesh = nullptr;
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

void GuiShape::setArrow(SHAPE dir, PixelColor rest, PixelColor lit)
{
	setShape(dir);
	setStyle(GuiShapeStyle(GuiPaint().color(rest)).onFocus(GuiPaint().color(lit)));
	setTransition(0.0f);
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
			blendStep = kBlendSteps;
			started = true;
			dirty = true;
		}
		else if(want != to)
		{
			from = shown;
			to = want;
			blend = transition > 0.0f ? 0.0f : 1.0f;
			blendStep = 0;
			if(blend >= 1.0f)
			{
				shown = to;
				blendStep = kBlendSteps;
				dirty = true;
			}
		}
		if(blend < 1.0f)
		{
			blend += video->getDeltaTime() / transition;
			if(blend >= 1.0f)
			{
				blend = 1.0f;
				blendStep = kBlendSteps;
				shown = to;
				dirty = true;
			}
			else
			{
				// Look only changes a few times per blend, so the in-between meshes can be shared
				const float eased = blend * blend * (3.0f - 2.0f * blend);
				const int step = (int)(eased * (float)kBlendSteps + 0.5f);
				if(step != blendStep)
				{
					blendStep = step;
					shown = GuiPaint::mix(from, to, (float)step / (float)kBlendSteps);
					dirty = true;
				}
			}
		}

		// look the geometry up again when anything it depends on changed
		const float scale = video->getUIScale();
		const float feather = scale > 0.0f ? 1.0f / scale : 1.0f;
		if(dirty || !mesh || meshWidth != width || meshHeight != height || meshFeather != feather)
		{
			GuiMeshKey key;
			key.kind = kind;
			key.width = width;
			key.height = height;
			key.corners = corners;
			key.paint = shown;
			key.feather = feather;
			key.tolerance = 0.3f * feather; // 0.3 of a physical pixel
			GuiSharedMesh * next = GuiMeshCache::acquire(key); // before releasing, in case it is the same mesh
			GuiMeshCache::release(mesh);
			mesh = next;
			meshWidth = width;
			meshHeight = height;
			meshFeather = feather;
			dirty = false;
		}

		// place it like an image: scaled about its own center, at the element's position
		const float sx = this->getScaleX();
		const float sy = this->getScaleY();
		const float hw = width * 0.5f;
		const float hh = height * 0.5f;
		GuiMeshCache::draw(mesh, this->getLeft() + hw - hw * sx, this->getTop() + hh - hh * sy, sx, sy, (uint8_t)a, scratch);
	}

	this->updateEffects();
}
