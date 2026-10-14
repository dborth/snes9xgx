/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPaint.cpp
 ***************************************************************************/

#include "GuiPaint.h"
#include <cmath>
#include <algorithm>

namespace {

float clamp01(float t)
{
	return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

uint8_t lerp8(uint8_t a, uint8_t b, float t)
{
	return (uint8_t)(lerp((float)a, (float)b, t) + 0.5f);
}

bool sameColor(const PixelColor& a, const PixelColor& b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool sameFill(const GuiFill& a, const GuiFill& b)
{
	if(a.gradient != b.gradient || !sameColor(a.from, b.from))
		return false;
	return !a.gradient || (sameColor(a.to, b.to) && a.angle == b.angle);
}

PixelColor mixColor(const PixelColor& a, const PixelColor& b, float t)
{
	PixelColor c;
	c.r = lerp8(a.r, b.r, t);
	c.g = lerp8(a.g, b.g, t);
	c.b = lerp8(a.b, b.b, t);
	c.a = lerp8(a.a, b.a, t);
	return c;
}

//A fill in gradient form, so a solid and a gradient can be blended
GuiFill asGradient(const GuiFill& f, float otherAngle)
{
	if(f.gradient)
		return f;
	return GuiFill::linear(f.from, f.from, otherAngle);
}

GuiFill mixFill(const GuiFill& a, const GuiFill& b, float t)
{
	if(!a.gradient && !b.gradient)
		return GuiFill::solid(mixColor(a.from, b.from, t));
	GuiFill ga = asGradient(a, b.angle), gb = asGradient(b, a.angle);
	return GuiFill::linear(mixColor(ga.from, gb.from, t), mixColor(ga.to, gb.to, t), lerp(ga.angle, gb.angle, t));
}

PixelColor transparent(PixelColor c)
{
	c.a = 0;
	return c;
}

} // namespace

GuiPaint::GuiPaint() :
	fill(GuiFill::solid(PixelColor{ 0, 0, 0, 0 })), hasFill(false),
	strokeFill(GuiFill::solid(PixelColor{ 0, 0, 0, 0 })), strokeWidth(0.0f), strokeAlign(STROKE::INSIDE),
	shadowColor{ 0, 0, 0, 0 }, shadowBlur(0.0f), shadowSpread(0.0f), shadowDx(0.0f), shadowDy(0.0f)
{}

GuiPaint& GuiPaint::color(PixelColor c)
{
	fill = GuiFill::solid(c);
	hasFill = true;
	return *this;
}

GuiPaint& GuiPaint::gradient(PixelColor from, PixelColor to, float angleDeg)
{
	fill = GuiFill::linear(from, to, angleDeg);
	hasFill = true;
	return *this;
}

GuiPaint& GuiPaint::stroke(float width, PixelColor c, STROKE align)
{
	strokeFill = GuiFill::solid(c);
	strokeWidth = width;
	strokeAlign = align;
	return *this;
}

GuiPaint& GuiPaint::shadow(float blur, PixelColor c, float dx, float dy, float spread)
{
	shadowColor = c;
	shadowBlur = blur;
	shadowDx = dx;
	shadowDy = dy;
	shadowSpread = spread;
	return *this;
}

GuiPaint& GuiPaint::glow(float blur, PixelColor c, float spread)
{
	return shadow(blur, c, 0.0f, 0.0f, spread);
}

bool GuiPaint::operator==(const GuiPaint& o) const
{
	if(hasFill != o.hasFill || (hasFill && !sameFill(fill, o.fill)))
		return false;
	if(strokeWidth != o.strokeWidth)
		return false;
	if(strokeWidth > 0.0f && (strokeAlign != o.strokeAlign || !sameFill(strokeFill, o.strokeFill)))
		return false;
	if(shadowColor.a != o.shadowColor.a)
		return false;
	if(shadowColor.a != 0 && (!sameColor(shadowColor, o.shadowColor) || shadowBlur != o.shadowBlur ||
		shadowSpread != o.shadowSpread || shadowDx != o.shadowDx || shadowDy != o.shadowDy))
		return false;
	return true;
}

GuiPaint GuiPaint::mix(const GuiPaint& a, const GuiPaint& b, float t)
{
	t = clamp01(t);
	if(t <= 0.0f)
		return a;
	if(t >= 1.0f)
		return b;

	GuiPaint p;

	// fill: a fill present on one side only fades in or out in its own color
	if(a.hasFill || b.hasFill)
	{
		GuiFill fa = a.hasFill ? a.fill : GuiFill::solid(transparent(b.fill.from));
		GuiFill fb = b.hasFill ? b.fill : GuiFill::solid(transparent(a.fill.from));
		if(!a.hasFill && b.fill.gradient)
			fa = GuiFill::linear(transparent(b.fill.from), transparent(b.fill.to), b.fill.angle);
		if(!b.hasFill && a.fill.gradient)
			fb = GuiFill::linear(transparent(a.fill.from), transparent(a.fill.to), a.fill.angle);
		p.fill = mixFill(fa, fb, t);
		p.hasFill = true;
	}

	// outline: a missing outline grows from zero width in the other's color
	if(a.strokeWidth > 0.0f || b.strokeWidth > 0.0f)
	{
		GuiFill sa = a.strokeWidth > 0.0f ? a.strokeFill : b.strokeFill;
		GuiFill sb = b.strokeWidth > 0.0f ? b.strokeFill : a.strokeFill;
		p.strokeFill = mixFill(sa, sb, t);
		p.strokeWidth = lerp(a.strokeWidth, b.strokeWidth, t);
		p.strokeAlign = t < 0.5f ? (a.strokeWidth > 0.0f ? a.strokeAlign : b.strokeAlign) : (b.strokeWidth > 0.0f ? b.strokeAlign : a.strokeAlign);
	}

	// shadow: a missing shadow fades in or out in the other's color and shape
	if(a.shadowColor.a != 0 || b.shadowColor.a != 0)
	{
		const GuiPaint& sa = a.shadowColor.a != 0 ? a : b;
		const GuiPaint& sb = b.shadowColor.a != 0 ? b : a;
		PixelColor ca = a.shadowColor.a != 0 ? a.shadowColor : transparent(b.shadowColor);
		PixelColor cb = b.shadowColor.a != 0 ? b.shadowColor : transparent(a.shadowColor);
		p.shadowColor = mixColor(ca, cb, t);
		p.shadowBlur = lerp(sa.shadowBlur, sb.shadowBlur, t);
		p.shadowSpread = lerp(sa.shadowSpread, sb.shadowSpread, t);
		p.shadowDx = lerp(sa.shadowDx, sb.shadowDx, t);
		p.shadowDy = lerp(sa.shadowDy, sb.shadowDy, t);
	}
	return p;
}

GuiShapeStyle::GuiShapeStyle(const GuiPaint& normalPaint) :
	normal(normalPaint), hasFocused(false), hasPressed(false), hasDisabled(false)
{}

GuiShapeStyle& GuiShapeStyle::onFocus(const GuiPaint& p)
{
	focused = p;
	hasFocused = true;
	return *this;
}

GuiShapeStyle& GuiShapeStyle::onPress(const GuiPaint& p)
{
	pressed = p;
	hasPressed = true;
	return *this;
}

GuiShapeStyle& GuiShapeStyle::onDisable(const GuiPaint& p)
{
	disabled = p;
	hasDisabled = true;
	return *this;
}

const GuiPaint& GuiShapeStyle::forState(SHAPE_STATE s) const
{
	switch(s)
	{
		case SHAPE_STATE::FOCUSED:  return hasFocused ? focused : normal;
		case SHAPE_STATE::PRESSED:  return hasPressed ? pressed : (hasFocused ? focused : normal);
		case SHAPE_STATE::DISABLED: return hasDisabled ? disabled : normal;
		default:                    return normal;
	}
}

void GuiAddPaintedShape(GuiMesh& mesh, SHAPE shape, const GuiRect& r, const GuiCorners& corners, const GuiPaint& paint)
{
	const float half = 0.5f * std::min(r.w, r.h);
	const GuiCorners outline = shape == SHAPE::ELLIPSE ? GuiCorners(half) : corners;

	if(paint.shadowColor.a != 0)
		mesh.addShadow(r, outline, paint.shadowBlur, paint.shadowSpread, paint.shadowDx, paint.shadowDy, paint.shadowColor);

	if(paint.hasFill)
	{
		// An opaque inside outline at least one feather wide sits exactly over the fill's
		// anti-aliased edge, and gives the shape its own; the fill's would only bleed its color
		// into that edge (and cost a third of the fill's triangles)
		const bool strokeOpaque = paint.strokeFill.from.a == 255 && (!paint.strokeFill.gradient || paint.strokeFill.to.a == 255);
		const bool edgeFringe = !(paint.strokeWidth > 0.0f && paint.strokeAlign == STROKE::INSIDE && strokeOpaque &&
			std::min(paint.strokeWidth, half) >= mesh.getFeather());

		if(shape == SHAPE::ELLIPSE)
			mesh.addEllipse(r, paint.fill, edgeFringe);
		else
			mesh.addRoundRect(r, corners, paint.fill, edgeFringe);
	}

	if(paint.strokeWidth > 0.0f)
	{
		if(shape == SHAPE::ELLIPSE)
			mesh.strokeEllipse(r, paint.strokeWidth, paint.strokeAlign, paint.strokeFill);
		else
			mesh.strokeRoundRect(r, corners, paint.strokeWidth, paint.strokeAlign, paint.strokeFill);
	}
}
