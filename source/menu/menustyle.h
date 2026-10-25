/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * menustyle.h
 *
 * The look of the menus, drawn in code: colors, metrics and shape styles for
 * the header/footer bars, the page background and the buttons, built from the
 * palette in menupalette.h (theme.cpp feeds the same palette to GuiTheme for
 * the shared widgets). Nothing here touches a platform API,
 * so it can be built on a host for previews. The GuiShape wrappers live in
 * menuart.h.
 *
 * The colors that give the app its identity are in menupalette.h.
 ***************************************************************************/
#pragma once
#include "libgui/GuiTheme.h"
#include "menupalette.h"

// design-canvas sizes
#define MENU_BTN_LONG_W		216
#define MENU_BTN_LONG_H		52
#define MENU_BTN_LARGE_W	164
#define MENU_BTN_LARGE_H	92
#define MENU_BAR_TOP_H		96
#define MENU_BAR_BOTTOM_H	112
// space kept clear at each side of the screen, for TVs that crop the picture
#define MENU_PAGE_MARGIN	44
// shapes that touch the screen edge are drawn this much larger than the screen,
// so their outline and feathered edge are never visible
#define MENU_BAR_BLEED		6

#define MENU_BTN_LONG_RADIUS	26.0f	// capsule
#define MENU_BTN_LARGE_RADIUS	46.0f	// capsule (half the button height)
#define MENU_BTN_ICON_INSET		3.0f	// icons stay clear of the button's outline (up to its highlighted width)

#define MENU_BTN_NORMAL_RADIUS	26.0f
#define MENU_BTN_SHORT_RADIUS	26.0f
#define MENU_BTN_SMALL_RADIUS	20.0f

#define MENU_BTN_NORMAL_W	196
#define MENU_BTN_NORMAL_H	52
#define MENU_BTN_SHORT_W	160
#define MENU_BTN_SHORT_H	52
#define MENU_BTN_SMALL_W	104
#define MENU_BTN_SMALL_H	40

// dialogs: a card with a chrome title band
#define MENU_DIALOG_W			448
#define MENU_DIALOG_H			288
#define MENU_DIALOG_HEADER_H	52
#define MENU_CREDITS_W			580
#define MENU_CREDITS_H			448
#define MENU_CREDITS_HEADER_H	60
#define MENU_DIALOG_RADIUS		14.0f
// title text sits this far down on the band, so its glyphs look centered in it
#define MENU_DIALOG_TITLE_Y		18
#define MENU_CREDITS_TITLE_Y	18

// the on-screen keyboard: a card with a band across the top that holds the text field
#define MENU_KEYBOARD_W			580
#define MENU_KEYBOARD_H			400
#define MENU_KEYBOARD_HEADER_H	60

// the gray stand-in for the screen in the zoom and position windows
#define MENU_SCREEN_W			68
#define MENU_SCREEN_H			64

// page-turning arrows: hit area of the button, and the glyph drawn inside it
#define MENU_ARROW_HIT_LONG		32
#define MENU_ARROW_HIT_SHORT	28
#define MENU_ARROW_LONG			24
#define MENU_ARROW_SHORT		17

// progress bar and spinner inside the progress dialog
#define MENU_PROGRESS_W			398
#define MENU_PROGRESS_H			22
#define MENU_SPINNER_SIZE		72
#define MENU_SPINNER_DOTS		8

// the two cards of the game list page (the list itself is drawn by GuiGameBrowser)
#define MENU_LIST_X			20
#define MENU_LIST_Y			107
#define MENU_LIST_W			330
#define MENU_LIST_H			249
#define MENU_PREVIEW_X		364
#define MENU_PREVIEW_W		248
#define MENU_PREVIEW_H		MENU_LIST_H

// option browser
#define MENU_OPTIONS_W		552
#define MENU_OPTIONS_H		MENU_LIST_H

