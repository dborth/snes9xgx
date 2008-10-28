/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * fileop.cpp
 *
 * FAT File operations
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <zlib.h>
#include "memmap.h"

#include "fileop.h"
#include "unzip.h"
#include "video.h"
#include "menudraw.h"
#include "filesel.h"
#include "sram.h"
#include "preferences.h"
#include "snes9xGX.h"

// FAT file pointer - the only one we should ever use!
FILE * fatfile;

/****************************************************************************
 * MountFAT
 * Attempts to mount the FAT device specified
 * Sets libfat to use the device by default
 * Enables read-ahead cache for SD/USB
 ***************************************************************************/
bool MountFAT(PARTITION_INTERFACE part)
{
	bool mounted = fatMountNormalInterface(part, 8);

	if(mounted)
	{
		fatSetDefaultInterface(part);
		#ifdef HW_RVL
		if(part == PI_INTERNAL_SD || part == PI_USBSTORAGE)
			fatEnableReadAhead (part, 6, 64);
		#endif
	}
	return mounted;
}

/****************************************************************************
 * UnmountFAT
 * Unmounts the FAT device specified
 ***************************************************************************/
void UnmountFAT(PARTITION_INTERFACE part)
{
	if(!fatUnmount(part))
		fatUnsafeUnmount(part);
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	UnmountFAT(PI_INTERNAL_SD);
	UnmountFAT(PI_USBSTORAGE);
#endif
	UnmountFAT(PI_SDGECKO_A);
	UnmountFAT(PI_SDGECKO_B);
}

/****************************************************************************
 * ChangeFATInterface
 * Unmounts all devices and attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeFATInterface(int method, bool silent)
{
	bool mounted = false;

	// unmount all FAT devices
	UnmountAllFAT();

	if(method == METHOD_SD)
	{
		#ifdef HW_RVL
		mounted = MountFAT(PI_INTERNAL_SD); // try Wii internal SD
		#endif

		if(!mounted) // internal SD not found
			mounted = MountFAT(PI_SDGECKO_A); // try SD Gecko on slot A
		if(!mounted) // internal SD and SD Gecko (on slot A) not found
			mounted = MountFAT(PI_SDGECKO_B); // try SD Gecko on slot B
		if(!mounted && !silent) // no SD device found
			WaitPrompt ((char *)"SD card not found!");
	}
	else if(method == METHOD_USB)
	{
		#ifdef HW_RVL
		mounted = MountFAT(PI_USBSTORAGE);
		if(!mounted && !silent)
			WaitPrompt ((char *)"USB drive not found!");
		#endif
	}

	return mounted;
}

/***************************************************************************
 * Browse FAT subdirectories
 **************************************************************************/
int
ParseFATdirectory(int method)
{
	int nbfiles = 0;
	DIR_ITER *fatdir;
	char filename[MAXPATHLEN];
	struct stat filestat;
	char msg[128];

	// initialize selection
	selection = offset = 0;

	// Clear any existing values
	memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	// open the directory
	fatdir = diropen(currentdir);
	if (fatdir == NULL)
	{
		sprintf(msg, "Couldn't open %s", currentdir);
		WaitPrompt(msg);

		// if we can't open the dir, open root dir
		sprintf(currentdir,"%s",ROOTFATDIR);

		fatdir = diropen(currentdir);

		if (fatdir == NULL)
		{
			sprintf(msg, "Error opening %s", currentdir);
			WaitPrompt(msg);
			return 0;
		}
	}

	// index files/folders
	while(dirnext(fatdir,filename,&filestat) == 0)
	{
		if(strcmp(filename,".") != 0)
		{
			memset(&filelist[nbfiles], 0, sizeof(FILEENTRIES));
			strncpy(filelist[nbfiles].filename, filename, MAXPATHLEN);
			strncpy(filelist[nbfiles].displayname, filename, MAXDISPLAY+1);	// crop name for display
			filelist[nbfiles].length = filestat.st_size;
			filelist[nbfiles].flags = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir
			nbfiles++;
		}
	}

	// close directory
	dirclose(fatdir);

	// Sort the file list
	qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

	return nbfiles;
}

/****************************************************************************
 * LoadFATFile
 ***************************************************************************/
int
LoadFATFile (char * rbuffer, int length)
{
	char zipbuffer[2048];
	char filepath[MAXPATHLEN];
	u32 size;

	/* Check filename length */
	if (!MakeROMPath(filepath, METHOD_SD))
	{
		WaitPrompt((char*) "Maximum filepath length reached!");
		return -1;
	}

	fatfile = fopen (filepath, "rb");
	if (fatfile > 0)
	{
		if(length > 0 && length <= 2048) // do a partial read (eg: to check file header)
		{
			fread (rbuffer, 1, length, fatfile);
			size = length;
		}
		else // load whole file
		{
			fread (zipbuffer, 1, 2048, fatfile);

			if (IsZipFile (zipbuffer))
			{
				size = UnZipBuffer ((unsigned char *)rbuffer, METHOD_SD);	// unzip from FAT
			}
			else
			{
				// Just load the file up
				fseek(fatfile, 0, SEEK_END);
				size = ftell(fatfile);				// get filesize
				fseek(fatfile, 2048, SEEK_SET);		// seek back to point where we left off
				memcpy (rbuffer, zipbuffer, 2048);	// copy what we already read

				ShowProgress ((char *)"Loading...", 2048, size);

				u32 offset = 2048;
				while(offset < size)
				{
					offset += fread (rbuffer + offset, 1, (1024*512), fatfile); // read in 512K chunks
					ShowProgress ((char *)"Loading...", offset, size);
				}
			}
		}
		fclose (fatfile);
		return size;
	}
	else
	{
		WaitPrompt((char*) "Error opening file");
		return 0;
	}
}

/****************************************************************************
 * LoadFATSzFile
 * Loads the selected file # from the specified 7z into rbuffer
 * Returns file size
 ***************************************************************************/
int
LoadFATSzFile(char * filepath, unsigned char * rbuffer)
{
	u32 size;
	fatfile = fopen (filepath, "rb");
	if (fatfile > 0)
	{
		size = SzExtractFile(filelist[selection].offset, rbuffer);
		fclose (fatfile);
		return size;
	}
	else
	{
		WaitPrompt((char*) "Error opening file");
		return 0;
	}
}

/****************************************************************************
 * Load savebuffer from FAT file
 ***************************************************************************/
int
LoadBufferFromFAT (char *filepath, bool silent)
{
    int size = 0;

    fatfile = fopen (filepath, "rb");

    if (fatfile <= 0)
    {
        if ( !silent )
        {
            char msg[100];
            sprintf(msg, "Couldn't open %s", filepath);
            WaitPrompt (msg);
        }
        return 0;
    }

    // Just load the file up
	fseek(fatfile, 0, SEEK_END); // go to end of file
	size = ftell(fatfile); // get filesize
	fseek(fatfile, 0, SEEK_SET); // go to start of file
	fread (savebuffer, 1, size, fatfile);
	fclose (fatfile);

    return size;
}

/****************************************************************************
 * Write savebuffer to FAT card file
 ***************************************************************************/
int
SaveBufferToFAT (char *filepath, int datasize, bool silent)
{
	if (datasize)
    {
        fatfile = fopen (filepath, "wb");

        if (fatfile <= 0)
        {
            char msg[100];
            sprintf(msg, "Couldn't save %s", filepath);
            WaitPrompt (msg);
            return 0;
        }

        fwrite (savebuffer, 1, datasize, fatfile);
        fclose (fatfile);
    }
    return datasize;
}
