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

/****************************************************************************
 * deviceThreading
 ***************************************************************************/
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

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(devicethread))
		usleep(100);
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
		usleep(100);
		devsleep -= 100;
	}

	while (1)
	{
#ifdef HW_RVL
		if(isMounted[METHOD_SD])
		{
			if(!sd->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD] = true;
				isMounted[METHOD_SD] = false;
			}
		}

		if(isMounted[METHOD_USB])
		{
			if(!usb->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_USB] = true;
				isMounted[METHOD_USB] = false;
			}
		}

		InitializeNetwork(SILENT);
		UpdateCheck();
#else
		if(isMounted[METHOD_SD_SLOTA])
		{
			if(!carda->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD_SLOTA] = true;
				isMounted[METHOD_SD_SLOTA] = false;
			}
		}
		if(isMounted[METHOD_SD_SLOTB])
		{
			if(!cardb->isInserted()) // check if the device was removed
			{
				unmountRequired[METHOD_SD_SLOTB] = true;
				isMounted[METHOD_SD_SLOTB] = false;
			}
		}
#endif
		devsleep = 1000*1000; // 1 sec

		while(devsleep > 0)
		{
			if(deviceHalt)
				LWP_SuspendThread(devicethread);
			usleep(100);
			devsleep -= 100;
		}
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
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	fatUnmount("sd:/");
	fatUnmount("usb:/");
#else
	fatUnmount("carda:/");
	fatUnmount("cardb:/");
#endif
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

bool MountFAT(int method)
{
	bool mounted = true; // assume our disc is already mounted
	char name[10];
	const DISC_INTERFACE* disc = NULL;

	switch(method)
	{
#ifdef HW_RVL
		case METHOD_SD:
			sprintf(name, "sd");
			disc = sd;
			break;
		case METHOD_USB:
			sprintf(name, "usb");
			disc = usb;
			break;
#else
		case METHOD_SD_SLOTA:
			sprintf(name, "carda");
			disc = carda;
			break;

		case METHOD_SD_SLOTB:
			sprintf(name, "cardb");
			disc = cardb;
			break;
#endif
		default:
			return false; // unknown device
	}

	sprintf(rootdir, "%s:/", name);

	if(unmountRequired[method])
	{
		unmountRequired[method] = false;
		fatUnmount(rootdir);
		disc->shutdown();
		isMounted[method] = false;
	}
	if(!isMounted[method])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMountSimple(name, disc))
			mounted = false;
	}

	isMounted[method] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(METHOD_SD);
	MountFAT(METHOD_USB);
#else
	MountFAT(METHOD_SD_SLOTA);
	MountFAT(METHOD_SD_SLOTB);
