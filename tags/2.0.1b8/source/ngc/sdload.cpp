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
#include "sdcard.h"
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
char rootSDdir[SDCARD_MAX_PATH_LEN];
extern int offset;
extern int selection;


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
     sprintf(temp,"%s",rootSDdir);
     test= strtok(temp,"\\");
     while (test != NULL)
     { 
       size = strlen(test);
       test = strtok(NULL,"\\");
     }
  
     /* remove last subdirectory name */
     size = strlen(rootSDdir) - size - 1;
     rootSDdir[size] = 0;

	 /* handles root name */
	 if (strcmp(rootSDdir,"dev0:") == 0)
	 {
	    sprintf(rootSDdir,"dev0:\\SNESROMS");
	    return -1;
	 }
	 
     return 1;
   }
   else
   {
     /* test new directory namelength */
     if ((strlen(rootSDdir)+1+strlen(filelist[selection].filename)) < SDCARD_MAX_PATH_LEN) 
     {
       /* handles root name */
	   if (strcmp(rootSDdir,"dev0:\\SNESROMS\\..") == 0) sprintf(rootSDdir,"dev0:");
	 
	   /* update current directory name */
       sprintf(rootSDdir, "%s\\%s",rootSDdir, filelist[selection].filename);
       return 1;
     }
     else
     {
         WaitPrompt ("Dirname is too long !"); 
         return -1;
     }
    } 
}

/***************************************************************************
 * Browse SDCARD subdirectories 
 ***************************************************************************/ 
int parseSDdirectory()
{
  int entries = 0;
  int nbfiles = 0;
  DIR *sddir = NULL;
  char tname[1024];
  
  offset = selection = 0;
  
  /* Get a list of files from the actual root directory */ 
  entries = SDCARD_ReadDir (rootSDdir, &sddir);
  
  if (entries < 0) entries = 0;   
  if (entries > MAXFILES) entries = MAXFILES;
    
  /* Move to DVD structure - this is required for the file selector */ 
  while (entries)
  {
      memcpy (tname, &sddir[nbfiles].fname, 1024);
      memset (&filelist[nbfiles], 0, sizeof (FILEENTRIES));
      strncpy(filelist[nbfiles].filename,tname,MAXJOLIET+1);
      filelist[nbfiles].filename[MAXJOLIET] = 0;
      strncpy(filelist[nbfiles].displayname,tname,MAXDISPLAY+1);
      filelist[nbfiles].filename[MAXDISPLAY] = 0;
	  filelist[nbfiles].length = sddir[nbfiles].fsize;
	  filelist[nbfiles].flags = (char)(sddir[nbfiles].fattr & SDCARD_ATTR_DIR);
      nbfiles++;
      entries--;
  }
  
  /*** Release memory ***/
  free(sddir);
  
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
  char filepath[SDCARD_MAX_PATH_LEN];
  sd_file *handle;
  char *rbuffer;
  PKZIPHEADER pkzip;
  z_stream zs;
  int res, outbytes = 0;
  int size;
  int have;

  rbuffer = (char *) Memory.ROM;

  /* Check filename length */
  if ((strlen(rootSDdir)+1+strlen(filelist[selection].filename)) < SDCARD_MAX_PATH_LEN)
     sprintf(filepath, "%s\\%s",rootSDdir,filelist[selection].filename); 
  else
  {
    WaitPrompt ("Maximum Filename Length reached !"); 
    haveSDdir = 0; // reset everything before next access
	return -1;
  }

  handle = SDCARD_OpenFile (filepath, "rb");
  if (handle > 0)
    {
      SDCARD_ReadFile (handle, zipbuffer, 2048);

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
	      SDCARD_CloseFile (handle);
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
	      SDCARD_ReadFile (handle, zipbuffer, 2048);

	    }
	  while (res != Z_STREAM_END
		 && (u32) outbytes < pkzip.uncompressedSize);

	  inflateEnd (&zs);

	  SDCARD_CloseFile (handle);
	  return pkzip.uncompressedSize;

	}
      else
	{
			/*** Just load the file up ***/
	  length = SDCARD_GetFileSize (handle);
	  sprintf (filepath, "Loading %d bytes", length);
	  ShowAction (filepath);
	  memcpy (rbuffer, zipbuffer, 2048);
	  SDCARD_ReadFile (handle, rbuffer + 2048, length - 2048);
	  SDCARD_CloseFile (handle);
	  return length;
	}
    }
  else
    {
      WaitPrompt ("Error opening file");
      return 0;
    }

  return 0;
}



