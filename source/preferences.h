/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <stddef.h>

// Asynchronous and silent: returns as soon as the settings are serialized and
// queued for the background writer.
bool SavePrefs();
bool SavePrefsAndWait();
void FixInvalidSettings();
void DefaultSettings();
void ApplySettings();
bool LoadPrefs();
void CreateMissingDirectories();

// Folder that holds settings.xml (false if not known yet). Files that belong next
// to the settings use it too, eg. favorites.txt.
bool GetPrefsFolder(char * folder, size_t folderSize);
// Rewrites a stored usb:/ usb2:/ usb3:/ path to the USB slot the same file is in now
void RemapUsbPathIfNeeded(char * path, size_t pathSize);