//!Blends a toward b (t = 0 gives a, t = 1 gives b); alpha is kept from a
static inline PixelColor MenuMix(PixelColor a, PixelColor b, float t)
{
	PixelColor c = a;
	c.r = (uint8_t)(a.r + (b.r - a.r) * t + 0.5f);
	c.g = (uint8_t)(a.g + (b.g - a.g) * t + 0.5f);
	c.b = (uint8_t)(a.b + (b.b - a.b) * t + 0.5f);
	return c;
}

//!The highlighted look shared by the button styles: a light, blue-tinged face, an accent outline
//!and a pale glow that reads on the chrome as well as on the page. Labels stay dark on it.
static inline GuiPaint MenuButtonLitPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surfaceRaised, p.accentTint, 0.9f))
		.stroke(3.0f, p.accent)
		.glow(14.0f, p.glow, 1.0f);
}

//!The pressed look shared by the button styles
static inline GuiPaint MenuButtonPressedPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(MenuMix(p.accentTint, p.outlineStrong, 0.25f), MenuMix(p.accentTint, p.outlineStrong, 0.5f))
		.stroke(2.0f, p.chromeDark)
		.shadow(4.0f, MenuRGBA(0, 0, 0, 80), 0.0f, 1.0f);
}

//!Resting, highlighted and pressed looks shared by the long and large buttons.
//!Highlight: warmer face, accent outline and a pale glow that reads on the chrome
//!as well as on the page.
static inline GuiShapeStyle MenuButtonStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();

	GuiPaint rest = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surface, p.outline, 0.45f))
		.stroke(2.0f, p.chromeDark)
		.shadow(8.0f, MenuRGBA(0, 0, 0, 100), 0.0f, 2.0f);

	return GuiShapeStyle(rest).onFocus(MenuButtonLitPaint()).onPress(MenuButtonPressedPaint());
}

//!The header bar: light at the top darkening downward, a fine light edge and a soft
//!shadow onto the page below.
static inline GuiPaint MenuBarTopPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.chromeLight, p.chromeDark)
		.stroke(1.5f, p.chromeEdge)
		.shadow(18.0f, MenuRGBA(0, 0, 0, 150), 0.0f, 4.0f);
}

//!The footer bar: same chrome, shadow cast upward.
static inline GuiPaint MenuBarBottomPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(MenuMix(p.chromeLight, p.chromeDark, 0.15f), MenuMix(p.chromeDark, p.chromeLight, 0.05f))
		.stroke(1.5f, p.chromeEdge)
		.shadow(18.0f, MenuRGBA(0, 0, 0, 150), 0.0f, -4.0f);
}

//!The page behind everything: a quiet vertical gradient. (It replaces the striped
//!flat color; stripes beat against the bars' shadows.)
static inline GuiPaint MenuBackgroundPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().gradient(p.backgroundTop, p.backgroundBottom);
}

//!The body of a dialog or the credits window: a light card with a firm chrome outline
//!and a deep shadow that lifts it off the page.
static inline GuiPaint MenuDialogBodyPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.surfaceRaised, p.surface)
		.stroke(2.0f, p.chromeDark)
		.shadow(26.0f, MenuRGBA(0, 0, 0, 150), 0.0f, 8.0f);
}

//!The title band across the top of a dialog (the title text on it is white)
static inline GuiPaint MenuDialogHeaderPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.chromeLight, p.chromeDark)
		.stroke(2.0f, p.chromeDark);
}

//!The empty part of a progress bar
static inline GuiPaint MenuProgressTrackPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().color(p.outline).stroke(1.5f, p.outlineStrong);
}

//!The filled part of a progress bar
static inline GuiPaint MenuProgressFillPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().gradient(MenuMix(p.accent, MenuRGBA(255, 255, 255), 0.3f), p.accent);
}

//!One dot of the busy spinner. level 0 is the head (bold, accent colored); higher
//!levels trail off smaller and paler.
static inline GuiPaint MenuSpinnerDotPaint(int level)
{
	const MenuPalette& p = MenuPaletteCurrent();
	const float t = (float)level / (float)(MENU_SPINNER_DOTS - 1);
	return GuiPaint().color(MenuMix(p.accent, p.outline, t));
}

