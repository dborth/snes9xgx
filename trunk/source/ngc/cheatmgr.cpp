/****************************************************************************
 * Snes9x 1.51
 *
 * Nintendo Wii/Gamecube Port
 * Tantric August 2008
 *
 * cheatmgr.cpp
 *
 * Cheat handling
 ****************************************************************************/

#include "snes9xGx.h"
#include "memmap.h"
#include "fileop.h"
#include "cheats.h"
#include "filesel.h"

extern SCheatData Cheat;

/****************************************************************************
 * SetupCheats
 *
 * Erases any prexisting cheats, loads cheats from a cheat file
 * Called when a ROM is first loaded
 ****************************************************************************/
void
SetupCheats()
{
	S9xInitCheatData ();
	S9xDeleteCheats ();

	char cheatFile[150] = { '\0' };

	int method = GCSettings.SaveMethod;

	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	if(method == METHOD_SD || method == METHOD_USB)
	{
		changeFATInterface(method, NOTSILENT);
		sprintf (cheatFile, "%s/snes9x/cheats/%s.cht", ROOTFATDIR, Memory.ROMFilename);
	}

	// load cheat file if present
	if(strlen(cheatFile) > 0)
	{
		if(S9xLoadCheatFile (cheatFile))
		{
			// disable all cheats loaded from the file
			for (uint16 i = 0; i < Cheat.num_cheats; i++)
				S9xDisableCheat(i);
		}
	}
}
