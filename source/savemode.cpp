/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * savemode.cpp
 *
 * Save Mode: what is saved and loaded without being asked (see savemode.h)
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include "snes9xgx.h"
#include "savemode.h"
#include "sram.h"
#include "freeze.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu/menu.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"

static bool liveStateChanged = false;

void MarkLiveStateChanged()
{
	liveStateChanged = true;
}

void ClearLiveStateChanged()
{
	liveStateChanged = false;
}

bool LiveStateChanged()
{
	return liveStateChanged;
}

/****************************************************************************
 * AutoLoadOnGameOpen
 *
 * The game has just been loaded and is cold booted. Runs on the thread that
 * loaded it.
 ***************************************************************************/
void AutoLoadOnGameOpen()
{
	MigrateSaveFileNames();

	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
		{
			const int snapshot = LoadSnapshotAuto(SILENT);

			if(snapshot == 1)
				break;

			// No Auto Snapshot yet (the first time, or the Save Mode was just
			// changed), or one that can't be loaded: the Cartridge Save is what is left
			const bool gameSave = LoadSRAMAuto(SILENT);

			if(snapshot < 0)
			{
				if(gameSave)
					ErrorPrompt("The Auto Snapshot couldn't be loaded. The Cartridge Save was loaded instead.");
				else
					ErrorPrompt("The Auto Snapshot couldn't be loaded. The game starts from the beginning.");
			}
			break;
		}

		case SAVEMODE_GAMESAVE:
			LoadSRAMAuto(SILENT);
			break;

		case SAVEMODE_MANUAL:
		default:
			break;
	}

	ClearLiveStateChanged(); // what was just loaded is what is in storage
}

/****************************************************************************
 * AutoSaveOnQuit
 ***************************************************************************/
void AutoSaveOnQuit(bool screenshotIsCurrent)
{
	if(SNESROMSize == 0)
		return;

	// In the menu, the game was already saved when it was left
	const bool inMenu = appRequest == AppRequest::MENU;

	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
		{
			// Something loaded or reset in the menu since then is what the game
			// should come back to
			if(inMenu && !LiveStateChanged())
				break;

			// The write from leaving the game may still be going to the same file
			WaitForBackgroundTasks(15000);

			StateSnapshot * snapshot = SnapshotStateAuto(!inMenu && screenshotIsCurrent);

			if(snapshot)
			{
				WriteStateSnapshot(snapshot, SILENT);
				FreeStateSnapshot(snapshot);
			}
			ClearLiveStateChanged();
			break;
		}

		case SAVEMODE_GAMESAVE:
			if(!inMenu)
				SaveSRAMAuto(SILENT);
			break;

		case SAVEMODE_MANUAL:
		default:
			break;
	}
}
