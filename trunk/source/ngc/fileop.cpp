/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * fileop.cpp
 *
 * File operations
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <zlib.h>
#include <malloc.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

#include "snes9xGX.h"
#include "fileop.h"
#include "networkop.h"
#include "dvd.h"
#include "memcardop.h"
#include "gcunzip.h"
#include "video.h"
#include "menu.h"
#include "filebrowser.h"
#include "preferences.h"

#define THREAD_SLEEP 100

unsigned char savebuffer[SAVEBUFFERSIZE] ATTRIBUTE_ALIGN(32);
static mutex_t bufferLock = LWP_MUTEX_NULL;
FILE * file; // file pointer - the only one we should ever use!
bool unmountRequired[9] = { false, false, false, false, false, false, false, false, false };
bool isMounted[9] = { false, false, false, false, false, false, false, false, false };

#ifdef HW_RVL
	const DISC_INTERFACE* sd = &__io_wiisd;
	const DISC_INTERFACE* usb = &__io_usbstorage;
#else
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;
#endif

// folder parsing thread
static lwp_t parsethread = LWP_THREAD_NULL;
static DIR_ITER * dirIter = NULL;
static bool parseHalt = true;
bool ParseDirEntries();

// device thread
static lwp_t devicethread = LWP_THREAD_NULL;
static bool deviceHalt = true;

/****************************************************************************
 * ResumeDeviceThread
 *
 * Signals the device thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeDeviceThread()
{
	deviceHalt = false;
	LWP_ResumeThread(devicethread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the device thread to stop.
 ***************************************************************************/
void
HaltDeviceThread()
{
	deviceHalt = true;

	#ifdef HW_RVL
	if(inNetworkInit) // don't wait for network to initialize
		return;
	#endif

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(devicethread))
		usleep(THREAD_SLEEP);
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

	while(!LWP_ThreadIsSuspended(parsethread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB removed) and
 * initializes the network in the background
 ***************************************************************************/
static int devsleep = 1*1000*1000;

static void *
devicecallback (void *arg)
{
	while(devsleep > 0)
	{
		if(deviceHalt)
			LWP_SuspendThread(devicethread);
		usleep(THREAD_SLEEP);
		devsleep -= THREAD_SLEEP;
	}

	while (1)
	{
#ifdef HW_RVL
		if(isMounted[DEVICE_SD])
		{
			if(!sd->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD] = true;
				isMounted[DEVICE_SD] = false;
			}
		}

		if(isMounted[DEVICE_USB])
		{
			if(!usb->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_USB] = true;
				isMounted[DEVICE_USB] = false;
			}
		}

		UpdateCheck();
		InitializeNetwork(SILENT);
#else
		if(isMounted[DEVICE_SD_SLOTA])
		{
			if(!carda->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD_SLOTA] = true;
				isMounted[DEVICE_SD_SLOTA] = false;
			}
		}
		if(isMounted[DEVICE_SD_SLOTB])
		{
			if(!cardb->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD_SLOTB] = true;
				isMounted[DEVICE_SD_SLOTB] = false;
			}
		}
#endif
		devsleep = 1000*1000; // 1 sec

		while(devsleep > 0)
		{
			if(deviceHalt)
				LWP_SuspendThread(devicethread);
			usleep(THREAD_SLEEP);
			devsleep -= THREAD_SLEEP;
		}
	}
	return NULL;
}

static void *
parsecallback (void *arg)
{
	while(1)
	{
		while(ParseDirEntries())
			usleep(THREAD_SLEEP);
		LWP_SuspendThread(parsethread);
	}
	return NULL;
}

/****************************************************************************
 * InitDeviceThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ***************************************************************************/
void
InitDeviceThread()
{
	LWP_CreateThread (&devicethread, devicecallback, NULL, NULL, 0, 40);
	LWP_CreateThread (&parsethread, parsecallback, NULL, NULL, 0, 80);
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
	fatUnmount("carda:");
	fatUnmount("cardb:");
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
	bool mounted = true; // assume our disc is already mounted
	char name[10], name2[10];
	const DISC_INTERFACE* disc = NULL;

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
			disc = carda;
			break;

		case DEVICE_SD_SLOTB:
			sprintf(name, "cardb");
			sprintf(name2, "cardb:");
			disc = cardb;
			break;
#endif
		default:
			return false; // unknown device
	}

	if(unmountRequired[device])
	{
		unmountRequired[device] = false;
		fatUnmount(name2);
		disc->shutdown();
		isMounted[device] = false;
	}
	if(!isMounted[device])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMountSimple(name, disc))
			mounted = false;
	}
	
	if(!mounted && !silent)
	{
		if(device == DEVICE_SD)
			ErrorPrompt("SD card not found!");
		else
			ErrorPrompt("USB drive not found!");
	}

	isMounted[device] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(DEVICE_SD, SILENT);
	MountFAT(DEVICE_USB, SILENT);
#else
	MountFAT(DEVICE_SD_SLOTA, SILENT);
	MountFAT(DEVICE_SD_SLOTB, SILENT);