#endif
}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int method, bool silent)
{
	bool mounted = false;

	if(method == METHOD_SD)
	{
		#ifdef HW_RVL
		mounted = MountFAT(METHOD_SD); // try Wii internal SD
		#else
		mounted = MountFAT(METHOD_SD_SLOTA); // try SD Gecko on slot A
		if(!mounted) // internal SD and SD Gecko (on slot A) not found
			mounted = MountFAT(METHOD_SD_SLOTB); // try SD Gecko on slot B
		#endif
		if(!mounted && !silent) // no SD device found
			ErrorPrompt("SD card not found!");
	}
	else if(method == METHOD_USB)
	{
		#ifdef HW_RVL
		mounted = MountFAT(method);

		if(!mounted && !silent)
			ErrorPrompt("USB drive not found!");
		#endif
	}
	else if(method == METHOD_DVD)
	{
		rootdir[0] = 0;
		mounted = MountDVD(silent);
	}
#ifdef HW_RVL
	else if(method == METHOD_SMB)
	{
		sprintf(rootdir, "smb:/");
		mounted = ConnectShare(silent);
	}
#endif
	else if(method == METHOD_MC_SLOTA)
	{
		rootdir[0] = 0;
		mounted = TestMC(CARD_SLOTA, silent);
	}
	else if(method == METHOD_MC_SLOTB)
	{
		rootdir[0] = 0;
		mounted = TestMC(CARD_SLOTB, silent);
	}

	return mounted;
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory(int method)
{
	DIR_ITER *dir = NULL;
	char fulldir[MAXPATHLEN];
	char filename[MAXPATHLEN];
	char tmpname[MAXPATHLEN];
	struct stat filestat;
	char msg[128];
	int retry = 1;
	bool mounted = false;

	// reset browser
	ResetBrowser();

	ShowAction("Loading...");

	// open the directory
	while(dir == NULL && retry == 1)
	{
		mounted = ChangeInterface(method, NOTSILENT);
		sprintf(fulldir, "%s%s", rootdir, browser.dir); // add device to path
		if(mounted) dir = diropen(fulldir);
		if(dir == NULL)
		{
			unmountRequired[method] = true;
			sprintf(msg, "Error opening %s", fulldir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try opening the root dir
	if (dir == NULL)
	{
		if(ChangeInterface(method, SILENT))
		{
			sprintf(browser.dir,"/");
			dir = diropen(rootdir);
			if (dir == NULL)
			{
				sprintf(msg, "Error opening %s", rootdir);
				ErrorPrompt(msg);
				return -1;
			}
		}
	}

	// index files/folders
	int entryNum = 0;

	while(dirnext(dir,filename,&filestat) == 0)
	{
		if(strcmp(filename,".") != 0)
		{
			BROWSERENTRY * newBrowserList = (BROWSERENTRY *)realloc(browserList, (entryNum+1) * sizeof(BROWSERENTRY));

			if(!newBrowserList) // failed to allocate required memory
			{
				ResetBrowser();
				ErrorPrompt("Out of memory: too many files!");
				entryNum = -1;
				break;
			}
			else
			{
				browserList = newBrowserList;
			}
			memset(&(browserList[entryNum]), 0, sizeof(BROWSERENTRY)); // clear the new entry

			strncpy(browserList[entryNum].filename, filename, MAXJOLIET);

			if(strcmp(filename,"..") == 0)
			{
				sprintf(browserList[entryNum].displayname, "Up One Level");
			}
			else
			{
				StripExt(tmpname, filename); // hide file extension
				strncpy(browserList[entryNum].displayname, tmpname, MAXDISPLAY);	// crop name for display
			}

			browserList[entryNum].length = filestat.st_size;
			browserList[entryNum].mtime = filestat.st_mtime;
			browserList[entryNum].isdir = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir

			entryNum++;
		}
	}

	// close directory
	dirclose(dir);

	// Sort the file list
	qsort(browserList, entryNum, sizeof(BROWSERENTRY), FileSortCallback);

	CancelAction();

	browser.numEntries = entryNum;
	return entryNum;
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
LoadFile (char * rbuffer, char *filepath, u32 length, int method, bool silent)
{
	char zipbuffer[2048];
	u32 size = 0;
	u32 readsize = 0;
	char fullpath[MAXPATHLEN];
	int retry = 1;

	switch(method)
	{
		case METHOD_DVD:
			return LoadDVDFile (rbuffer, filepath, length, silent);
			break;
		case METHOD_MC_SLOTA:
			return LoadMCFile (rbuffer, CARD_SLOTA, filepath, silent);
			break;
		case METHOD_MC_SLOTB:
			return LoadMCFile (rbuffer, CARD_SLOTB, filepath, silent);
			break;
	}

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// open the file
	while(!size && retry == 1)
	{
		if(ChangeInterface(method, silent))
		{
			sprintf(fullpath, "%s%s", rootdir, filepath); // add device to filepath
			file = fopen (fullpath, "rb");

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
							size = UnZipBuffer ((unsigned char *)rbuffer, method); // unzip
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
				unmountRequired[method] = true;
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

u32 LoadFile(char * filepath, int method, bool silent)
{
	return LoadFile((char *)savebuffer, filepath, 0, method, silent);
}

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
u32
SaveFile (char * buffer, char *filepath, u32 datasize, int method, bool silent)
{
	char fullpath[MAXPATHLEN];
	u32 written = 0;
	int retry = 1;

	if(datasize == 0)
		return 0;

	ShowAction("Saving...");

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		if(method == METHOD_MC_SLOTA)
			return SaveMCFile (buffer, CARD_SLOTA, filepath, datasize, silent);
		else
			return SaveMCFile (buffer, CARD_SLOTB, filepath, datasize, silent);
	}

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	while(!written && retry == 1)
	{
		if(ChangeInterface(method, silent))
		{
			sprintf(fullpath, "%s%s", rootdir, filepath); // add device to filepath
			file = fopen (fullpath, "wb");

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
			unmountRequired[method] = true;
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

u32 SaveFile(char * filepath, u32 datasize, int method, bool silent)
{
	return SaveFile((char *)savebuffer, filepath, datasize, method, silent);
}
