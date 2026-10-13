/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiArrow.h
 ***************************************************************************/
#pragma once

//!Which way a GuiArrow points
//!\ingroup grp_shapes
enum class ARROW {
	UP,
	DOWN,
	LEFT,
	RIGHT
};

//!A small filled triangle, drawn from geometry: the arrow on a scrollbar button.
//!It is meant to be a GuiButton's icon: the button mirrors its state into it, and
//!it switches from the resting color to the lit one while selected, held or clicked.
//!Not selectable by itself.
//!\ingroup grp_shapes
class GuiArrow : public GuiElement
{
	public:
		//!\param dir Which way it points
		//!\param w,h Size of the triangle's bounding box in design pixels
		//!\param rest Color at rest
		//!\param lit Color while the button it belongs to is highlighted
		GuiArrow(ARROW dir, int w, int h, PixelColor rest, PixelColor lit);
		void draw() override;
	private:
		ARROW direction;
		PixelColor restColor;
		PixelColor litColor;
		PixelColor meshColor;	//!< Color the mesh was built with
		int meshWidth;
		int meshHeight;
		float meshFeather;
		GuiMesh mesh;
		std::vector<MeshVertex> scratch;
};
