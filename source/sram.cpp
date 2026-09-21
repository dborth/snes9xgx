/****************************************************************************
 * Snes9x GX
 *
 * crunchy2 April 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * sram.cpp
 *
 * SRAM save/load/import/export handling
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snes9xgx.h"
#include "menu.h"
#include "sram.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/srtc.h"

bool HiROM;
bool LoROM;

/****************************************************************************
 * Load SRAM
 ***************************************************************************/
bool LoadSRAM (char * filepath, bool silent)
{
	int len = 0;
	int device;
	bool result = false;

	if(!FindDevice(filepath, &device))
		return 0;

	Memory.ClearSRAM();

	int size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (LoROM)
		size = size < 0x70000 ? size : 0x70000;
	else if (HiROM)
		size = size < 0x40000 ? size : 0x40000;

	if (size)
	{
		len = LoadFile((char *)Memory.SRAM, filepath, 0, size, silent);

		if (len > 0)
		{
			if (len - size == 512)
				memmove(Memory.SRAM, Memory.SRAM + 512, size);

			if (Settings.SRTC || Settings.SPC7110RTC)
			{
				int pathlen = strlen(filepath);
				filepath[pathlen-3] = 'r';
				filepath[pathlen-2] = 't';
				filepath[pathlen-1] = 'c';
				LoadFile((char *)RTCData.reg, filepath, 0, 20, silent);
			}
			result = true;
		}
		else if(!silent)
		{
			// if we reached here, nothing was done!
			ErrorPrompt("SRAM file not found");
		}
		S9xSoftReset();
	}
	return result;
}

bool LoadSRAMAuto (bool silent)
{
	char filepath[MAXPATHLEN];

	// look for Auto save file
	if(!MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, 0))
		return false;

	if (LoadSRAM(filepath, silent))
		return true;

	if (!EmuSettings.appendAuto)
		return false;

	// look for file with no number or Auto appended
	if(!MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, -1))
		return false;

	if(LoadSRAM(filepath, silent))
		return true;

	return false;
}

/****************************************************************************
 * Save SRAM
 ***************************************************************************/
// \return size in bytes of the SRAM to save - 0 if there is none
static int GetSRAMSaveSize()
{
	if (Settings.SuperFX && Memory.ROMType < 0x15) // doesn't have SRAM
		return 0;

	if (Settings.SA1 && Memory.ROMType == 0x34)    // doesn't have SRAM
		return 0;

	int size = Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0;

	if (LoROM)
		size = size < 0x70000 ? size : 0x70000;
	else if (HiROM)
		size = size < 0x40000 ? size : 0x40000;

	return size;
}

bool SaveSRAM (char * filepath, bool silent)
{
	bool retval = false;
	int offset = 0;
	int device;

	if(!FindDevice(filepath, &device))
		return 0;

	if (Settings.SuperFX && Memory.ROMType < 0x15) // doesn't have SRAM
		return true;

	if (Settings.SA1 && Memory.ROMType == 0x34)    // doesn't have SRAM
		return true;

	// determine SRAM size
	int size = GetSRAMSaveSize();

	if (size > 0)
	{
		offset = SaveFile((char *)Memory.SRAM, filepath, size, silent);

		if (Settings.SRTC || Settings.SPC7110RTC)
		{
			int pathlen = strlen(filepath);
			filepath[pathlen-3] = 'r';
			filepath[pathlen-2] = 't';
			filepath[pathlen-1] = 'c';
			SaveFile((char *)RTCData.reg, filepath, 20, silent);
		}

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

/****************************************************************************
 * Deferred auto-save
 *
 * SnapshotSRAMAuto() copies everything that is needed (the SRAM and where it
 * goes) so it can be called from the main thread at the moment the game is
 * left. WriteSRAMSnapshot() then does the slow part - the device I/O - and
 * can run whenever, on any thread, even after another game has been loaded.
 ***************************************************************************/
struct SRAMSnapshot
{
	char path[MAXPATHLEN];       // the Auto file
	char legacyPath[MAXPATHLEN]; // file with no number or Auto appended - used if it exists
	unsigned char * data;
	int size;
	bool hasRTC;
	unsigned char rtc[20];
};

SRAMSnapshot * SnapshotSRAMAuto ()
{
	int size = GetSRAMSaveSize();

	if(size <= 0)
		return nullptr;

	SRAMSnapshot * snapshot = (SRAMSnapshot *)calloc(1, sizeof(SRAMSnapshot));

	if(!snapshot)
		return nullptr;

	snapshot->data = (unsigned char *)malloc(size);

	if(!snapshot->data
		|| !MakeFilePath(snapshot->legacyPath, FILE_SRAM, Memory.ROMFilename, -1)
		|| !MakeFilePath(snapshot->path, FILE_SRAM, Memory.ROMFilename, 0))
	{
		FreeSRAMSnapshot(snapshot);
		return nullptr;
	}

	memcpy(snapshot->data, Memory.SRAM, size);
	snapshot->size = size;

	if (Settings.SRTC || Settings.SPC7110RTC)
	{
		snapshot->hasRTC = true;
		memcpy(snapshot->rtc, RTCData.reg, sizeof(snapshot->rtc));
	}

	return snapshot;
}

bool WriteSRAMSnapshot (SRAMSnapshot * snapshot, bool silent)
{
	char filepath[MAXPATHLEN];
	int device;

	if(!snapshot || !snapshot->data)
		return false;

	// use the file with no number or Auto appended if there is one
	snprintf(filepath, sizeof(filepath), "%s", snapshot->legacyPath);
	FILE * fp = fopen (filepath, "rb");

	if(fp) // file found
		fclose (fp);
	else
		snprintf(filepath, sizeof(filepath), "%s", snapshot->path);

	if(!FindDevice(filepath, &device))
		return false;

	int offset = SaveFile((char *)snapshot->data, filepath, snapshot->size, silent);

	if (snapshot->hasRTC)
	{
		int pathlen = strlen(filepath);
		filepath[pathlen-3] = 'r';
		filepath[pathlen-2] = 't';
		filepath[pathlen-1] = 'c';
		SaveFile((char *)snapshot->rtc, filepath, sizeof(snapshot->rtc), silent);
	}

	return offset > 0;
}

void FreeSRAMSnapshot (SRAMSnapshot * snapshot)
{
	if(!snapshot)
		return;

	free(snapshot->data);
	free(snapshot);
}

bool SaveSRAMAuto (bool silent)
{
	char filepath[1024];

	// look for file with no number or Auto appended
	if(!MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, -1))
		return false;

	FILE * fp = fopen (filepath, "rb");

	if(fp) // file found
	{
		fclose (fp);
	}	
	else
	{
		if(!MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, 0))
			return false;
	}

	return SaveSRAM(filepath, silent);
}
