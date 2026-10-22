/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerview.cpp
 *
 * See controllerview.h.
 ***************************************************************************/

#include "controllerview.h"
#include <cmath>
#include <algorithm>

static PixelColor withAlpha(PixelColor c, uint8_t a)
{
	c.a = a;
	return c;
}

static PixelColor mixToWhite(PixelColor c, float t)
{
	c.r = (uint8_t)(c.r + (255 - c.r) * t);
	c.g = (uint8_t)(c.g + (255 - c.g) * t);
	c.b = (uint8_t)(c.b + (255 - c.b) * t);
	return c;
}

void BuildControllerBody(GuiMesh& mesh, const ControllerLayout& layout, float scale)
{
	const GuiTheme& theme = GuiTheme::current();
	const float outline = std::max(1.5f, 2.0f * scale);
	const GuiFill outlineFill = GuiFill::solid(theme.outlineStrong);
	const GuiFill bodyFill = GuiFill::linear(GuiMixColor(theme.surface, theme.surfaceRaised, 0.3f), GuiMixColor(theme.background, theme.surface, 0.5f), 90);

	std::vector<std::vector<GuiPoint> > pieces;
	for(size_t i = 0; i < layout.body.size(); i++)
	{
		const BodyPiece& b = layout.body[i];
		GuiRect r = { b.x * scale, b.y * scale, b.w * scale, b.h * scale };
		pieces.push_back(GuiRoundRectPolygon(r, b.radius * scale, b.angle));
	}
	// outlines that are not rounded rectangles (concave is fine)
	for(size_t i = 0; i < layout.bodyPaths.size(); i++)
	{
		std::vector<std::vector<GuiPoint> > contours = GuiFlattenPath(layout.bodyPaths[i], scale, mesh.getTolerance() * 0.5f);
		for(size_t k = 0; k < contours.size(); k++)
			pieces.push_back(contours[k]);
	}

	// outlines first, fills after: where pieces overlap, the fill of one covers the outline of the other
	for(size_t i = 0; i < pieces.size(); i++)
	{
		mesh.addPolygon(pieces[i], outlineFill);
		mesh.strokePolygon(pieces[i], outline, STROKE::OUTSIDE, outlineFill);
	}
	for(size_t i = 0; i < pieces.size(); i++)
		mesh.addPolygon(pieces[i], bodyFill);

	// controls that never change appearance
	for(size_t i = 0; i < layout.spots.size(); i++)
		if(layout.spots[i].id == 0)
			BuildControllerSpot(mesh, layout.spots[i], CTL_STATE::NORMAL, scale);
}

// the polygons of a spot's path, in drawing units
static std::vector<std::vector<GuiPoint> > spotContours(const GuiMesh& mesh, const ControlSpot& spot, float scale)
{
	return GuiFlattenPath(spot.path, scale, mesh.getTolerance() * 0.5f);
}

