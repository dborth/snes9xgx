/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiLayoutSolver.h
 *
 * Platform-agnostic layout math: given the natural sizes of some items and a
 * few rules, work out where each one goes. The layout containers (GuiRow,
 * GuiColumn, GuiGrid, GuiOverlay) are thin adapters that feed GuiElements to
 * these functions and apply the answers. Everything here is plain numbers,
 * so it is tested on a host machine.
 *
 * Coordinates are relative to the container's top-left corner.
 ***************************************************************************/
#pragma once
#include <vector>
#include "GuiGeometry.h"

//!How an item sits across the main direction of a stack, or inside a grid cell.
//!\ingroup grp_layout
enum class LAYOUT_ALIGN {
	START,		//!< Top or left
	CENTRE,		//!< Centered
	END,		//!< Bottom or right
	STRETCH		//!< Fills the available space
};

//!How leftover space along a stack's main direction is spent (when no item grows).
//!\ingroup grp_layout
enum class LAYOUT_JUSTIFY {
	START,			//!< Items packed at the start
	CENTRE,			//!< Items packed in the middle
	END,			//!< Items packed at the end
	SPACE_BETWEEN	//!< First item at the start, last at the end, equal gaps between
};

//!Space around the inside edge of a container.
//!\ingroup grp_layout
struct GuiInsets {
	float left, top, right, bottom;
	GuiInsets(float all = 0.0f) : left(all), top(all), right(all), bottom(all) {}
	GuiInsets(float horizontal, float vertical) : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
	GuiInsets(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
};

//!One child of a stack, as the solver sees it.
//!\ingroup grp_layout
struct GuiStackItem {
	float w, h;				/*!< Natural size (for a gap: its fixed size along the main axis). */
	float grow;				/*!< Share of leftover main-axis space this item takes (0 = none). */
	bool useContainerAlign;	/*!< true: use the stack's alignItems; false: use `align`. */
	LAYOUT_ALIGN align;		/*!< Cross-axis alignment when not using the container's. */
	GuiStackItem() : w(0), h(0), grow(0), useContainerAlign(true), align(LAYOUT_ALIGN::START) {}
};

//!Rules for a row or column. A fixed size of 0 means "fit the content" on that axis.
//!\ingroup grp_layout
struct GuiStackSpec {
	bool vertical;			/*!< true = column, false = row. */
	GuiInsets padding;
	float spacing;			/*!< Gap between neighbouring items. */
	float fixedW, fixedH;	/*!< Container size; 0 = fit. */
	LAYOUT_JUSTIFY justify;
	LAYOUT_ALIGN alignItems;
	GuiStackSpec() : vertical(false), spacing(0), fixedW(0), fixedH(0), justify(LAYOUT_JUSTIFY::START), alignItems(LAYOUT_ALIGN::START) {}
};

//!The solver's answer: the container's size and each item's rectangle.
//!\ingroup grp_layout
struct GuiLayoutResult {
	float width, height;
	std::vector<GuiRect> rects;
	GuiLayoutResult() : width(0), height(0) {}
};

//!Lays items out along one axis.
//!Items keep their natural size unless they grow (main axis) or stretch (cross axis).
//!Space is never taken back: if the content is larger than a fixed container it overflows.
//!\ingroup grp_layout
GuiLayoutResult GuiSolveStack(const GuiStackSpec& spec, const std::vector<GuiStackItem>& items);

//!One child of a grid.
//!\ingroup grp_layout
struct GuiGridItem {
	float w, h;
	LAYOUT_ALIGN alignH, alignV;	/*!< Placement inside the cell. */
	GuiGridItem() : w(0), h(0), alignH(LAYOUT_ALIGN::START), alignV(LAYOUT_ALIGN::START) {}
};

//!Rules for a grid, filled row by row.
//!\ingroup grp_layout
struct GuiGridSpec {
	int columns;
	GuiInsets padding;
	float spacingX, spacingY;
	float fixedW, fixedH;	/*!< Container size; 0 = fit. A fixed width makes the columns equal. */
	GuiGridSpec() : columns(1), spacingX(0), spacingY(0), fixedW(0), fixedH(0) {}
};

//!Lays items out in rows. Without a fixed width, each column is as wide as its widest
//!item; with one, the columns share the width equally. Rows are as tall as their tallest item.
//!\ingroup grp_layout
GuiLayoutResult GuiSolveGrid(const GuiGridSpec& spec, const std::vector<GuiGridItem>& items);

//!One child of an overlay.
//!\ingroup grp_layout
struct GuiOverlayItem {
	float w, h;
	LAYOUT_ALIGN alignH, alignV;
	bool background;		/*!< Fills the whole box, ignores padding, and does not count toward the fit size. */
	GuiOverlayItem() : w(0), h(0), alignH(LAYOUT_ALIGN::CENTRE), alignV(LAYOUT_ALIGN::CENTRE), background(false) {}
};

//!Rules for an overlay: children share one box.
//!\ingroup grp_layout
struct GuiOverlaySpec {
	GuiInsets padding;
	float fixedW, fixedH;	/*!< Container size; 0 = fit. */
	GuiOverlaySpec() : fixedW(0), fixedH(0) {}
};

//!Stacks items on top of each other in one box. The box fits the largest non-background item.
//!\ingroup grp_layout
GuiLayoutResult GuiSolveOverlay(const GuiOverlaySpec& spec, const std::vector<GuiOverlayItem>& items);

//!Directions for moving the selection.
//!\ingroup grp_layout
enum class NAV_DIR { LEFT, RIGHT, UP, DOWN };

//!An item that selection can move between.
//!\ingroup grp_layout
struct GuiNavItem {
	GuiRect r;			/*!< On-screen rectangle. */
	bool selectable;
};

//!Picks which item the selection moves to. Chooses the nearest selectable item
//!in the direction, preferring ones that line up. Moving sideways past the end of a
//!row continues on the next (or previous) row; moving up or down stops at the edge.
//!\param items Everything on screen that could take the selection
//!\param current Index of the selected item, or -1 for none (picks the first in reading order)
//!\param dir Direction of travel
//!\return Index of the new selection, or -1 to leave it where it is
//!\ingroup grp_layout
int GuiNavigate(const std::vector<GuiNavItem>& items, int current, NAV_DIR dir);
