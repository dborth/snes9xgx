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

int preparePrefsData ();
bool decodePrefsData ();
bool SavePrefs (int method, bool silent);
bool LoadPrefs (int method, bool silent);
bool quickLoadPrefs (bool8 silent);
bool quickSavePrefs (bool8 silent);
