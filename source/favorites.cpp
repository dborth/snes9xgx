/****************************************************************************
 * Snes9x GX - Favorites
 *
 * favorites.cpp
 *
 * Overview
 * --------
 * A favorite is a ROM's full path, e.g. "sd:/snes9xgx/roms/Contra III.smc".
 * The list is kept in a text file, one path per line, called favorites.txt.
 * It lives in the preferences folder - the folder settings.xml is in.
 *
 * Two copies of the list are kept in memory:
 *  - favorites:      the full paths. Needed for the "Favorites" folder and for
 *                    writing the file back.
 *  - favoriteHashes: a sorted array of 32-bit hashes of those paths. This is
 *                    what the file browser asks while it reads a folder, to
 *                    decide whether an entry gets a star: hash the entry's
 *                    path once, binary search. The answer is stored in
 *                    BROWSERENTRY.favorite, so nothing is hashed while drawing.
 *                    A hash collision would only show a star that should not
 *                    be there - the list itself is always compared as text.
 *
 * Threads: folders are read by the background worker while the menu (main
 * thread) can toggle a favorite. Everything that touches the two lists above
 * takes favoritesLock. No file access happens while it is held.
 *
 * Note: plain fopen() is used on purpose instead of LoadFile()/SaveFile() -
 * those call HaltParseThread(), which would cut short the background scan of
 * the folder the user is currently browsing.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>

#include "snes9xgx.h"
#include "filebrowser.h"
#include "favorites.h"
#include "fileop.h"
#include "preferences.h"
#include "menu/menu.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"
#include "drivers/Mutex.h"
#include "drivers/safestr.h"

// More than a few hundred favorites is not a favorites list any more. The cap also keeps the
// "Favorites" folder well below MAX_BROWSER_SIZE (1000 on GameCube) and the memory use small.
#define MAX_FAVORITES   300

// The file is never read past this many bytes (MAX_FAVORITES long paths fit with room to spare),
// so a huge or endless file cannot stall the menu or use up memory.
#define FAV_FILE_MAX    (128 * 1024)

static std::vector<std::string> favorites;     // full paths, in the order they were added
static std::vector<uint32_t> favoriteHashes;   // sorted hashes of the paths above
static bool favoritesLoaded = false;           // false: list not read yet, or its file could not be reached
static char favoritesPathLoaded[MAXPATHLEN] = { 0 };

// snapshot of the list that SaveFavoritesTask() writes
static std::string pendingSave;
static bool favoritesDirty = false;

// A 7z opened from the Favorites folder: "Up One Level" returns there, not to the 7z's folder
static bool szOpenedFromFavorites = false;

// Created on first use: a Mutex needs the platform, which does not exist yet while static
// objects are being constructed. The first use is EnsureLoaded(), before any other thread asks.
static Mutex & FavoritesLock()
{
	static Mutex lock;
	return lock;
}

/****************************************************************************
 * Small helpers
 ***************************************************************************/
static bool GetFavoritesPath(char * path, size_t pathSize)
{
	char folder[MAXPATHLEN];

	if(!GetPrefsFolder(folder, sizeof(folder)))
		return false; // the settings location is not known (yet)

	return FormatString(path, pathSize, "%s/%s", folder, FAVORITES_FILE);
}

// Case-folded FNV-1a (the same hash preferences.cpp uses for its change check). The folding
// matches how FAT treats names, so "GAME.SMC" and "Game.smc" are one ROM. The hash is fed in
// two parts so the browser can hash "<folder>/" and "<file name>" without joining them first.
#define FAV_HASH_START  2166136261u

