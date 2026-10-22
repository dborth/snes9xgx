/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * mappingui.h
 *
 * The button mapping screen. One screen replaces the three that came before it
 * (the emulated device, the hardware, the list of buttons): two chips in the
 * header choose the device being emulated and the hardware being mapped, the
 * rows list what each SNES button uses, and a drawing of the hardware shows the
 * control the highlighted row is mapped to.
 ***************************************************************************/

#pragma once

/**
 * Runs the mapping screen on the menu's main window until the user leaves it.
 * Returns the menu to go to next (a MENU_* value from menu.h).
 */
int MappingScreen();