#endif
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
	else if(strncmp(filepath, "dvd:", 4) == 0)
	{
		*device = DEVICE_DVD;
		return true;
	}
	else if(strncmp(filepath, "smb:", 4) == 0)
	{
		*device = DEVICE_SMB;
		return true;
	}
	else if(strncmp(filepath, "carda:", 5) == 0)
	{
		*device = DEVICE_SD_SLOTA;
		return true;
	}
	else if(strncmp(filepath, "cardb:", 5) == 0)
	{
		*device = DEVICE_SD_SLOTB;
		return true;
	}
	else if(strncmp(filepath, "mca:", 4) == 0)
	{
		*device = DEVICE_MC_SLOTA;
		return true;
	}
	else if(strncmp(filepath, "mcb:", 4) == 0)
	{
		*device = DEVICE_MC_SLOTB;
		return true;
	}
	return false;
}

char * StripDevice(char * path)
{
	if(path == NULL)
		return NULL;
	
	char * newpath = strchr(path,'/');
	
	if(newpath != NULL)
		newpath++;
	
	return newpath;
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int device, bool silent)
{
	bool mounted = false;
	
	switch(device)
	{
		case DEVICE_SD:
		case DEVICE_USB:
			mounted = MountFAT(device, silent);
			break;
		case DEVICE_DVD:
			mounted = MountDVD(silent);
			break;
#ifdef HW_RVL
		case DEVICE_SMB:
			mounted = ConnectShare(silent);
			break;
#endif
		case DEVICE_MC_SLOTA:
			mounted = TestMC(CARD_SLOTA, silent);
			break;
		case DEVICE_MC_SLOTB:
			mounted = TestMC(CARD_SLOTB, silent);
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
	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;
	
	char * loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	int pos = 0;

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0)
	{
		pos++;
		path[1] = 's';
		path[2] = 'd';
	}
	if(ChangeInterface(&path[pos], SILENT))
		strncpy(appPath, &path[pos], MAXPATHLEN);
	appPath[MAXPATHLEN-1] = 0;
	free(path);
}

