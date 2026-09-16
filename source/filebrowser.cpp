/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * filebrowser.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>

#include "snes9x/port.h"
#include "snes9xgx.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "fileop.h"
#include "input.h"
#include "utils/decompress.h"
#include "freeze.h"
#include "sram.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/cheats.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"

extern "C" {
extern char* strcasestr(const char *, const char *);
}

BROWSERINFO browser;
BROWSERENTRY * browserList = nullptr; // list of files/folders in browser
bool browserDeviceListChanged = false;

static char szpath[MAXPATHLEN];
char szname[MAXPATHLEN];
bool inSz = false;

unsigned long SNESROMSize = 0;
bool bsxBiosLoadFailed;

extern bool isBSX();

bool isValidLoadDevice(int device)
{
	int numLoadDevices;
	const int * loadDevices = platform->getFileSystem()->getValidLoadDevices(numLoadDevices);
	for (int i = 0; i < numLoadDevices; i++) {
		if (loadDevices[i] == device) {
			return true;
		}
	}
	return false;
}

bool isValidSaveDevice(int device)
{
	int numSaveDevices;
	const int * saveDevices = platform->getFileSystem()->getValidSaveDevices(numSaveDevices);
	for (int i = 0; i < numSaveDevices; i++) {
		if (saveDevices[i] == device) {
			return true;
		}
	}
	return false;
}

int getNextLoadDevice(int device)
{
	int numLoadDevices;
	const int * loadDevices = platform->getFileSystem()->getValidLoadDevices(numLoadDevices);
	for (int i = 0; i < numLoadDevices; i++) {
		if (loadDevices[i] == device) {
			return loadDevices[(i + 1) % numLoadDevices];
		}
	}
	return DEVICE_AUTO;
}

