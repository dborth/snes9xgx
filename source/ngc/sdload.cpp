/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * sdload.cpp
 *
 * Load ROMS from SD Card
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include "sdload.h"
#include "unzip.h"
#include "memmap.h"
#include "video.h"
#include "ftfont.h"
#include "dvd.h"
#include "filesel.h"
#include "sram.h"
#include "preferences.h"

#include <zlib.h>
extern unsigned char savebuffer[];
extern char output[16384];
FILE * filehandle;

char currSDdir[MAXPATHLEN];
extern int offset;
extern int selection;

extern int loadtype;

extern FILEENTRIES filelist[MAXFILES];

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
 * Update FAT (sdcard, usb) curent directory name 
 ***************************************************************************/ 
int updateFATdirname()
{
  int size=0;
  char *test;
  char temp[1024];

   /* current directory doesn't change */
   if (strcmp(filelist[selection].filename,".") == 0) return 0; 
   
   /* go up to parent directory */
   else if (strcmp(filelist[selection].filename,"..") == 0) 
   {
     /* determine last subdirectory namelength */
     sprintf(temp,"%s",currSDdir);
        test = strtok(temp,"/");
        while (test != NULL) { 
       size = strlen(test);
            test = strtok(NULL,"/");
     }
  
     /* remove last subdirectory name */
     size = strlen(currSDdir) - size - 1;
     currSDdir[size] = 0;

	 /* handles root name */
        if (strcmp(currSDdir, "/") == 0)
            
	 
     return 1;
   }
   else		/* Open a directory */
   {
     /* test new directory namelength */
		if ((strlen(currSDdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN) 
		{
			/* handles root name */
			sprintf(temp, "/%s/..", SNESROMDIR);
			if (strcmp(currSDdir, temp) == 0) 
				sprintf(currSDdir,"%s",ROOTSDDIR);
	 
			/* update current directory name */
			sprintf(currSDdir, "%s/%s",currSDdir, filelist[selection].filename);
			return 1;
		} else {
			WaitPrompt((char*)"Dirname is too long !"); 
			return -1;
		}
    } 
}

/***************************************************************************
 * Browse FAT (sdcard, usb) subdirectories 
 ***************************************************************************/ 
int parseFATdirectory() {
    int nbfiles = 0;
    DIR_ITER *sddir;
    char filename[MAXPATHLEN];
    struct stat filestat;
        char msg[128];
    
    /* initialize selection */
    selection = offset = 0;

    /* open the directory */ 
    sddir = diropen(currSDdir);
    if (sddir == NULL) {
		/*** if we can't open the previous dir, open root dir ***/
		if (loadtype == LOAD_USB)
			sprintf(currSDdir,"fat4:/");
		else // LOAD_SDC
	        //sprintf(currSDdir,"%s",ROOTSDDIR);	
			sprintf(currSDdir,"fat3:/");
			
        sddir = diropen(currSDdir);
        WaitPrompt(msg);
        if (sddir == NULL) {
            sprintf(msg, "Error opening %s", currSDdir);
            WaitPrompt(msg);
            return 0;
        }
    }
    
  /* Move to DVD structure - this is required for the file selector */ 
    while(dirnext(sddir,filename,&filestat) == 0) {
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
    dirclose(sddir);
	
	/* Sort the file list */
	qsort(filelist, nbfiles, sizeof(FILEENTRIES), FileSortCallback);
  
    return nbfiles;
}

/****************************************************************************
 * LoadSDFile
 ****************************************************************************/
extern bool haveSDdir;
extern bool haveUSBdir;
int
LoadSDFile (char *filename, int length)
{
  char zipbuffer[2048];
  char filepath[MAXPATHLEN];
  FILE *handle;
  unsigned char *rbuffer;
  u32 size;

  rbuffer = (unsigned char *) Memory.ROM;

  /* Check filename length */
  if ((strlen(currSDdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
     sprintf(filepath, "%s/%s",currSDdir,filelist[selection].filename); 
  else
  {
    WaitPrompt((char*) "Maximum Filename Length reached !"); 
    haveSDdir = 0; // reset everything before next access
	haveUSBdir = 0;
	return -1;
  }
  
  handle = fopen (filepath, "rb");
  if (handle > 0)
    {
	      fread (zipbuffer, 1, 2048, handle);

	      if (IsZipFile (zipbuffer))
		{
			/*** Unzip the ROM ***/
		  size = UnZipBuffer (rbuffer, 0, 0, handle);	// unzip from SD

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
 * Load savebuffer from SD card file
 ****************************************************************************/
int
LoadBufferFromSD (char *filepath, bool silent)
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
 * Write savebuffer to SD card file
 ****************************************************************************/
int
SaveBufferToSD (char *filepath, int datasize, bool silent)
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


/****************************************************************************
 * Save SRAM to SD Card
 ****************************************************************************/
void SaveSRAMToSD (bool silent)
{
    char filepath[1024];
    int datasize;
    int offset;

	if (!silent)
		ShowAction ((char*) "Saving SRAM to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s.srm", ROOTSDDIR, SNESSAVEDIR, Memory.ROMFilename);
#else
    sprintf (filepath, "%s/%s/%08x.srm", ROOTSDDIR, SNESSAVEDIR, Memory.ROMCRC32);
#endif
    
    datasize = prepareEXPORTsavedata ();

    if ( datasize )
    {
        offset = SaveBufferToSD (filepath, datasize, silent);
        
        if ( (offset > 0) && (!silent) )
        {
            sprintf (filepath, "Wrote %d bytes", offset);
            WaitPrompt (filepath);
        }
    }
}


/****************************************************************************
 * Load SRAM From SD Card
 ****************************************************************************/
void
LoadSRAMFromSD (bool silent)
{
    char filepath[MAXPATHLEN];
    int offset = 0;
    
    ShowAction ((char*) "Loading SRAM from SD...");
	
	// check for 'old' version of sram
		sprintf (filepath, "%s/%s/%s.srm", ROOTSDDIR, SNESSAVEDIR, Memory.ROMName); // Build old SRAM filename
		
		offset = LoadBufferFromSD (filepath, silent);	// load file
		
		if (offset > 0) // old sram found
		{
			if (WaitPromptChoice ((char*)"Old SRAM found. Convert and delete?", (char*)"Cancel", (char*)"Do it"))
			{
				decodesavedata (offset);
				remove (filepath);	// delete old sram
				SaveSRAMToSD (silent);
			}
		}
	//
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s.srm", ROOTSDDIR, SNESSAVEDIR, Memory.ROMFilename);
#else
    sprintf (filepath, "%s/%s/%08x.srm", ROOTSDDIR, SNESSAVEDIR, Memory.ROMCRC32);
#endif
    
    offset = LoadBufferFromSD (filepath, silent);
    
    if (offset > 0)
    {
        decodesavedata (offset);
        if ( !silent )
        {
            sprintf (filepath, "Loaded %d bytes", offset);
            WaitPrompt(filepath);
        }
        S9xSoftReset();
    }
}


/****************************************************************************
 * Save Preferences to SD Card
 ****************************************************************************/
void
SavePrefsToSD (bool silent)
{
  char filepath[1024];
  int datasize;
  int offset;

	if (!silent)
		ShowAction ((char*) "Saving prefs to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s", ROOTSDDIR, SNESSAVEDIR, PREFS_FILE_NAME);
#else
    sprintf (filepath, "%s/%s/%s", ROOTSDDIR, SNESSAVEDIR, PREFS_FILE_NAME);
#endif
    
    datasize = preparePrefsData ();
    offset = SaveBufferToSD (filepath, datasize, silent);
    
    if ( (offset > 0) && (!silent) )
    {
        sprintf (filepath, "Wrote %d bytes", offset);
        WaitPrompt (filepath);
    }
}


/****************************************************************************
 * Load Preferences from SD Card
 ****************************************************************************/
void
LoadPrefsFromSD (bool silent)
{
    char filepath[1024];
    int offset = 0;
    
    ShowAction ((char*) "Loading prefs from SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s", ROOTSDDIR, SNESSAVEDIR, PREFS_FILE_NAME);
#else
    sprintf (filepath, "%s/%s/%s", ROOTSDDIR, SNESSAVEDIR, PREFS_FILE_NAME);
#endif
    
    offset = LoadBufferFromSD (filepath, silent);
    
    if (offset > 0)
    {
        decodePrefsData ();
        if ( !silent )
        {
            sprintf (filepath, "Loaded %d bytes", offset);
            WaitPrompt(filepath);
        }
    }
}
