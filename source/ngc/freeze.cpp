/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
  * Michniewski 2008
 * Tantric August 2008
 *
 * freeze.cpp
 *
 * Snapshots Memory File System
 *
 * This is a single global memory file controller.
 * Don't even think of opening two at the same time!
 ***************************************************************************/

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

#include "Snes9xGX.h"
#include "images/saveicon.h"
#include "freeze.h"
#include "filesel.h"
#include "menudraw.h"
#include "smbop.h"
#include "fileop.h"
#include "memcardop.h"

extern void S9xSRTCPreSaveState ();
extern void NGCFreezeStruct ();
extern bool8 S9xUnfreezeGame (const char *filename);

static int bufoffset;

char freezecomment[2][32];


/****************************************************************************
 * GetMem
 *
 * Return x bytes from memory buffer
 ***************************************************************************/
int
GetMem (char *buffer, int len)
{
	memcpy (buffer, savebuffer + bufoffset, len);
	bufoffset += len;

	return len;
}

/****************************************************************************
 * PutMem
 *
 * Put some values in memory buffer
 ***************************************************************************/
static void
PutMem (char *buffer, int len)
{
	memcpy (savebuffer + bufoffset, buffer, len);
	bufoffset += len;
}

void
NGCFreezeBlock (char *name, uint8 * block, int size)
{
	char buffer[512];
	sprintf (buffer, "%s:%06d:", name, size);
	PutMem (buffer, strlen (buffer));
	PutMem ((char *) block, size);
}

/****************************************************************************
 * NGCFreezeMembuffer
 *
 * Copies a snapshot of Snes9x state into memory
 ***************************************************************************/
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


/****************************************************************************
 * NGCFreezeGame
 *
 * Do freeze game for Nintendo Gamecube
 ***************************************************************************/
int
NGCFreezeGame (int method, bool8 silent)
{
	ShowAction ((char*) "Saving...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	char filename[1024];
	int offset = 0;
	char msg[100];

	S9xSetSoundMute (TRUE);
	S9xPrepareSoundForSnapshotSave (FALSE);

	AllocSaveBuffer ();
	NGCFreezeMemBuffer (); // copy freeze mem into savebuffer

	S9xPrepareSoundForSnapshotSave (TRUE);
	S9xSetSoundMute (FALSE);

	if (method == METHOD_SD || method == METHOD_USB) // FAT devices
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filename, "%s/%s/%s.frz", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);
			offset = SaveBufferToFAT (filename, bufoffset, silent);
		}
	}
	else if (method == METHOD_SMB) // SMB
	{
		sprintf (filename, "%s/%s.frz", GCSettings.SaveFolder, Memory.ROMFilename);
		offset = SaveBufferToSMB (filename, bufoffset, silent);
	}
	else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC Slot A or B
	{
		sprintf (filename, "%s.snz", Memory.ROMName);

		/*** Copy in save icon ***/
		int woffset = sizeof (saveicon);
		memcpy (savebuffer, saveicon, woffset);

		/*** And the freezecomment ***/
		sprintf (freezecomment[0], "%s Freeze", VERSIONSTR);
		sprintf (freezecomment[1], Memory.ROMName);
		memcpy (savebuffer + woffset, freezecomment, 64);
		woffset += 64;

		/*** Zip and copy in the freeze ***/
		uLongf DestBuffSize = (uLongf) SAVEBUFFERSIZE;
		int err= compress2((Bytef*)(savebuffer+woffset+8), (uLongf*)&DestBuffSize, (const Bytef*)savebuffer, (uLongf)bufoffset, Z_BEST_COMPRESSION);

		if(err!=Z_OK)
		{
			sprintf (msg, "zip error %s ",zError(err));
			WaitPrompt (msg);
			return 0;
		}

		int zippedsize = (int)DestBuffSize;
		memcpy (savebuffer + woffset, &zippedsize, 4);
		woffset += 4;

		int decompressedsize = (int)bufoffset;
		memcpy (savebuffer + woffset, &decompressedsize, 4);
		woffset += 4;

		woffset += zippedsize;

		if(method == METHOD_MC_SLOTA)
			offset = SaveBufferToMC ( savebuffer, CARD_SLOTA, filename, woffset, SILENT );
		else
			offset = SaveBufferToMC ( savebuffer, CARD_SLOTB, filename, woffset, SILENT );
	}

	FreeSaveBuffer ();

	if(offset > 0) // save successful!
	{
		if(!silent)
			WaitPrompt((char*) "Save successful");
		return 1;
	}
    return 0;
}

/****************************************************************************
 * NGCUnFreezeBlock
 ***************************************************************************/
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

/****************************************************************************
 * NGCUnfreezeGame
 ***************************************************************************/
int
NGCUnfreezeGame (int method, bool8 silent)
{
	ShowAction ((char*) "Loading...");
	char filename[1024];
	int offset = 0;
	char msg[80];

	bufoffset = 0;

    if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because snapshot needs R/W

    AllocSaveBuffer ();

	if (method == METHOD_SD || method == METHOD_USB) // SD & USB
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filename, "%s/%s/%s.frz", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);
			offset = LoadBufferFromFAT (filename, silent);
		}
	}
	else if (method == METHOD_SMB) // Network (SMB)
	{
		sprintf (filename, "%s/%s.frz", GCSettings.SaveFolder, Memory.ROMFilename);
		offset = LoadBufferFromSMB (filename, silent);
	}
    else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC in slot A or slot B
	{
    	sprintf (filename, "%s.snz", Memory.ROMName);

		int ret = 0;

		if(method == METHOD_MC_SLOTA)
			ret = LoadBufferFromMC ( savebuffer, CARD_SLOTA, filename, silent );
		else
			ret = LoadBufferFromMC ( savebuffer, CARD_SLOTB, filename, silent );

		if (ret)
		{
			char zipbuffer[SAVEBUFFERSIZE];

			// skip the saveicon and comment
			offset = (sizeof(saveicon) + 64);
			uLongf zipsize = 0;
			uLongf decompressedsize = 0;

			memcpy (&zipsize, savebuffer+offset, 4);
			offset += 4;

			memcpy (&decompressedsize, savebuffer+offset, 4);
			offset += 4;

			uLongf DestBuffSize = SAVEBUFFERSIZE;
			int err= uncompress((Bytef*)zipbuffer, (uLongf*)&DestBuffSize, (const Bytef*)(savebuffer + offset), zipsize);

			if ( err!=Z_OK )
			{
				sprintf (msg, "Unzip error %s ",zError(err));
				WaitPrompt (msg);
				return 0;
			}
			else if ( DestBuffSize != decompressedsize )
			{
				WaitPrompt((char*) "Unzipped size doesn't match expected size!");
				return 0;
			}
			else
			{
				offset = SAVEBUFFERSIZE;
				memcpy (savebuffer, zipbuffer, SAVEBUFFERSIZE);
			}
		}
    }

	int result = 0;

	if(offset > 0)
	{
		if (S9xUnfreezeGame ("AGAME") == SUCCESS)
			result = 1;
		else
			WaitPrompt((char*) "Error thawing");
	}
	else
	{
		if(!silent)
			WaitPrompt((char*) "Freeze file not found");
	}
	FreeSaveBuffer ();
	return result;
}
