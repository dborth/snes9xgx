/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTheme.h
 *
 * One place for the colors and metrics a code-drawn UI shares, and ready
 * made styles built from them (card, panel, button, chip). Platform-agnostic.
 * Metrics are design-canvas pixels.
 *
 * The core defines no look of its own: every value comes from the
 * application's GuiThemeInit() (see below).
 ***************************************************************************/
#pragma once
#include "GuiPaint.h"

//!Colors, metrics and the style recipes built from them. Change a value on
//!GuiTheme::current() and every shape styled afterwards follows. The values
//!are set by the application's GuiThemeInit(); a GuiTheme made any other way
//!starts with every field zero.
//!\ingroup grp_shapes
struct GuiTheme {
	// colors
	PixelColor background;		/*!< Screen background. */
	PixelColor surface;			/*!< Panels and resting cards. */
	PixelColor surfaceRaised;	/*!< Lighter end of a card's gradient, chips. */
	PixelColor outline;			/*!< Quiet outlines. */
	PixelColor outlineStrong;	/*!< Resting button outline and fill. */
	PixelColor accent;			/*!< Highlight: focus outline, focused button. */
	PixelColor accentLight;		/*!< Light end of the focused-button gradient. */
	PixelColor accentDark;		/*!< Pressed-button fill. */
	PixelColor text;			/*!< Primary text. */
	PixelColor textMuted;		/*!< Secondary text. */
	PixelColor textOnAccent;	/*!< Text drawn over the accent color. */
	PixelColor textInverse;		/*!< Ink drawn over a text-colored fill (the letter or symbol on a glyph badge). */
	PixelColor good;			/*!< Positive state (connected, charged). */
	PixelColor highlight;		/*!< Selected row or list item (a light tint on a light theme). */
	PixelColor mark;			/*!< What is being edited or pointed at (a control on a controller picture and the line to it). */
	PixelColor shadow;			/*!< Drop shadow of a resting card; its alpha is the peak opacity. */

	// metrics
	float radius;				/*!< Corner radius of cards and panels. */
	float strokeWidth;			/*!< Outline width. */
	float padding;				/*!< Space inside a card. */
	float spacing;				/*!< Gap between siblings. */
	float shadowBlur;			/*!< Softness of a resting card's shadow. */

	// glyphs (GuiGlyph)
	float glyphSize;			/*!< Width and height of a round glyph (face button, D-pad arrow, stick, +, -, home). */
	float glyphBumperWidth;		/*!< Width of a bumper glyph (L, R, ZL, ZR). */
	float glyphBumperHeight;	/*!< Height of a bumper glyph. */
	int glyphFontSize;			/*!< Font size of a one-character letter on a glyph, in pixels. */
	int glyphFontSizeSmall;		/*!< Font size of a longer letter on a glyph ("ZL", "ZR"). */

	// style recipe tuning: the numbers the recipes below use with the colors above
	float shadowOffsetY;		/*!< How far a resting card's shadow falls. */
	float glowGrow;				/*!< How much wider (blur and spread) a focused card's glow is than its shadow. */
	uint8_t focusGlowAlpha;		/*!< Peak opacity of a focused card's glow. */
	uint8_t pressGlowAlpha;		/*!< Peak opacity of a pressed card's glow. */
	float buttonGlowBlur;		/*!< Softness of a focused button's glow. */
	uint8_t buttonGlowAlpha;	/*!< Peak opacity of a focused button's glow. */
	uint8_t rowPressAlpha;		/*!< Opacity of a pressed list row's accent tint. */
	uint8_t cardDisabledAlpha;	/*!< Opacity of a disabled card. */
	uint8_t buttonDisabledAlpha;/*!< Opacity of a disabled button. */
	float buttonStrokeLight;	/*!< How far (0..1) a focused or pressed button's outline is lightened toward white. */
	float barDarken;			/*!< How far (0..1) a bar's fill is darkened from the background toward black. */

	//!Every field zero. Nothing here is a look: GuiThemeInit() provides it.
	GuiTheme();

	//!The theme the rest of the library reads. The first call fills it in with GuiThemeInit().
	static GuiTheme& current();

	//!A quiet container: flat surface with a thin outline
	GuiShapeStyle panelStyle() const;
	//!A raised card: gradient surface, outline, drop shadow; a glowing accent outline when focused
	GuiShapeStyle cardStyle() const;
	//!A capsule or rounded button: lights up in the accent color when focused, darkens when pressed
	GuiShapeStyle buttonStyle() const;
	//!A small label background (a "chip")
	GuiShapeStyle chipStyle() const;
	//!A flat list row: invisible at rest, a gray bar when focused, tinted when pressed
	GuiShapeStyle rowStyle() const;
	//!A dark strip for a hint or status bar
	GuiShapeStyle barStyle() const;
};

//!Fills in every color and metric of the theme. **Defined by the application**,
//!like main(): the core has no default look, so a program that does not define
//!it fails to link. Called once, the first time GuiTheme::current() is used; it
//!must not call GuiTheme::current() itself. The libgui template defines it in
//!source/theme.cpp.
//!\ingroup grp_shapes
void GuiThemeInit(GuiTheme& theme);
