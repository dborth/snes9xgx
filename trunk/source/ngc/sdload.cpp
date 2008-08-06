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
#include "ngcunzip.h"
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
char rootSDdir[MAXPATHLEN] = "fat:/";
char currSDdir[MAXPATHLEN];
extern int offset;
extern int selection;

extern FILEENTRIES filelist[MAXFILES];

/***************************************************************************
 * Update SDCARD curent directory name 
 ***************************************************************************/ 
int updateSDdirname()
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
				sprintf(currSDdir,"%s",rootSDdir);
	 
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
 * Browse SDCARD subdirectories 
 ***************************************************************************/ 
int parseSDdirectory() {
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
        sprintf(currSDdir,"%s",rootSDdir);	// if we can't open the previous dir, open root dir
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
  
    return nbfiles;
}

/****************************************************************************
 * LoadSDFile
 ****************************************************************************/
extern int haveSDdir;
int
LoadSDFile (char *filename, int length)
{
  char zipbuffer[2048];
  char filepath[MAXPATHLEN];
  FILE *handle;
  char *rbuffer;
  PKZIPHEADER pkzip;
  z_stream zs;
  int res, outbytes = 0;
  int size;
  int have;

  rbuffer = (char *) Memory.ROM;

  /* Check filename length */
  if ((strlen(currSDdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
     sprintf(filepath, "%s/%s",currSDdir,filelist[selection].filename); 
  else
  {
    WaitPrompt((char*) "Maximum Filename Length reached !"); 
    haveSDdir = 0; // reset everything before next access
	return -1;
  }

  handle = fopen (filepath, "rb");
  if (handle > 0)
    {
	      fread (zipbuffer, 1, 2048, handle);

	      if (IsZipFile (zipbuffer))
		{
				/*** Unzip the ROM ***/
		  memcpy (&pkzip, zipbuffer, sizeof (PKZIPHEADER));
		  pkzip.uncompressedSize = FLIP32 (pkzip.uncompressedSize);
		  memset (&zs, 0, sizeof (zs));
		  zs.zalloc = Z_NULL;
		  zs.zfree = Z_NULL;
		  zs.opaque = Z_NULL;
		  zs.avail_in = 0;
		  zs.next_in = Z_NULL;

		  res = inflateInit2 (&zs, -MAX_WBITS);

		  if (res != Z_OK)
		    {
		      fclose (handle);
		      return 0;
		    }

		  size = (sizeof (PKZIPHEADER) +
			  FLIP16 (pkzip.filenameLength) +
			  FLIP16 (pkzip.extraDataLength));

		  do
		    {
		      zs.avail_in = 2048 - size;
		      zs.next_in = (Bytef *) zipbuffer + size;

		      do
			{
			  zs.avail_out = 16384;
			  zs.next_out = (Bytef *) output;

			  res = inflate (&zs, Z_NO_FLUSH);

			  have = 16384 - zs.avail_out;

			  if (have)
			    {
			      memcpy (rbuffer + outbytes, output, have);
			      outbytes += have;
			    }

			}
		      while (zs.avail_out == 0);

		      sprintf (filepath, "Read %d bytes of %d", outbytes,
			       pkzip.uncompressedSize);
		      //ShowAction (filepath);
		      ShowProgress (filepath, outbytes, pkzip.uncompressedSize);

		      size = 0;
		      fread (zipbuffer, 1, 2048, handle);

		    }
		  while (res != Z_STREAM_END
			 && (u32) outbytes < pkzip.uncompressedSize);

		  inflateEnd (&zs);

		  fclose (handle);
		  return pkzip.uncompressedSize;

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
void SaveSRAMToSD (int slot, bool silent)
{
    char filepath[1024];
    int datasize;
    int offset;

    ShowAction ((char*) "Saving SRAM to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s.srm", rootSDdir, SNESSAVEDIR, Memory.ROMName);
#else
    sprintf (filepath, "%s/SNESSAVE/%08x.srm", rootSDdir, Memory.ROMCRC32);
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
LoadSRAMFromSD (int slot, bool silent)
{
    char filepath[1024];
    int offset = 0;
    
    ShowAction ((char*) "Loading SRAM from SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s.srm", rootSDdir, SNESSAVEDIR, Memory.ROMName);
#else
    sprintf (filepath, "%s/%s/%08x.srm", rootSDdir, SNESSAVEDIR, Memory.ROMCRC32);
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
SavePrefsToSD (int slot, bool silent)
{
  char filepath[1024];
  int datasize;
  int offset;

    ShowAction ((char*) "Saving prefs to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s", rootSDdir, SNESSAVEDIR, PREFS_FILE_NAME);
#else
    sprintf (filepath, "%s/%s/%s", rootSDdir, SNESSAVEDIR, PREFS_FILE_NAME);
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
LoadPrefsFromSD (int slot, bool silent)
{
    char filepath[1024];
    int offset = 0;
    
    ShowAction ((char*) "Loading prefs from SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "%s/%s/%s", rootSDdir, SNESSAVEDIR, PREFS_FILE_NAME);
#else
    sprintf (filepath, "%s/%s/%s", rootSDdir, SNESSAVEDIR, PREFS_FILE_NAME);
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
