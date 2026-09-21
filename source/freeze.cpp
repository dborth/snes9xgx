/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * freeze.cpp
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snes9xgx.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "freeze.h"
#include "video.h"

#include "snes9x/snes9x.h"
#include "snes9x/port.h"
#include "snes9x/memmap.h"
#include "snes9x/snapshot.h"
#include "snes9x/language.h"

bool8 S9xOpenSnapshotFile(const char *, bool8, STREAM *)
{
	return FALSE;
}

void S9xCloseSnapshotFile(STREAM)
{

}

/****************************************************************************
 * SaveSnapshot
 ***************************************************************************/

int
SaveSnapshot (char * filepath, bool silent)
{
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	// save screenshot
	if(gameScreenPng.size > 0)
	{
		char screenpath[1024];
		strcpy(screenpath, filepath);
		screenpath[strlen(screenpath)-4] = 0;
		strcat(screenpath, ".png");
		SaveFile((char *)gameScreenPng.buffer, screenpath, gameScreenPng.size, silent);
	}

	STREAM fp = OPEN_STREAM(filepath, "wb");
	
	if(!fp)
	{
		if(!silent)
			ErrorPrompt("Save failed!");
		return 0;
	}

	S9xFreezeToStream(fp);
	CLOSE_STREAM(fp);

	if(!silent)
		InfoPrompt("Save successful");
	return 1;
}

/****************************************************************************
 * Deferred auto-save
 *
 * SnapshotStateAuto() serializes the state and copies the screenshot, so it
 * can be called from the main thread at the moment the game is left.
 * WriteStateSnapshot() then does the slow part - the device I/O - and can
 * run whenever, on any thread, even after another game has been loaded and
 * the screenshot has been cleared.
 ***************************************************************************/
struct StateSnapshot
{
	char path[MAXPATHLEN];
	unsigned char * data;
	int size;
	unsigned char * png; // screenshot, or nullptr
	int pngSize;
};

StateSnapshot * SnapshotStateAuto ()
{
	StateSnapshot * snapshot = (StateSnapshot *)calloc(1, sizeof(StateSnapshot));

	if(!snapshot)
		return nullptr;

	if(!MakeFilePath(snapshot->path, FILE_STATE, Memory.ROMFilename, 0))
	{
		FreeStateSnapshot(snapshot);
		return nullptr;
	}

	uint32 size = S9xFreezeSize();

	if(size > 0)
		snapshot->data = (unsigned char *)malloc(size);

	if(!snapshot->data)
	{
		FreeStateSnapshot(snapshot);
		return nullptr;
	}

	S9xFreezeGameMem(snapshot->data, size);
	snapshot->size = size;

	if(gameScreenPng.size > 0 && gameScreenPng.buffer)
	{
		snapshot->png = (unsigned char *)malloc(gameScreenPng.size);

		if(snapshot->png)
		{
			memcpy(snapshot->png, gameScreenPng.buffer, gameScreenPng.size);
			snapshot->pngSize = gameScreenPng.size;
		}
	}

	return snapshot;
}

bool WriteStateSnapshot (StateSnapshot * snapshot, bool silent)
{
	int device;

	if(!snapshot || !snapshot->data || !FindDevice(snapshot->path, &device))
		return false;

	if(snapshot->png)
	{
		char screenpath[MAXPATHLEN];
		snprintf(screenpath, sizeof(screenpath), "%s", snapshot->path);
		screenpath[strlen(screenpath)-4] = 0;
		strcat(screenpath, ".png");
		SaveFile((char *)snapshot->png, screenpath, snapshot->pngSize, silent);
	}

	return SaveFile((char *)snapshot->data, snapshot->path, snapshot->size, silent) > 0;
}

void FreeStateSnapshot (StateSnapshot * snapshot)
{
	if(!snapshot)
		return;

	free(snapshot->png);
	free(snapshot->data);
	free(snapshot);
}

/****************************************************************************
 * LoadSnapshot
 ***************************************************************************/
int
LoadSnapshot (char * filepath, bool silent)
{
	int device;
				
	if(!FindDevice(filepath, &device))
		return 0;

	STREAM fp = OPEN_STREAM(filepath, "rb");

	if(!fp)
	{
		if(!silent)
			ErrorPrompt("Unable to open state!");
		return 0;
	}

	int	result = S9xUnfreezeFromStream(fp);
	CLOSE_STREAM(fp);

	if (result == SUCCESS)
		return 1;

	switch (result)
	{
		case WRONG_FORMAT:
			ErrorPrompt(SAVE_ERR_WRONG_FORMAT);
			break;

		case WRONG_VERSION:
			ErrorPrompt(SAVE_ERR_WRONG_VERSION);
			break;

		case SNAPSHOT_INCONSISTENT:
			ErrorPrompt(MOVIE_ERR_SNAPSHOT_INCONSISTENT);
			break;
	}
	return 0;
}

int
LoadSnapshotAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_STATE, Memory.ROMFilename, 0))
		return false;

	return LoadSnapshot(filepath, silent);
}

/****************************************************************************
 * SavePreview
 ***************************************************************************/

int SavePreviewImg (char * filepath, bool silent)
{
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	// save screenshot
	if(gameScreenPng.size > 0)
	{
		char screenpath[1024];
		strcpy(screenpath, filepath);
		screenpath[strlen(screenpath)] = 0;
		strcat(screenpath, ".png");
		SaveFile((char *)gameScreenPng.buffer, screenpath, gameScreenPng.size, silent);
	}
	return 1;
}