int getNextSaveDevice(int device)
{
	int numSaveDevices;
	const int * saveDevices = platform->getFileSystem()->getValidSaveDevices(numSaveDevices);
	for (int i = 0; i < numSaveDevices; i++) {
		if (saveDevices[i] == device) {
			return saveDevices[(i + 1) % numSaveDevices];
		}
	}
	return DEVICE_AUTO;
}

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load device
* Returns device set
****************************************************************************/
int autoLoadMethod()
{
	if(EmuSettings.LoadMethod > DEVICE_AUTO && isValidLoadDevice(EmuSettings.LoadMethod)) {
		return EmuSettings.LoadMethod;
	}

	char defaultFolderPath[MAXPATHLEN];
	char fullPath[MAXPATHLEN];
	int device = DEVICE_AUTO;
	int firstConnectedDevice = DEVICE_AUTO;

	GetDefaultFolderPath(defaultFolderPath, loadFolder[LOADFOLDER_ROMS].name);

	int numLoadDevices;
	const int * loadDevices = platform->getFileSystem()->getValidLoadDevices(numLoadDevices);

	// Single pass: mount each candidate device at most once. Prefer the
	// first one that already has the default ROMs folder; if none do, fall
	// back to the first one that mounted at all.
	for (int i = 1; i < numLoadDevices && device == DEVICE_AUTO; i++) {
		if (!ChangeInterface(loadDevices[i], SILENT))
			continue;

		if (firstConnectedDevice == DEVICE_AUTO)
			firstConnectedDevice = loadDevices[i];

		MakeFilePathForFolderPath(fullPath, loadDevices[i], defaultFolderPath);
		if (DirExists(fullPath))
			device = loadDevices[i];
	}

	if (device == DEVICE_AUTO)
		device = firstConnectedDevice;

	EmuSettings.LoadMethod = device; // load device found for later use
	CancelAction();
	return device;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save device
* Returns device set
****************************************************************************/
int autoSaveMethod()
{
	if(EmuSettings.SaveMethod > DEVICE_AUTO && isValidSaveDevice(EmuSettings.SaveMethod)) {
		return EmuSettings.SaveMethod;
	}

	char defaultFolderPath[MAXPATHLEN];
	char fullPath[MAXPATHLEN];
	int device = DEVICE_AUTO;
	int firstConnectedDevice = DEVICE_AUTO;

	GetDefaultFolderPath(defaultFolderPath, saveFolder[SAVEFOLDER_SAVES].name);

	int numSaveDevices;
	const int * saveDevices = platform->getFileSystem()->getValidSaveDevices(numSaveDevices);

	for (int i = 1; i < numSaveDevices && device == DEVICE_AUTO; i++) {
		if (!ChangeInterface(saveDevices[i], SILENT))
			continue;

		if (firstConnectedDevice == DEVICE_AUTO)
			firstConnectedDevice = saveDevices[i];

		MakeFilePathForFolderPath(fullPath, saveDevices[i], defaultFolderPath);
		if (DirExists(fullPath))
			device = saveDevices[i];
	}

	if (device == DEVICE_AUTO)
		device = firstConnectedDevice;

	EmuSettings.SaveMethod = device; // save device found for later use

	CancelAction();
	return device;
}

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser()
{
	browser.numEntries = 0;
	browser.selIndex = 0;
	browser.pageIndex = 0;
	browser.size = 0;
}

bool AddBrowserEntry()
{
	if(browser.size >= MAX_BROWSER_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
		return false; // out of space
	}

	memset(&(browserList[browser.size]), 0, sizeof(BROWSERENTRY)); // clear the new entry
	browser.size++;
	return true;
}

/****************************************************************************
 * CleanupPath()
 * Cleans up the filepath, removing double // and replacing \ with /
 ***************************************************************************/
static void CleanupPath(char * path)
{
	if(!path || path[0] == 0)
		return;
	
	int pathlen = strlen(path);
	int j = 0;
	for(int i=0; i < pathlen && i < MAXPATHLEN; i++)
	{
		if(path[i] == '\\')
			path[i] = '/';

		if(j == 0 || !(path[j-1] == '/' && path[i] == '/'))
			path[j++] = path[i];
	}
	path[j] = 0;
}

bool IsDeviceRoot(char * path)
{
	if(path == nullptr || path[0] == 0)
		return false;

	if( strcmp(path, "sd:/")    == 0 ||
		strcmp(path, "usb:/")   == 0 ||
		strcmp(path, "dvd:/")   == 0 ||
		strcmp(path, "smb:/")   == 0 ||
		strcmp(path, "carda:/") == 0 ||
		strcmp(path, "cardb:/") == 0 ||
		strcmp(path, "port2:/") == 0 ||
		strcmp(path, "gcloader:/") == 0 )
	{
		return true;
	}
	return false;
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName()
{
	int size=0;
	char * test;
	char temp[1024];

	/* nothing to do if there are no entries or the selected index is invalid */
	if(browser.numEntries == 0 || browser.selIndex < 0 || browser.selIndex >= browser.numEntries) {
		return 1;
	}

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
	{
		// already at the top level
		if(IsDeviceRoot(browser.dir))
		{
			browser.dir[0] = 0; // remove device - we are going to the device listing screen
		}
		else
		{
			/* determine last subdirectory namelength */
			sprintf(temp,"%s",browser.dir);
			test = strtok(temp,"/");
			while (test != nullptr)
			{
				size = strlen(test);
				test = strtok(nullptr,"/");
			}
	
			/* remove last subdirectory name */
			size = strlen(browser.dir) - size - 1;
			strncpy(EmuSettings.LastFileLoaded, &browser.dir[size], strlen(browser.dir) - size - 1); //set as loaded file the previous dir
			EmuSettings.LastFileLoaded[strlen(browser.dir) - size - 1] = 0;
			browser.dir[size] = 0;
		}

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser.dir+strlen(browser.dir), "%s/", browserList[browser.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

void GetDefaultFolderPath(char *folderPath, const char *folderName) {
    sprintf(folderPath, "%s/%s", APPFOLDER, folderName);
}

void MakeFilePathForFolderPath(char *fullPath, int device, const char *folder) {
	platform->getFileSystem()->getPath(fullPath, MAXPATHLEN, device, folder);
}

bool MakeFilePath(char filepath[], int type, char * filename, int filenum)
{
	char file[512];
	char folder[1024];
	char ext[4];
	char temppath[MAXPATHLEN];

	if(type == FILE_ROM)
	{
		// Check path length
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) >= MAXPATHLEN)
		{
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s%s",browser.dir,browserList[browser.selIndex].filename);
		}
	}
	else
	{
		if(EmuSettings.SaveMethod == DEVICE_AUTO)
			return false;

		switch(type)
		{
			case FILE_SRAM:
			case FILE_STATE:
				sprintf(folder, EmuSettings.SaveFolder);

				if(type == FILE_SRAM) sprintf(ext, "srm");
				else sprintf(ext, "frz");

				if(filenum >= -1)
				{
					if(filenum == -1)
						snprintf(file, sizeof(file), "%s.%s", filename, ext);
					else if(filenum == 0)
						if (!EmuSettings.AppendAuto)
							snprintf(file, sizeof(file), "%s.%s", filename, ext);
						else
							snprintf(file, sizeof(file), "%s Auto.%s", filename, ext);
					else
						snprintf(file, sizeof(file), "%s %i.%s", filename, filenum, ext);
				}
				else
				{
					snprintf(file, sizeof(file), "%s", filename);
				}
				break;
			case FILE_CHEAT:
				sprintf(folder, EmuSettings.CheatFolder);
				snprintf(file, sizeof(file), "%s.cht", Memory.ROMFilename);
				break;
		}
		platform->getFileSystem()->getPath(temppath, EmuSettings.SaveMethod, folder, file);
	}
	CleanupPath(temppath); // cleanup path
	snprintf(filepath, MAXPATHLEN, "%s", temppath);
	return true;
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY *)f1)->filename[0] == '.' || ((BROWSERENTRY *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY *)f1)->isdir && !(((BROWSERENTRY *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY *)f1)->isdir) && ((BROWSERENTRY *)f2)->isdir) return 1;

	return strcasecmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
}

