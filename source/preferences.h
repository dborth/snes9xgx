/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

void FixInvalidSettings();
void DefaultSettings();
void ApplySettings();
bool SavePrefs();
bool LoadPrefs();
void CreateMissingDirectories();
