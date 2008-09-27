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
#include "memcardop.h"
#include "fileop.h"
#include "smbop.h"
#include "filesel.h"

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
		WaitPrompt((char*)"Incompatible SRAM save!");
	}
}

/****************************************************************************
 * Load SRAM
 ***************************************************************************/
int
LoadSRAM (int method, bool silent)
{
	ShowAction ((char*) "Loading...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because SRAM needs R/W

	char filepath[1024];
	int offset = 0;

	AllocSaveBuffer ();

	if(method == METHOD_SD || method == METHOD_USB)
	{
		ChangeFATInterface(method, NOTSILENT);
		sprintf (filepath, "%s/%s/%s.srm", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);
		offset = LoadBufferFromFAT (filepath, silent);
	}
	else if(method == METHOD_SMB)
	{
		sprintf (filepath, "%s/%s.srm", GCSettings.SaveFolder, Memory.ROMFilename);
		offset = LoadBufferFromSMB (filepath, silent);
	}
	else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		sprintf (filepath, "%s.srm", Memory.ROMName);

		if(method == METHOD_MC_SLOTA)
			offset = LoadBufferFromMC (savebuffer, CARD_SLOTA, filepath, silent);
		else
			offset = LoadBufferFromMC (savebuffer, CARD_SLOTB, filepath, silent);
	}

	if (offset > 0)
	{
		decodesavedata (method, offset);
		S9xSoftReset();
	}

	FreeSaveBuffer ();

	if(offset > 0)
	{
		return 1;
	}
	else
	{
		// if we reached here, nothing was done!
		if(!silent)
			WaitPrompt ((char*) "SRAM file not found");

		return 0;
	}
}

/****************************************************************************
 * Save SRAM
 ***************************************************************************/
bool
SaveSRAM (int method, bool silent)
{
	ShowAction ((char*) "Saving...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	bool retval = false;
	char filepath[1024];
	int datasize;
	int offset = 0;

	AllocSaveBuffer ();

	datasize = preparesavedata (method);

	if ( datasize )
	{
		if(method == METHOD_SD || method == METHOD_USB)
		{
			if(ChangeFATInterface(method, NOTSILENT))
			{
				sprintf (filepath, "%s/%s/%s.srm", ROOTFATDIR, GCSettings.SaveFolder, Memory.ROMFilename);
				offset = SaveBufferToFAT (filepath, datasize, silent);
			}
		}
		else if(method == METHOD_SMB)
		{
			sprintf (filepath, "%s/%s.srm", GCSettings.SaveFolder, Memory.ROMFilename);
			offset = SaveBufferToSMB (filepath, datasize, silent);
		}
		else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
		{
			sprintf (filepath, "%s.srm", Memory.ROMName);

			if(method == METHOD_MC_SLOTA)
				offset = SaveBufferToMC (savebuffer, CARD_SLOTA, filepath, datasize, silent);
			else
				offset = SaveBufferToMC (savebuffer, CARD_SLOTB, filepath, datasize, silent);
		}

		if (offset > 0)
		{
			if ( !silent )
				WaitPrompt((char *)"Save successful");
			retval = true;
		}
	}
	else
	{
		if(!silent)
			WaitPrompt((char *)"No SRAM data to save!");
	}

	FreeSaveBuffer ();
	return retval;
}