bool ParseDirEntries()
{
	if(!dirIter)
		return false;

	char filename[MAXPATHLEN];
	struct stat filestat;

	int i, res;

	for(i=0; i < 20; i++)
	{
		res = dirnext(dirIter,filename,&filestat);

		if(res != 0)
			break;

		if(strcmp(filename,".") == 0)
		{
			i--;
			continue;
		}
		
		if(AddBrowserEntry())
		{
			strncpy(browserList[browser.numEntries+i].filename, filename, MAXJOLIET);
			browserList[browser.numEntries+i].length = filestat.st_size;
			browserList[browser.numEntries+i].mtime = filestat.st_mtime;
			browserList[browser.numEntries+i].isdir = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir
	
			if(browserList[browser.numEntries+i].isdir)
			{
				if(strcmp(filename, "..") == 0)
					sprintf(browserList[browser.numEntries+i].displayname, "Up One Level");
				else
					strncpy(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename, MAXJOLIET);
				browserList[browser.numEntries+i].icon = ICON_FOLDER;
			}
			else
			{
				StripExt(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename); // hide file extension
			}
		}
	}

	// Sort the file list
	if(i >= 0)
	{
		browser.numEntries += i;
		qsort(browserList, browser.numEntries, sizeof(BROWSERENTRY), FileSortCallback);
	}

	if(res != 0 || parseHalt)
	{
		dirclose(dirIter); // close directory
		dirIter = NULL;
		return false; // no more entries
	}
	return true; // more entries
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory(bool waitParse)
{
	char msg[128];
	int retry = 1;
	bool mounted = false;
	
	ResetBrowser(); // reset browser
	
	// open the directory
	while(dirIter == NULL && retry == 1)
	{
		mounted = ChangeInterface(browser.dir, NOTSILENT);
		if(mounted)
			dirIter = diropen(browser.dir);
		else
			return -1;

		if(dirIter == NULL)
		{
			sprintf(msg, "Error opening %s", browser.dir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try higher levels
	if (dirIter == NULL)
	{
		while(!IsDeviceRoot(browser.dir))
		{
			char * devEnd = strrchr(browser.dir, '/');
			devEnd[0] = 0; // strip remaining file listing
			dirIter = diropen(browser.dir);
			if (dirIter)
				break;
		}
	}
	
	if(dirIter == NULL)
		return -1;

	if(IsDeviceRoot(browser.dir))
	{
		browser.numEntries = 1;
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1; // flag this as a dir
	}

	parseHalt = false;
	ParseDirEntries(); // index first 20 entries
	LWP_ResumeThread(parsethread); // index remaining entries

	if(waitParse) // wait for complete parsing
	{
		ShowAction("Loading...");

		while(!LWP_ThreadIsSuspended(parsethread))
			usleep(THREAD_SLEEP);

		CancelAction();
	}

	return browser.numEntries;
}

/****************************************************************************
 * AllocSaveBuffer ()
 * Clear and allocate the savebuffer
 ***************************************************************************/
void
AllocSaveBuffer ()
{
	if(bufferLock == LWP_MUTEX_NULL)
		LWP_MutexInit(&bufferLock, false);

	if(bufferLock != LWP_MUTEX_NULL)
		LWP_MutexLock(bufferLock);
	memset (savebuffer, 0, SAVEBUFFERSIZE);
}

/****************************************************************************
 * FreeSaveBuffer ()
 * Free the savebuffer memory
 ***************************************************************************/
void
FreeSaveBuffer ()
{
	if(bufferLock != LWP_MUTEX_NULL)
		LWP_MutexUnlock(bufferLock);
}

/****************************************************************************
 * LoadSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
u32
LoadSzFile(char * filepath, unsigned char * rbuffer)
{
	u32 size = 0;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	parseHalt = true;

	file = fopen (filepath, "rb");
	if (file > 0)
	{
		size = SzExtractFile(browserList[browser.selIndex].offset, rbuffer);
		fclose (file);
	}
	else
	{
		ErrorPrompt("Error opening file");
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	return size;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
u32
LoadFile (char * rbuffer, char *filepath, u32 length, bool silent)
{
	char zipbuffer[2048];
	u32 size = 0;
	u32 readsize = 0;
	int retry = 1;
	int device;
	
	if(!FindDevice(filepath, &device))
		return 0;

	switch(device)
	{
		case DEVICE_DVD:
			return LoadDVDFile (rbuffer, StripDevice(filepath), length, silent);
			break;
		case DEVICE_MC_SLOTA:
			return LoadMCFile (rbuffer, CARD_SLOTA, StripDevice(filepath), silent);
			break;
		case DEVICE_MC_SLOTB:
			return LoadMCFile (rbuffer, CARD_SLOTB, StripDevice(filepath), silent);
			break;
	}

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	parseHalt = true;

	// open the file
	while(!size && retry == 1)
	{
		if(ChangeInterface(device, silent))
		{
			file = fopen (filepath, "rb");

			if(file > 0)
			{
				if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
				{
					size = fread (rbuffer, 1, length, file);
				}
				else // load whole file
				{
					readsize = fread (zipbuffer, 1, 2048, file);

					if(readsize > 0)
					{
						if (IsZipFile (zipbuffer))
						{
							size = UnZipBuffer ((unsigned char *)rbuffer, device); // unzip
						}
						else
						{
							struct stat fileinfo;
							if(fstat(file->_file, &fileinfo) == 0)
							{
								size = fileinfo.st_size;

								memcpy (rbuffer, zipbuffer, readsize); // copy what we already read

								u32 offset = readsize;
								u32 nextread = 0;
								while(offset < size)
								{
									if(size - offset > 4*1024) nextread = 4*1024;
									else nextread = size-offset;
									ShowProgress ("Loading...", offset, size);
									readsize = fread (rbuffer + offset, 1, nextread, file); // read in next chunk

									if(readsize <= 0 || readsize > nextread)
										break; // read failure

									if(readsize > 0)
										offset += readsize;
								}
								CancelAction();

								if(offset != size) // # bytes read doesn't match # expected
									size = 0;
							}
						}
					}
				}
				fclose (file);
			}
		}
		if(!size)
		{
			if(!silent)
			{
				unmountRequired[device] = true;
				retry = ErrorPromptRetry("Error loading file!");
			}
			else
			{
				retry = 0;
			}
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();
	CancelAction();
	return size;
}

u32 LoadFile(char * filepath, bool silent)
{
	return LoadFile((char *)savebuffer, filepath, 0, silent);
}

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
u32
SaveFile (char * buffer, char *filepath, u32 datasize, bool silent)
{
	u32 written = 0;
	int retry = 1;
	int device;
		
	if(!FindDevice(filepath, &device))
		return 0;

	if(datasize == 0)
		return 0;

	ShowAction("Saving...");

	if(device == DEVICE_MC_SLOTA || device == DEVICE_MC_SLOTB)
	{
		if(device == DEVICE_MC_SLOTA)
			return SaveMCFile (buffer, CARD_SLOTA, StripDevice(filepath), datasize, silent);
		else
			return SaveMCFile (buffer, CARD_SLOTB, StripDevice(filepath), datasize, silent);
	}

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	while(!written && retry == 1)
	{
		if(ChangeInterface(device, silent))
		{
			file = fopen (filepath, "wb");

			if (file > 0)
			{
				u32 writesize, nextwrite;
				while(written < datasize)
				{
					if(datasize - written > 4*1024) nextwrite=4*1024;
					else nextwrite = datasize-written;
					writesize = fwrite (buffer+written, 1, nextwrite, file);
					if(writesize != nextwrite) break; // write failure
					written += writesize;
				}

				if(written != datasize) written = 0;
				fclose (file);
			}
		}
		if(!written)
		{
			unmountRequired[device] = true;
			if(!silent)
				retry = ErrorPromptRetry("Error saving file!");
			else
				retry = 0;
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	CancelAction();
	return written;
}

u32 SaveFile(char * filepath, u32 datasize, bool silent)
{
	return SaveFile((char *)savebuffer, filepath, datasize, silent);
}
