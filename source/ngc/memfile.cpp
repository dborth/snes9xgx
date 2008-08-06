/****************************************************************************
 * Snes9x 1.50 - GX 2.0
 *
 * NGC Snapshots Memory File System
 *
 * This is a single global memory file controller. Don't even think of opening two
 * at the same time !
 *
 * There's just enough here to do SnapShots - you should add anything else you
 * need.
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <zlib.h>

#include "memfile.h"
#include "snes9x.h"
#include "memmap.h"
#include "soundux.h"
#include "snapshot.h"
#include "srtc.h"

#include "Snes9xGx.h"
#include "filesel.h"
#include "ftfont.h"
#include "smbload.h"
#include "mcsave.h"

extern "C"
{
#include "smb.h"
}

#define MEMBUFFER (512 * 1024)

extern void S9xSRTCPreSaveState ();
extern void NGCFreezeStruct ();
extern bool8 S9xUnfreezeGame (const char *filename);
extern unsigned char savebuffer[];

static int bufoffset;
static char membuffer[MEMBUFFER];

char freezecomment[2][32] = { {"Snes9x GX 004 Freeze"}, {"Freeze"} };


/**
 * GetMem
 *
 * Return x bytes from memory buffer
 */
int
GetMem (char *buffer, int len)
{
  memcpy (buffer, membuffer + bufoffset, len);
  bufoffset += len;

  return len;
}

/**
 * PutMem
 *
 * Put some values in memory buffer
 */
static void
PutMem (char *buffer, int len)
{
  memcpy (membuffer + bufoffset, buffer, len);
  bufoffset += len;
}

void
NGCFreezeBlock (char *name, uint8 * block, int size)
{
  char buffer[512];

//  char msg[90];
//  sprintf (msg, "name=%s", name);
//  WaitPrompt(msg);

  sprintf (buffer, "%s:%06d:", name, size);
  PutMem (buffer, strlen (buffer));
  PutMem ((char *) block, size);
}

/**
 * NGCFreezeMembuffer
 */
static int
NGCFreezeMemBuffer ()
{
    int i;
    char buffer[1024];
    
    bufoffset = 0;
    
    S9xUpdateRTC ();
    S9xSRTCPreSaveState ();
    
    for (i = 0; i < 8; i++)
    {
        SoundData.channels[i].previous16[0] =
        (int16) SoundData.channels[i].previous[0];
        SoundData.channels[i].previous16[1] =
        (int16) SoundData.channels[i].previous[1];
    }
    
    sprintf (buffer, "%s:%04d\n", SNAPSHOT_MAGIC, SNAPSHOT_VERSION);
    PutMem (buffer, strlen (buffer));
    sprintf (buffer, "NAM:%06d:%s%c", (int) strlen (Memory.ROMFilename) + 1,
    Memory.ROMFilename, 0);
    
    PutMem (buffer, strlen (buffer) + 1);
    
    NGCFreezeStruct ();
    
    return 0;
}


/**
 * NGCFreezeGame
 *
 * Do freeze game for Nintendo Gamecube
 */
