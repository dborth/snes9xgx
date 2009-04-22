/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Tantric September 2008
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ***************************************************************************/

bool SaveSRAM (char * filepath, int method, bool silent);
bool SaveSRAMAuto (int method, bool silent);
bool LoadSRAM (char * filepath, int method, bool silent);
bool LoadSRAMAuto (int method, bool silent);
