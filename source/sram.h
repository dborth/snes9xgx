/****************************************************************************
 * Snes9x GX
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ***************************************************************************/

bool SaveSRAM (char * filepath, bool silent);
bool SaveSRAMAuto (bool silent);
bool LoadSRAM (char * filepath, bool silent);
bool LoadSRAMAuto (bool silent);