int
NGCFreezeGame (int where, bool8 silent)
{
    char filename[1024];
    SMBFILE smbfile;
    FILE *handle;
    int len = 0;
    int wrote = 0;
    int offset = 0;
    char msg[100];
    
    if (where == 4)
    {
        /*** Freeze to SMB ***/
        sprintf (filename, "/%s/%s.frz", SNESSAVEDIR, Memory.ROMFilename);
    }
    else if (where == 2 || where == 3)
    {      
        /*** Freeze to SDCard in slot A or slot B ***/

#ifdef SDUSE_LFN
        sprintf (filename, "%s/%s/%s.frz", ROOTSDDIR, SNESSAVEDIR, Memory.ROMFilename);
#else
        /*** Until we have LFN on SD ... ***/
        sprintf (filename, "%s/%s/%08x.frz", ROOTSDDIR, SNESSAVEDIR, Memory.ROMCRC32);
#endif
    }
    else
    {
        /*** Freeze to MC in slot A or slot B ***/
        sprintf (filename, "%s.snz", Memory.ROMFilename);
    }
    
    S9xSetSoundMute (TRUE);
    S9xPrepareSoundForSnapshotSave (FALSE);
    
    NGCFreezeMemBuffer ();
    
    S9xPrepareSoundForSnapshotSave (TRUE);
    S9xSetSoundMute (FALSE);
    
    if (where == 4)  /*** SMB ***/
    {
        ConnectSMB ();
        
        smbfile = SMB_Open (filename, SMB_OPEN_WRITING | SMB_DENY_NONE,
            SMB_OF_CREATE | SMB_OF_TRUNCATE);
        
        if (smbfile)
        {
			if (!silent)
				ShowAction ((char*) "Saving freeze game...");
            
            len = bufoffset;
            offset = 0;
            while (len > 0)
            {
                if (len > 1024)
                    wrote =
                        SMB_Write ((char *) membuffer + offset, 1024, offset,
                        smbfile);
                else
                    wrote =
                        SMB_Write ((char *) membuffer + offset, len, offset,
                        smbfile);
                
                offset += wrote;
                len -= wrote;
            }
            
            SMB_Close (smbfile);
            
            if ( !silent )
            {
                sprintf (filename, "Written %d bytes", bufoffset);
                WaitPrompt (filename);
            }
        }
        else
        {
            char msg[100];
            sprintf(msg, "Couldn't save to SMB:\\%s\\", SNESSAVEDIR);
            WaitPrompt (msg);
        }
    }
    else if (where == 2 || where == 3)  /*** SDCard slot A or slot B ***/
    {
        handle = fopen (filename, "wb");
        
        if (handle > 0)
        {
			if (!silent)
				ShowAction ((char*) "Saving freeze game...");
            
            len = fwrite (membuffer, 1, bufoffset, handle);
            fclose (handle);

            if (len != bufoffset)
                WaitPrompt((char*) "Error writing freeze file");
            else if ( !silent )
            {
                sprintf (filename, "Written %d bytes", bufoffset);
                WaitPrompt (filename);
            }
            
        }
        else
        {
            sprintf(msg, "Couldn't save to %s/%s/", ROOTSDDIR, SNESSAVEDIR);
            WaitPrompt (msg);
        }
    }
    else  /*** MC in slot A or slot B ***/
    {
		if (!silent)
			ShowAction ((char*) "Saving freeze game...");
        
        ClearSaveBuffer ();
        
        /*** Copy in save icon ***/
        int offset = sizeof (saveicon);
        memcpy (savebuffer, saveicon, offset);
        
        /*** And the freezecomment ***/
        sprintf (freezecomment[1], "%s", Memory.ROMName);
        memcpy (savebuffer + offset, freezecomment, 64);
        offset += 64;

        /*** Zip and copy in the freeze ***/
        uLongf DestBuffSize = (uLongf) SAVEBUFFERSIZE;
        int err= compress2((Bytef*)(savebuffer+offset+8), (uLongf*)&DestBuffSize, (const Bytef*)membuffer, (uLongf)bufoffset, Z_BEST_COMPRESSION);
        
        if(err!=Z_OK)
        {
            sprintf (msg, "zip error %s ",zError(err));
            WaitPrompt (msg);
            return 0;
        }

		int zippedsize = (int)DestBuffSize;
		memcpy (savebuffer + offset, &zippedsize, 4);
        offset += 4;
		
		int decompressedsize = (int)bufoffset;
		memcpy (savebuffer + offset, &decompressedsize, 4);
        offset += 4;
        
        offset += zippedsize;
        
        int ret = SaveBufferToMC ( savebuffer, where, filename, offset, SILENT );
        if ( ret && !silent )
        {
            sprintf (filename, "Written %d bytes", ret);
            WaitPrompt (filename);
        }
    }
    
    return 0;
}

/**
 * NGCUnFreezeBlock
 */
int
NGCUnFreezeBlock (char *name, uint8 * block, int size)
{
  char buffer[20], *e;
  int len = 0;
  int rem = 0;

  GetMem (buffer, 11);

  if (strncmp (buffer, name, 3) != 0 || buffer[3] != ':' ||
      buffer[10] != ':' || (len = strtol (&buffer[4], &e, 10)) == 0 ||
      e != buffer + 10)
    {
      return WRONG_FORMAT;
    }

  if (len > size)
    {
      rem = len - size;
      len = size;
    }

  ZeroMemory (block, size);

  GetMem ((char *) block, len);

  if (rem)
    {
      bufoffset += rem;
    }

  return SUCCESS;
}

/**
 * NGCUnfreezeGame
 */
