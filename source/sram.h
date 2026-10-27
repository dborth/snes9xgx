/****************************************************************************
 * Snes9x GX
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * sram.cpp
 *
 * Cartridge Save (battery-backed SRAM) save/load handling
 ***************************************************************************/

bool SaveSRAM (char * filepath, bool silent);
bool SaveSRAMAuto (bool silent);
bool LoadSRAM (char * filepath, bool silent);
bool LoadSRAMAuto (bool silent);

// Size in bytes of the Cartridge Save this game saves: 0 when it has none (no battery)
int GetSRAMSaveSize ();

// Deferred auto-save: SnapshotSRAMAuto() copies the Cartridge Save and its destination right
// now (call it from the thread that owns the emulator, while the game is still
// loaded); WriteSRAMSnapshot() does the device I/O later, on any thread.
// SnapshotSRAMAuto() returns nullptr if there is nothing to save. The snapshot
// must be freed with FreeSRAMSnapshot(), which accepts nullptr.
struct SRAMSnapshot;
SRAMSnapshot * SnapshotSRAMAuto ();
bool WriteSRAMSnapshot (SRAMSnapshot * snapshot, bool silent);
void FreeSRAMSnapshot (SRAMSnapshot * snapshot);