static uint32_t HashPathPart(uint32_t hash, const char * s)
{
	for(; *s != 0; s++)
	{
		unsigned char c = (unsigned char)*s;
		if(c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		hash ^= c;
		hash *= 16777619u;
	}
	return hash;
}

// (caller holds favoritesLock)
static int FindFavorite(const char * fullpath)
{
	for(size_t i = 0; i < favorites.size(); i++)
	{
		if(strcasecmp(favorites[i].c_str(), fullpath) == 0)
			return (int)i;
	}
	return -1;
}

// (caller holds favoritesLock)
static void RebuildHashes()
{
	favoriteHashes.clear();
	for(size_t i = 0; i < favorites.size(); i++)
		favoriteHashes.push_back(HashPathPart(FAV_HASH_START, favorites[i].c_str()));
	std::sort(favoriteHashes.begin(), favoriteHashes.end());
}

// (caller holds favoritesLock)
static void BuildPendingSave()
{
	pendingSave.clear();
	for(size_t i = 0; i < favorites.size(); i++)
	{
		pendingSave += favorites[i];
		pendingSave += '\n';
	}
	if(pendingSave.empty())
		pendingSave = "\n"; // never write a 0-byte file
	favoritesDirty = true;
}

// "sd:/snes9xgx/roms/Game.smc" -> dir "sd:/snes9xgx/roms/", name "Game.smc"
static bool SplitPath(const char * full, char * dir, size_t dirSize, char * name, size_t nameSize)
{
	const char * slash = strrchr(full, '/');
	if(slash == NULL || slash[1] == 0)
		return false;

	size_t dirLen = (size_t)(slash - full) + 1;
	if(dirLen >= dirSize || strlen(slash + 1) >= nameSize)
		return false;

	memcpy(dir, full, dirLen);
	dir[dirLen] = 0;
	memcpy(name, slash + 1, strlen(slash + 1) + 1);
	return true;
}

// The files the game list shows (see the filter in ParseDirEntries()). A folder is
// never a favorite, and neither is anything inside a 7z - but the 7z itself is.
static bool HasRomExtension(const char * filename)
{
	const char * p = strrchr(filename, '.');
	if(p == NULL)
		return false;

	return strcasecmp(p, ".smc") == 0 || strcasecmp(p, ".sfc") == 0 ||
	       strcasecmp(p, ".fig") == 0 || strcasecmp(p, ".swc") == 0 ||
	       strcasecmp(p, ".bs")  == 0 || strcasecmp(p, ".zip") == 0 ||
	       strcasecmp(p, ".7z")  == 0;
}

static bool IsSelectionValid()
{
	return browser.numEntries > 0 && browser.selIndex >= 0 && browser.selIndex < browser.numEntries;
}

static BROWSERENTRY * FindBrowserEntry(const char * filename)
{
	for(int i = 0; i < browser.numEntries; i++)
	{
		if(strcmp(browserList[i].filename, filename) == 0)
			return &browserList[i];
	}
	return NULL;
}

/****************************************************************************
 * Is this file a favorite? Called by the folder reader (background thread)
 * for every file it adds to the game list.
 ***************************************************************************/
bool IsFavoriteFile(const char * dir, const char * filename)
{
	if(!favoritesLoaded) // (set once, after the lock exists)
		return false;

	uint32_t hash = HashPathPart(HashPathPart(FAV_HASH_START, dir), filename);

	MutexLock lock(FavoritesLock());
	return std::binary_search(favoriteHashes.begin(), favoriteHashes.end(), hash);
}

/****************************************************************************
 * Reading the file
 *
 * The file may have been edited by hand, damaged, or be something else
 * altogether. Every line is checked on its own: whatever is valid is used,
 * the rest is skipped. The file itself is NOT rewritten after reading - it
 * is only written when the user changes the list, so reading can never
 * destroy anything.
 ***************************************************************************/

// A line is usable when it is a ROM path on a known device ("sd:/dir/Game.smc") without
// control characters (no NUL, tab, CR in the middle, ...) and without "." / ".." parts.
static bool IsValidFavoriteLine(const char * line, size_t len)
{
	if(len == 0 || len > MAXJOLIET) // longer paths do not fit in a browser entry
		return false;

	for(size_t i = 0; i < len; i++)
	{
		unsigned char c = (unsigned char)line[i];
		if(c < 0x20 || c == 0x7F)
			return false;
	}

	char path[MAXJOLIET + 1];
	memcpy(path, line, len);
	path[len] = 0;

	// "<known device>:/..." - FindDevice() alone only compares the start of the text
	const char * colon = strchr(path, ':');
	int device;
	if(colon == NULL || colon[1] != '/' || !FindDevice(path, &device))
		return false;

	if(strstr(path, "/../") != NULL || strstr(path, "/./") != NULL)
		return false;

	const char * slash = strrchr(path, '/');
	return slash[1] != 0 && HasRomExtension(slash + 1);
}

// Adds one line from the file to list, unless it is unusable or already there
static void AddFavoriteLine(std::vector<std::string> & list, char * line, size_t len, bool tooLong)
{
	if(tooLong)
		return;

	// CRLF line ends, and spaces around the path
	while(len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' '))
		len--;
	size_t start = 0;
	while(start < len && line[start] == ' ')
		start++;

	if(list.size() >= MAX_FAVORITES || !IsValidFavoriteLine(line + start, len - start))
		return;

	std::string path(line + start, len - start);
	for(size_t i = 0; i < list.size(); i++)
	{
		if(strcasecmp(list[i].c_str(), path.c_str()) == 0)
			return; // duplicate
	}
	list.push_back(path);
}