int
NGCUnfreezeGame (int from, bool8 silent)
{
    char filename[1024];
    SMBFILE smbfile;
    FILE *handle;
    int read = 0;
    int offset = 0;
    char msg[80];
    
    bufoffset = 0;
    
    if (from == 4)
    {
        /*** From SMB ***/
        sprintf (filename, "\\%s\\%s.frz", SNESSAVEDIR, Memory.ROMFilename);
        ConnectSMB ();
        
        /*** Read the file into memory ***/
        smbfile =
            SMB_Open (filename, SMB_OPEN_READING | SMB_DENY_NONE, SMB_OF_OPEN);
        
        if (smbfile)
        {
            ShowAction ((char*) "Loading freeze file...");
            while ((read =
                        SMB_Read ((char *) membuffer + offset, 1024, offset,
                        smbfile)) > 0)
                offset += read;
            
            SMB_Close (smbfile);
            
            ShowAction ((char*) "Unpacking freeze file");
            if (S9xUnfreezeGame ("AGAME") != SUCCESS)
            {
                WaitPrompt((char*) "Error thawing");
                return 0;
            }
        }
        else if ( !silent )
        {
            WaitPrompt((char*) "No freeze file found");
            return 0;
        }
    }
    else if (from == 2 || from == 3)  /*** From SD slot A or slot B ***/
    {
        
#ifdef SDUSE_LFN
        sprintf (filename, "%s/%s/%s.frz", ROOTSDDIR, SNESSAVEDIR, Memory.ROMFilename);
#else
        /*** From SDCard ***/
        sprintf (filename, "%s/%s/%08x.frz", ROOTSDDIR, SNESSAVEDIR, Memory.ROMCRC32);
#endif
        
        handle = fopen (filename, "rb");
        
        if (handle > 0)
        {
            ShowAction ((char*) "Loading freeze file...");
            
            offset = 0;
            /*** Usual chunks into memory ***/
            while ((read = fread (membuffer + offset, 1, 2048, handle)) >
                            0)
                offset += read;
            
            fclose (handle);
            
            ShowAction ((char*) "Unpacking freeze file");
            
            if (S9xUnfreezeGame ("AGAME") != SUCCESS)
            {
                WaitPrompt((char*) "Error thawing");
                return 0;
            }
        }
        else if ( !silent )
        {
            WaitPrompt((char*) "No freeze file found");
            return 0;
        }
    }
    else       /*** From MC in slot A or slot B ***/
    {
        ShowAction ((char*) "Loading freeze file...");
        
        sprintf (filename, "%s.snz", Memory.ROMFilename);
        
        int ret = LoadBufferFromMC ( savebuffer, from, filename, silent );
        if ( ret )
        {
            ShowAction ((char*) "Unpacking freeze file");
            
            // skip the saveicon and comment
            offset = (sizeof(saveicon) + 64);
            uLongf zipsize = 0;
            uLongf decompressedsize = 0;
            
            memcpy (&zipsize, savebuffer+offset, 4);
            offset += 4;
            
            memcpy (&decompressedsize, savebuffer+offset, 4);
            offset += 4;
            
            memset(membuffer, 0, MEMBUFFER);

            uLongf DestBuffSize = MEMBUFFER;
            int err= uncompress((Bytef*)membuffer, (uLongf*)&DestBuffSize, (const Bytef*)(savebuffer + offset), zipsize);
            
            if ( err!=Z_OK )
            {
                sprintf (msg, "unzip error %s ",zError(err));
                WaitPrompt (msg);
                return 0;
            }

            if ( DestBuffSize != decompressedsize )
            {
                WaitPrompt((char*) "Unzipped size doesn't match expected size!");
                return 0;
            }

            if (S9xUnfreezeGame ("AGAME") != SUCCESS)
            {
                WaitPrompt((char*) "Error thawing");
                return 0;
            }
        }
        else if ( !silent )
        {
            sprintf(msg, "Couldn't load from MC slot %s", (from ? "B" : "A"));
            WaitPrompt (msg);
            return 0;
        }

    }
    
    return 1;
    
}


void quickLoadFreeze (bool8 silent)
{
    if ( QUICK_SAVE_SLOT >= 0 )
        NGCUnfreezeGame ( QUICK_SAVE_SLOT, silent );
}


void quickSaveFreeze (bool8 silent)
{
    if ( QUICK_SAVE_SLOT >= 0 )
        NGCFreezeGame ( QUICK_SAVE_SLOT, silent );
}