/****************************************************************************
 * IsValidROM
 *
 * Checks if the specified file is a valid ROM
 * For now we will just check the file extension and file size
 * If the file is a zip, we will check the file extension / file size of the
 * first file inside
 ***************************************************************************/
static bool IsValidROM()
{
	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != nullptr)
		{
			char * zippedFilename = nullptr;
			
			if(strcasecmp(p, ".zip") == 0 && !inSz)
			{
				// we need to check the file extension of the first file in the archive
				zippedFilename = GetFirstZipFilename ();

				if(zippedFilename && strlen(zippedFilename) > 4)
					p = strrchr(zippedFilename, '.');
				else
					p = nullptr;
			}

			if(p != nullptr)
			{
				if (strcasecmp(p, ".bs") == 0 ||
					strcasecmp(p, ".fig") == 0 ||
					strcasecmp(p, ".sfc") == 0 ||
					strcasecmp(p, ".smc") == 0 ||
					strcasecmp(p, ".swc") == 0)
				{
					if(zippedFilename) free(zippedFilename);
					return true;
				}
			}
			if(zippedFilename) free(zippedFilename);
		}
	}
	ErrorPrompt("Unknown file type!");
	return false;
}

/****************************************************************************
 * IsSz
 *
 * Checks if the specified file is a 7z
 ***************************************************************************/
bool IsSz()
{
	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != nullptr)
			if(strcasecmp(p, ".7z") == 0)
				return true;
	}
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt(char* returnstring, char * inputstring)
{
	char* loc_dot;

	snprintf (returnstring, MAXJOLIET, "%s", inputstring);

	if(inputstring == nullptr || strlen(inputstring) < 4)
		return;

	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != nullptr)
		*loc_dot = 0; // strip file extension
}

/****************************************************************************
 * BrowserLoadSz
 *
 * Opens the selected 7z file, and parses a listing of the files within
 ***************************************************************************/
