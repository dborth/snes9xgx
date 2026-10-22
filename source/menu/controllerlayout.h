/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerlayout.h
 *
 * A controller described as data: a silhouette made of rounded pieces and a
 * table of controls (buttons, sticks, triggers), each with a position, shape
 * and id. ControllerView draws any layout, lights controls up and highlights
 * them; supporting another controller means adding a table, not drawing code.
 * Control ids are the INPUT_BTN_* / INPUT_TRIGGER_* / INPUT_THUMB_* bits from
 * InputData.h, so an input state maps straight onto what lights up.
 ***************************************************************************/
#pragma once

#include <cstddef>
#include <vector>
#include "drivers/InputData.h"
#include "libgui/GuiGeometry.h"

//!A control with no input bit of its own on any supported hardware (the Switch Pro / Wii U capture button).
enum { CTL_CAPTURE = (1 << 24) };

//!How a control is drawn.
enum class CTL_SHAPE {
	CIRCLE,		//!< A round button
	ROUNDRECT,	//!< A rounded rectangle: bumpers, triggers, D-pad arms, small square buttons
	STICK,		//!< An analog stick: an outer ring with a cap that can move. The ring is an ellipse, or the spot's path (a hexagonal or octagonal gate)
	PATH		//!< Any outline, given as the spot's path (a D-pad, a trigger cut to the body's edge, a Nunchuk button)
};

//!Flags for ControlSpot::flags.
enum {
	CTL_GHOST = 1	//!< Drawn only when pressed or highlighted: the control's resting look is part of a shape drawn elsewhere (a D-pad's arms inside its cross)
};

//!One control on a controller drawing. Coordinates are in the layout's own units.
struct ControlSpot {
	int id;				/*!< Input bit this control stands for; 0 for decoration (never lit or highlighted). */
	CTL_SHAPE shape;
	float x, y, w, h;	/*!< Bounding box. */
	float radius;		/*!< Corner radius for ROUNDRECT. For STICK, the diameter of the cap when it is above 0 (the default is 0.56 of the ring, with a second ring drawn inside). */
	const char * label;	/*!< Text drawn on the control ("A", "ZL", "+"), or null. */
	GuiPath path;		/*!< The outline of a PATH spot, or a STICK's ring if not empty. x, y, w and h still give its bounding box. */
	unsigned flags;		/*!< CTL_* flags. */
};

//!One rounded rectangle of the silhouette (the pieces are merged into one outline).
struct BodyPiece {
	float x, y, w, h;	/*!< Rectangle before rotation. */
	float radius;
	float angle;		/*!< Rotation about its center, degrees clockwise. */
};

//!A controller: its drawing size, silhouette and controls.
struct ControllerLayout {
	float width, height;
	std::vector<BodyPiece> body;
	std::vector<GuiPath> bodyPaths;	//!< Outlines that are not rounded rectangles; merged into the silhouette with the pieces
	std::vector<ControlSpot> spots;

	//!Finds a control by id (null if the layout has none)
	const ControlSpot * find(int id) const;
};

//!Control ids match universal input: a Wii U Pro, GamePad or Classic has the Pro set; the GameCube pad's Start is
//!INPUT_BTN_PLUS and its Z is INPUT_TRIGGER_ZR; the Nunchuk's C is INPUT_TRIGGER_L and its Z is
//!INPUT_TRIGGER_ZL; a stick spot's id is its click bit (INPUT_THUMB_L / INPUT_THUMB_R) whether or not the
//!hardware has a click. A D-pad is one drawn cross plus four ghost arms (INPUT_BTN_UP ...) that light up.
const ControllerLayout& WiiUProControllerLayout();
const ControllerLayout& GamePadControllerLayout();		//!< Wii U GamePad
const ControllerLayout& GameCubeControllerLayout();
const ControllerLayout& ClassicControllerLayout();
const ControllerLayout& WiimoteControllerLayout();		//!< Held upright (pointer style)
const ControllerLayout& WiimoteSidewaysControllerLayout();	//!< Held sideways (D-pad on the left)
const ControllerLayout& WiimoteNunchukControllerLayout();	//!< Nunchuk on the left, Wiimote on the right
const ControllerLayout& DS4ControllerLayout();			//!< DualShock 4
const ControllerLayout& Xbox360ControllerLayout();		//!< Xbox 360 pad
const ControllerLayout& UsbControllerLayout();			//!< Generic USB gamepad
