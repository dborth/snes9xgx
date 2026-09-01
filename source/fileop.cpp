/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * fileop.cpp
 *
 * File operations
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>
#include <ogc/cond.h>
#include <di/di.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "snes9xgx.h"
#include "fileop.h"
#include "memmanager.h"
#include "networkop.h"
#include "gcunzip.h"
#include "menu.h"
#include "filebrowser.h"
#include "libgui/Gui.h"
#include "drivers/Thread.h"
#include "drivers/Mutex.h"
#include "drivers/Cond.h"

#define THREAD_SLEEP 100

struct ThreadSync
{
	Mutex mutex;
	Cond  workCond; // main -> worker: work/wake available
	Cond  idleCond; // worker -> main: now idle/halted
};

static ThreadSync & DeviceSync() { static ThreadSync s; return s; }
static ThreadSync & ParseSync()  { static ThreadSync s; return s; }
static ThreadSync & WorkerSync() { static ThreadSync s; return s; }
static Mutex & SaveBufferLock()  { static Mutex m; return m; }

unsigned char *savebuffer = nullptr;
uint8_t *ext_font_ttf = nullptr;
FILE * file; // file pointer - the only one we should ever use!
bool unmountRequired[9] = { false, false, false, false, false, false, false, false, false };
bool isMounted[9] = { false, false, false, false, false, false, false, false, false };

#ifdef HW_RVL
	static DISC_INTERFACE* sd = &__io_wiisd;
	static DISC_INTERFACE* usb = &__io_usbstorage;
	static DISC_INTERFACE* dvd = &__io_wiidvd;
#else
	static DISC_INTERFACE* dvd = &__io_gcdvd;
	static DISC_INTERFACE* gcloader = &__io_gcode;
#endif

// folder parsing thread
static Thread parseThread;
static DIR *dir = nullptr;
static volatile bool parseHalt = true;
static bool parseFilter = true;
static bool ParseDirEntries();
int selectLoadedFile = 0;

// parse thread synchronization - ParseSync().workCond signals
// main -> parse: work available; ParseSync().idleCond signals parse -> main: now idle
static bool parseActive = false; // protected by ParseSync().mutex

// device checking thread
static Thread deviceThread;
static volatile bool deviceCheckingHalt = true;

#ifdef HW_RVL
// device thread synchronization - DeviceSync().workCond signals main -> device:
// wake / re-check halt; DeviceSync().idleCond signals device -> main: now halted
static bool deviceIdle = false; // protected by DeviceSync().mutex
#endif

#define WORKER_THREAD_STACKSIZE (96 * 1024)
#define DEVICE_THREAD_STACKSIZE (32 * 1024)
#define PARSE_THREAD_STACKSIZE  (32 * 1024)

/****************************************************************************
 * Background worker thread
 ***************************************************************************/

typedef int (*BgTaskFn)(void *arg);

// worker thread synchronization - WorkerSync().workCond signals main -> worker:
// task available; WorkerSync().idleCond signals worker -> main: now idle
static Thread   workerThread;
static bool     workerBusy      = false; // protected by WorkerSync().mutex - true while a task is running
static BgTaskFn workerFn        = nullptr;  // protected by WorkerSync().mutex
static void *   workerArg       = nullptr;  // protected by WorkerSync().mutex
static int      workerResult    = 0;     // protected by WorkerSync().mutex - result of the last completed task

/****************************************************************************
 * ResumeDeviceCheckingThread
 *
 * Signals the device thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeDeviceCheckingThread()
{
#ifdef HW_RVL
	DeviceSync().mutex.lock();
	deviceCheckingHalt = false;
	DeviceSync().workCond.signal();
	DeviceSync().mutex.unlock();
#endif
}

/****************************************************************************
 * HaltGui
 *
 * Signals the device thread to stop.
 ***************************************************************************/
void
HaltDeviceCheckingThread()
{
#ifdef HW_RVL
	deviceCheckingHalt = true;
	DeviceSync().mutex.lock();
	DeviceSync().workCond.signal(); // interrupt condvar sleep if the thread is in one
	while(!deviceIdle)
		DeviceSync().idleCond.wait(DeviceSync().mutex);
	DeviceSync().mutex.unlock();
#endif
}

/****************************************************************************
 * HaltParseThread
 *
 * Signals the parse thread to stop.
 ***************************************************************************/
