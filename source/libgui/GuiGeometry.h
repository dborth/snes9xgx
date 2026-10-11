/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGeometry.h
 *
 * Platform-agnostic shape tessellation. Turns rounded rectangles, ellipses,
 * polygons, strokes, arcs and soft shadows into triangle meshes with
 * per-vertex colors. Edges are anti-aliased by a thin feathered fringe whose
 * vertices fade to alpha 0, so no multisampling is needed. Nothing here
 * touches a platform API; a mesh is drawn by whatever renders MeshVertex
 * triangle lists.
 *
 * Coordinates are design-canvas pixels, +Y down. Every triangle is emitted
 * with the same (clockwise on screen) winding.
 ***************************************************************************/
#pragma once
#include <cstddef>
#include <vector>
#include "../drivers/Mesh.h"

//!A 2D point in design-canvas pixels.
//!\ingroup grp_shapes
struct GuiPoint {
	float x;	/*!< X coordinate. */
	float y;	/*!< Y coordinate. */
};

//!An axis-aligned rectangle (top-left origin) in design-canvas pixels.
//!\ingroup grp_shapes
struct GuiRect {
	float x;	/*!< Left edge. */
	float y;	/*!< Top edge. */
	float w;	/*!< Width. */
	float h;	/*!< Height. */
};

//!Per-corner radii, in the order top-left, top-right, bottom-right, bottom-left.
//!A radius larger than half the shorter side is clamped.
//!\ingroup grp_shapes
struct GuiCorners {
	float tl;	/*!< Top-left radius. */
	float tr;	/*!< Top-right radius. */
	float br;	/*!< Bottom-right radius. */
	float bl;	/*!< Bottom-left radius. */
	//!Same radius on every corner (0 = square)
	GuiCorners(float r = 0) : tl(r), tr(r), br(r), bl(r) {}
	GuiCorners(float tl_, float tr_, float br_, float bl_) : tl(tl_), tr(tr_), br(br_), bl(bl_) {}
};

//!Where a stroke sits relative to the shape's geometric edge.
//!\ingroup grp_shapes
enum class STROKE {
	INSIDE,		//!< Entirely inside the edge (the shape keeps its size)
	CENTRE,		//!< Centered on the edge
	OUTSIDE		//!< Entirely outside the edge
};

//!A solid color or a two-color linear gradient.
//!\ingroup grp_shapes
struct GuiFill {
	PixelColor from;	/*!< Solid color, or the gradient's start color. */
	PixelColor to;		/*!< The gradient's end color (unused when solid). */
	float angle;		/*!< Gradient direction in degrees: 0 = left to right, 90 = top to bottom. */
	bool gradient;		/*!< true for a gradient, false for a solid color. */

	//!A single flat color
	static GuiFill solid(PixelColor c);
	//!A linear gradient across the shape's bounding box
	//!\param a Start color
	//!\param b End color
	//!\param angleDeg 0 = left to right, 90 = top to bottom
	static GuiFill linear(PixelColor a, PixelColor b, float angleDeg);
};

//!A rounded rectangle as a convex polygon, rotated about its center. Use it where a shape
//!has to be tilted, or where several shapes should merge into one silhouette (draw each
//!piece's outline stroke first and the fills after, and the joins disappear).
//!\param r Rectangle before rotation
//!\param radius Corner radius (clamped to half the shorter side)
//!\param angleDeg Rotation in degrees, clockwise on screen
//!\param tolerance Largest allowed distance between the true curve and the polygon
//!\ingroup grp_shapes
std::vector<GuiPoint> GuiRoundRectPolygon(const GuiRect& r, float radius, float angleDeg, float tolerance = 0.15f);

//!Accumulates shapes as a triangle list of MeshVertex. Build one when a
//!shape's size or style changes and keep it; drawing it again is free of
//!any geometry work.
//!\ingroup grp_shapes
class GuiMesh
{
	public:
		//!Constructor
		//!\param featherWidth Width of the anti-aliasing fringe, in design-canvas pixels.
		//!Pass 1 / VideoDriver::getUIScale() for a one-physical-pixel fringe; 0 disables it.
		//!\param curveTolerance Largest allowed distance between a true curve and its
		//!polygon approximation, in design-canvas pixels
		GuiMesh(float featherWidth = 1.0f, float curveTolerance = 0.15f);

		//!Removes all geometry (keeps the feather and tolerance settings)
		void clear();
		//!Gets the number of vertices (a multiple of 3)
		size_t size() const { return verts.size(); }
		//!Checks whether the mesh has no geometry
		bool empty() const { return verts.empty(); }
		//!Gets the triangle list
		const std::vector<MeshVertex>& vertices() const { return verts; }
		//!Gets the feather width the mesh was built with
		float getFeather() const { return feather; }

		//!Adds a filled rectangle
		void addRect(const GuiRect& r, const GuiFill& fill);
		//!Adds a filled rounded rectangle. Corner radius h/2 makes a capsule.
		void addRoundRect(const GuiRect& r, const GuiCorners& corners, const GuiFill& fill);
		//!Adds a filled ellipse inscribed in a rectangle (a circle when w == h)
		void addEllipse(const GuiRect& r, const GuiFill& fill);
		//!Adds a filled convex polygon (triangle, chevron, ...). Concave shapes
		//!must be split into convex pieces.
		void addPolygon(const std::vector<GuiPoint>& pts, const GuiFill& fill);
		//!Adds a straight line with flat ends
		void addLine(GuiPoint a, GuiPoint b, float width, const GuiFill& fill);

		//!Adds a rounded-rectangle outline
		void strokeRoundRect(const GuiRect& r, const GuiCorners& corners, float width, STROKE align, const GuiFill& fill);
		//!Adds an ellipse outline
		void strokeEllipse(const GuiRect& r, float width, STROKE align, const GuiFill& fill);
		//!Adds a convex-polygon outline with mitered corners
		void strokePolygon(const std::vector<GuiPoint>& pts, float width, STROKE align, const GuiFill& fill);
		//!Adds an arc stroke centered on a radius, with flat ends. Used for
		//!progress rings. Angles are in degrees, 0 = +X (right), increasing clockwise on screen.
		void strokeArc(GuiPoint centre, float radius, float startDeg, float sweepDeg, float width, PixelColor color);

		//!Adds a soft drop shadow or glow for a rounded rectangle. Draw it before the shape it belongs to.
		//!\param blur Width of the soft edge
		//!\param spread Grows (positive) or shrinks (negative) the shadow's edge
		//!\param dx Horizontal offset
		//!\param dy Vertical offset
		//!\param color Shadow color; its alpha is the shadow's peak opacity
		void addShadow(const GuiRect& r, const GuiCorners& corners, float blur, float spread, float dx, float dy, PixelColor color);

	private:
		std::vector<MeshVertex> verts;
		float feather;
		float tolerance;
};
