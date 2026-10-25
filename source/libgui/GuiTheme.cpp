/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTheme.cpp
 *
 * No colors or numbers that make up a look live here: they come from the
 * application's GuiThemeInit().
 ***************************************************************************/

#include <cstring>
#include "GuiTheme.h"

static PixelColor withAlpha(PixelColor c, uint8_t a)
{
	c.a = a;
	return c;
}

// the ends the lighten/darken mixes run toward
static const PixelColor WHITE = { 255, 255, 255, 255 };
static const PixelColor BLACK = { 0, 0, 0, 255 };

GuiTheme::GuiTheme()
{
	memset(this, 0, sizeof(*this));
}

static GuiTheme& makeCurrent()
{
	static GuiTheme theme;
	GuiThemeInit(theme);
	return theme;
}

GuiTheme& GuiTheme::current()
{
	static GuiTheme& theme = makeCurrent();
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
		.shadow(shadowBlur, shadow, 0.0f, shadowOffsetY);
	GuiPaint lit = GuiPaint()
		.gradient(surfaceRaised, surface)
		.stroke(strokeWidth, accent)
		.glow(shadowBlur + glowGrow, withAlpha(accent, focusGlowAlpha), glowGrow);
	GuiPaint pressed = GuiPaint()
		.gradient(surface, surfaceRaised)
		.stroke(strokeWidth, accent)
		.glow(shadowBlur, withAlpha(accent, pressGlowAlpha));
	GuiPaint off = GuiPaint()
		.color(withAlpha(surface, cardDisabledAlpha))
		.stroke(strokeWidth, withAlpha(outline, cardDisabledAlpha));
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed).onDisable(off);
}

GuiShapeStyle GuiTheme::buttonStyle() const
{
	const PixelColor litStroke = GuiMixColor(accentLight, WHITE, buttonStrokeLight);
	GuiPaint rest = GuiPaint()
		.gradient(outlineStrong, outline)
		.stroke(strokeWidth, outlineStrong);
	GuiPaint lit = GuiPaint()
		.gradient(accentLight, accent)
		.stroke(strokeWidth, litStroke)
		.glow(buttonGlowBlur, withAlpha(accent, buttonGlowAlpha));
	GuiPaint pressed = GuiPaint()
		.gradient(accent, accentDark)
		.stroke(strokeWidth, litStroke);
	GuiPaint off = GuiPaint()
		.color(withAlpha(surface, buttonDisabledAlpha))
		.stroke(strokeWidth, withAlpha(outline, buttonDisabledAlpha));
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed).onDisable(off);
}

GuiShapeStyle GuiTheme::chipStyle() const
{
	return GuiShapeStyle(GuiPaint().color(outline));
}

GuiShapeStyle GuiTheme::rowStyle() const
{
	GuiPaint lit = GuiPaint().color(highlight);
	GuiPaint pressed = GuiPaint().color(withAlpha(accent, rowPressAlpha));
	return GuiShapeStyle(GuiPaint()).onFocus(lit).onPress(pressed);
}

GuiShapeStyle GuiTheme::barStyle() const
{
	return GuiShapeStyle(GuiPaint().color(GuiMixColor(background, BLACK, barDarken)));
}