int BrowserLoadSz()
{
	memset(szpath, 0, MAXPATHLEN);
	size_t dirLen = strlen(browser.dir);
	if(dirLen > 0)
		strncpy(szpath, browser.dir, dirLen - 1);
	
	strncpy(szname, strrchr(szpath, '/') + 1, strrchr(szpath, '.') - strrchr(szpath, '/'));
	*strrchr(szname, '.') = '\0';

	int szfiles = SzParse(szpath);
	if(szfiles)
	{
		browser.numEntries = szfiles;
		inSz = true;
	}
	else
		ErrorPrompt("Error opening archive!");

	return szfiles;
}

int ROMLoader()
{
	size_t size;
	char filepath[1024];

	memset(Memory.NSRTHeader, 0, sizeof(Memory.NSRTHeader));
	Memory.HeaderCount = 0;

	if(!inSz)
	{
		if(!MakeFilePath(filepath, FILE_ROM))
			return 0;

		size = LoadFile ((char *)Memory.ROM, filepath, 0, Memory.MAX_ROM_SIZE, NOTSILENT);
	}
	else
	{
		size = LoadSzFile(szpath, (unsigned char *)Memory.ROM);

		if(size <= 0)
		{
			browser.selIndex = 0;
			BrowserChangeFolder();
		}
	}

	if(size <= 0)
		return 0;

	SNESROMSize = Memory.HeaderRemove(size, Memory.ROM);
	bsxBiosLoadFailed = false;

	if(isBSX()) {
		platform->getFileSystem()->getPath(filepath, EmuSettings.LoadMethod, APPFOLDER, "BS-X.bin");
		if(LoadFile ((char *)Memory.BIOSROM, filepath, 0, 0x100000, SILENT) == 0) {
			bsxBiosLoadFailed = true;
		}
	}

	return SNESROMSize;
}

/****************************************************************************
 * BrowserLoadFile
 *
 * Loads the selected ROM
 ***************************************************************************/
int BrowserLoadFile()
{
	int loaded = 0;
	int device;

	if(!FindDevice(browser.dir, &device))
		return 0;

	// check that this is a valid ROM
	if(!IsValidROM())
		goto done;

	// store the filename (w/o ext) - used for sram/freeze naming
	StripExt(Memory.ROMFilename, browserList[browser.selIndex].filename);
	snprintf(EmuSettings.LastFileLoaded, MAXPATHLEN, "%s", browserList[browser.selIndex].filename);
	strncpy(Memory.ROMFilePath, browser.dir, PATH_MAX);
	Memory.ROMFilePath[PATH_MAX] = 0;

	SNESROMSize = 0;
	S9xDeleteCheats();
	Memory.LoadROM("ROM");

	if (SNESROMSize == 0)
	{
		ErrorPrompt("Error loading game!");
	}
	else
	{
		// load SRAM or snapshot
		if (EmuSettings.AutoLoad == AUTOLOAD_SRAM)
			LoadSRAMAuto(SILENT);
		else if (EmuSettings.AutoLoad == AUTOLOAD_STATE)
			LoadSnapshotAuto(SILENT);

		ResetBrowser();
		loaded = 1;
	}
done:
	CancelAction();
	return loaded;
}

void CloseSzIfOpen() {
	if(inSz) {
		inSz = false;
		SzClose();
	}
}

/****************************************************************************
 * DeviceIcon
 ***************************************************************************/
static int DeviceIcon(int deviceId)
{
	switch(deviceId)
	{
		case DEVICE_SD:
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
			return ICON_SD;
		case DEVICE_USB: return ICON_USB;
		case DEVICE_DVD: return ICON_DVD;
		case DEVICE_SMB: return ICON_SMB;
		default:         return ICON_NONE;
	}
}

/****************************************************************************
 * AddDeviceListing
 *
 * Builds the root "choose a device" listing shown when there's no current
 * directory. Sourced from the platform's FileSystemDriver.
 ***************************************************************************/