//!Diameter of the spinner dot at a trail level
static inline float MenuSpinnerDotSize(int level)
{
	return 14.0f - 6.0f * (float)level / (float)(MENU_SPINNER_DOTS - 1);
}

//!Resting and lit colors of the page-turning arrows (gray, turning to the accent)
static inline PixelColor MenuArrowRest() { return MenuPaletteCurrent().outlineStrong; }
static inline PixelColor MenuArrowLit() { return MenuPaletteCurrent().accent; }

// ---- the controller screens' looks (shared with the DOSBox Wii menu; they follow the palette)

enum {
	UI_FONT_TITLE = 26,
	UI_FONT_HEADING = 20,
	UI_FONT_BODY = 16,
	UI_FONT_SMALL = 13
};

//!The one button look for the controller screens (and the bars): white and outlined at rest like the home
//!screen's, and the same light blue-tinged face, accent outline and glow as every other button when highlighted
//!(the label stays dark; see GuiTextButton::litTextColor).
//!`marked` outlines a resting button in the accent color: the choice a control has now.
static inline GuiShapeStyle MenuBarButtonStyle(bool marked = false)
{
	const MenuPalette& p = MenuPaletteCurrent();

	GuiPaint rest = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surface, p.outline, 0.45f))
		.stroke(marked ? 3.5f : 2.0f, marked ? p.accent : p.chromeDark)
		.shadow(6.0f, MenuRGBA(0, 0, 0, 110), 0.0f, 2.0f);

	return GuiShapeStyle(rest).onFocus(MenuButtonLitPaint()).onPress(MenuButtonPressedPaint());
}

//!The one card behind a page's content: opaque, so the game picture never shows through the controls
static inline GuiShapeStyle MenuPageCardStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint()
		.gradient(p.surfaceRaised, p.surface)
		.stroke(1.5f, p.outline)
		.shadow(14.0f, MenuRGBA(0, 0, 0, 140), 0.0f, 4.0f));
}

//!A popover: one flat light panel with a fine outline and a soft shadow, lifting it off the dimmed screen under it
static inline GuiShapeStyle MenuPopoverStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint()
		.color(p.surfaceRaised)
		.stroke(1.5f, p.outlineStrong)
		.shadow(22.0f, MenuRGBA(0, 0, 0, 120), 0.0f, 6.0f));
}

//!A key on the key picker: a flat, light cap with a fine edge and a hint of a shadow, so a grid of them reads as a keyboard.
//!The key in use has a darker edge; the one under the cursor takes the row highlight.
static inline GuiShapeStyle MenuKeycapStyle(bool inUse = false)
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint rest = GuiPaint().color(p.surface).stroke(inUse ? 2.0f : 1.0f, inUse ? p.accent : p.outline)
		.shadow(3.0f, MenuRGBA(0, 0, 0, 60), 0.0f, 1.5f);
	GuiPaint lit = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.14f)).stroke(2.0f, p.mark);
	GuiPaint pressed = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.30f)).stroke(2.0f, p.mark);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!A flat tinted area that groups things on a card (a player, a section of buttons)
static inline GuiShapeStyle MenuSectionStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint().color(MenuMix(p.surface, p.outline, 0.5f)));
}

//!A light tile on a section that can be selected (outline when highlighted)
static inline GuiShapeStyle MenuTileStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint rest = GuiPaint().color(p.surfaceRaised).stroke(1.5f, p.outline);
	GuiPaint lit = GuiPaint().color(p.surfaceRaised).stroke(3.0f, p.accent).glow(8.0f, p.glow, 0.6f);
	GuiPaint pressed = GuiPaint().color(MenuMix(p.accentTint, p.surfaceRaised, 0.5f)).stroke(3.0f, p.accent);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!A selectable row on a section: no look at rest; highlighted, a warm tint with a slim outline in the same hue.
//!The fill carries the selection (the outline alone read as a hairline box crowding its neighbours), so rows
//!need a few pixels of air between them rather than a thick border.
static inline GuiShapeStyle MenuRowHighlightStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint lit = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.14f)).stroke(1.5f, MenuMix(p.surfaceRaised, p.mark, 0.8f));
	GuiPaint pressed = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.30f)).stroke(1.5f, p.mark);
	return GuiShapeStyle(GuiPaint()).onFocus(lit).onPress(pressed);
}