void
HaltParseThread()
{
	parseHalt = true;
	ParseSync().mutex.lock();
	while(parseActive)
		ParseSync().idleCond.wait(ParseSync().mutex);
	ParseSync().mutex.unlock();
}


/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB/DVD removed)
 ***************************************************************************/
#ifdef HW_RVL
static void *
devicecallback (void *arg)
{
	while (1)
	{
		if(isMounted[DEVICE_SD])
		{
			if(!sd->isInserted(sd)) // check if the device was removed
			{
				unmountRequired[DEVICE_SD] = true;
				isMounted[DEVICE_SD] = false;
				parseHalt = true; // abort any in-progress dir parse on this device
			}
		}

		if(isMounted[DEVICE_USB])
		{
			if(!usb->isInserted(usb)) // check if the device was removed
			{
				unmountRequired[DEVICE_USB] = true;
				isMounted[DEVICE_USB] = false;
				parseHalt = true; // abort any in-progress dir parse on this device
			}
		}

		if(isMounted[DEVICE_DVD])
		{
			if(!dvd->isInserted(dvd)) // check if the device was removed
			{
				unmountRequired[DEVICE_DVD] = true;
				isMounted[DEVICE_DVD] = false;
				parseHalt = true; // abort any in-progress dir parse on this device
			}
		}

		// sleep ~1 sec in 100us steps so we can react to a halt request quickly
		for(int i = 0; i < 10000 && !deviceCheckingHalt; i++)
			usleep(THREAD_SLEEP);

		// if halted, block here until ResumeDeviceCheckingThread wakes us
		if(deviceCheckingHalt)
		{
			DeviceSync().mutex.lock();
			deviceIdle = true;
			DeviceSync().idleCond.signal(); // tell HaltDeviceCheckingThread we've stopped
			while(deviceCheckingHalt)
				DeviceSync().workCond.wait(DeviceSync().mutex);
			deviceIdle = false;
			DeviceSync().mutex.unlock();
		}
	}
	return nullptr;
}
#endif

static void *
parsecallback (void *arg)
{
	ParseSync().mutex.lock();
	while(1)
	{
		// sleep until ParseDirectory signals there is work to do
		while(!parseActive)
			ParseSync().workCond.wait(ParseSync().mutex);
		ParseSync().mutex.unlock();

		while(ParseDirEntries())
			usleep(THREAD_SLEEP);

		ParseSync().mutex.lock();
		parseActive = false;
		ParseSync().idleCond.signal(); // wake HaltParseThread / waitParse callers
	}
	return nullptr;
}

/****************************************************************************
 * WorkerThread
 ***************************************************************************/
static void * workercallback (void *arg)
{
	WorkerSync().mutex.lock();
	while(1)
	{
		// sleep until RunOnWorkerThread() signals there is work to do
		while(!workerBusy)
			WorkerSync().workCond.wait(WorkerSync().mutex);
		BgTaskFn fn = workerFn;
		void * farg = workerArg;
		WorkerSync().mutex.unlock();

		int result = fn ? fn(farg) : 0;

		WorkerSync().mutex.lock();
		workerResult = result;
		workerBusy = false;
		WorkerSync().idleCond.signal();
	}
	return nullptr;
}

bool RunOnWorkerThread(BgTaskFn fn, void * arg)
{
	WorkerSync().mutex.lock();
	if(workerBusy)
	{
		WorkerSync().mutex.unlock();
		return false;
	}
	workerFn = fn;
	workerArg = arg;
	workerBusy = true;
	WorkerSync().workCond.signal();
	WorkerSync().mutex.unlock();
	return true;
}

bool IsWorkerThreadFinished()
{
	MutexLock guard(WorkerSync().mutex);
	return !workerBusy;
}

int GetWorkerThreadResult()
{
	MutexLock guard(WorkerSync().mutex);
	return workerResult;
}

/****************************************************************************
 * InitFileOpThreads
 *
 * Starts the device-checking, folder-parsing, and background worker
 * threads via the libgui Thread/Mutex/Cond HAL (see ThreadSync above).
 ***************************************************************************/
