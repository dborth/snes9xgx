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
#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>

#include "snes9xgx.h"
#include "fileop.h"
#include "menu.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "utils/decompress.h"
#include "libgui/Gui.h"
#include "drivers/Thread.h"
#include "drivers/Mutex.h"
#include "drivers/Time.h"
#include "drivers/Cond.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"
#include "drivers/SmbDriver.h"

#define PARSE_FIRST_BATCH 20
#define PARSE_BATCH_SIZE  100

static ThreadSync & WorkerSync() { static ThreadSync s; return s; }
static Mutex & SaveBufferLock()  { static Mutex m; return m; }

unsigned char *savebuffer = nullptr;
uint8_t *ext_font_ttf = nullptr;

static DIR *dir = nullptr;
static volatile bool parseHalt = true;
static bool parseFilter = true;
static char parsePrefix[MAXJOLIET + 1] = { 0 }; // if set, only entries whose name starts with this are listed
static size_t parsePrefixLen = 0;
static bool ParseDirEntries(int batchSize);
static int  ContinueParseTask(void *);
static void * devicecallback(void *);
int selectLoadedFile = 0;

static Thread deviceThread;
static bool deviceCheckingArmed = false;    // ArmDeviceChecking() called, StopDeviceChecking() not yet
static bool deviceCheckingRunning = false;  // the thread has actually been started
static Ticks deviceCheckingArmedAt = 0;
#define DEVICE_CHECK_ARM_DELAY_MS 5000 // let the browser's own boot work (mount/parse/preview) go first

#define WORKER_THREAD_STACKSIZE (96 * 1024)
#define DEVICE_THREAD_STACKSIZE (32 * 1024)

/****************************************************************************
 * Background worker thread
 ***************************************************************************/

// worker thread synchronization - WorkerSync().workCond signals main -> worker:
// task available; WorkerSync().idleCond signals worker -> main: now idle
static Thread   workerThread;
static bool     workerBusy      = false; // protected by WorkerSync().mutex - true while a task is running
static BgTaskFn workerFn        = nullptr;  // protected by WorkerSync().mutex
static void *   workerArg       = nullptr;  // protected by WorkerSync().mutex
static int      workerResult    = 0;     // protected by WorkerSync().mutex - result of the last completed task

// queued fire-and-forget tasks, run by the worker thread whenever it has nothing
// else to do - see QueueBackgroundTask()
#define BG_TASK_QUEUE_SIZE 4
struct BgQueuedTask { BgTaskFn fn; void * arg; };
static BgQueuedTask bgTasks[BG_TASK_QUEUE_SIZE]; // ring buffer, protected by WorkerSync().mutex
static int  bgHead    = 0;     // protected by WorkerSync().mutex
static int  bgCount   = 0;     // protected by WorkerSync().mutex
static bool bgRunning = false; // protected by WorkerSync().mutex - a queued task is running right now

/****************************************************************************
 * ArmDeviceChecking / UpdateDeviceCheckingArm / StopDeviceChecking
 ***************************************************************************/
void ArmDeviceChecking()
{
	deviceCheckingArmed = true;
	deviceCheckingArmedAt = SystemTime::now();
}

void UpdateDeviceCheckingArm()
{
	if(!deviceCheckingArmed || deviceCheckingRunning)
		return;

	if(SystemTime::diffMillisecs(deviceCheckingArmedAt, SystemTime::now()) < DEVICE_CHECK_ARM_DELAY_MS)
		return;

	if(platform->getFileSystem()->hasRemovableStorageDevices())
	{
		deviceThread.start(devicecallback, nullptr, DEVICE_THREAD_STACKSIZE, ThreadPriority::Low);
		deviceCheckingRunning = true;
	}

	deviceCheckingArmed = false; // armed once per StopDeviceChecking(); either started above, or nothing to check
}

void StopDeviceChecking()
{
	deviceCheckingArmed = false;

	if(!deviceCheckingRunning)
		return;

	deviceThread.requestStop();
	deviceThread.join();
	deviceCheckingRunning = false;
}

