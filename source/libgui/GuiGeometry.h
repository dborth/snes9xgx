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

	constexpr GuiPoint() : x(0.0f), y(0.0f) {}
	constexpr GuiPoint(float x_, float y_) : x(x_), y(y_) {}
};

//!An axis-aligned rectangle (top-left origin) in design-canvas pixels.
//!\ingroup grp_shapes
struct GuiRect {
	float x;	/*!< Left edge. */
	float y;	/*!< Top edge. */
	float w;	/*!< Width. */
	float h;	/*!< Height. */

	constexpr GuiRect() : x(0.0f), y(0.0f), w(0.0f), h(0.0f) {}
	constexpr GuiRect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
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
	PixelColor from = { 0, 0, 0, 0 };	/*!< Solid color, or the gradient's start color. */
	PixelColor to = { 0, 0, 0, 0 };		/*!< The gradient's end color (unused when solid). */
	float angle = 0.0f;					/*!< Gradient direction in degrees: 0 = left to right, 90 = top to bottom. */
	bool gradient = false;				/*!< true for a gradient, false for a solid color. */

	//!A single flat color
	static GuiFill solid(PixelColor c);
	//!A linear gradient across the shape's bounding box
	//!\param a Start color
	//!\param b End color
	//!\param angleDeg 0 = left to right, 90 = top to bottom
	static GuiFill linear(PixelColor a, PixelColor b, float angleDeg);
};

//!A closed outline made of straight lines and cubic Beziers, in whatever units the caller likes.
//!It is how a silhouette that is not a handful of rounded rectangles is stored (the controller
//!outlines converted from SVG are lists of these). Commands: MOVE starts a contour (1 point), LINE
//!draws to a point (1), CUBIC draws a cubic Bezier (3: two control points, then the end), CLOSE ends
//!the contour (0). Points are stored flat as x, y pairs.
//!\ingroup grp_shapes
struct GuiPath {
	enum CMD : uint8_t { MOVE = 'M', LINE = 'L', CUBIC = 'C', CLOSE = 'Z' };
	std::vector<uint8_t> cmds;	/*!< One CMD per segment. */
	std::vector<float> pts;		/*!< x, y pairs, as many as the commands consume. */

	//!Checks whether the path has no commands
	bool empty() const { return cmds.empty(); }
	GuiPath& moveTo(float x, float y) { cmds.push_back(MOVE); pts.push_back(x); pts.push_back(y); return *this; }
	GuiPath& lineTo(float x, float y) { cmds.push_back(LINE); pts.push_back(x); pts.push_back(y); return *this; }
	GuiPath& cubicTo(float x1, float y1, float x2, float y2, float x, float y)
	{
		cmds.push_back(CUBIC);
		pts.push_back(x1); pts.push_back(y1); pts.push_back(x2); pts.push_back(y2); pts.push_back(x); pts.push_back(y);
		return *this;
	}
	GuiPath& close() { cmds.push_back(CLOSE); return *this; }
};

//!Turns a path into one polygon per contour, multiplied by `scale`. Curves are split until no part of the
//!polygon is farther than `tolerance` from the true curve, so a flat stretch costs one segment and a tight
//!corner costs more. A contour with fewer than 3 points is dropped.
//!\param path The path
//!\param scale Multiplies every coordinate
//!\param tolerance Largest allowed distance between the true curve and the polygon, in the scaled units
//!\ingroup grp_shapes
std::vector<std::vector<GuiPoint> > GuiFlattenPath(const GuiPath& path, float scale, float tolerance = 0.15f);

//!Gets the bounding box of a path's points (control points included, so it can be a little larger than the
//!drawn curve), multiplied by `scale`
//!\ingroup grp_shapes
GuiRect GuiPathBounds(const GuiPath& path, float scale = 1.0f);

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
		GuiMesh(float featherWidth = 1.0f, float curveTolerance = 0.3f);

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
		//!Gets the curve tolerance the mesh was built with
		float getTolerance() const { return tolerance; }

		//!Adds a filled rectangle
		void addRect(const GuiRect& r, const GuiFill& fill, bool edgeFringe = true);
		//!Adds a filled rounded rectangle. Corner radius h/2 makes a capsule.
		//!\param edgeFringe false leaves out the anti-aliased edge, for a fill whose edge is
		//!entirely covered by an opaque outline drawn on top of it (saves a third of the fill's triangles)
		void addRoundRect(const GuiRect& r, const GuiCorners& corners, const GuiFill& fill, bool edgeFringe = true);
		//!Adds a filled ellipse inscribed in a rectangle (a circle when w == h)
		//!\param edgeFringe see addRoundRect()
		void addEllipse(const GuiRect& r, const GuiFill& fill, bool edgeFringe = true);
		//!Adds a filled polygon (triangle, chevron, a controller silhouette, ...). The polygon may be
		//!concave but must not cross itself. A convex one is filled as a fan; a concave one is
		//!triangulated (ear clipping, quadratic in the vertex count, so build it once and keep the mesh).
		void addPolygon(const std::vector<GuiPoint>& pts, const GuiFill& fill, bool edgeFringe = true);
		//!Adds a straight line with flat ends
		void addLine(GuiPoint a, GuiPoint b, float width, const GuiFill& fill);

		//!Adds a rounded-rectangle outline
		void strokeRoundRect(const GuiRect& r, const GuiCorners& corners, float width, STROKE align, const GuiFill& fill);
		//!Adds an ellipse outline
		void strokeEllipse(const GuiRect& r, float width, STROKE align, const GuiFill& fill);
		//!Adds a polygon outline with mitered corners (the miter is capped on very sharp corners). A concave
		//!polygon works as long as the stroke is thin compared with its smallest notch.
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
