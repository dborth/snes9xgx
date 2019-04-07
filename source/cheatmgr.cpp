/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2019
 *
 * cheatmgr.cpp
 *
 * Cheat handling
 ***************************************************************************/


#include "snes9x/port.h"
#include "snes9x/cheats.h"

#include "snes9xgx.h"
#include "fileop.h"
#include "filebrowser.h"

#define MAX_CHEATS      150

extern SCheatData Cheat;

/****************************************************************************
 * LoadCheatFile
 *
 * Loads cheat file from save buffer
 * Custom version of S9xLoadCheatFile()
 ***************************************************************************/

static bool LoadCheatFile (int length)
{
	uint8 data [28];
	int offset = 0;

	while (offset < length) {
		if(Cheat.g.size() >= MAX_CHEATS || (length - offset) < 28)
			break;

		memcpy (data, savebuffer+offset, 28);
		offset += 28;

		SCheat c;
		char name[21];
		char cheat[10];
		c.byte = data[1];
		c.address = data[2] | (data[3] << 8) |  (data[4] << 16);
		memcpy (name, &data[8], 20);
		name[20] = 0;

		snprintf (cheat, 10, "%x=%x", c.address, c.byte);
		S9xAddCheatGroup (name, cheat);
	}
	return true;
}

void ToggleCheat(uint32 num) {
	if(Cheat.g[num].enabled) {
		S9xDisableCheatGroup(num);
	}
	else {
		S9xEnableCheatGroup(num);
	}

	for(int i=0; i < Cheat.g.size(); i++) {
		if(Cheat.g[i].enabled) {
			Cheat.enabled = TRUE;
			return;
		}
	}
	Cheat.enabled = FALSE;
}

/****************************************************************************
 * SetupCheats
 *
 * Erases any prexisting cheats, loads cheats from a cheat file
 * Called when a ROM is first loaded
 ***************************************************************************/
void
WiiSetupCheats()
{
	char filepath[1024];
	int offset = 0;

	if(!MakeFilePath(filepath, FILE_CHEAT))
		return;

	AllocSaveBuffer();

	offset = LoadFile(filepath, SILENT);

	// load cheat file if present
	if(offset > 0)
		LoadCheatFile (offset);

	FreeSaveBuffer ();
}