/****************************************************************************
 * HaltParseThread
 *
 * Tells an in-progress or queued directory scan to stop at its next opportunity
 ***************************************************************************/
void HaltParseThread()
{
	parseHalt = true;
}

/****************************************************************************
 * WakeWorkerThread
 *
 * Thread::JoinAll()'s wake callback - breaks the worker thread out of
 * whatever cond it may be parked in so it can notice stopRequested() and
 * actually return.
 ***************************************************************************/
static void WakeWorkerThread()
{
	WorkerSync().mutex.lock();
	WorkerSync().workCond.signal();
	WorkerSync().mutex.unlock();
}

/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB/DVD removed).
 ***************************************************************************/
static void * devicecallback(void *)
{
	while (!deviceThread.stopRequested())
	{
		int removed[MAX_STORAGE_DEVICES];
		int removedCount = 0;
		bool deviceListChanged = false;

		platform->getFileSystem()->pollStorageDevices(removed, removedCount, deviceListChanged);

		if(removedCount > 0)
		{
			parseHalt = true; // abort any in-progress dir parse if a device it's using just disappeared

			for(int i = 0; i < removedCount; i++)
			{
				if(removed[i] >= 0 && removed[i] < 32)
					removedDeviceMask |= (1u << removed[i]);
			}
		}

		if(deviceListChanged)
			browserDeviceListChanged = true; // signal the menu loop to refresh the device listing if it's on screen

		// 3 sec between checks (in 100ms steps, so a stop request is still noticed quickly)
		for(int i = 0; i < 30 && !deviceThread.stopRequested(); i++)
			usleep(100000);
	}
	return nullptr;
}

/****************************************************************************
 * WorkerThread
 ***************************************************************************/