void BuildControllerSpot(GuiMesh& mesh, const ControlSpot& spot, CTL_STATE state, float scale)
{
	const GuiTheme& theme = GuiTheme::current();
	const bool lit = state == CTL_STATE::LIT || state == CTL_STATE::LIT_HIGHLIGHT;
	const bool hl = state == CTL_STATE::HIGHLIGHT || state == CTL_STATE::LIT_HIGHLIGHT;

	// a ghost control is part of a shape drawn elsewhere, so it shows only when it has something to say
	if((spot.flags & CTL_GHOST) && !lit && !hl)
		return;

	const float stroke = std::max(1.2f, 1.5f * scale);
	GuiRect r = { spot.x * scale, spot.y * scale, spot.w * scale, spot.h * scale };

	PixelColor fill = spot.shape == CTL_SHAPE::STICK ? theme.background : theme.surface;
	PixelColor edge = theme.outlineStrong;
	if(lit)
	{
		fill = spot.shape == CTL_SHAPE::STICK ? theme.background : theme.good;
		edge = mixToWhite(theme.good, 0.35f);
	}
	if(hl)
	{
		edge = theme.mark;
		if(!lit)
			fill = spot.shape == CTL_SHAPE::STICK ? fill : mixToWhite(theme.mark, 0.78f);
	}

	const float edgeWidth = hl ? stroke * 1.8f : stroke;
	const bool outline = !spot.path.empty() && (spot.shape == CTL_SHAPE::PATH || spot.shape == CTL_SHAPE::STICK);
	std::vector<std::vector<GuiPoint> > contours;
	if(outline)
		contours = spotContours(mesh, spot, scale);

	if(hl)
	{
		if(outline)
		{
			// a glow that follows the outline: three soft bands growing outward
			const float widths[3] = { 6.0f, 4.0f, 2.0f };
			const uint8_t alphas[3] = { 26, 44, 70 };
			for(size_t k = 0; k < contours.size(); k++)
				for(int g = 0; g < 3; g++)
					mesh.strokePolygon(contours[k], widths[g] * std::max(scale, 0.8f), STROKE::OUTSIDE, GuiFill::solid(withAlpha(theme.mark, alphas[g])));
		}
		else
		{
			const float blur = 8.0f * std::max(scale, 0.8f);
			mesh.addShadow(r, spot.shape == CTL_SHAPE::ROUNDRECT ? GuiCorners(spot.radius * scale) : GuiCorners(10000.0f), blur, 1.0f, 0.0f, 0.0f, withAlpha(theme.mark, 110));
		}
	}

	if(outline)
	{
		for(size_t k = 0; k < contours.size(); k++)
		{
			mesh.addPolygon(contours[k], GuiFill::solid(fill));
			mesh.strokePolygon(contours[k], edgeWidth, STROKE::INSIDE, GuiFill::solid(edge));
		}
		return;
	}

	if(spot.shape == CTL_SHAPE::ROUNDRECT)
	{
		mesh.addRoundRect(r, GuiCorners(spot.radius * scale), GuiFill::solid(fill));
		mesh.strokeRoundRect(r, GuiCorners(spot.radius * scale), edgeWidth, STROKE::INSIDE, GuiFill::solid(edge));
	}
	else
	{
		mesh.addEllipse(r, GuiFill::solid(fill));
		mesh.strokeEllipse(r, edgeWidth, STROKE::INSIDE, GuiFill::solid(edge));
		if(spot.shape == CTL_SHAPE::STICK && spot.radius <= 0.0f)
		{
			// a second ring inside, as on the real stick's gate
			const float inset = r.w * 0.11f;
			GuiRect inner = { r.x + inset, r.y + inset, r.w - 2.0f * inset, r.h - 2.0f * inset };
			mesh.strokeEllipse(inner, std::max(1.0f, stroke * 0.7f), STROKE::INSIDE, GuiFill::solid(withAlpha(theme.outlineStrong, 150)));
		}
	}
}

void BuildControllerStickCap(GuiMesh& mesh, const ControlSpot& spot, float x, float y, bool lit, float scale)
{
	const GuiTheme& theme = GuiTheme::current();
	const float d = spot.radius > 0.0f ? spot.radius * scale : spot.w * scale * 0.56f;
	const float travel = (spot.w * scale - d) * 0.5f * 0.85f;
	// keep the cap inside the ring when the stick is pushed diagonally
	float dx = x, dy = y;
	const float len = sqrtf(dx * dx + dy * dy);
	if(len > 1.0f)
	{
		dx /= len;
		dy /= len;
	}
	const float cx = (spot.x + spot.w * 0.5f) * scale + dx * travel;
	const float cy = (spot.y + spot.h * 0.5f) * scale + dy * travel;
	GuiRect r = { cx - d * 0.5f, cy - d * 0.5f, d, d };

	const PixelColor fill = lit ? theme.good : theme.outline;
	const PixelColor edge = lit ? mixToWhite(theme.good, 0.35f) : theme.outlineStrong;
	mesh.addEllipse(r, GuiFill::linear(mixToWhite(fill, 0.12f), fill, 90));
	mesh.strokeEllipse(r, std::max(1.2f, 1.5f * scale), STROKE::INSIDE, GuiFill::solid(edge));
}

void BuildControllerOutline(GuiMesh& mesh, const ControllerLayout& layout, float scale, float width, PixelColor color)
{
	const GuiFill fill = GuiFill::solid(color);
	const float tol = mesh.getTolerance() * 0.5f;
	auto strokeAll = [&](const GuiPath& path) {
		std::vector<std::vector<GuiPoint> > contours = GuiFlattenPath(path, scale, tol);
		for(size_t k = 0; k < contours.size(); k++)
			mesh.strokePolygon(contours[k], width, STROKE::CENTRE, fill);
	};

	for(size_t i = 0; i < layout.body.size(); i++)
	{
		const BodyPiece& b = layout.body[i];
		GuiRect r = { b.x * scale, b.y * scale, b.w * scale, b.h * scale };
		mesh.strokePolygon(GuiRoundRectPolygon(r, b.radius * scale, b.angle), width, STROKE::CENTRE, fill);
	}
	for(size_t i = 0; i < layout.bodyPaths.size(); i++)
		strokeAll(layout.bodyPaths[i]);

	for(size_t i = 0; i < layout.spots.size(); i++)
	{
		const ControlSpot& s = layout.spots[i];
		if(s.flags & CTL_GHOST)
			continue;
		GuiRect r = { s.x * scale, s.y * scale, s.w * scale, s.h * scale };
		if(!s.path.empty() && (s.shape == CTL_SHAPE::PATH || s.shape == CTL_SHAPE::STICK))
			strokeAll(s.path);
		else if(s.shape == CTL_SHAPE::ROUNDRECT)
			mesh.strokeRoundRect(r, GuiCorners(s.radius * scale), width, STROKE::CENTRE, fill);
		else
			mesh.strokeEllipse(r, width, STROKE::CENTRE, fill);
		if(s.shape == CTL_SHAPE::STICK)
		{
			const float d = (s.radius > 0.0f ? s.radius : s.w * 0.56f) * scale;
			GuiRect cap = { r.x + (r.w - d) * 0.5f, r.y + (r.h - d) * 0.5f, d, d };
			mesh.strokeEllipse(cap, width, STROKE::CENTRE, fill);
		}
	}
}

