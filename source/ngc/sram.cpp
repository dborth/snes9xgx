/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Tantric September 2008
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
#include "images/saveicon.h"
#include "menudraw.h"
#include "fileop.h"

extern int padcal;
extern unsigned short gcpadmap[];

char sramcomment[2][32];

/****************************************************************************
 * Prepare SRAM Save Data
 *
 * This sets up the savebuffer for saving in a format compatible with
 * snes9x on other platforms.
 ***************************************************************************/
int
preparesavedata (int method)
{
	int offset = 0;
	int size;

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		// Copy in save icon
		memcpy (savebuffer, saveicon, sizeof(saveicon));
		offset += sizeof (saveicon);
	}

	// Copy in the sramcomments
	sprintf (sramcomment[0], "%s SRAM", VERSIONSTR);
	sprintf (sramcomment[1], Memory.ROMName);
	memcpy (savebuffer + offset, sramcomment, 64);
	offset += 64;

	if(method != METHOD_MC_SLOTA && method != METHOD_MC_SLOTB)
	{
		// make it a 512 byte header so it is compatible with other platforms
		if (offset <= 512)
			offset = 512;
		// header was longer than 512 bytes - hopefully this never happens!
		else
			return 0;
	}

	// Copy in the SRAM
	size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (size > 0x20000)
		size = 0x20000;

	if (size != 0)
	{
		memcpy (savebuffer + offset, Memory.SRAM, size);
		offset += size;
	}
	else
	{
		offset = 0;
	}

	return offset;
}

/****************************************************************************
 * Decode Save Data
 ***************************************************************************/
void
decodesavedata (int method, int readsize)
{
	int offset = 0;
	char sramsavecomment[32];

	int size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (size > 0x20000)
		size = 0x20000;

	// memory card save
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
		offset = sizeof (saveicon); // skip save icon

	// Check for sram comment
	memcpy (sramsavecomment, savebuffer+offset, 32);

	// version 0xx found!
	if ( (strncmp (sramsavecomment, "Snes9x GX 0", 11) == 0) )
	{
		// adjust offset
		if(method != METHOD_MC_SLOTA && method != METHOD_MC_SLOTB)
			offset = 512; // skip entire 512 byte header
		else
			offset += 64; // skip savecomments

		// import the SRAM
		memcpy (Memory.SRAM, savebuffer + offset, size);
	}
	// check for SRAM from other version/platform of snes9x
	else if ( readsize == size || readsize == size + SRTC_SRAM_PAD)
	{
		// SRAM data should be at the start of the file, just import it and
		// ignore anything after the SRAM
		memcpy (Memory.SRAM, savebuffer, size);
	}
	else if ( readsize == size + 512 )
	{
		// SRAM has a 512 byte header - remove it, then import the SRAM,
		// ignoring anything after the SRAM
		memcpy(Memory.SRAM, savebuffer+512, size);
	}
	else
	{
		WaitPrompt("Incompatible SRAM save!");
	}
}

/****************************************************************************
 * Load SRAM
 ***************************************************************************/
int
LoadSRAM (int method, bool silent)
{
	char filepath[1024];
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because SRAM needs R/W

	if(!MakeFilePath(filepath, FILE_SRAM, method))
		return 0;

	ShowAction ("Loading...");

	AllocSaveBuffer();

	offset = LoadFile(filepath, method, silent);

	if (offset > 0)
	{
		decodesavedata (method, offset);
		S9xSoftReset();
		FreeSaveBuffer ();
		return 1;
	}
	else
	{
		FreeSaveBuffer ();

		// if we reached here, nothing was done!
		if(!silent)
			WaitPrompt ("SRAM file not found");

		return 0;
	}
}

/****************************************************************************
 * Save SRAM
 ***************************************************************************/
bool
SaveSRAM (int method, bool silent)
{
	bool retval = false;
	char filepath[1024];
	int datasize;
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	if(!MakeFilePath(filepath, FILE_SRAM, method))
		return false;

	ShowAction ("Saving...");

	AllocSaveBuffer ();

	datasize = preparesavedata (method);

	if (datasize)
	{
		offset = SaveFile(filepath, datasize, method, silent);

		if (offset > 0)
		{
			if ( !silent )
				WaitPrompt("Save successful");
			retval = true;
		}
	}
	else
	{
		if(!silent)
			WaitPrompt("No SRAM data to save!");
	}

	FreeSaveBuffer ();
	return retval;
}
