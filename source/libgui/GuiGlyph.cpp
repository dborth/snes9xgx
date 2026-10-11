/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGlyph.cpp
 ***************************************************************************/

#include "Gui.h"

GuiGlyph::GuiGlyph(GLYPH k, const char * letter) : kind(k), text(nullptr), meshFeather(-1.0f)
{
	float w, h;
	GuiGlyph::size(kind, w, h);
	width = (int)w;
	height = (int)h;

	const bool takesLetter = kind == GLYPH::BUTTON || kind == GLYPH::BUMPER || kind == GLYPH::STICK;
	if(takesLetter && letter && *letter)
	{
		const int px = (int)strlen(letter) > 1 ? 11 : 14;
		text = new GuiText(letter, px, GuiTheme::current().background);
		text->setParent(this);
		text->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	}
}

GuiGlyph::~GuiGlyph()
{
	delete text;
}

void GuiGlyph::draw()
{
	if(!this->isVisible())
		return;

	const int a = this->getAlpha();
	if(a > 0)
	{
		VideoDriver * video = platform->getVideo();
		const float uiScale = video->getUIScale();
		const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
		if(feather != meshFeather)
		{
			mesh = GuiMesh(feather);
			GuiGlyph::build(mesh, kind, (float)width, (float)height);
			meshFeather = feather;
		}

		const float ox = (float)this->getLeft(), oy = (float)this->getTop();
		const std::vector<MeshVertex>& src = mesh.vertices();
		scratch.resize(src.size());
		for(size_t i = 0; i < src.size(); i++)
		{
			scratch[i] = src[i];
			scratch[i].x += ox;
			scratch[i].y += oy;
			if(a < 255)
				scratch[i].c.a = (uint8_t)((src[i].c.a * a + 127) / 255);
		}
		if(!scratch.empty())
			video->getImageRenderer()->drawMesh(scratch.data(), scratch.size());

		if(text)
			text->draw();
	}

	this->updateEffects();
}

void GuiGlyph::size(GLYPH kind, float& w, float& h)
{
	switch(kind)
	{
		case GLYPH::BUMPER: w = 30.0f; h = 20.0f; break;
		default: w = 22.0f; h = 22.0f; break;
	}
}

void GuiGlyph::build(GuiMesh& mesh, GLYPH kind, float w, float h)
{
	const GuiTheme& theme = GuiTheme::current();
	const GuiFill light = GuiFill::solid(theme.text);
	const GuiFill dark = GuiFill::solid(theme.background);
	const GuiRect box = { 0.0f, 0.0f, w, h };
	const float cx = w * 0.5f, cy = h * 0.5f, u = std::min(w, h) / 22.0f; // u = size relative to a 22-pixel glyph

	switch(kind)
	{
		case GLYPH::BUMPER:
			mesh.addRoundRect(box, GuiCorners(6.0f * u), light);
			return;
		case GLYPH::CAPTURE:
			mesh.addRoundRect(box, GuiCorners(5.0f * u), light);
			mesh.strokeEllipse(GuiRect{ cx - 6.0f * u, cy - 6.0f * u, 12.0f * u, 12.0f * u }, 1.6f * u, STROKE::INSIDE, dark);
			return;
		default:
			break;
	}

	mesh.addEllipse(box, light);
	switch(kind)
	{
		case GLYPH::UP:
		case GLYPH::DOWN:
		case GLYPH::LEFT:
		case GLYPH::RIGHT:
		{
			// a triangle pointing up, turned to face the right way
			const float a = kind == GLYPH::UP ? 0.0f : kind == GLYPH::RIGHT ? 90.0f : kind == GLYPH::DOWN ? 180.0f : 270.0f;
			const float rad = a * 3.14159265f / 180.0f, c = cosf(rad), s = sinf(rad);
			const float pts[3][2] = { { 0.0f, -5.5f * u }, { -5.0f * u, 4.0f * u }, { 5.0f * u, 4.0f * u } };
			std::vector<GuiPoint> tri;
			for(int i = 0; i < 3; i++)
				tri.push_back(GuiPoint{ cx + pts[i][0] * c - pts[i][1] * s, cy + pts[i][0] * s + pts[i][1] * c });
			mesh.addPolygon(tri, dark);
			break;
		}
		case GLYPH::PLUS:
			mesh.addRect(GuiRect{ cx - 5.5f * u, cy - 1.3f * u, 11.0f * u, 2.6f * u }, dark);
			mesh.addRect(GuiRect{ cx - 1.3f * u, cy - 5.5f * u, 2.6f * u, 11.0f * u }, dark);
			break;
		case GLYPH::MINUS:
			mesh.addRect(GuiRect{ cx - 5.5f * u, cy - 1.3f * u, 11.0f * u, 2.6f * u }, dark);
			break;
		case GLYPH::HOME:
		{
			std::vector<GuiPoint> house = { { cx, cy - 6.0f * u }, { cx + 6.5f * u, cy - 0.5f * u }, { cx + 4.8f * u, cy + 5.5f * u },
				{ cx - 4.8f * u, cy + 5.5f * u }, { cx - 6.5f * u, cy - 0.5f * u } };
			mesh.addPolygon(house, dark);
			break;
		}
		case GLYPH::STICK:
			mesh.strokeEllipse(GuiRect{ 2.5f * u, 2.5f * u, w - 5.0f * u, h - 5.0f * u }, 1.2f * u, STROKE::INSIDE, dark);
			break;
		default:
			break;
	}
}