// Reads at most FAV_FILE_MAX bytes, a few hundred at a time, so no big buffer is needed
// (and a file without a single line break cannot grow a line without bound).
static void ReadFavoritesFile(FILE * fp, std::vector<std::string> & list)
{
	char line[MAXJOLIET + 2]; // one more than the longest valid line, so too long lines are noticed
	size_t len = 0;
	bool tooLong = false;
	size_t total = 0;
	bool first = true;
	bool endedOnLineBreak = true;
	unsigned char chunk[512];

	while(total < FAV_FILE_MAX)
	{
		size_t n = fread(chunk, 1, sizeof(chunk), fp);
		if(n == 0)
			break;
		total += n;

		size_t i = 0;
		if(first && n >= 3 && chunk[0] == 0xEF && chunk[1] == 0xBB && chunk[2] == 0xBF)
			i = 3; // UTF-8 byte order mark
		first = false;

		for(; i < n; i++)
		{
			char c = (char)chunk[i];
			endedOnLineBreak = (c == '\n');

			if(c == '\n')
			{
				AddFavoriteLine(list, line, len, tooLong);
				len = 0;
				tooLong = false;
			}
			else if(len < sizeof(line))
				line[len++] = c;
			else
				tooLong = true;
		}
	}

	// The last line has no line break at the end of the file - but if reading stopped at the size
	// limit instead, that line is cut off somewhere and must not be used.
	if(!endedOnLineBreak && total < FAV_FILE_MAX)
		AddFavoriteLine(list, line, len, tooLong);
}

/****************************************************************************
 * Load / save the list (no HaltParseThread - see header comment)
 ***************************************************************************/
static void EnsureLoaded()
{
	char path[MAXPATHLEN];
	if(!GetFavoritesPath(path, sizeof(path)))
		return; // settings location not known yet - try again later

	{
		MutexLock lock(FavoritesLock());
		// already loaded from this very location?
		if(favoritesLoaded && strcmp(path, favoritesPathLoaded) == 0)
			return;
	}

	// If the device cannot be reached, the list stays "not loaded" and can't be changed. Treating
	// that as an empty list would let the next change overwrite a file that is merely out of reach.
	if(!ChangeInterface(path, SILENT))
		return;

	std::vector<std::string> list;
	FILE * fp = fopen(path, "rb");

	if(fp != NULL)
	{
		ReadFavoritesFile(fp, list);
		fclose(fp);
	}
	else
	{
		// No favorites.txt yet is fine - but only if the folder it would be in is really there
		char folder[MAXPATHLEN];
		if(!GetPrefsFolder(folder, sizeof(folder)) || !DirExists(folder))
			return;
	}

	MutexLock lock(FavoritesLock());
	favorites.swap(list);
	RebuildHashes();
	snprintf(favoritesPathLoaded, sizeof(favoritesPathLoaded), "%s", path);
	favoritesLoaded = true;
}

int FavoritesLoadTask(void *)
{
	EnsureLoaded();
	return 1;
}

