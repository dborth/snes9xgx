/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

// Asynchronous and silent: returns as soon as the settings are serialized and
// queued for the background writer.
bool SavePrefs();
bool SavePrefsAndWait();
void FixInvalidSettings();
void DefaultSettings();
void ApplySettings();
bool LoadPrefs();
void CreateMissingDirectories();
