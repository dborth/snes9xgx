/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * freeze.cpp
 *
 * Snapshots Memory File System
 *
 * This is a single global memory file controller.
 * Don't even think of opening two at the same time!
 ***************************************************************************/

#include <malloc.h>
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <zlib.h>

#include "snes9x.h"
#include "memmap.h"
#include "soundux.h"
#include "snapshot.h"
#include "srtc.h"

#include "snes9xGX.h"
#include "freeze.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "pngu.h"

extern void S9xSRTCPreSaveState ();
extern void NGCFreezeStruct ();
extern bool8 S9xUnfreezeGame (const char *filename);

static int bufoffset;

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
 * SaveSnapshot
 ***************************************************************************/
int
SaveSnapshot (char * filepath, bool silent)
{
	int offset = 0; // bytes written (actual)
	int woffset = 0; // bytes written (expected)
	int imgSize = 0; // image screenshot bytes written
	int device;
				
	if(!FindDevice(filepath, &device))
		return 0;

	// save screenshot - I would prefer to do this from gameScreenTex
	if(gameScreenTex2 != NULL)
	{
		AllocSaveBuffer ();

		IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

		if (pngContext != NULL)
		{
			imgSize = PNGU_EncodeFromGXTexture(pngContext, screenwidth, screenheight, gameScreenTex2, 0);
			PNGU_ReleaseImageContext(pngContext);
		}

		if(imgSize > 0)
		{
			char screenpath[1024];
			strncpy(screenpath, filepath, 1024);
			screenpath[strlen(screenpath)-4] = 0;
			sprintf(screenpath, "%s.png", screenpath);
			SaveFile(screenpath, imgSize, silent);
		}

		FreeSaveBuffer ();
	}

	S9xSetSoundMute (TRUE);
	S9xPrepareSoundForSnapshotSave (FALSE);

	AllocSaveBuffer ();
	// copy freeze mem into savebuffer - bufoffset contains # bytes written
	NGCFreezeMemBuffer ();
	woffset = bufoffset;

	S9xPrepareSoundForSnapshotSave (TRUE);
	S9xSetSoundMute (FALSE);

	offset = SaveFile(filepath, woffset, silent);

	FreeSaveBuffer ();

	if(offset > 0) // save successful!
	{
		if(!silent)
			InfoPrompt("Save successful");
		return 1;
	}
    return 0;
}

int
SaveSnapshotAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, Memory.ROMFilename, 0))
		return false;

	return SaveSnapshot(filepath, silent);
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
		bufoffset -= 11; // go back to where we started
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
 * LoadSnapshot
 ***************************************************************************/
int
LoadSnapshot (char * filepath, bool silent)
{
	int offset = 0;
	int result = 0;
	bufoffset = 0;
	int device;
				
	if(!FindDevice(filepath, &device))
		return 0;
	
	AllocSaveBuffer();

    offset = LoadFile(filepath, silent);

	if(offset > 0)
	{
		if (S9xUnfreezeGame ("AGAME") == SUCCESS)
			result = 1;
		else
			ErrorPrompt("Error thawing");
	}
	else
	{
		if(!silent)
			ErrorPrompt("Freeze file not found");
	}
	FreeSaveBuffer();
	return result;
}

int
LoadSnapshotAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, Memory.ROMFilename, 0))
		return false;

	return LoadSnapshot(filepath, silent);
}
