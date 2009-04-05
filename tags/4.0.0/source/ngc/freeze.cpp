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

#include "pngu/pngu.h"

#include "snes9x.h"
#include "memmap.h"
#include "soundux.h"
#include "snapshot.h"
#include "srtc.h"

#include "snes9xGX.h"
#include "memcardop.h"
#include "freeze.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"

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
 * NGCFreezeGame
 *
 * Do freeze game for Nintendo Gamecube
 ***************************************************************************/
int
NGCFreezeGame (char * filepath, int method, bool silent)
{
	int offset = 0; // bytes written (actual)
	int woffset = 0; // bytes written (expected)
	int imgSize = 0; // image screenshot bytes written
	char msg[100];

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return 0;

	S9xSetSoundMute (TRUE);
	S9xPrepareSoundForSnapshotSave (FALSE);

	AllocSaveBuffer ();
	// copy freeze mem into savebuffer - bufoffset contains # bytes written
	NGCFreezeMemBuffer ();
	woffset = bufoffset;

	S9xPrepareSoundForSnapshotSave (TRUE);
	S9xSetSoundMute (FALSE);

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC Slot A or B
	{
		// set freezecomment
		char freezecomment[2][32];
		memset(freezecomment, 0, 64);
		sprintf (freezecomment[0], "%s Snapshot", APPNAME);
		sprintf (freezecomment[1], Memory.ROMName);
		SetMCSaveComments(freezecomment);

		char * zipbuffer = (char *)memalign(32, SAVEBUFFERSIZE);

		// Zip and copy in the freeze
		uLongf DestBuffSize = (uLongf) SAVEBUFFERSIZE;
		int err = compress2((Bytef*)(zipbuffer), (uLongf*)&DestBuffSize, (const Bytef*)savebuffer, (uLongf)bufoffset, Z_BEST_COMPRESSION);

		if(err!=Z_OK)
		{
			sprintf (msg, "Zip error %s",zError(err));
			ErrorPrompt(msg);
			goto done;
		}

		int zippedsize = (int)DestBuffSize;
		int decompressedsize = (int)bufoffset;
		memset(savebuffer, 0, SAVEBUFFERSIZE);
		memcpy(savebuffer, &zippedsize, 4);
		memcpy(savebuffer+4, &decompressedsize, 4);
		memcpy(savebuffer+8, zipbuffer, DestBuffSize);
		free(zipbuffer);

		woffset = zippedsize + 8;
	}

	offset = SaveFile(filepath, woffset, method, silent);

done:

	FreeSaveBuffer ();

	// save screenshot - I would prefer to do this from gameScreenTex
	if(gameScreenTex2 != NULL && method != METHOD_MC_SLOTA && method != METHOD_MC_SLOTB)
	{
		AllocSaveBuffer ();

		IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

		if (pngContext != NULL)
		{
			imgSize = PNGU_EncodeFromGXTexture(pngContext, 640, 480, gameScreenTex2, 0);
			PNGU_ReleaseImageContext(pngContext);
		}

		if(imgSize > 0)
		{
			char screenpath[1024];
			filepath[strlen(filepath)-4] = 0;
			sprintf(screenpath, "%s.png", filepath);
			SaveFile(screenpath, imgSize, method, silent);
		}

		FreeSaveBuffer ();
	}

	if(offset > 0) // save successful!
	{
		if(!silent)
			InfoPrompt("Save successful");
		return 1;
	}
    return 0;
}

int
NGCFreezeGameAuto (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, method, Memory.ROMFilename, 0))
		return false;

	return NGCFreezeGame(filepath, method, silent);
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
 * NGCUnfreezeGame
 ***************************************************************************/
int
NGCUnfreezeGame (char * filepath, int method, bool silent)
{
	int offset = 0;
	int result = 0;
	char msg[80];

	bufoffset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent); // we use 'Save' because snapshot needs R/W

	if(method == METHOD_AUTO)
		return 0;

	AllocSaveBuffer();

    offset = LoadFile(filepath, method, silent);

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB) // MC in slot A or slot B
	{
		if (offset)
		{
			char * zipbuffer = (char *)memalign(32, SAVEBUFFERSIZE);
			memset (zipbuffer, 0, SAVEBUFFERSIZE);
			uLongf zipsize = 0;
			uLongf decompressedsize = 0;
			uLongf DestBuffSize = SAVEBUFFERSIZE;
			memcpy (&zipsize, savebuffer, 4);
			memcpy (&decompressedsize, savebuffer+4, 4);

			int err= uncompress((Bytef*)zipbuffer, (uLongf*)&DestBuffSize, (const Bytef*)(savebuffer+8), zipsize);

			if ( err!=Z_OK )
			{
				offset = 0;
				sprintf (msg, "Unzip error %s",zError(err));
				ErrorPrompt(msg);
			}
			else if ( DestBuffSize != decompressedsize )
			{
				offset = 0;
				ErrorPrompt("Unzipped size doesn't match expected size!");
			}
			else
			{
				offset = decompressedsize;
				memset(savebuffer, 0, SAVEBUFFERSIZE);
				memcpy (savebuffer, zipbuffer, decompressedsize);
			}
			free(zipbuffer);
		}
    }

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
NGCUnfreezeGameAuto (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, method, Memory.ROMFilename, 0))
		return false;

	return NGCUnfreezeGame(filepath, method, silent);
}
