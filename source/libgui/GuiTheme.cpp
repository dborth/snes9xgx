/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTheme.cpp
 ***************************************************************************/

#include "GuiTheme.h"

static PixelColor rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	PixelColor c = { r, g, b, a };
	return c;
}

GuiTheme::GuiTheme() :
	background(rgb(18, 22, 29)),
	surface(rgb(28, 35, 48)),
	surfaceRaised(rgb(36, 46, 64)),
	outline(rgb(43, 54, 71)),
	outlineStrong(rgb(74, 90, 115)),
	accent(rgb(255, 150, 40)),
	accentLight(rgb(255, 200, 110)),
	accentDark(rgb(200, 110, 20)),
	text(rgb(232, 237, 244)),
	textMuted(rgb(152, 164, 181)),
	textOnAccent(rgb(30, 20, 5)),
	good(rgb(63, 208, 176)),
	radius(12.0f),
	strokeWidth(2.0f),
	padding(12.0f),
	spacing(8.0f),
	shadowBlur(14.0f)
{}

GuiTheme& GuiTheme::current()
{
	static GuiTheme theme;
	return theme;
}

GuiShapeStyle GuiTheme::panelStyle() const
{
	return GuiShapeStyle(GuiPaint().color(surface).stroke(strokeWidth, outline));
}

GuiShapeStyle GuiTheme::cardStyle() const
{
	GuiPaint rest = GuiPaint()
		.gradient(surfaceRaised, surface)
		.stroke(strokeWidth, outline)
		.shadow(shadowBlur, rgb(0, 0, 0, 150), 0.0f, 4.0f);
	GuiPaint lit = GuiPaint()
		.gradient(surfaceRaised, surface)
		.stroke(strokeWidth, accent)
		.glow(shadowBlur + 2.0f, rgb(accent.r, accent.g, accent.b, 110), 2.0f);
	GuiPaint pressed = GuiPaint()
		.gradient(surface, surfaceRaised)
		.stroke(strokeWidth, accent)
		.glow(shadowBlur, rgb(accent.r, accent.g, accent.b, 70));
	GuiPaint off = GuiPaint()
		.color(rgb(surface.r, surface.g, surface.b, 140))
		.stroke(strokeWidth, rgb(outline.r, outline.g, outline.b, 140));
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed).onDisable(off);
}

GuiShapeStyle GuiTheme::buttonStyle() const
{
	GuiPaint rest = GuiPaint()
		.gradient(outlineStrong, outline)
		.stroke(strokeWidth, outlineStrong);
	GuiPaint lit = GuiPaint()
		.gradient(accentLight, accent)
		.stroke(strokeWidth, rgb(255, 220, 160))
		.glow(10.0f, rgb(accent.r, accent.g, accent.b, 90));
	GuiPaint pressed = GuiPaint()
		.gradient(accent, accentDark)
		.stroke(strokeWidth, rgb(255, 220, 160));
	GuiPaint off = GuiPaint()
		.color(rgb(surface.r, surface.g, surface.b, 150))
		.stroke(strokeWidth, rgb(outline.r, outline.g, outline.b, 150));
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed).onDisable(off);
}

GuiShapeStyle GuiTheme::chipStyle() const
{
	return GuiShapeStyle(GuiPaint().color(outline));
}

GuiShapeStyle GuiTheme::rowStyle() const
{
	GuiPaint lit = GuiPaint().color(rgb(62, 74, 94));
	GuiPaint pressed = GuiPaint().color(rgb(accent.r, accent.g, accent.b, 110));
	return GuiShapeStyle(GuiPaint()).onFocus(lit).onPress(pressed);
}

GuiShapeStyle GuiTheme::barStyle() const
{
	return GuiShapeStyle(GuiPaint().color(rgb(8, 10, 14)));
}