void
InitFileOpThreads()
{
	SaveBufferLock();

#ifdef HW_RVL
	DeviceSync();
	deviceThread.start(devicecallback, nullptr, DEVICE_THREAD_STACKSIZE, ThreadPriority::Low);
#endif
	ParseSync();
	parseThread.start(parsecallback, nullptr, PARSE_THREAD_STACKSIZE, ThreadPriority::High);

	WorkerSync();
	workerThread.start(workercallback, nullptr, WORKER_THREAD_STACKSIZE, ThreadPriority::High);
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	fatUnmount("sd:");
	fatUnmount("usb:");
#else
	fatUnmount("port2:");
	fatUnmount("carda:");
	fatUnmount("cardb:");
	fatUnmount("gcloader:");
#endif
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

static bool MountFAT(int device, int silent)
{
	bool mounted = false;
	int retry = 1;
	char name[10], name2[10];
	DISC_INTERFACE* disc = NULL;

	switch(device)
	{
#ifdef HW_RVL
		case DEVICE_SD:
			sprintf(name, "sd");
			sprintf(name2, "sd:");
			disc = sd;
			break;
		case DEVICE_USB:
			sprintf(name, "usb");
			sprintf(name2, "usb:");
			disc = usb;
			break;
#else
		case DEVICE_SD_SLOTA:
			sprintf(name, "carda");
			sprintf(name2, "carda:");
			disc = get_io_gcsda();
			break;
		case DEVICE_SD_SLOTB:
			sprintf(name, "cardb");
			sprintf(name2, "cardb:");
			disc = get_io_gcsdb();
			break;
		case DEVICE_SD_PORT2:
			sprintf(name, "port2");
			sprintf(name2, "port2:");
			disc = get_io_gcsd2();
			break;
		case DEVICE_SD_GCLOADER:
			sprintf(name, "gcloader");
			sprintf(name2, "gcloader:");
			disc = gcloader;
			break;
#endif
		default:
			return false; // unknown device
	}

	if(unmountRequired[device])
	{
		unmountRequired[device] = false;
		fatUnmount(name2);
		disc->shutdown(disc);
		isMounted[device] = false;
	}

	while(retry)
	{
		if(fatMountSimple(name, disc))
			mounted = true;

		if(mounted || silent)
			break;

#ifdef HW_RVL
		if(device == DEVICE_SD)
			retry = ErrorPromptRetry("SD card not found!");
		else
			retry = ErrorPromptRetry("USB drive not found!");
#else
		retry = ErrorPromptRetry("SD card not found!");
#endif
	}

	isMounted[device] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(DEVICE_SD, SILENT);
	MountFAT(DEVICE_USB, SILENT);
#endif
}

/****************************************************************************
 * MountDVD()
 *
 * Tests if a ISO9660 DVD is inserted and available, and mounts it
 ***************************************************************************/
bool MountDVD(bool silent)
{
	bool mounted = false;
	int retry = 1;

	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount("dvd:");
	}

	while(retry)
	{
		ShowAction("Loading DVD...");

#ifdef HW_DOL
		DVD_Mount();
#endif
		if(!dvd->isInserted(dvd))
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("No disc inserted!");
		}
		else if(!ISO9660_Mount("dvd", dvd))
		{
			if(silent)
				break;
			
			retry = ErrorPromptRetry("Unrecognized DVD format.");
		}
		else
		{
			mounted = true;
			break;
		}
	}
	CancelAction();
	isMounted[DEVICE_DVD] = mounted;
	return mounted;
}

bool FindDevice(char * filepath, int * device)
{
	if(!filepath || filepath[0] == 0)
		return false;

	if(strncmp(filepath, "sd:", 3) == 0)
	{
		*device = DEVICE_SD;
		return true;
	}
	else if(strncmp(filepath, "usb:", 4) == 0)
	{
		*device = DEVICE_USB;
		return true;
	}
	else if(strncmp(filepath, "smb:", 4) == 0)
	{
		*device = DEVICE_SMB;
		return true;
	}
	else if(strncmp(filepath, "carda:", 6) == 0)
	{
		*device = DEVICE_SD_SLOTA;
		return true;
	}
	else if(strncmp(filepath, "cardb:", 6) == 0)
	{
		*device = DEVICE_SD_SLOTB;
		return true;
	}
	else if(strncmp(filepath, "port2:", 6) == 0)
	{
		*device = DEVICE_SD_PORT2;
		return true;
	}
	else if(strncmp(filepath, "dvd:", 4) == 0)
	{
		*device = DEVICE_DVD;
		return true;
	}
	else if(strncmp(filepath, "gcloader:", 9) == 0)
	{
		*device = DEVICE_SD_GCLOADER;
		return true;
	}
	return false;
}

