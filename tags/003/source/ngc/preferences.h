/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * crunchy2 April 2007
 *
 * preferences.cpp
 *
 * Preferences save/load preferences utilities
 ****************************************************************************/

#define PREFS_FILE_NAME "snes9xGx.prf"

int preparePrefsData ();
void decodePrefsData ();

void quickLoadPrefs (bool8 silent);
void quickSavePrefs (bool8 silent);
