/****************************************************************************
 * Snes9x GX
 *
 * theme.cpp
 *
 * The GuiTheme the shared libgui widgets (file browser, keyboard, cards,
 * glyphs) read: the menu palette plus the metrics and style tuning the
 * palette does not cover. libgui's core defines no look of its own, so every
 * application defines GuiThemeInit(); it runs once, the first time the theme
 * is used.
 ***************************************************************************/

#include "../libgui/GuiTheme.h"
#include "menupalette.h"

void GuiThemeInit(GuiTheme& t)
{
	const MenuPalette& p = MenuPaletteCurrent();

	// colors: from the palette
	t.background    = p.backgroundBottom;
	t.surface       = p.surface;
	t.surfaceRaised = p.surfaceRaised;
	t.outline       = p.outline;
	t.outlineStrong = p.outlineStrong;
	t.accent        = p.accent;
	t.accentLight   = p.accentTint;
	t.accentDark    = p.accentDark;
	t.text          = p.text;
	t.textMuted     = p.textMuted;
	t.textOnAccent  = p.textOnAccent;
	t.textInverse   = p.textOnAccent;	// the letter or symbol on a glyph badge (which is filled with the text color)
	t.highlight     = p.accentTint;
	t.mark          = p.mark;

	// colors the palette does not cover
	t.good   = MenuRGBA(63, 208, 176);
	t.shadow = MenuRGBA(0, 0, 0, 64);

	// metrics
	t.radius      = 14.0f;
	t.strokeWidth = 1.5f;
	t.padding     = 12.0f;
	t.spacing     = 8.0f;
	t.shadowBlur  = 14.0f;

	// glyphs
	t.glyphSize          = 19.0f;
	t.glyphBumperWidth   = 27.0f;
	t.glyphBumperHeight  = 18.0f;
	t.glyphFontSize      = 13;
	t.glyphFontSizeSmall = 10;

	// style recipe tuning
	t.shadowOffsetY       = 4.0f;
	t.glowGrow            = 2.0f;
	t.focusGlowAlpha      = 110;
	t.pressGlowAlpha      = 70;
	t.buttonGlowBlur      = 10.0f;
	t.buttonGlowAlpha     = 90;
	t.rowPressAlpha       = 110;
	t.cardDisabledAlpha   = 140;
	t.buttonDisabledAlpha = 150;
	t.buttonStrokeLight   = 0.35f;
	t.barDarken           = 0.55f;
}
