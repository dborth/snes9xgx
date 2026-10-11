/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPaint.h
 *
 * Platform-agnostic description of how a shape looks: fill, outline and
 * shadow, plus the per-state sets a widget switches between (normal,
 * focused, pressed, disabled). Paints are plain values that can be copied,
 * compared and blended, which is what lets a highlight fade in instead of
 * snapping.
 ***************************************************************************/
#pragma once
#include "GuiGeometry.h"

//!Basic shape kinds a paint can be applied to.
//!\ingroup grp_shapes
enum class SHAPE {
	RECTANGLE,	//!< Rectangle, with optional rounded corners (see GuiCorners)
	ELLIPSE		//!< Ellipse inscribed in the rectangle (a circle when it is square)
};

//!The look of a shape: an optional fill, an optional outline and an optional
//!shadow or glow. Setters chain, so a paint reads as a sentence:
//!`GuiPaint().gradient(top, bottom).stroke(2, edge).shadow(14, black, 0, 4)`
//!\ingroup grp_shapes
struct GuiPaint {
	GuiFill fill;			/*!< Fill, used when hasFill. */
	bool hasFill;			/*!< Whether the shape is filled. */
	GuiFill strokeFill;		/*!< Outline color. */
	float strokeWidth;		/*!< Outline width in design-canvas pixels; 0 = no outline. */
	STROKE strokeAlign;		/*!< Where the outline sits relative to the shape's edge. */
	PixelColor shadowColor;	/*!< Shadow or glow color; its alpha is the peak opacity (0 = none). */
	float shadowBlur;		/*!< Width of the shadow's soft edge. */
	float shadowSpread;		/*!< Grows (positive) or shrinks (negative) the shadow's edge. */
	float shadowDx;			/*!< Shadow horizontal offset. */
	float shadowDy;			/*!< Shadow vertical offset. */

	//!An empty paint: nothing is drawn until a fill, outline or shadow is set
	GuiPaint();

	//!Fills with a solid color
	GuiPaint& color(PixelColor c);
	//!Fills with a two-color linear gradient (angle 0 = left to right, 90 = top to bottom)
	GuiPaint& gradient(PixelColor from, PixelColor to, float angleDeg = 90.0f);
	//!Adds an outline
	GuiPaint& stroke(float width, PixelColor c, STROKE align = STROKE::INSIDE);
	//!Adds a drop shadow
	GuiPaint& shadow(float blur, PixelColor c, float dx = 0.0f, float dy = 0.0f, float spread = 0.0f);
	//!Adds a glow (a shadow with no offset)
	GuiPaint& glow(float blur, PixelColor c, float spread = 0.0f);

	//!Field-by-field comparison
	bool operator==(const GuiPaint& o) const;
	bool operator!=(const GuiPaint& o) const { return !(*this == o); }

	//!Blends two paints. A part present in only one of them fades in or out
	//!rather than appearing or disappearing.
	//!\param a Paint at t = 0
	//!\param b Paint at t = 1
	//!\param t Blend amount, clamped to 0..1
	static GuiPaint mix(const GuiPaint& a, const GuiPaint& b, float t);
};

//!The states a styled shape distinguishes.
//!\ingroup grp_shapes
enum class SHAPE_STATE {
	NORMAL,
	FOCUSED,	//!< Highlighted: under the cursor, or selected with the D-pad
	PRESSED,
	DISABLED
};

//!A set of paints, one per state. States left unset fall back to the normal paint.
//!`GuiShapeStyle(rest).onFocus(lit).onPress(pressed)`
//!\ingroup grp_shapes
struct GuiShapeStyle {
	GuiPaint normal;
	GuiPaint focused;
	GuiPaint pressed;
	GuiPaint disabled;
	bool hasFocused;
	bool hasPressed;
	bool hasDisabled;

	GuiShapeStyle(const GuiPaint& normalPaint = GuiPaint());
	GuiShapeStyle& onFocus(const GuiPaint& p);
	GuiShapeStyle& onPress(const GuiPaint& p);
	GuiShapeStyle& onDisable(const GuiPaint& p);
	//!Gets the paint for a state, falling back to normal when none was set
	const GuiPaint& forState(SHAPE_STATE s) const;
};

//!Adds a painted shape to a mesh: shadow first, then fill, then outline.
//!For SHAPE::ELLIPSE the corner radii are ignored; the shadow of an ellipse
//!follows its outline.
//!\ingroup grp_shapes
void GuiAddPaintedShape(GuiMesh& mesh, SHAPE shape, const GuiRect& r, const GuiCorners& corners, const GuiPaint& paint);