int AddDeviceListing()
{
	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = platform->getFileSystem()->enumerateStorageDevices(devices);
	int i = 0;

	for(int d = 0; d < count; d++)
	{
		StorageDevice & dev = devices[d];

		if(!dev.alwaysListed && !platform->getFileSystem()->isDevicePresent(dev.id))
			continue;

		AddBrowserEntry();
		sprintf(browserList[i].filename, "%s", dev.prefix);
		sprintf(browserList[i].displayname, "%s", dev.name);
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = DeviceIcon(dev.id);
		i++;
	}

	return i;
}

/****************************************************************************
 * BrowserChangeFolder
 *
 * Update current directory and set new entry list if directory has changed
 ***************************************************************************/
int BrowserChangeFolder()
{
	if(inSz && browser.selIndex == 0) // inside a 7z, requesting to leave
	{
		CloseSzIfOpen();
	}

	if(!UpdateDirName()) {
		CloseSzIfOpen();
		return -1;
	}

	HaltParseThread();
	CleanupPath(browser.dir);
	ResetBrowser();

	if(browser.dir[0] != 0)
	{
		// skip if device is no longer mounted
		if(!ChangeInterface(browser.dir, NOTSILENT)) {
			CloseSzIfOpen();
			browser.numEntries = 0;
		}
		else {
			if(strstr(browser.dir, ".7z"))
			{
				BrowserLoadSz();
			}
			else
			{
				ParseDirectory(true, true);
			}
			FindAndSelectLastLoadedFile();
		}
	}

	if(browser.numEntries == 0)
	{
		browser.dir[0] = 0;
		browser.numEntries += AddDeviceListing();
	}
	
	if(browser.dir[0] == 0)
	{
		EmuSettings.LoadFolder[0] = 0;
		EmuSettings.LoadMethod = DEVICE_AUTO;
	}
	else
	{
		char * path = StripDevice(browser.dir);
		if(path != nullptr)
			strcpy(EmuSettings.LoadFolder, path);
		FindDevice(browser.dir, &EmuSettings.LoadMethod);
	}

	return browser.numEntries;
}

/****************************************************************************
 * OpenROM
 * Displays a list of ROMS on load device
 ***************************************************************************/
int
OpenGameList ()
{
	int device = EmuSettings.LoadMethod;

	if(device > 0 && ChangeInterface(device, NOTSILENT)) {
		// change current dir to roms directory
		platform->getFileSystem()->getPath(browser.dir, device, EmuSettings.LoadFolder, "");

		if(strlen(EmuSettings.LoadFolder) > 0) {
			DIR *dir = opendir(browser.dir);

			if(dir == nullptr) {
				platform->getFileSystem()->getPath(browser.dir, device, "");
			}
			else {
				closedir(dir);
			}
		}
	}
	else {
		browser.dir[0] = 0;
		browser.numEntries = 0;
	}
	
	BrowserChangeFolder();
	return browser.numEntries;
}

bool AutoloadGame(char* filepath, char* filename) {
	ResetBrowser();

	selectLoadedFile = 1;
	std::string dir(filepath);
	dir.assign(&dir[dir.find_last_of(":") + 2]);
	strncpy(EmuSettings.LoadFolder, dir.c_str(), sizeof(EmuSettings.LoadFolder) - 1);
	EmuSettings.LoadFolder[sizeof(EmuSettings.LoadFolder) - 1] = 0;
	OpenGameList();

	for(int i = 0; i < browser.numEntries; i++) {
		// Skip it
		if (strcmp(browserList[i].filename, ".") == 0 || strcmp(browserList[i].filename, "..") == 0) {
			continue;
		}
		if(strcasestr(browserList[i].filename, filename) != nullptr) {
			browser.selIndex = i;
			if(IsSz()) {
				BrowserLoadSz();
				browser.selIndex = 1;
			}
			break;
		}
	}
	if(BrowserLoadFile() > 0) {
		return true;
	}
	return false;
}
