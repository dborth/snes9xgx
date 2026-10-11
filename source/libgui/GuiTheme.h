/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTheme.h
 *
 * One place for the colors and metrics a code-drawn UI shares, and ready
 * made styles built from them (card, panel, button, chip). Platform-agnostic.
 * Metrics are design-canvas pixels.
 ***************************************************************************/
#pragma once
#include "GuiPaint.h"

//!Colors, metrics and the style recipes built from them. Change a value on
//!GuiTheme::current() and every shape styled afterwards follows.
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
	PixelColor good;			/*!< Positive state (connected, charged). */

	// metrics
	float radius;				/*!< Corner radius of cards and panels. */
	float strokeWidth;			/*!< Outline width. */
	float padding;				/*!< Space inside a card. */
	float spacing;				/*!< Gap between siblings. */
	float shadowBlur;			/*!< Softness of a resting card's shadow. */

	//!The default dark theme
	GuiTheme();

	//!The theme the rest of the library reads. Starts as the default dark theme.
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