int SaveFavoritesTask(void *)
{
	std::string data;
	{
		MutexLock lock(FavoritesLock());
		if(!favoritesDirty)
			return 1;
		data = pendingSave;
	}

	char path[MAXPATHLEN];
	if(!GetFavoritesPath(path, sizeof(path)))
		return 0;

	if(!ChangeInterface(path, SILENT))
		return 0;

	FILE * fp = fopen(path, "wb");
	if(fp == NULL)
		return 0; // eg. read-only card

	bool ok = fwrite(data.data(), 1, data.size(), fp) == data.size();
	if(fclose(fp) != 0)
		ok = false;

	if(ok)
	{
		MutexLock lock(FavoritesLock());
		if(pendingSave == data) // not changed again while writing
			favoritesDirty = false;
	}
	return ok ? 1 : 0;
}

/****************************************************************************
 * Favorites view
 ***************************************************************************/
bool InFavoritesView()
{
	return strcmp(browser.dir, FAVORITES_DIR) == 0;
}

struct FavSortItem
{
	std::string display;
	std::string path;
};

static bool FavSortLess(const FavSortItem & a, const FavSortItem & b)
{
	return strcasecmp(a.display.c_str(), b.display.c_str()) < 0;
}

// Fills the game list with the favorites. announceEmpty: tell the user how to add one
// when there is none (not wanted after the last one was just removed).
static int ShowFavorites(bool announceEmpty)
{
	EnsureLoaded();

	std::vector<std::string> paths;
	{
		MutexLock lock(FavoritesLock());
		paths = favorites;
	}

	HaltParseThread();
	ResetBrowser();
	snprintf(browser.dir, sizeof(browser.dir), "%s", FAVORITES_DIR);
	szOpenedFromFavorites = false;

	// ".." entry so the user can go back to the device listing. The list is never empty, so
	// BrowserChangeFolder() does not throw the user out to the device listing.
	if(!AddBrowserEntry())
		return 0;
	sprintf(browserList[0].filename, "..");
	sprintf(browserList[0].displayname, "Up One Level");
	browserList[0].isdir = 1;
	browserList[0].icon = ICON_FOLDER;

	std::vector<FavSortItem> items;
	for(size_t i = 0; i < paths.size(); i++)
	{
		char dir[MAXPATHLEN + 1];
		char name[MAXJOLIET + 1];
		char utf8Name[MAXJOLIET + 1];
		char display[MAXJOLIET + 1];

		if(!SplitPath(paths[i].c_str(), dir, sizeof(dir), name, sizeof(name)))
			continue;

		platform->getFileSystem()->nameToDisplay(name, utf8Name, sizeof(utf8Name));
		StripExt(display, utf8Name);

		FavSortItem item;
		item.display = display;
		item.path = paths[i];
		items.push_back(item);
	}
	std::sort(items.begin(), items.end(), FavSortLess);

	// The same game can be on two devices: say which is which ("Super Metroid [usb]")
	for(size_t i = 0; i < items.size(); i++)
	{
		bool duplicate = false;
		for(size_t j = 0; j < items.size() && !duplicate; j++)
			duplicate = (j != i) && strcasecmp(items[i].display.c_str(), items[j].display.c_str()) == 0;

		const char * colon = strchr(items[i].path.c_str(), ':');
		if(duplicate && colon != NULL)
			items[i].display += " [" + items[i].path.substr(0, colon - items[i].path.c_str()) + "]";
	}

	for(size_t i = 0; i < items.size(); i++)
	{
		if(!AddBrowserEntry())
			break;
		BROWSERENTRY & e = browserList[browser.size - 1];
		snprintf(e.filename, sizeof(e.filename), "%s", items[i].path.c_str()); // the full path is the "file name"
		snprintf(e.displayname, sizeof(e.displayname), "%s", items[i].display.c_str());
		e.isdir = 0;
		e.icon = ICON_NONE;
	}

	browser.numEntries = browser.size;
	browser.selIndex = 0;
	browser.pageIndex = 0;

	if(announceEmpty && items.empty())
	{
// (the prompt does not do line breaks of its own: a "\n" in the text is dropped)
#ifdef HW_DOL
		InfoPrompt("No favorites yet. Press Start on a game to add it.");
#else
		InfoPrompt("No favorites yet. Press + (Start on a GameCube controller) on a game to add it.");
#endif
	}
	return browser.numEntries;
}

