/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2009
 *
 * cheatmgr.cpp
 *
 * Cheat handling
 ***************************************************************************/

#include "port.h"
#include "cheats.h"

#include "snes9xGX.h"
#include "fileop.h"
#include "filebrowser.h"

extern SCheatData Cheat;

/****************************************************************************
 * NGCLoadCheatFile
 *
 * Loads cheat file from save buffer
 * Custom version of S9xLoadCheatFile()
 ***************************************************************************/

static bool NGCLoadCheatFile (int length)
{
	Cheat.num_cheats = 0;

	uint8 data [28];
	int offset = 0;

	while (offset < length)
	{
		if(Cheat.num_cheats >= MAX_CHEATS || (length - offset) < 28)
			break;

		memcpy (data, savebuffer+offset, 28);
		offset += 28;

		Cheat.c [Cheat.num_cheats].enabled = (data [0] & 4) == 0;
		Cheat.c [Cheat.num_cheats].byte = data [1];
		Cheat.c [Cheat.num_cheats].address = data [2] | (data [3] << 8) | (data [4] << 16);
		Cheat.c [Cheat.num_cheats].saved_byte = data [5];
		Cheat.c [Cheat.num_cheats].saved = (data [0] & 8) != 0;
		memcpy (Cheat.c [Cheat.num_cheats].name, &data[8], 20);
		Cheat.c [Cheat.num_cheats].name[20] = 0;
		Cheat.num_cheats++;
	}
	return true;
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
	uint16 i;

	if(!MakeFilePath(filepath, FILE_CHEAT))
		return;

	AllocSaveBuffer();

	offset = LoadFile(filepath, SILENT);

	// load cheat file if present
	if(offset > 0)
	{
		if(NGCLoadCheatFile (offset))
		{
			// disable all cheats loaded from the file
			for (i = 0; i < Cheat.num_cheats; i++)
				S9xDisableCheat(i);
		}
	}

	FreeSaveBuffer ();
}