char * StripDevice(char * path)
{
	if(path == nullptr)
		return nullptr;
	
	char * newpath = strchr(path,'/');
	
	if(newpath != nullptr)
		newpath++;
	
	return newpath;
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int device, bool silent)
{
	if(device == DEVICE_AUTO)
		return false;

	if(isMounted[device])
		return true;

	bool mounted = false;

	switch(device)
	{
#ifdef HW_RVL
		case DEVICE_SD:
		case DEVICE_USB:
#else
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
#endif
			mounted = MountFAT(device, silent);
			break;
		case DEVICE_DVD:
			mounted = MountDVD(silent);
			break;
		case DEVICE_SMB:
			mounted = ConnectShare(silent);
			break;
	}

	return mounted;
}

bool ChangeInterface(char * filepath, bool silent)
{
	int device = -1;

	if(!FindDevice(filepath, &device))
		return false;

	return ChangeInterface(device, silent);
}

void CreateAppPath(char * origpath)
{
	if(!origpath || origpath[0] == 0)
		return;

	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;
	
	char * loc = strrchr(path,'/');
	if (loc != nullptr)
		*loc = 0; // strip file name

	int pos = 0;

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0 || strncmp(path, "sd1:/", 5) == 0)
	{
		pos++;
		path[1] = 's';
		path[2] = 'd';
	}
	if(ChangeInterface(&path[pos], SILENT))
		snprintf(appPath, MAXPATHLEN-1, "%s", &path[pos]);

	free(path);
}

static char *GetExt(char *file)
{
	if(!file)
		return nullptr;

	char *ext = strrchr(file,'.');
	if(ext != nullptr)
	{
		ext++;
		int extlen = strlen(ext);
		if(extlen > 5)
			return nullptr;
	}
	return ext;
}

void FindAndSelectLastLoadedFile () 
{
	int indexFound = -1;
	
	for(int j=1; j < browser.numEntries; j++)
	{
		if(strcmp(browserList[j].filename, GCSettings.LastFileLoaded) == 0)
		{
			indexFound = j;
			break;
		}
	}

	// move to this file
	if(indexFound > 0)
	{
		if(indexFound >= FILE_PAGESIZE)
		{			
			int newIndex = (floor(indexFound/(float)FILE_PAGESIZE)) * FILE_PAGESIZE;

			if(newIndex + FILE_PAGESIZE > browser.numEntries)
				newIndex = browser.numEntries - FILE_PAGESIZE;

			if(newIndex < 0)
				newIndex = 0;

			browser.pageIndex = newIndex;
		}
		browser.selIndex = indexFound;
	}
	
	selectLoadedFile = 2; // selecting done
}

