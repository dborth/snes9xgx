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
#include "sdload.h"
#include "cheats.h"
 
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

	if(GCSettings.SaveMethod == METHOD_SD)
		sprintf (cheatFile, "%s/snes9x/cheats/%s.cht", ROOTSDDIR, Memory.ROMFilename);
	else if(GCSettings.SaveMethod == METHOD_USB)
		sprintf (cheatFile, "%s/snes9x/cheats/%s.cht", ROOTUSBDIR, Memory.ROMFilename);
		
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