static int LeaveFavorites()
{
	HaltParseThread();
	browser.dir[0] = 0;
	ResetBrowser();
	browser.numEntries = AddDeviceListing();

	// same as BrowserChangeFolder() does for the device listing
	EmuSettings.loadFolder[0] = 0;
	EmuSettings.loadDevice = DEVICE_AUTO;
	return browser.numEntries;
}

// A 7z in the Favorites folder was clicked: list what is inside, like the normal browser does.
// browser.dir becomes the archive's real path, so everything from here on is the normal 7z code.
static int OpenFavoriteSz()
{
	char full[MAXPATHLEN + 1];
	if(!FormatString(full, sizeof(full), "%s", browserList[browser.selIndex].filename))
		return ShowFavorites(false);

	RemapUsbPathIfNeeded(full, sizeof(full));

	char dir[MAXPATHLEN + 1];
	if(!FormatString(dir, sizeof(dir), "%s/", full) || !ChangeInterface(dir, NOTSILENT))
		return ShowFavorites(false); // the user is told why; the list stays as it was

	HaltParseThread();
	ResetBrowser();
	snprintf(browser.dir, sizeof(browser.dir), "%s", dir);

	if(BrowserLoadSz() > 0)
	{
		szOpenedFromFavorites = true;
		return browser.numEntries;
	}

	return ShowFavorites(false); // BrowserLoadSz() has shown the error
}

bool FavoritesChangeFolder(int * result)
{
	// Whatever is clicked next, the "7z opened from Favorites" state is over - unless it is
	// "Up One Level" inside that 7z, which is handled right below.
	bool wasOpenedFromFavorites = szOpenedFromFavorites;
	szOpenedFromFavorites = false;

	if(!IsSelectionValid())
		return false;

	const char * selected = browserList[browser.selIndex].filename;

	// click on "Favorites" in the device listing
	if(browser.dir[0] == 0 && strcmp(selected, FAVORITES_DIR) == 0)
	{
		*result = ShowFavorites(true);
		return true;
	}

	// "Up One Level" inside a 7z that was opened from the favorites: back to the favorites
	if(wasOpenedFromFavorites && inSz && browser.selIndex == 0)
	{
		CloseSzIfOpen();
		*result = ShowFavorites(false);
		return true;
	}

	if(InFavoritesView())
	{
		// click on ".." inside the favorites view
		if(strcmp(selected, "..") == 0)
		{
			*result = LeaveFavorites();
			return true;
		}

		// click on a 7z (anything else in the list is a ROM, which BrowserLoadFile() handles)
		if(IsSz())
		{
			*result = OpenFavoriteSz();
			return true;
		}
	}

	return false;
}

int AddFavoritesListingEntry(int index)
{
	if(!AddBrowserEntry())
		return 0;

	sprintf(browserList[index].filename, "%s", FAVORITES_DIR);
	sprintf(browserList[index].displayname, "Favorites");
	browserList[index].length = 0;
	browserList[index].isdir = 1;
	browserList[index].icon = ICON_STAR;
	return 1;
}

/****************************************************************************
 * Loading a favorite
 *
 * BrowserLoadFile() and ROMLoader() work on browser.dir + selected filename,
 * so both are pointed at the real location of the ROM first. On failure
 * (ROM moved or deleted, device not there, ...) the user gets the usual error
 * message, the list is NOT rebuilt (the GUI may be reading it) - the one
 * entry we touched and browser.dir are simply put back, and the user is still
 * in the Favorites folder.
 ***************************************************************************/
