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
#include <smb.h>

#include "snes9x.h"
#include "memmap.h"
#include "soundux.h"
#include "snapshot.h"
#include "srtc.h"

#include "memfile.h"
#include "Snes9xGx.h"
#include "filesel.h"
#include "menudraw.h"
#include "smbop.h"
#include "fileop.h"
#include "mcsave.h"

#define MEMBUFFER (512 * 1024)

extern void S9xSRTCPreSaveState ();
extern void NGCFreezeStruct ();
extern bool8 S9xUnfreezeGame (const char *filename);
extern unsigned char savebuffer[];
extern SMBCONN smbconn;

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
NGCFreezeGame (int method, bool8 silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	char filename[1024];
	SMBFILE smbfile;
	FILE *handle;
	int len = 0;
	int wrote = 0;
	int offset = 0;

	char msg[100];

	if (method == METHOD_SD || method == METHOD_USB) // SD
	{
		changeFATInterface(method, NOTSILENT);
		sprintf (filename, "%s/%s/%s.frz", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);
	}
	else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC Slot A or B
	{
		sprintf (filename, "%s.snz", Memory.ROMFilename);
	}
	else if (method == METHOD_SMB) // SMB
	{
		sprintf (filename, "%s/%s.frz", GCSettings.SaveFolder, Memory.ROMFilename);
	}

	S9xSetSoundMute (TRUE);
	S9xPrepareSoundForSnapshotSave (FALSE);

	NGCFreezeMemBuffer ();

	S9xPrepareSoundForSnapshotSave (TRUE);
	S9xSetSoundMute (FALSE);

	if (method == METHOD_SD || method == METHOD_USB) // FAT devices
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
			changeFATInterface(GCSettings.SaveMethod, NOTSILENT);
			sprintf(msg, "Couldn't save to %s/%s/", ROOTFATDIR, GCSettings.SaveFolder);
			WaitPrompt (msg);
		}
	}
	else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC Slot A or B
	{
		if (!silent)
			ShowAction ((char*) "Saving freeze game...");

		ClearSaveBuffer ();

		/*** Copy in save icon ***/
		int offset = sizeof (saveicon);
		memcpy (savebuffer, saveicon, offset);

		/*** And the freezecomment ***/
		sprintf (freezecomment[1], "%s", Memory.ROMFilename);
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

		int ret;

		if(method == METHOD_MC_SLOTA)
			ret = SaveBufferToMC ( savebuffer, CARD_SLOTA, filename, offset, SILENT );
		else
			ret = SaveBufferToMC ( savebuffer, CARD_SLOTB, filename, offset, SILENT );

		if ( ret && !silent )
		{
			sprintf (filename, "Written %d bytes", ret);
			WaitPrompt (filename);
		}
	}
	else if (method == METHOD_SMB) // SMB
	{
		smbfile = SMB_OpenFile (SMBPath(filename), SMB_OPEN_WRITING | SMB_DENY_NONE,	SMB_OF_CREATE | SMB_OF_TRUNCATE, smbconn);

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
				SMB_WriteFile ((char *) membuffer + offset, 1024, offset,
				smbfile);
				else
				wrote =
				SMB_WriteFile ((char *) membuffer + offset, len, offset,
				smbfile);

				offset += wrote;
				len -= wrote;
			}

			SMB_CloseFile (smbfile);

			if ( !silent )
			{
				sprintf (filename, "Written %d bytes", bufoffset);
				WaitPrompt (filename);
			}
		}
		else
		{
			char msg[100];
			sprintf(msg, "Couldn't save to SMB: %s", GCSettings.SaveFolder);
			WaitPrompt (msg);
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
NGCUnfreezeGame (int method, bool8 silent)
{
    char filename[1024];
    SMBFILE smbfile;
    FILE *handle;
    int read = 0;
    int offset = 0;
    char msg[80];

    bufoffset = 0;

    if(method == METHOD_AUTO)
		method = autoLoadMethod();

	if (method == METHOD_SD || method == METHOD_USB) // SD & USB
	{
		changeFATInterface(method, NOTSILENT);
		sprintf (filename, "%s/%s/%s.frz", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);

		handle = fopen (filename, "rb");

		if (handle > 0)
		{
			if ( !silent )
				ShowAction ((char*) "Loading freeze file...");

			offset = 0;
			/*** Usual chunks into memory ***/
			while ((read = fread (membuffer + offset, 1, 2048, handle)) > 0)
				offset += read;

			fclose (handle);

			if ( !silent )
				ShowAction ((char*) "Unpacking freeze file");

			if (S9xUnfreezeGame ("AGAME") != SUCCESS)
			{
				WaitPrompt((char*) "Error thawing");
				return 0;
			}
			return 1;
		}

		WaitPrompt((char*) "Freeze file not found");
		return 0;
    }

    else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC in slot A or slot B
	{
		if ( !silent )
			ShowAction ((char*) "Loading freeze file...");

		sprintf (filename, "%s.snz", Memory.ROMFilename);

		int ret = 0;

		if(method == METHOD_MC_SLOTA)
			LoadBufferFromMC ( savebuffer, CARD_SLOTA, filename, silent );
		else
			LoadBufferFromMC ( savebuffer, CARD_SLOTB, filename, silent );

		if ( ret )
		{
			if ( !silent )
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

		WaitPrompt((char*) "Freeze file not found");
		return 0;
    }
    else if (method == METHOD_SMB) // Network (SMB)
    {
		sprintf (filename, "%s/%s.frz", GCSettings.SaveFolder, Memory.ROMFilename);

		// Read the file into memory
		smbfile = SMB_OpenFile (SMBPath(filename), SMB_OPEN_READING, SMB_OF_OPEN, smbconn);

		if (smbfile)
		{
			if ( !silent )
				ShowAction ((char*) "Loading freeze file...");
			while ((read =
					SMB_ReadFile ((char *) membuffer + offset, 1024, offset,
					smbfile)) > 0)
			offset += read;

			SMB_CloseFile (smbfile);

			if ( !silent )
				ShowAction ((char*) "Unpacking freeze file");
			if (S9xUnfreezeGame ("AGAME") != SUCCESS)
			{
				WaitPrompt((char*) "Error thawing");
				return 0;
			}
		}
		else if ( !silent )
		{
			WaitPrompt((char*) "Freeze file not found");
			return 0;
		}
		return 1;
    }
	return 0; // if we reached here, nothing was done!
}