static bool ParseDirEntries()
{
	if(!dir)
		return false;

	char *ext;
	struct dirent *entry = nullptr;
	int isdir;

	int i = 0;

	while(i < 20 && !parseHalt)
	{
		entry = readdir(dir);

		if(entry == nullptr)
			break;

		if(entry->d_name[0] == '.' && entry->d_name[1] != '.')
			continue;

		if(strcmp(entry->d_name, "..") == 0)
		{
			isdir = 1;
		}
		else
		{
			if(entry->d_type==DT_DIR)
				isdir = 1;
			else
				isdir = 0;
			
			// don't show the file if it's not a valid ROM
			if(parseFilter && !isdir)
			{
				ext = GetExt(entry->d_name);
				
				if(ext == nullptr)
					continue;

				if(	strcasecmp(ext, "bs") != 0 && strcasecmp(ext, "smc") != 0 &&
					strcasecmp(ext, "fig") != 0 && strcasecmp(ext, "sfc") != 0 &&
					strcasecmp(ext, "swc") != 0 && strcasecmp(ext, "zip") != 0 &&
					strcasecmp(ext, "7z") != 0)
					continue;
			}
		}

		if(!AddBrowserEntry())
		{
			parseHalt = true;
			break;
		}

		snprintf(browserList[browser.numEntries+i].filename, MAXJOLIET, "%s", entry->d_name);
		browserList[browser.numEntries+i].isdir = isdir; // flag this as a dir

		if(isdir)
		{
			if(strcmp(entry->d_name, "..") == 0)
				sprintf(browserList[browser.numEntries+i].displayname, "Up One Level");
			else
				snprintf(browserList[browser.numEntries+i].displayname, MAXJOLIET, "%s", browserList[browser.numEntries+i].filename);
			browserList[browser.numEntries+i].icon = ICON_FOLDER;
		}
		else
		{
			StripExt(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename); // hide file extension
		}
		i++;
	}

	if(!parseHalt)
	{
		// Sort the file list
		if(i >= 0)
			qsort(browserList, browser.numEntries+i, sizeof(BROWSERENTRY), FileSortCallback);
	
		browser.numEntries += i;
	}

	if(entry == nullptr || parseHalt)
	{
		closedir(dir); // close directory
		dir = nullptr;
		
		return false; // no more entries
	}
	return true; // more entries
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory(bool waitParse, bool filter)
{
	int retry = 1;
	bool mounted = false;
	parseFilter = filter;
	
	ResetBrowser(); // reset browser
	
	// add trailing slash
	if(browser.dir[strlen(browser.dir)-1] != '/')
		strcat(browser.dir, "/");

	// open the directory
	while(dir == nullptr && retry == 1)
	{
		mounted = ChangeInterface(browser.dir, NOTSILENT);

		if(mounted)
			dir = opendir(browser.dir);
		else
			return -1;

		if(dir == nullptr)
			retry = ErrorPromptRetry("Error opening directory!");
	}

	// if we can't open the dir, try higher levels
	if (dir == nullptr)
	{
		char * devEnd = strrchr(browser.dir, '/');

		while(!IsDeviceRoot(browser.dir))
		{
			devEnd[0] = 0; // strip slash
			devEnd = strrchr(browser.dir, '/');

			if(devEnd == nullptr)
				break;

			devEnd[1] = 0; // strip remaining file listing
			dir = opendir(browser.dir);
			if (dir)
				break;
		}
	}
	
	if(dir == nullptr)
		return -1;

	if(IsDeviceRoot(browser.dir))
	{
		AddBrowserEntry();
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].isdir = 1; // flag this as a dir
		browserList[0].icon = ICON_FOLDER;
		browser.numEntries++;
	}

	parseHalt = false;
	ParseDirEntries(); // index first 20 entries

	// signal parse thread to continue indexing remaining entries
	ParseSync().mutex.lock();
	parseActive = true;
	ParseSync().workCond.signal();
	ParseSync().mutex.unlock();

	if(waitParse) // wait for complete parsing
	{
        ShowAction("Loading...");

		ParseSync().mutex.lock();
		while(parseActive)
			ParseSync().idleCond.wait(ParseSync().mutex);
		ParseSync().mutex.unlock();

		CancelAction();
	}

	return browser.numEntries;
}

bool DirExists(const char * path) {
	DIR *dir = opendir(path);
	if (dir) {
		closedir(dir);
		return true;
	}
	return false;
}

bool CreateDirectory(char * path) {
	if(DirExists(path)) {
		return true;
	}
	if(mkdir(path, 0777) != 0) {
		return false;
	}
	return true;
}

/****************************************************************************
 * AllocSaveBuffer ()
 * Clear and allocate the savebuffer
 ***************************************************************************/
