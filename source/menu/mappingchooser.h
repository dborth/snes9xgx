/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * mappingchooser.h
 *
 * Asks what a row of the mapping screen should use, in a popover beside the
 * row. It opens waiting for a button to be pressed on the hardware being
 * mapped; the other kinds are a list of the hardware's buttons and none.
 * Whatever is chosen is a value for  btnmap[][][]: an INPUT_* button, or 0.
 *
 ***************************************************************************/

#pragma once

#include <stdint.h>
#include "libgui/Gui.h"

/**
 * Runs the chooser over the screen that is showing (which the caller has disabled and which stays drawn) until a
 * choice is made or it is closed.
 * @param rowName What is being set ("Select")
 * @param hw The hardware being mapped (an INPUT_HW_* value)
 * @param now What the row uses now (0 for none)
 * @param defaultButton What the row uses by default (0 for none)
 * @param anchor The row it opens beside
 * @param chosen The choice, when true is returned
 * @return true if a choice was made; false if it was closed or the application is exiting
 */
bool MappingChoose(const char * rowName, int hw, uint32_t now, uint32_t defaultButton, const GuiPopover::Anchor& anchor, uint32_t & chosen);
