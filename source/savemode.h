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
 *      turned the console off. Leaving the game writes the Auto Snapshot (the
 *      game's slot 0), and opening the game loads it. If there is no Auto
 *      Snapshot, or it can't be loaded, the game's Game Save File is loaded
 *      instead.
 *  Game Save File - Like a real console: the game's own save is kept for you,
 *      and the game starts fresh each time. Leaving the game writes the Game
 *      Save File, and opening the game loads it.
 *  Manual - Nothing is saved or loaded unless you do it yourself.
 ***************************************************************************/

#ifndef _SAVEMODE_H_
#define _SAVEMODE_H_

// Called when a game has just been loaded (cold booted): applies the Save Mode
void AutoLoadOnGameOpen();

// Called when the app quits, or the OS is about to take it away (Wii U): the
// last chance to reach storage. Synchronous. screenshotIsCurrent is false when
// the game is still running, so the last screenshot taken is out of date.
void AutoSaveOnQuit(bool screenshotIsCurrent);

// Whether the emulated machine has changed since the Auto Snapshot was written
// (it ran, or a Snapshot or Game Save File was loaded, or it was reset from the
// menu). Quitting from the menu only has anything to save if it has.
void MarkLiveStateChanged();
void ClearLiveStateChanged();
bool LiveStateChanged();

#endif