static void * workercallback (void *)
{
	WorkerSync().mutex.lock();
	while(!workerThread.stopRequested())
	{
		// sleep until RunOnWorkerThread() or QueueBackgroundTask() signals there is work to do
		while(!workerBusy && bgCount == 0 && !workerThread.stopRequested())
			WorkerSync().workCond.wait(WorkerSync().mutex);

		if(!workerBusy && bgCount == 0 && workerThread.stopRequested())
			break;

		if(workerBusy) // something is waiting on this - always ahead of queued tasks
		{
			BgTaskFn fn = workerFn;
			void * farg = workerArg;
			WorkerSync().mutex.unlock();

			int result = fn ? fn(farg) : 0;

			WorkerSync().mutex.lock();
			workerResult = result;
			workerBusy = false;
			WorkerSync().idleCond.signal();
		}
		else
		{
			BgQueuedTask task = bgTasks[bgHead];
			bgHead = (bgHead + 1) % BG_TASK_QUEUE_SIZE;
			bgCount--;
			bgRunning = true;
			WorkerSync().mutex.unlock();

			task.fn(task.arg);

			WorkerSync().mutex.lock();
			bgRunning = false;
		}
	}
	WorkerSync().mutex.unlock();
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

bool QueueBackgroundTask(BgTaskFn fn, void * arg)
{
	MutexLock guard(WorkerSync().mutex);

	if(!fn || !workerThread.isRunning() || workerThread.stopRequested())
		return false;

	for(int i = 0; i < bgCount; i++)
	{
		const BgQueuedTask & queued = bgTasks[(bgHead + i) % BG_TASK_QUEUE_SIZE];
		if(queued.fn == fn && queued.arg == arg)
			return true; // already waiting to run - it will see whatever state is current when it does
	}

	if(bgCount >= BG_TASK_QUEUE_SIZE)
		return false;

	bgTasks[(bgHead + bgCount) % BG_TASK_QUEUE_SIZE] = { fn, arg };
	bgCount++;
	WorkerSync().workCond.signal();
	return true;
}

bool BackgroundTasksIdle()
{
	MutexLock guard(WorkerSync().mutex);
	return bgCount == 0 && !bgRunning;
}

bool WaitForBackgroundTasks(uint32_t timeoutMs)
{
	if(!workerThread.isRunning() || BackgroundTasksIdle())
		return true;

	Ticks start = SystemTime::now();
	do
	{
		usleep(10000);
	} while(!BackgroundTasksIdle() && SystemTime::diffMillisecs(start, SystemTime::now()) < timeoutMs);

	return BackgroundTasksIdle();
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
 * Starts the background worker thread via the libgui Thread/Mutex/Cond HAL
 * (see ThreadSync above).
 ***************************************************************************/
void InitFileOpThreads()
{
	SaveBufferLock();

	WorkerSync();
	workerThread.start(workercallback, nullptr, WORKER_THREAD_STACKSIZE, ThreadPriority::High, WakeWorkerThread);
}

/****************************************************************************
 * MountAllFAT
 * Silently (pre-)mounts whatever devices the platform driver flags as
 * auto-mount-at-startup (eg. Wii's SD/USB). No-op on platforms with none.
 ***************************************************************************/
void MountAllFAT()
{
	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = platform->getFileSystem()->enumerateStorageDevices(devices);

	for(int i = 0; i < count; i++)
		if(devices[i].autoMountAtStartup)
			ChangeInterface(devices[i].id, SILENT);
}

bool FindDevice(char * filepath, int * device)
{
	if(!filepath || filepath[0] == 0)
		return false;

	int count = 0;
	const int * candidates = platform->getFileSystem()->getValidLoadDevices(count);

	for(int i = 0; i < count; i++)
	{
		if(candidates[i] == DEVICE_AUTO)
			continue;

		const char * prefix = platform->getFileSystem()->getDevicePrefix(candidates[i]);
		size_t len = prefix ? strlen(prefix) : 0; // eg. "sd:/" -> compare against "sd:"
		if(len > 1 && strncmp(filepath, prefix, len - 1) == 0)
		{
			*device = candidates[i];
			return true;
		}
	}
	return false;
}

/****************************************************************************
 * StripDevice
 *
 * Returns a pointer into path just past the device's prefix (eg. "sd:/roms/"
 * -> "roms/", "/vol/external01/roms/" -> "roms/"), or nullptr if path isn't
 * on a known device.
 ***************************************************************************/
char * StripDevice(char * path)
{
	int device;

	if(!FindDevice(path, &device))
		return nullptr;

	const char * prefix = platform->getFileSystem()->getDevicePrefix(device);
	size_t len = prefix ? strlen(prefix) : 0;

	if(len < 2)
		return nullptr;

	// FindDevice() matches the prefix without its trailing '/', so path may
	// end right there (eg. "sd:" or "/vol/external01")
	char * newpath = path + (len - 1);

	if(*newpath == '/')
		newpath++;

	return newpath;
}

/****************************************************************************
 * ConnectShare / CloseShare
 *
 * Owns the retry/prompt policy around connecting; the SmbDriver only makes
 * a single connect() attempt per call.
 ***************************************************************************/
bool ConnectShare(bool silent)
{
	bool invalidShare = strlen(EmuSettings.smbShare.share) == 0;
	bool invalidIp = strlen(EmuSettings.smbShare.host) == 0;

	if(invalidShare || invalidIp)
	{
		if(!silent)
		{
			char msg[50];
			char msg2[100];

			if(invalidShare && invalidIp)
				sprintf(msg, "Check settings.xml.");
			else if(invalidShare)
				sprintf(msg, "Share name is blank.");
			else
				sprintf(msg, "Share IP is blank.");

			sprintf(msg2, "Invalid network settings - %s", msg);
			ErrorPrompt(msg2);
		}
		return false;
	}

	SmbDriver * smb = platform->getFileSystem()->getSmb();
	int retry = 1;
	SmbConnectResult result = SmbConnectResult::InvalidSettings;

	while(retry)
	{
		bool networkUp = smb->isNetworkUp();
		if(!networkUp)
		{
			if(!silent)
				ShowAction("Initializing network...");

			networkUp = smb->ensureNetworkUp();
		}

		if(networkUp)
		{
			if(!silent)
				ShowAction("Connecting to network share...");

			result = smb->connect(EmuSettings.smbShare);
		}
		else
		{
			result = SmbConnectResult::NetworkUnavailable;
		}

		if(!silent)
			CancelAction();

		if(result == SmbConnectResult::Success || silent)
			break;

		retry = ErrorPromptRetry(smb->connectResultMessage(result));
	}

	return result == SmbConnectResult::Success;
}

void CloseShare()
{
	platform->getFileSystem()->getSmb()->disconnect();
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified. Owns the retry/prompt
 * policy; the platform driver just reports a single mount attempt's result.
 ***************************************************************************/
bool ChangeInterface(int device, bool silent)
{
	if(device == DEVICE_AUTO)
		return false;

	if(device == DEVICE_SMB)
		return ConnectShare(silent);

	if(device == DEVICE_DVD)
		ShowAction("Loading DVD...");

	int retry = 1;
	MountResult result = MountResult::DeviceNotFound;

	while(retry)
	{
		result = platform->getFileSystem()->mountStorageDevice(device);

		if(result == MountResult::Success || silent)
			break;

		retry = ErrorPromptRetry(platform->getFileSystem()->mountResultMessage(device, result));
	}

	if(device == DEVICE_DVD)
		CancelAction();

	return result == MountResult::Success;
}

bool ChangeInterface(char * filepath, bool silent)
{
	int device = -1;

	if(!FindDevice(filepath, &device))
	{
		if(!silent)
			ErrorPrompt("Device not found!");
		return false;
	}

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
		if(strcmp(browserList[j].filename, EmuSettings.lastFileLoaded) == 0)
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

static bool ParseDirEntries(int batchSize)
{
	if(!dir)
		return false;

	char *ext;
	struct dirent *entry = nullptr;
	int isdir;
	bool listFull = false;

	int i = 0;

	while(i < batchSize && !parseHalt)
	{
		entry = readdir(dir);

		if(entry == nullptr)
			break;

		if(entry->d_name[0] == '.')
			continue;

		if(parsePrefixLen > 0 && strncmp(entry->d_name, parsePrefix, parsePrefixLen) != 0)
			continue;

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

		if(!AddBrowserEntry())
		{
			listFull = true; // out of room - keep (and sort) what fits, ignore the rest
			break;
		}

		snprintf(browserList[browser.numEntries+i].filename, MAXJOLIET, "%s", entry->d_name);
		browserList[browser.numEntries+i].isdir = isdir; // flag this as a dir

		if(isdir)
		{
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

	if(entry == nullptr || parseHalt || listFull)
	{
		closedir(dir); // close directory
		dir = nullptr;
		
		return false; // no more entries
	}
	return true; // more entries
}

/****************************************************************************
 * ContinueParseTask
 *
 * Queued on the worker thread, indexing a directory PARSE_BATCH_SIZE 
 * entries at a time
 ***************************************************************************/
static int ContinueParseTask(void *)
{
	if(ParseDirEntries(PARSE_BATCH_SIZE))
		QueueBackgroundTask(ContinueParseTask, nullptr);

	return 0;
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int ParseDirectory(bool waitParse, bool filter, const char * namePrefix)
{
	int retry = 1;
	bool mounted = false;
	parseFilter = filter;
	snprintf(parsePrefix, sizeof(parsePrefix), "%s", namePrefix ? namePrefix : "");
	parsePrefixLen = strlen(parsePrefix);
	
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

	// Always add a static "Up One Level" entry
	AddBrowserEntry();
	sprintf(browserList[0].filename, "..");
	sprintf(browserList[0].displayname, "Up One Level");
	browserList[0].isdir = 1; // flag this as a dir
	browserList[0].icon = ICON_FOLDER;
	browser.numEntries++;

	parseHalt = false;

	// Always show the first small batch right away
	bool more = ParseDirEntries(PARSE_FIRST_BATCH);

	if(waitParse) // caller needs the complete list before it can continue
	{
		while(more && !parseHalt)
			more = ParseDirEntries(PARSE_BATCH_SIZE);
	}
	else if(more) // interactive browsing - show what we have, keep going in the background
	{
		QueueBackgroundTask(ContinueParseTask, nullptr);
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
void AllocSaveBuffer ()
{
	SaveBufferLock().lock();
	memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
 * FreeSaveBuffer ()
 * Free the savebuffer memory
 ***************************************************************************/
void FreeSaveBuffer ()
{
	SaveBufferLock().unlock();
}

/****************************************************************************
 * LoadSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
size_t LoadSzFile(char * filepath, unsigned char * rbuffer)
{
	size_t size = 0;

	// halt parsing
	HaltParseThread();

	FILE * fp = fopen (filepath, "rb");
	if (fp)
	{
		size = SzExtractFile(fp, browserList[browser.selIndex].filenum, rbuffer);
		fclose (fp);
	}
	else
	{
		ErrorPrompt("Error opening file!");
	}

	return size;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
size_t LoadFile (char * rbuffer, char *filepath, size_t length, size_t buffersize, bool silent)
{
	char zipbuffer[2048];
	size_t size = 0, offset = 0, readsize = 0;
	int retry = 1;
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	HaltParseThread();

	// open the file
	while(retry)
	{
		if(!ChangeInterface(device, silent))
			break;

		FILE * fp = fopen (filepath, "rb");

		if(!fp)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error opening file!");
			continue;
		}

		if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
		{
			size = fread (rbuffer, 1, length, fp);
		}
		else // load whole file
		{
			readsize = fread (zipbuffer, 1, 32, fp);

			if(!readsize)
			{
				if(silent) // an empty/unreadable file is not worth a prompt when nobody asked for feedback
				{
					fclose (fp);
					break;
				}

				platform->getFileSystem()->invalidateStorageDevice(device);
				retry = ErrorPromptRetry("Error reading file!");
				fclose (fp);
				continue;
			}

			if (IsZipFile (zipbuffer))
			{
				size = UnZipBuffer (fp, (unsigned char *)rbuffer, buffersize); // unzip
			}
			else
			{
				fseeko(fp,0,SEEK_END);
				size = ftello(fp);
				fseeko(fp,0,SEEK_SET);

				if(size > buffersize) {
					size = 0;
				}
				else {
					while(!feof(fp))
					{
						size_t chunk = buffersize - offset; // never read past the end of the caller's buffer
						if(chunk > FILE_READ_CHUNK)
							chunk = FILE_READ_CHUNK;

						ShowProgress ("Loading...", offset, size);
						readsize = fread (rbuffer + offset, 1, chunk, fp); // read in next chunk

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
		fclose (fp);
	}

	CancelAction();
	return size;
}

size_t LoadFile(char * filepath, bool silent)
{
	return LoadFile((char *)savebuffer, filepath, 0, SAVEBUFFERSIZE, silent);
}

#ifndef HW_DOL
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
size_t SaveFile (char * buffer, char *filepath, size_t datasize, bool silent)
{
	size_t written = 0;
	size_t writesize, nextwrite;
	int retry = 1;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	if(datasize == 0)
		return 0;

	HaltParseThread();

	if(!silent)
		ShowAction("Saving...");

	while(!written && retry == 1)
	{
		if(!ChangeInterface(device, silent))
			break;

		FILE * fp = fopen (filepath, "wb");

		if(!fp)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error creating file!");
			continue;
		}

		while(written < datasize)
		{
			if(datasize - written > FILE_WRITE_CHUNK) nextwrite=FILE_WRITE_CHUNK;
			else nextwrite = datasize-written;
			writesize = fwrite (buffer+written, 1, nextwrite, fp);
			if(writesize != nextwrite) break; // write failure
			written += writesize;
		}
		fclose (fp);

		if(written != datasize) written = 0;

		if(!written)
		{
			platform->getFileSystem()->invalidateStorageDevice(device);
			if(silent) break;
			retry = ErrorPromptRetry("Error saving file!");
		}
	}

	if(!silent)
		CancelAction();
	return written;
}

size_t SaveFile(char * filepath, size_t datasize, bool silent)
{
	return SaveFile((char *)savebuffer, filepath, datasize, silent);
}
