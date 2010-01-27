/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * freeze.cpp
 ***************************************************************************/

#include <malloc.h>
#include <gccore.h>
#include <stdio.h>

#include "snes9x.h"
#include "port.h"
#include "memmap.h"
#include "snapshot.h"
#include "language.h"

#include "snes9xGX.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "pngu.h"

bool8 S9xOpenSnapshotFile(const char *filepath, bool8 readonly, STREAM *file)
{
	return FALSE;
}

void S9xCloseSnapshotFile(STREAM s)
{

}

/****************************************************************************
 * SaveSnapshot
 ***************************************************************************/

int
SaveSnapshot (char * filepath, bool silent)
{
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

int
SaveSnapshotAuto (bool silent)
{
	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, Memory.ROMFilename, 0))
		return false;

	return SaveSnapshot(filepath, silent);
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
			ErrorPrompt("Unable to open snapshot!");
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

	if(!MakeFilePath(filepath, FILE_SNAPSHOT, Memory.ROMFilename, 0))
		return false;

	return LoadSnapshot(filepath, silent);
}
