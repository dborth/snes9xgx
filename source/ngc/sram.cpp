/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * crunchy2 April 2007-July 2007
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>

#include "snes9x.h"
#include "memmap.h"
#include "srtc.h"

#include "snes9xGx.h"
#include "images/saveicon.h"
#include "menudraw.h"
#include "memcardop.h"
#include "fileop.h"
#include "smbop.h"
#include "filesel.h"

extern unsigned char savebuffer[];
extern int padcal;
extern unsigned short gcpadmap[];

char sramcomment[2][32];

/****************************************************************************
 * Prepare Memory Card SRAM Save Data
 *
 * This sets up the save buffer for saving to a memory card.
 ****************************************************************************/
int
prepareMCsavedata ()
{
  int offset = sizeof (saveicon);
  int size;

  ClearSaveBuffer ();

  /*** Copy in save icon ***/
  memcpy (savebuffer, saveicon, offset);

  /*** And the sramcomments ***/
  sprintf (sramcomment[0], "%s SRAM", VERSIONSTR);
  sprintf (sramcomment[1], Memory.ROMName);
  memcpy (savebuffer + offset, sramcomment, 64);
  offset += 64;

  /*** Copy SRAM size ***/
  size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

  if (size > 0x20000)
    size = 0x20000;

  memcpy (savebuffer + offset, &size, 4);
  offset += 4;

  /*** Copy SRAM ***/
  if (size != 0)
  {
    memcpy (savebuffer + offset, Memory.SRAM, size);
    offset += size;
  }

//  /*** Save Joypad Configuration ***/
//  memcpy (savebuffer + offset, &currconfig, 16);
  offset += 16;
//  memcpy (savebuffer + offset, &padcal, 4);
  offset += 4;

  return offset;
}

/****************************************************************************
 * Prepare Exportable SRAM Save Data
 *
 * This sets up the save buffer for saving in a format compatible with
 * snes9x on other platforms. This is used when saving to SD / USB / SMB.
 ****************************************************************************/
int
prepareEXPORTsavedata ()
{
  int offset = 0;
  int size;

  ClearSaveBuffer ();

  /*** Copy in the sramcomments ***/
  sprintf (sramcomment[1], "%s", Memory.ROMName);
  memcpy (savebuffer + offset, sramcomment, 64);
  offset += 64;

//	/*** Save Joypad Configuration ***/
//  memcpy (savebuffer + offset, &currconfig, 16);
  offset += 16;
//  memcpy (savebuffer + offset, &padcal, 4);
  offset += 4;

  if ( offset <= 512 )
  {
	// make it a 512 byte header so it is compatible with other platforms
	offset = 512;

	/*** Copy in the SRAM ***/
	size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (size > 0x20000)
	  size = 0x20000;

	if (size != 0)
	{
	  memcpy (savebuffer + offset, Memory.SRAM, size);
	  offset += size;
	}

	return offset;
  }
  else
  {
    // header was longer than 512 bytes - hopefully this never happens!
    return 0;
  }
}

/****************************************************************************
 * Decode Save Data
 ****************************************************************************/
void
decodesavedata (int readsize)
{
  int offset;
  int size;
  char sramcomment[32];

  // Check for exportable format sram - it has the sram comment at the start
  memcpy (sramcomment, savebuffer, 32);

  if ( (strncmp (sramcomment, "Snes9x GX 2.0", 13) == 0) || (strncmp (sramcomment, "Snes9x GX 00", 12) == 0) )	// version 2.0.XX or 00x
  {
    offset = 64;

    // Get the control pad configuration
//	memcpy (&currconfig, savebuffer + offset, 16);
	offset += 16;
//	memcpy (&padcal, savebuffer + offset, 4);
	offset += 4;

//	for (size = 0; size < 4; size++)
//	  gcpadmap[size] = padmap[currconfig[size]];

	// move to start of SRAM which is after the 512 byte header
	offset = 512;

    // import the SRAM
	size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (size > 0x20000)
	  size = 0x20000;

	memcpy (Memory.SRAM, savebuffer + offset, size);
	offset += size;
  }
  else
  {
    // else, check for a v2.0 memory card format save
    offset = sizeof (saveicon);
    memcpy (sramcomment, savebuffer + offset, 32);

	if ( strncmp (sramcomment, "Snes9x GX 2.0", 13) == 0 )
	{
      //WaitPrompt((char*) "Memory Card format save");
	  offset += 64;
	  memcpy (&size, savebuffer + offset, 4);
	  offset += 4;
	  memcpy (Memory.SRAM, savebuffer + offset, size);
	  offset += size;

      // If it is an old 2.0 format save, skip over the Settings as we
      // don't save them in SRAM now
      if ( strcmp (sramcomment, "Snes9x GX 2.0") == 0 )
	    offset += sizeof (Settings);

      // Get the control pad configuration
//      memcpy (&currconfig, savebuffer + offset, 16);
	  offset += 16;
//	  memcpy (&padcal, savebuffer + offset, 4);
	  offset += 4;

//	  for (size = 0; size < 4; size++)
//		gcpadmap[size] = padmap[currconfig[size]];
	}
    else if ( strncmp (sramcomment, "Snes9x 1.43 SRAM (GX", 20) == 0)
    {
      // it's an older SnesGx memory card format save
	  size = Memory.SRAMSize ? ( 1 << (Memory.SRAMSize + 3)) * 128 : 0;

	  // import the SRAM
	  if ( size )
		memcpy(&Memory.SRAM[0], &savebuffer[sizeof(saveicon)+68], size);

	  // Ignore the settings saved in the file

	  // NOTE: need to add import of joypad config?? Nah.
	}
	else
	{
	  // else, check for SRAM from other version/platform of snes9x
	  size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	  if ( readsize == size || readsize == size + SRTC_SRAM_PAD)
	  {
	    //WaitPrompt("readsize=size or + SRTC_SRAM_PAD");
		// SRAM data should be at the start of the file, just import it and
		// ignore anything after the SRAM
		memcpy (Memory.SRAM, savebuffer, size);
      }
	  else if ( readsize == size + 512 )
	  {
		//WaitPrompt("readsize=size+512");
		// SRAM has a 512 byte header - remove it, then import the SRAM,
		// ignoring anything after the SRAM
		memmove (savebuffer, savebuffer + 512, size);
		memcpy (Memory.SRAM, savebuffer, size);
	  }
	  else
	  	WaitPrompt((char*)"Incompatible SRAM save!");
	}
  }
}

/****************************************************************************
 * Load SRAM
 ****************************************************************************/
int
LoadSRAM (int method, bool silent)
{
	ShowAction ((char*) "Loading...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because SRAM needs R/W

	char filepath[1024];
	int offset = 0;

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
		decodesavedata (offset);
		S9xSoftReset();
		return 1;
	}

	// if we reached here, nothing was done!
	if(!silent)
		WaitPrompt ((char*) "SRAM file not found");

	return 0;
}

/****************************************************************************
 * Save SRAM
 ****************************************************************************/
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

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
		datasize = prepareMCsavedata ();
	else
		datasize = prepareEXPORTsavedata ();

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
	return retval;
}
