/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * fileop.cpp
 *
 * File operations
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include "fileop.h"
#include "unzip.h"
#include "memmap.h"
#include "video.h"
#include "menudraw.h"
#include "dvd.h"
#include "filesel.h"
#include "sram.h"
#include "preferences.h"
#include "snes9xGx.h"

#include <zlib.h>
extern unsigned char savebuffer[];
extern char output[16384];
FILE * filehandle;

char currFATdir[MAXPATHLEN];
extern int offset;
extern int selection;

extern FILEENTRIES filelist[MAXFILES];

/****************************************************************************
 * fat_is_mounted
 * to check whether FAT media are detected.
 ****************************************************************************/

bool fat_is_mounted(PARTITION_INTERFACE partition) {
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
****************************************************************************/
bool changeFATInterface(int method)
{
	bool devFound = false;

	if(method == METHOD_SD)
	{
		// check which SD device is loaded

		#ifdef HW_RVL
		if (fat_is_mounted(PI_INTERNAL_SD))
		{
			devFound = true;
			fatSetDefaultInterface(PI_INTERNAL_SD);
		}
		#endif

		if (!devFound && fat_is_mounted(PI_SDGECKO_A))
		{
			devFound = true;
			fatSetDefaultInterface(PI_SDGECKO_A);
		}
		if(!devFound && fat_is_mounted(PI_SDGECKO_B))
		{
			devFound = true;
			fatSetDefaultInterface(PI_SDGECKO_B);
		}
	}
	else if(method == METHOD_USB)
	{
		#ifdef HW_RVL
		if(fat_is_mounted(PI_USBSTORAGE))
		{
			devFound = true;
			fatSetDefaultInterface(PI_USBSTORAGE);
		}
		#endif
	}

	return devFound;
}

/****************************************************************************
 * fat_enable_readahead_all
 ****************************************************************************/

void fat_enable_readahead_all() {
    int i;
    for (i=1; i <= 4; ++i) {
        if (fat_is_mounted((PARTITION_INTERFACE)i)) fatEnableReadAhead((PARTITION_INTERFACE)i, 64, 128);
    }
}

/****************************************************************************
 * fat_remount
 ****************************************************************************/

bool fat_remount(PARTITION_INTERFACE partition) {
	//ShowAction("remounting...");
	/*	// removed to make usb work...
	if (fat_is_mounted(partition))
	{
		fatUnmount(partition);
	}
	*/

	fatMountNormalInterface(partition, 8);
	fatSetDefaultInterface(partition);
	//fatEnableReadAhead(partition, 64, 128);

	if (fat_is_mounted(partition))
	{
		//ShowAction("remount successful.");
		sleep(1);
		return 1;
	} else {
		ShowAction("FAT mount failed.");
		sleep(1);
		return 0;
	}
}

/***************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
static int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((FILEENTRIES *)f1)->filename[0] == '.' || ((FILEENTRIES *)f2)->filename[0] == '.')
	{
		if(strcmp(((FILEENTRIES *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((FILEENTRIES *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((FILEENTRIES *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((FILEENTRIES *)f1)->flags == 1 && ((FILEENTRIES *)f2)->flags == 0) return -1;
	if(((FILEENTRIES *)f1)->flags == 0 && ((FILEENTRIES *)f2)->flags == 1) return 1;

	return stricmp(((FILEENTRIES *)f1)->filename, ((FILEENTRIES *)f2)->filename);
}

/***************************************************************************
 * Update FATCARD curent directory name
 ***************************************************************************/
int updateFATdirname(int method)
{
	int size=0;
	char *test;
	char temp[1024];

	/* current directory doesn't change */
	if (strcmp(filelist[selection].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(filelist[selection].filename,"..") == 0)
	{
		/* determine last subdirectory namelength */
		sprintf(temp,"%s",currFATdir);
		test = strtok(temp,"/");
		while (test != NULL)
		{
			size = strlen(test);
			test = strtok(NULL,"/");
		}

		/* remove last subdirectory name */
		size = strlen(currFATdir) - size - 1;
		currFATdir[size] = 0;

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(currFATdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
		{
			/* handles root name */
			sprintf(temp, "/%s/..", GCSettings.LoadFolder);
			if (strcmp(currFATdir, temp) == 0)
			{
				sprintf(currFATdir,"%s",ROOTFATDIR);
			}

			/* update current directory name */
			sprintf(currFATdir, "%s/%s",currFATdir, filelist[selection].filename);
			return 1;
		}
		else
		{
			WaitPrompt((char*)"Dirname is too long !");
			return -1;
		}
	}
}

/***************************************************************************
 * Browse FAT subdirectories
 ***************************************************************************/
int parseFATdirectory(int method)
{
    int nbfiles = 0;
    DIR_ITER *fatdir;
    char filename[MAXPATHLEN];
    struct stat filestat;
        char msg[128];

    /* initialize selection */
    selection = offset = 0;

    /* open the directory */
    fatdir = diropen(currFATdir);
    if (fatdir == NULL)
	{
        sprintf(msg, "Couldn't find %s", currFATdir);
        WaitPrompt(msg);

		// if we can't open the previous dir, open root dir
        sprintf(currFATdir,"%s",ROOTFATDIR);

        fatdir = diropen(currFATdir);

        if (fatdir == NULL)
		{
            sprintf(msg, "Error opening %s", currFATdir);
            WaitPrompt(msg);
            return 0;
        }
    }

  /* Move to DVD structure - this is required for the file selector */
    while(dirnext(fatdir,filename,&filestat) == 0) {
        if(strcmp(filename,".") != 0) {
            memset(&filelist[nbfiles], 0, sizeof(FILEENTRIES));
            strncpy(filelist[nbfiles].filename, filename, MAXPATHLEN);
			strncpy(filelist[nbfiles].displayname, filename, MAXDISPLAY+1);	// crop name for display
            filelist[nbfiles].length = filestat.st_size;
            filelist[nbfiles].flags = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1;
            nbfiles++;
        }
	}

    /*** close directory ***/
    dirclose(fatdir);

	/* Sort the file list */
	qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);

    return nbfiles;
}

/****************************************************************************
 * LoadFATFile
 ****************************************************************************/
extern int haveFATdir;
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
  if ((strlen(currFATdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
     sprintf(filepath, "%s/%s",currFATdir,filelist[selection].filename);
  else
  {
    WaitPrompt((char*) "Maximum Filename Length reached !");
    haveFATdir = 0; // reset everything before next access
	return -1;
  }

  handle = fopen (filepath, "rb");
  if (handle > 0)
    {
	      fread (zipbuffer, 1, 2048, handle);

	      if (IsZipFile (zipbuffer))
		{
			/*** Unzip the ROM ***/
		  size = UnZipBuffer (rbuffer, 0, 0, handle);	// unzip from FAT

		  fclose (handle);
		  return size;

		}
	      else
		{
				/*** Just load the file up ***/

		  fseek(handle, 0, SEEK_END);
		  length = ftell(handle);				// get filesize
		  fseek(handle, 2048, SEEK_SET);		// seek back to point where we left off

		  sprintf (filepath, "Loading %d bytes", length);
		  ShowAction (filepath);
		  memcpy (rbuffer, zipbuffer, 2048);	// copy what we already read
		  fread (rbuffer + 2048, 1, length - 2048, handle);
		  fclose (handle);

		  return length;
		}
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
 ****************************************************************************/
int
LoadBufferFromFAT (char *filepath, bool silent)
{
	FILE *handle;
    int offset = 0;
    int read = 0;

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

    memset (savebuffer, 0, 0x22000);

    /*** This is really nice, just load the file and decode it ***/
    while ((read = fread (savebuffer + offset, 1, 1024, handle)) > 0)
    {
        offset += read;
    }

    fclose (handle);

    return offset;
}

/****************************************************************************
 * Write savebuffer to FAT card file
 ****************************************************************************/
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

    return datasize;
}
