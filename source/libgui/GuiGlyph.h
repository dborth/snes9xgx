/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGlyph.h
 ***************************************************************************/
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "GuiElement.h"
#include "GuiPaint.h"
#include "GuiTheme.h"

//!Small control icons for lists and hints (the "A" in a circle, a bumper, a D-pad arrow).
//!\ingroup grp_shapes
enum class GLYPH {
	BUTTON,		//!< A light circle (face buttons; the letter goes on it)
	BUMPER,		//!< A light rounded rectangle (L, R, ZL, ZR)
	UP,			//!< D-pad arrows: a circle with a triangle
	DOWN,
	LEFT,
	RIGHT,
	PLUS,
	MINUS,
	STICK,		//!< A circle with an inner ring (stick click; the letter goes on it)
	CAPTURE,	//!< A rounded square with a ring
	HOME
};

class GuiText;

//!A small control icon drawn from shapes: a light badge with a dark symbol or letter, for list rows and hint
//!bars. Not selectable; it is placed by a layout like any element and sized by its kind.
//!`ui.glyph(GLYPH::BUTTON, "A")`, `ui.glyph(GLYPH::UP)`
//!\ingroup grp_shapes
class GuiGlyph : public GuiElement
{
	public:
		//!\param kind Which icon
		//!\param letter Text on the badge for BUTTON, BUMPER and STICK ("A", "ZL", "R"); ignored by the others
		GuiGlyph(GLYPH kind, const char * letter = "");
		~GuiGlyph();
		void draw() override;

		//!Gets the natural size of a glyph, from the theme (glyphSize, glyphBumperWidth, glyphBumperHeight).
		//!\ingroup grp_shapes
		static void size(GLYPH kind, float& w, float& h);

		//!Builds a glyph filling a w x h box at the origin, in the theme's light-on-dark colors. Letters are text and are not part of the mesh.
		//!\ingroup grp_shapes
		static void build(GuiMesh& mesh, GLYPH kind, float w, float h);

	private:
		GLYPH kind;
		GuiText * text;
		GuiMesh mesh;
		float meshFeather;
		std::vector<MeshVertex> scratch;
};

inline void GuiGlyph::size(GLYPH kind, float& w, float& h)
{
	const GuiTheme& theme = GuiTheme::current();
	switch(kind)
	{
		case GLYPH::BUMPER: w = theme.glyphBumperWidth; h = theme.glyphBumperHeight; break;
		default: w = theme.glyphSize; h = theme.glyphSize; break;
	}
}

inline void GuiGlyph::build(GuiMesh& mesh, GLYPH kind, float w, float h)
{
	const GuiTheme& theme = GuiTheme::current();
	const GuiFill light = GuiFill::solid(theme.text);
	const GuiFill dark = GuiFill::solid(theme.textInverse); // the symbol on the badge: the ink that reads on theme.text
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