// ---- the element

ControllerView::ControllerView(const ControllerLayout& l, float s) :
	layout(&l), scale(s), leader(false), leaderX(0), leaderY(0), meshFeather(-1.0f), dirty(true), originX(0), originY(0), alpha(255)
{
	rebuild();
}

ControllerView::~ControllerView()
{
	freeAllBaked();
	clearLabels();
}

void ControllerView::freeBaked(Baked& b)
{
	if(b.handle)
	{
		VideoDriver * video = platform ? platform->getVideo() : nullptr;
		if(video && video->getImageRenderer())
			video->getImageRenderer()->destroyMesh(b.handle);
	}
	b = Baked();
}

void ControllerView::freeAllBaked()
{
	freeBaked(bodyBaked);
	for(size_t i = 0; i < spots.size(); i++)
		for(int k = 0; k < 4; k++)
			freeBaked(spots[i].baked[k]);
}

void ControllerView::clearLabels()
{
	for(size_t i = 0; i < labels.size(); i++)
		delete labels[i];
	labels.clear();
	labelLit.clear();
}

void ControllerView::rebuild()
{
	// (a hair is taken off first: 400 * 0.6f is 240.000004 in floats and must not become 241)
	width = (int)ceilf(layout->width * scale - 0.01f);
	height = (int)ceilf(layout->height * scale - 0.01f);

	freeAllBaked();
	spots.assign(layout->spots.size(), SpotCache());
	for(size_t i = 0; i < spots.size(); i++)
		for(int k = 0; k < 4; k++)
			spots[i].built[k] = false;
	stickX.assign(layout->spots.size(), 0.0f);
	stickY.assign(layout->spots.size(), 0.0f);

	// labels are text children centered on their control
	clearLabels();
	const GuiTheme& theme = GuiTheme::current();
	for(size_t i = 0; i < layout->spots.size(); i++)
	{
		const ControlSpot& s = layout->spots[i];
		GuiText * t = nullptr;
		if(s.label && (!labelFilter || labelFilter(s.id)))
		{
			t = new GuiText(s.label, (int)(15.0f * std::max(scale, 0.7f)), theme.text);
			t->setParent(this);
			t->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
			const float cx = (s.x + s.w * 0.5f) * scale - width * 0.5f;
			const float cy = (s.y + s.h * 0.5f) * scale - height * 0.5f;
			t->setPosition((int)floorf(cx + 0.5f), (int)floorf(cy + 0.5f));
		}
		labels.push_back(t);
		labelLit.push_back(false);
	}
	dirty = true;
}

void ControllerView::setLayout(const ControllerLayout& l)
{
	layout = &l;
	highlighted.clear();
	rebuild();
}

void ControllerView::setScale(float s)
{
	scale = s;
	rebuild();
}

void ControllerView::setHighlight(int id)
{
	highlighted.clear();
	if(id != 0)
		highlighted.push_back(id);
}

void ControllerView::setHighlight(const std::vector<int>& ids)
{
	highlighted = ids;
}

void ControllerView::setLeader(bool on, float fromX, float fromY)
{
	leader = on;
	leaderX = fromX;
	leaderY = fromY;
}

void ControllerView::setStick(int id, float x, float y)
{
	for(size_t i = 0; i < layout->spots.size(); i++)
	{
		if(layout->spots[i].id == id && layout->spots[i].shape == CTL_SHAPE::STICK)
		{
			stickX[i] = x;
			stickY[i] = y;
		}
	}
}

bool ControllerView::getSpotCentre(int id, float& x, float& y) const
{
	const ControlSpot * s = layout->find(id);
	if(!s)
		return false;
	x = originX + (s->x + s->w * 0.5f) * scale;
	y = originY + (s->y + s->h * 0.5f) * scale;
	return true;
}

void ControllerView::flushScratch()
{
	if(!scratch.empty())
	{
		platform->getVideo()->getImageRenderer()->drawMesh(scratch.data(), scratch.size());
		scratch.clear();
	}
}

