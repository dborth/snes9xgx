/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * mappingmodel.h
 *
 * What the button mapping screen shows, as data: for each emulated device the
 * rows it lists (grouped, in the order of btnmap's indexes), which controller
 * drawing goes with each piece of hardware, and the name of the button a row
 * is mapped to. Nothing here draws or reads input, and nothing here changes
 * how mappings are stored: a row's `index` is its index in
 * btnmap[device][hardware][].
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include <vector>
#include "controllerlayout.h"

//!The icon at the start of a row (the SNES button it stands for)
enum class MAP_ICON {
	NONE,
	A, B, X, Y,
	L, R,
	START, SELECT,
	UP, DOWN, LEFT, RIGHT
};

//!One mappable button of an emulated device
struct MapRow {
	const char * name;		//!< "Select", "Aim Offscreen"
	int index;				//!< Index into btnmap[device][hardware][]
	MAP_ICON icon;
};

//!A titled run of rows
struct MapGroup {
	const char * title;		//!< "D-PAD"
	std::vector<MapRow> rows;
};

//!The rows of one emulated device, split into the column left of the controller drawing and the one right of it
struct MapProfile {
	std::vector<MapGroup> left;
	std::vector<MapGroup> right;
	int rowCount() const;
};

//!The rows for an emulated device (CTRL_PAD, CTRL_SCOPE, CTRL_MOUSE or CTRL_JUST); empty for anything else
const MapProfile& MappingProfile(int snesCtrl);

//!The drawing that goes with a piece of hardware (an INPUT_HW_* value). `sideways` is the Wiimote held on its side.
const ControllerLayout& MappingLayout(int hw, bool sideways);

//!How big to draw a layout to fit in `maxW` x `maxH` design pixels
float MappingScale(const ControllerLayout& layout, float maxW, float maxH);

//!The name of a button on a piece of hardware ("START", "A"), or null if it has none by that name (including 0, no button)
const char * MappingButtonName(int hw, uint32_t btn);

//!The control on a drawing that a button is: its id, or 0 if the drawing has no such control
int MappingSpotFor(const ControllerLayout& layout, uint32_t btn);
