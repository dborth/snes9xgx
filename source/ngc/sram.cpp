/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>

#include "snes9x.h"
#include "memmap.h"
#include "srtc.h"

#include "snes9xGX.h"
#include "menu.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"

/****************************************************************************
 * Load SRAM
 ***************************************************************************/
bool
LoadSRAM (char * filepath, int method, bool silent)
{
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent); // we use 'Save' because SRAM needs R/W

	if(method == METHOD_AUTO)
		return false;

	AllocSaveBuffer();

	offset = LoadFile(filepath, method, silent);

	if (offset > 0)
	{
		int size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

		if (size > 0x20000)
			size = 0x20000;

		if (readsize == size + 512 || readsize == size + 512 + SRTC_SRAM_PAD)
		{
			// SRAM has a 512 byte header - remove it, then import the SRAM,
			// ignoring anything after the SRAM
			memcpy(Memory.SRAM, savebuffer+512, size);
		}
		else if (readsize == size || readsize == size + SRTC_SRAM_PAD)
		{
			// SRAM data should be at the start of the file, just import it and
			// ignore anything after the SRAM
			memcpy (Memory.SRAM, savebuffer, size);
		}
		else
		{
			ErrorPrompt("Incompatible SRAM save!");
		}

		S9xSoftReset();
		FreeSaveBuffer ();
		return true;
	}
	else
	{
		FreeSaveBuffer ();

		// if we reached here, nothing was done!
		if(!silent)
			ErrorPrompt("SRAM file not found");

		return false;
	}
}

bool
LoadSRAMAuto (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SRAM, method, Memory.ROMFilename, 0))
		return false;

	return LoadSRAM(filepath, method, silent);
}

/****************************************************************************
 * Save SRAM
 ***************************************************************************/
bool
SaveSRAM (char * filepath, int method, bool silent)
{
	bool retval = false;
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	// determine SRAM size
	int size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (size > 0x20000)
		size = 0x20000;

	if (size > 0)
	{
		if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
		{
			// Set the sramcomments
			char sramcomment[2][32];
			memset(sramcomment, 0, 64);
			sprintf (sramcomment[0], "%s SRAM", APPNAME);
			sprintf (sramcomment[1], Memory.ROMName);
			SetMCSaveComments(sramcomment);
		}

		AllocSaveBuffer ();
		// copy in the SRAM, leaving a 512 byte header (for compatibility with other platforms)
		memcpy(savebuffer+512, Memory.SRAM, size);
		offset = SaveFile(filepath, size+512, method, silent);
		FreeSaveBuffer ();

		if (offset > 0)
		{
			if (!silent)
				InfoPrompt("Save successful");
			retval = true;
		}
	}
	else
	{
		if(!silent)
			ErrorPrompt("No SRAM data to save!");
	}
	return retval;
}

bool
SaveSRAMAuto (int method, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[1024];

	if(!MakeFilePath(filepath, FILE_SRAM, method, Memory.ROMFilename, 0))
		return false;

	return SaveSRAM(filepath, method, silent);
}