void ControllerView::drawStatic(const GuiMesh& m, Baked& baked)
{
	const std::vector<MeshVertex>& src = m.vertices();
	if(src.empty())
		return;

	ImageRenderer * images = platform->getVideo()->getImageRenderer();
	baked.uses++;
	if(!baked.handle && !baked.noHandle && baked.uses >= 2)
	{
		baked.handle = images->createMesh(src.data(), src.size());
		if(!baked.handle)
			baked.noHandle = true;
	}

	// The backend's copy has its colors baked in, so only a fully opaque draw can use it; a fading
	// view goes through emit(), which rewrites the alpha
	if(baked.handle && alpha == 255)
	{
		flushScratch();	// what was collected so far is underneath this
		images->drawMeshInstance(baked.handle, originX, originY, 1.0f, 1.0f);
		return;
	}
	emit(m);
}

void ControllerView::emit(const GuiMesh& m)
{
	const std::vector<MeshVertex>& src = m.vertices();
	for(size_t i = 0; i < src.size(); i++)
	{
		MeshVertex v = src[i];
		v.x += originX;
		v.y += originY;
		if(alpha < 255)
			v.c.a = (uint8_t)((v.c.a * alpha + 127) / 255);
		scratch.push_back(v);
	}
}

void ControllerView::draw()
{
	if(!this->isVisible())
		return;

	alpha = this->getAlpha();
	if(alpha > 0)
	{
		VideoDriver * video = platform->getVideo();
		const float uiScale = video->getUIScale();
		const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
		if(dirty || feather != meshFeather)
		{
			freeAllBaked();
			body = GuiMesh(feather);
			BuildControllerBody(body, *layout, scale);
			for(size_t i = 0; i < spots.size(); i++)
				for(int k = 0; k < 4; k++)
				{
					spots[i].mesh[k] = GuiMesh(feather);
					spots[i].built[k] = false;
				}
			meshFeather = feather;
			dirty = false;
		}

		originX = (float)this->getLeft();
		originY = (float)this->getTop();
		scratch.clear();
		drawStatic(body, bodyBaked);

		// resting controls first, highlighted ones on top so their glow is not painted over
		for(int pass = 0; pass < 2; pass++)
		{
			for(size_t i = 0; i < layout->spots.size(); i++)
			{
				const ControlSpot& s = layout->spots[i];
				if(s.id == 0)
					continue;
				const bool hl = std::find(highlighted.begin(), highlighted.end(), s.id) != highlighted.end();
				if((pass == 1) != hl)
					continue;
				const bool isLit = lit && lit(s.id);
				const int k = (isLit ? 1 : 0) + (hl ? 2 : 0);
				if(!spots[i].built[k])
				{
					BuildControllerSpot(spots[i].mesh[k], s, (CTL_STATE)k, scale);
					spots[i].built[k] = true;
				}
				drawStatic(spots[i].mesh[k], spots[i].baked[k]);

				if(labels[i] && labelLit[i] != isLit)
				{
					labels[i]->setColor(isLit ? GuiTheme::current().textOnAccent : GuiTheme::current().text);
					labelLit[i] = isLit;
				}
			}
		}

		// stick caps move, so they are built each frame
		for(size_t i = 0; i < layout->spots.size(); i++)
		{
			const ControlSpot& s = layout->spots[i];
			if(s.shape != CTL_SHAPE::STICK)
				continue;
			GuiMesh cap(feather);
			BuildControllerStickCap(cap, s, stickX[i], stickY[i], lit && lit(s.id), scale);
			emit(cap);
		}

		// a line from a list row to the highlighted control
		if(leader && !highlighted.empty())
		{
			float tx, ty;
			if(getSpotCentre(highlighted[0], tx, ty))
			{
				const GuiTheme& theme = GuiTheme::current();
				GuiMesh line(feather);
				line.addLine({ leaderX, leaderY }, { tx, ty }, 2.0f, GuiFill::solid(withAlpha(theme.mark, 220)));
				GuiRect dot = { tx - 4.0f, ty - 4.0f, 8.0f, 8.0f };
				line.addEllipse(dot, GuiFill::solid(theme.mark));
				// the mesh is already in canvas coordinates: draw it as it is
				const std::vector<MeshVertex>& src = line.vertices();
				for(size_t i = 0; i < src.size(); i++)
				{
					MeshVertex v = src[i];
					if(alpha < 255)
						v.c.a = (uint8_t)((v.c.a * alpha + 127) / 255);
					scratch.push_back(v);
				}
			}
		}

		flushScratch();

		for(size_t i = 0; i < labels.size(); i++)
			if(labels[i])
				labels[i]->draw();
	}

	this->updateEffects();
}