//!The capsule a toggle pill's segments sit in (MenuToggle): a white, finely outlined track
static inline GuiShapeStyle MenuToggleCapsuleStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint().color(p.surfaceRaised).stroke(1.5f, p.outlineStrong).shadow(4.0f, MenuRGBA(0, 0, 0, 70), 0.0f, 1.5f));
}

//!One segment of a toggle pill: the one that is on has a dark face (its label is white); the others are bare until the cursor
//!is on them, when they take the row highlight. A segment that is on keeps its dark face under the cursor, with an accent outline.
static inline GuiShapeStyle MenuToggleSegmentStyle(bool on)
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiShapeStyle s = MenuRowHighlightStyle();
	if(on)
	{
		s.normal = GuiPaint().gradient(p.chromeLight, p.chromeDark);
		s.focused = GuiPaint().gradient(p.chromeLight, p.chromeDark).stroke(2.5f, p.accent).glow(10.0f, p.glow, 1.0f);
		s.pressed = GuiPaint().gradient(p.chromeDark, p.chromeDark).stroke(2.5f, p.accent);
	}
	return s;
}

//!A chip that opens a popover (Preset, In DOS, Setup): a flat light face with a fine edge, the row highlight when the cursor is on it
static inline GuiShapeStyle MenuChipStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiShapeStyle s = MenuRowHighlightStyle();
	s.normal = GuiPaint().color(p.surfaceRaised).stroke(1.0f, p.outline).shadow(4.0f, MenuRGBA(0, 0, 0, 70), 0.0f, 1.5f);
	return s;
}

//!The kind (or tab) that is showing in a popover: a flat light-gray fill at rest, the same highlight as any row when the cursor is on it
static inline GuiShapeStyle MenuPopoverShownStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiShapeStyle s = MenuRowHighlightStyle();
	s.normal = GuiPaint().color(p.accentTint);
	return s;
}

//!A device tile on the setup screen's rail. Light at every state so the dark icon always reads on it:
//!the device being drawn has a tinted face and a dark outline; a highlighted tile gets the orange of a
//!highlighted row (no glow, so it never spills onto the tile beside it).
static inline GuiShapeStyle MenuRailTileStyle(bool showing)
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint rest = showing ?
		GuiPaint().color(p.surfaceRaised).stroke(2.0f, p.chromeDark) :
		GuiPaint().color(p.surface).stroke(1.5f, p.outline);
	GuiPaint lit = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.14f)).stroke(2.0f, p.mark);
	GuiPaint pressed = GuiPaint().color(MenuMix(p.surfaceRaised, p.mark, 0.30f)).stroke(2.0f, p.mark);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!A small round-ish button on the bars (the player stepper): the shared bar look, but the highlight is a
//!dark face with a slim white outline and no glow, so it never touches the button or the title next to it.
static inline GuiShapeStyle MenuStepperStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint rest = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surface, p.outline, 0.45f))
		.stroke(1.5f, p.chromeDark)
		.shadow(4.0f, MenuRGBA(0, 0, 0, 110), 0.0f, 2.0f);
	GuiPaint lit = GuiPaint().gradient(p.accent, p.accentDark).stroke(2.0f, p.surfaceRaised);
	GuiPaint pressed = GuiPaint().color(p.accentDark).stroke(2.0f, p.outlineStrong);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!Arrow colors for a button in the bar look: dark on the white resting face, white on the dark highlighted one
static inline PixelColor MenuBarArrowRest() { return MenuRGBA(56, 56, 56); }
static inline PixelColor MenuBarArrowLit() { return MenuRGBA(255, 255, 255); }

//!Text on the chrome bars (the page title and the hint line)
static inline PixelColor MenuChromeText() { return MenuRGBA(255, 255, 255); }
static inline PixelColor MenuChromeTextMuted() { return MenuRGBA(206, 206, 206); }
