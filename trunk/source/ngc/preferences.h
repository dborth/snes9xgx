/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

void FixInvalidSettings();
void DefaultSettings();
bool SavePrefs (bool silent);
bool LoadPrefs ();
