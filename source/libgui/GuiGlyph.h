/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGlyph.h
 ***************************************************************************/
#pragma once

//!Small control icons for lists and hints (the "A" in a circle, a bumper, a D-pad arrow).
//!\ingroup grp_controller
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

//!A small control icon drawn from shapes: a light badge with a dark symbol or letter, for list rows and hint
//!bars. Not selectable; it is placed by a layout like any element and sized by its kind.
//!`ui.glyph(GLYPH::BUTTON, "A")`, `ui.glyph(GLYPH::UP)`
//!\ingroup grp_controller
class GuiGlyph : public GuiElement
{
	public:
		//!\param kind Which icon
		//!\param letter Text on the badge for BUTTON, BUMPER and STICK ("A", "ZL", "R"); ignored by the others
		GuiGlyph(GLYPH kind, const char * letter = "");
		~GuiGlyph();
		void draw() override;

		//!Gets the natural size of a glyph.
		//!\ingroup grp_controller
		static void size(GLYPH kind, float& w, float& h);

		//!Builds a glyph filling a w x h box at the origin, in the theme's light-on-dark colors. Letters are text and are not part of the mesh.
		//!\ingroup grp_controller
		static void build(GuiMesh& mesh, GLYPH kind, float w, float h);

	private:
		GLYPH kind;
		GuiText * text;
		GuiMesh mesh;
		float meshFeather;
		std::vector<MeshVertex> scratch;
};
