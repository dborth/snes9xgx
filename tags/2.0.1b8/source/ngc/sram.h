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

void quickLoadSRAM (bool8 silent);
void quickSaveSRAM (bool8 silent);
