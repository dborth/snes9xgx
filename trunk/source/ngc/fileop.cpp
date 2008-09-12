/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
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

FILE * filehandle;

extern unsigned char savebuffer[];
extern char output[16384];
extern int offset;
extern int selection;
extern char currentdir[MAXPATHLEN];
extern FILEENTRIES filelist[MAXFILES];

/****************************************************************************
 * fat_is_mounted
 * to check whether FAT media are detected.
 ***************************************************************************/

bool FatIsMounted(PARTITION_INTERFACE partition) {
    char prefix[] = "fatX:/";
    prefix[3] = partition + '0';
    DIR_ITER *dir = diropen(prefix);
    if (dir) {
        dirclose(dir);
        return true;
    }
    return false;
}

/****************************************************************************
 * changeFATInterface
 * Checks if the device (method) specified is available, and
 * sets libfat to use the device
 ***************************************************************************/
bool ChangeFATInterface(int method, bool silent)
{
	bool devFound = false;

	if(method == METHOD_SD)
	{
		// check which SD device is loaded

		#ifdef HW_RVL
		if (FatIsMounted(PI_INTERNAL_SD))
		{
			devFound = true;
			fatSetDefaultInterface(PI_INTERNAL_SD);
		}
		#endif

		if (!devFound && FatIsMounted(PI_SDGECKO_A))
		{
			devFound = true;
			fatSetDefaultInterface(PI_SDGECKO_A);
		}
		if(!devFound && FatIsMounted(PI_SDGECKO_B))
		{
			devFound = true;
			fatSetDefaultInterface(PI_SDGECKO_B);
		}
		if(!devFound)
		{
			if(!silent)
				WaitPrompt ((char *)"SD card not found!");
		}
	}
	else if(method == METHOD_USB)
	{
		#ifdef HW_RVL
		if(FatIsMounted(PI_USBSTORAGE))
		{
			devFound = true;
			fatSetDefaultInterface(PI_USBSTORAGE);
		}
		else
		{
			if(!silent)
				WaitPrompt ((char *)"USB flash drive not found!");
		}
		#endif
	}

	return devFound;
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
LoadFATFile (char *filename, int length)
{
	char zipbuffer[2048];
	char filepath[MAXPATHLEN];
	FILE *handle;
	unsigned char *rbuffer;
	u32 size;

	rbuffer = (unsigned char *) Memory.ROM;

	/* Check filename length */
	if ((strlen(currentdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
		sprintf(filepath, "%s/%s",currentdir,filelist[selection].filename);
	else
	{
		WaitPrompt((char*) "Maximum filepath length reached!");
		return -1;
	}

	handle = fopen (filepath, "rb");
	if (handle > 0)
	{
		fread (zipbuffer, 1, 2048, handle);

		if (IsZipFile (zipbuffer))
		{
			size = UnZipFile (rbuffer, handle);	// unzip from FAT
		}
		else
		{
			// Just load the file up
			fseek(handle, 0, SEEK_END);
			length = ftell(handle);				// get filesize
			fseek(handle, 2048, SEEK_SET);		// seek back to point where we left off
			memcpy (rbuffer, zipbuffer, 2048);	// copy what we already read
			fread (rbuffer + 2048, 1, length - 2048, handle);
			size = length;
		}
		fclose (handle);
		return size;
	}
	else
	{
		WaitPrompt((char*) "Error opening file");
		return 0;
	}

	return 0;
}

/****************************************************************************
 * Load savebuffer from FAT file
 ***************************************************************************/
int
LoadBufferFromFAT (char *filepath, bool silent)
{
	FILE *handle;
    int boffset = 0;
    int read = 0;

    ClearSaveBuffer ();

    handle = fopen (filepath, "rb");

    if (handle <= 0)
    {
        if ( !silent )
        {
            char msg[100];
            sprintf(msg, "Couldn't open %s", filepath);
            WaitPrompt (msg);
        }
        return 0;
    }

    /*** This is really nice, just load the file and decode it ***/
    while ((read = fread (savebuffer + boffset, 1, 1024, handle)) > 0)
    {
        boffset += read;
    }

    fclose (handle);

    return boffset;
}

/****************************************************************************
 * Write savebuffer to FAT card file
 ***************************************************************************/
int
SaveBufferToFAT (char *filepath, int datasize, bool silent)
{
	FILE *handle;

    if (datasize)
    {
        handle = fopen (filepath, "wb");

        if (handle <= 0)
        {
            char msg[100];
            sprintf(msg, "Couldn't save %s", filepath);
            WaitPrompt (msg);
            return 0;
        }

        fwrite (savebuffer, 1, datasize, handle);
        fclose (handle);
    }

    ClearSaveBuffer ();
    return datasize;
}