/****************************************************************************
 * Load savebuffer from SD card file
 ****************************************************************************/
int
LoadBufferFromSD (char *filepath, bool8 silent)
{
    sd_file *handle;
    int offset = 0;
    int read = 0;
    
    SDCARD_Init ();
    
    handle = SDCARD_OpenFile (filepath, "rb");
    
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
    while ((read = SDCARD_ReadFile (handle, savebuffer + offset, 1024)) > 0)
    {
        offset += read;
    }
    
    SDCARD_CloseFile (handle);
    
    return offset;
}


/****************************************************************************
 * Write savebuffer to SD card file
 ****************************************************************************/
int
SaveBufferToSD (char *filepath, int datasize, bool8 silent)
{
    sd_file *handle;
    
    SDCARD_Init ();
    
    if (datasize)
    {
        handle = SDCARD_OpenFile (filepath, "wb");
        
        if (handle <= 0)
        {
            char msg[100];
            sprintf(msg, "Couldn't save %s", filepath);
            WaitPrompt (msg);
            return 0;
        }
        
        SDCARD_WriteFile (handle, savebuffer, datasize);
        SDCARD_CloseFile (handle);
    }
    
    return datasize;
}


/****************************************************************************
 * Save SRAM to SD Card
 ****************************************************************************/
void
SaveSRAMToSD (uint8 slot, bool8 silent)
{
    char filepath[1024];
    int datasize;
    int offset;

    ShowAction ("Saving SRAM to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "dev%d:\\%s\\%s.srm", slot, SNESSAVEDIR, Memory.ROMName);
#else
    sprintf (filepath, "dev%d:\\SNESSAVE\\%08x.srm", slot, Memory.ROMCRC32);
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
LoadSRAMFromSD (uint8 slot, bool8 silent)
{
    char filepath[1024];
    int offset = 0;
    
    ShowAction ("Loading SRAM from SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "dev%d:\\%s\\%s.srm", slot, SNESSAVEDIR, Memory.ROMName);
    //  sprintf (filepath, "dev%d:\\%s.srm", Memory.ROMName);
#else
    sprintf (filepath, "dev%d:\\SNESSAVE\\%08x.srm", slot, Memory.ROMCRC32);
    //  sprintf (filepath, "dev0:\\%08x.srm", Memory.ROMCRC32);
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
SavePrefsToSD (uint8 slot, bool8 silent)
{
  char filepath[1024];
  int datasize;
  int offset;

    ShowAction ("Saving prefs to SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "dev%d:\\%s\\%s", slot, SNESSAVEDIR, PREFS_FILE_NAME);
#else
    sprintf (filepath, "dev%d:\\SNESSAVE\\%s", slot, PREFS_FILE_NAME);
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
LoadPrefsFromSD (uint8 slot, bool8 silent)
{
    char filepath[1024];
    int offset = 0;
    
    ShowAction ("Loading prefs from SD...");
    
#ifdef SDUSE_LFN
    sprintf (filepath, "dev%d:\\%s\\%s", slot, SNESSAVEDIR, PREFS_FILE_NAME);
    //  sprintf (filepath, "dev%d:\\%s.srm", Memory.ROMName);
#else
    sprintf (filepath, "dev%d:\\SNESSAVE\\%s", slot, PREFS_FILE_NAME);
    //  sprintf (filepath, "dev0:\\%08x.srm", Memory.ROMCRC32);
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
