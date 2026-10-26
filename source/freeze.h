/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * freeze.h
 ***************************************************************************/

#ifndef _FREEZE_H_
#define _FREEZE_H_

int SaveSnapshot (char * filepath, bool silent);
int LoadSnapshot (char * filepath, bool silent);
// \return 1 if loaded, 0 if there is no Auto Snapshot, -1 if it exists but couldn't be loaded
int LoadSnapshotAuto (bool silent);
int SavePreviewImg (char * filepath, bool silent);

// The snapshot this session's game was started from, or last saved to or loaded from:
// the saves screen points at it, and offers to update it. Empty when there is none.
// ClearPlayingFrom() is for when another game is loaded.
const char * GetPlayingFrom ();
void ClearPlayingFrom ();

// Deferred auto-save: SnapshotStateAuto() captures the state (and screenshot) and
// its destination right now (call it from the thread that owns the emulator, while
// the game is still loaded); WriteStateSnapshot() does the device I/O later, on any
// thread. SnapshotStateAuto() returns nullptr if nothing could be captured. The
// snapshot must be freed with FreeStateSnapshot(), which accepts nullptr.
struct StateSnapshot;
StateSnapshot * SnapshotStateAuto (bool withScreenshot = true);
bool WriteStateSnapshot (StateSnapshot * snapshot, bool silent);
void FreeStateSnapshot (StateSnapshot * snapshot);
#endif
