/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * savemode.h
 *
 * Save Mode: what is saved and loaded without being asked.
 *
 *  Snapshot (default) - Picks up exactly where you left off, as if you never
 *      turned the console off. Opening the game loads the Auto Snapshot (the
 *      game's slot 0). If there is no Auto Snapshot, or it can't be loaded, the
 *      game's Cartridge Save is loaded instead.
 *  Cartridge Save - Like a real console: the game's own save is kept for you,
 *      and the game starts fresh each time. Opening the game loads it.
 *  Manual - Nothing is saved or loaded unless you do it yourself.
 *
 * When the autosave (the Auto Snapshot, or the Cartridge Save) is written:
 * when the game is left for good - quitting it to the game list, quitting the
 * app, powering off, or choosing another game - or when the Saves screen is
 * asked to update it. Returning to the menu writes nothing, with one exception:
 * the first time, if the game has no Auto Snapshot yet, so the Saves screen has
 * something to show. Otherwise someone who dies, goes to the menu and loads the
 * Auto Snapshot wants the place they started from, not the place they died.
 ***************************************************************************/

#ifndef _SAVEMODE_H_
#define _SAVEMODE_H_

// Called when a game has just been loaded (cold booted): applies the Save Mode
void AutoLoadOnGameOpen();

// Called when the app quits, or the OS is about to take it away (Wii U): the
// last chance to reach storage. Synchronous. screenshotIsCurrent is false when
// the game is still running, so the last screenshot taken is out of date.
void AutoSaveOnQuit(bool screenshotIsCurrent);

// Whether the emulated machine has changed since the autosave of the current Save
// Mode was written: it ran, or a Snapshot or Cartridge Save was loaded, or it was
// reset from the menu. Leaving the game for good only has anything to save if it has.
void MarkLiveStateChanged();
void ClearLiveStateChanged();
bool LiveStateChanged();

// The Auto Snapshot / the Cartridge Save has just been written (by leaving the
// game, or on request from the Saves screen): it is up to date again
void AutoSnapshotWritten();
void CartridgeSaveWritten();

// Called when the game has just been left for the menu (the screenshot has been
// taken): in Snapshot mode, writes the Auto Snapshot - and its image - if the
// game doesn't have one yet. Does nothing once there is one, so later returns to
// the menu write nothing. The write itself is queued in the background.
void AutoSnapshotOnFirstMenuReturn();

// The screenshot of the game, taken when the menu was entered, still shows what
// the emulated machine is doing: nothing has been loaded or reset since. A
// Snapshot written from the menu uses it only then.
void MarkScreenshotCurrent();
bool ScreenshotIsCurrent();

#endif
