/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * crunchy2 April 2007
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ****************************************************************************/

int prepareMCsavedata ();
int prepareEXPORTsavedata ();
void decodesavedata (int readsize);

bool SaveSRAM (int method, bool silent);
bool LoadSRAM (int method, bool silent);
bool quickLoadSRAM (bool silent);
bool quickSaveSRAM (bool silent);