int BrowserLoadFavorite()
{
	if(!IsSelectionValid() || browserList[browser.selIndex].isdir)
		return 0;

	int sel = browser.selIndex;
	char full[MAXJOLIET + 1];
	char dir[MAXPATHLEN + 1];
	char name[MAXJOLIET + 1];

	snprintf(full, sizeof(full), "%s", browserList[sel].filename);

	// A USB drive can be in another slot than when the favorite was added (usb:/ -> usb2:/)
	char moved[MAXJOLIET + 1];
	memcpy(moved, full, sizeof(full));
	RemapUsbPathIfNeeded(moved, sizeof(moved));

	if(!SplitPath(moved, dir, sizeof(dir), name, sizeof(name)) || !FormatString(browser.dir, sizeof(browser.dir), "%s", dir))
	{
		snprintf(browser.dir, sizeof(browser.dir), "%s", FAVORITES_DIR);
		ErrorPrompt("Invalid favorite entry!");
		return 0;
	}

	int loaded = 0;

	if(ChangeInterface(browser.dir, NOTSILENT))
	{
		snprintf(browserList[sel].filename, sizeof(browserList[sel].filename), "%s", name);
		loaded = BrowserLoadFile(); // browser.dir is no longer the favorites dir
	}

	if(loaded)
	{
		// remember the ROM's folder so the normal browser opens there next time
		const char * path = StripDevice(browser.dir);
		if(path != NULL)
		{
			strncpy(EmuSettings.loadFolder, path, sizeof(EmuSettings.loadFolder) - 1);
			EmuSettings.loadFolder[sizeof(EmuSettings.loadFolder) - 1] = 0;
		}
		FindDevice(browser.dir, &EmuSettings.loadDevice);
	}
	else
	{
		snprintf(browserList[sel].filename, sizeof(browserList[sel].filename), "%s", full);
		snprintf(browser.dir, sizeof(browser.dir), "%s", FAVORITES_DIR);
	}
	return loaded;
}

/****************************************************************************
 * Toggle favorite state of the selected entry (memory only)
 ***************************************************************************/
int ToggleFavoriteOnSelection()
{
	EnsureLoaded();

	if(!IsSelectionValid())
		return FAVORITE_NOT_USABLE;

	// snapshot - a background directory scan may re-sort browserList under us
	char filename[MAXJOLIET + 1];
	snprintf(filename, sizeof(filename), "%s", browserList[browser.selIndex].filename);
	bool isdir = browserList[browser.selIndex].isdir != 0;

	// favorites view: the button removes the entry. This works for entries that no longer
	// exist or whose device is gone too - they could not be cleaned up otherwise.
	if(InFavoritesView())
	{
		if(isdir)
			return FAVORITE_NOT_USABLE;

		{
			MutexLock lock(FavoritesLock());
			int idx = FindFavorite(filename);
			if(idx < 0)
				return FAVORITE_NOT_USABLE;
			if(!favoritesLoaded)
				return FAVORITE_NO_LOCATION;

			favorites.erase(favorites.begin() + idx);
			RebuildHashes();
			BuildPendingSave();
		}

		int oldSel = browser.selIndex;
		ShowFavorites(false);
		browser.selIndex = (oldSel < browser.numEntries) ? oldSel : browser.numEntries - 1;
		return FAVORITE_REMOVED;
	}

	// normal browser: only ROM files (and 7z files) - no folders, nothing inside a 7z, and not
	// on the device listing
	if(inSz || browser.dir[0] == 0 || isdir || !HasRomExtension(filename))
		return FAVORITE_NOT_USABLE;

	char full[MAXPATHLEN + 1];
	if(strlen(browser.dir) + strlen(filename) > MAXJOLIET || !FormatString(full, sizeof(full), "%s%s", browser.dir, filename))
		return FAVORITE_NOT_USABLE; // would not fit in a browser entry

	BROWSERENTRY * e = FindBrowserEntry(filename);

	MutexLock lock(FavoritesLock());

	if(!favoritesLoaded)
		return FAVORITE_NO_LOCATION; // the file could not be read, so it must not be overwritten

	int idx = FindFavorite(full);

	if(idx >= 0)
	{
		favorites.erase(favorites.begin() + idx);
		RebuildHashes();
		BuildPendingSave();
		if(e != NULL)
			e->favorite = 0;
		return FAVORITE_REMOVED;
	}

	if(favorites.size() >= MAX_FAVORITES)
		return FAVORITE_LIST_FULL;

	favorites.push_back(full);
	RebuildHashes();
	BuildPendingSave();
	if(e != NULL)
		e->favorite = 1;
	return FAVORITE_ADDED;
}
