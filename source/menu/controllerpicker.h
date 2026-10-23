/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerpicker.h
 *
 * The controller (the device plugged into the emulated console) chosen from a
 * popover beside the chip on the game menu.
 ***************************************************************************/
#pragma once

#include <string>
#include "libgui/Gui.h"

/** The name of the controller in use now (EmuSettings.controller), translated: what the chip shows. */
std::string ControllerChipValue();

/**
 * Runs the controller popover beside `anchor`, over the screen that is showing (which the caller has disabled), until
 * a choice is made or it is closed. Sets EmuSettings.controller. True if a controller was chosen.
 */
bool ChooseController(const GuiPopover::Anchor& anchor);
