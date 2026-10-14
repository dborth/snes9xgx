/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiShape.h
 ***************************************************************************/
#pragma once

//!A code-drawn shape: a rectangle (optionally rounded) or ellipse with a fill,
//!outline and shadow, and no image assets. Use it anywhere a GuiImage fits,
//!for example as a GuiButton's image: the button mirrors its state into the
//!shape, so a GuiShapeStyle gives the button its resting, highlighted,
//!pressed and disabled looks, and changes between them fade.
//!The element's size is the shape's size; the shadow and glow extend past it.
//!\ingroup grp_shapes
class GuiShape : public GuiElement
{
	public:
		//!Constructor
		//!\param w Width
		//!\param h Height
		GuiShape(int w = 0, int h = 0);
		//!Destructor
		~GuiShape();
		GuiShape(const GuiShape&) = delete;
		GuiShape& operator=(const GuiShape&) = delete;
		//!Sets the shape kind (rectangle or ellipse)
		void setShape(SHAPE s);
		//!Sets the corner radii of a rectangle
		void setCorners(const GuiCorners& c);
		//!Rounds a rectangle's ends fully (radius half the shorter side)
		void setCapsule();
		//!Sets the look for every state; applies immediately
		void setStyle(const GuiShapeStyle& s);
		//!Sets one look for every state
		void setPaint(const GuiPaint& p);
		//!Sets how long a change of state takes to blend, in seconds (0 = instant)
		void setTransition(float seconds);
		//!Constantly called to draw the shape
		void draw() override;
	private:
		SHAPE kind;
		GuiCorners corners;
		GuiShapeStyle style;
		float transition;
		GuiPaint from;		//!< Paint when the current blend started
		GuiPaint to;		//!< Paint being blended toward
		GuiPaint shown;		//!< Paint drawn this frame
		float blend;		//!< 0..1 progress from `from` to `to`
		int blendStep;		//!< `blend` rounded to one of a few levels; `shown` only changes when this does
		bool started;		//!< Whether `shown` holds a paint yet
		bool dirty;			//!< The mesh must be looked up again
		int meshWidth;
		int meshHeight;
		float meshFeather;
		GuiSharedMesh * mesh;	//!< Shared with every shape that looks the same, see GuiMeshCache
		std::vector<MeshVertex> scratch;
};
