/****************************************************************************
 * Snes9x GX - Favorites
 *
 * favorites.h
 *
 * Mark ROMs as favorites and browse them in a virtual "Favorites" folder
 * that shows up next to SD / USB / ... on the device listing screen.
 * The list is stored in favorites.txt, next to settings.xml.
 ***************************************************************************/

#ifndef _FAVORITES_H_
#define _FAVORITES_H_

#include <stddef.h>

// Fake "directory" stored in browser.dir while the favorites list is shown.
// The same string is the filename of the entry on the device listing.
#define FAVORITES_DIR  "favorites:/"
#define FAVORITES_FILE "favorites.txt"   // stored in the preferences folder (where settings.xml is)

enum
{
	FAVORITE_REMOVED    = 0,
	FAVORITE_ADDED      = 1,
	FAVORITE_NOT_USABLE = -1, // selected entry can't be a favorite (folder, 7z content, ...)
	FAVORITE_LIST_FULL  = -2,
	FAVORITE_SAVE_ERROR = -3, // only used by the menu code when SaveFavoritesTask fails
	FAVORITE_NO_LOCATION = -4 // favorites.txt can't be reached (device missing, settings location unknown)
};

bool InFavoritesView();

// ParseDirEntries(): is dir + filename ("sd:/roms/" + "Game.smc") in the list? Thread-safe.
// The folder reader stores the answer in BROWSERENTRY.favorite, once per file.
bool IsFavoriteFile(const char * dir, const char * filename);

// --- filebrowser.cpp hooks -------------------------------------------------
// Top of BrowserChangeFolder(): returns true if the click was handled (enter /
// leave the favorites view, open a 7z from it and come back); *result is then
// BrowserChangeFolder()'s return value.
bool FavoritesChangeFolder(int * result);
// Top of BrowserLoadFile() while InFavoritesView() is true.
int BrowserLoadFavorite();
// AddDeviceListing(): adds the "Favorites" entry at browserList[index]. Returns 0 or 1.
int AddFavoritesListingEntry(int index);

// --- menu.cpp hooks --------------------------------------------------------
// Toggles the favorite state of the selected browser entry (list in memory only,
// nothing is written to disk). Returns a FAVORITE_* code. After FAVORITE_ADDED /
// FAVORITE_REMOVED call SaveFavoritesTask() - preferably on the worker thread.
int ToggleFavoriteOnSelection();

// Worker-thread tasks (BgTaskFn signature): load the list from / save it to the
// save device. Both return 1 on success.
int FavoritesLoadTask(void * arg);
int SaveFavoritesTask(void * arg);

#endif