void
AllocSaveBuffer ()
{
	SaveBufferLock().lock();
	memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
 * FreeSaveBuffer ()
 * Free the savebuffer memory
 ***************************************************************************/
void
FreeSaveBuffer ()
{
	SaveBufferLock().unlock();
}

/****************************************************************************
 * LoadSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
size_t
LoadSzFile(char * filepath, unsigned char * rbuffer)
{
	size_t size = 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceCheckingThread();

	// halt parsing
	HaltParseThread();

	file = fopen (filepath, "rb");
	if (file)
	{
		size = SzExtractFile(browserList[browser.selIndex].filenum, rbuffer);
		fclose (file);
	}
	else
	{
		ErrorPrompt("Error opening file!");
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceCheckingThread();

	return size;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
size_t
LoadFile (char * rbuffer, char *filepath, size_t length, size_t buffersize, bool silent)
{
	char zipbuffer[2048];
	size_t size = 0, offset = 0, readsize = 0;
	int retry = 1;
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceCheckingThread();

	// halt parsing
	HaltParseThread();

	// open the file
	while(retry)
	{
		if(!ChangeInterface(device, silent))
			break;

		file = fopen (filepath, "rb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error opening file!");
			continue;
		}

		if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
		{
			size = fread (rbuffer, 1, length, file);
		}
		else // load whole file
		{
			readsize = fread (zipbuffer, 1, 32, file);

			if(!readsize)
			{
				unmountRequired[device] = true;
				retry = ErrorPromptRetry("Error reading file!");
				fclose (file);
				continue;
			}

			if (IsZipFile (zipbuffer))
			{
				size = UnZipBuffer ((unsigned char *)rbuffer, buffersize); // unzip
			}
			else
			{
				fseeko(file,0,SEEK_END);
				size = ftello(file);
				fseeko(file,0,SEEK_SET);

				if(size > buffersize) {
					size = 0;
				}
				else {
					while(!feof(file))
					{
						ShowProgress ("Loading...", offset, size);
						readsize = fread (rbuffer + offset, 1, 4096, file); // read in next chunk

						if(readsize <= 0)
							break; // reading finished (or failed)

						offset += readsize;
					}
					size = offset;
					CancelAction();
				}
			}
		}
		retry = 0;
		fclose (file);
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceCheckingThread();
	CancelAction();
	return size;
}

size_t LoadFile(char * filepath, bool silent)
{
	return LoadFile((char *)savebuffer, filepath, 0, SAVEBUFFERSIZE, silent);
}

#ifdef HW_RVL
size_t LoadFont(char * filepath)
{
	FILE *file = fopen (filepath, "rb");

	if(!file) {
		ErrorPrompt("Font file not found!");
		return 0;
	}

	fseeko(file,0,SEEK_END);
	size_t loadSize = ftello(file);

	if(loadSize == 0) {
		ErrorPrompt("Error loading font!");
		return 0;
	}

	if(ext_font_ttf) {
		extmem_free(ext_font_ttf);
	}

	ext_font_ttf = (uint8_t *)extmem_malloc(loadSize);

	if(!ext_font_ttf) {
		ErrorPrompt("Font file is too large!");
		fclose(file);
		return 0;
	}

	fseeko(file,0,SEEK_SET);
	fread (ext_font_ttf, 1, loadSize, file);
	fclose(file);
	return loadSize;
}

void LoadBgMusic()
{
	char filepath[MAXPATHLEN];
	sprintf(filepath, "%s/bg_music.ogg", appPath);
	FILE *file = fopen (filepath, "rb");
	if(!file) {
		return;
	}

	fseeko(file,0,SEEK_END);
	size_t ogg_size = ftello(file);

	if(ogg_size == 0) {
		return;
	}

	uint8_t * ogg_data = (uint8_t *)extmem_malloc(ogg_size);

	if(!ogg_data) {
		return;
	}

	fseeko(file, 0, SEEK_SET);
	fread (ogg_data, 1, ogg_size, file);
	fclose(file);
	bg_music = ogg_data;
	bg_music_size = ogg_size;
}
#endif

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
size_t
SaveFile (char * buffer, char *filepath, size_t datasize, bool silent)
{
	size_t written = 0;
	size_t writesize, nextwrite;
	int retry = 1;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	if(datasize == 0)
		return 0;

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceCheckingThread();

	// halt parsing
	HaltParseThread();

	if(!silent)
		ShowAction("Saving...");

	while(!written && retry == 1)
	{
		if(!ChangeInterface(device, silent))
			break;

		file = fopen (filepath, "wb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error creating file!");
			continue;
		}

		while(written < datasize)
		{
			if(datasize - written > 4096) nextwrite=4096;
			else nextwrite = datasize-written;
			writesize = fwrite (buffer+written, 1, nextwrite, file);
			if(writesize != nextwrite) break; // write failure
			written += writesize;
		}
		fclose (file);

		if(written != datasize) written = 0;

		if(!written)
		{
			unmountRequired[device] = true;
			if(silent) break;
			retry = ErrorPromptRetry("Error saving file!");
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceCheckingThread();
	if(!silent)
		CancelAction();
	return written;
}

size_t SaveFile(char * filepath, size_t datasize, bool silent)
{
	return SaveFile((char *)savebuffer, filepath, datasize, silent);
}
