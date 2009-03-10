/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Tantric September 2008
 *
 * memcardop.cpp
 *
 * Memory Card routines
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "snes9xGX.h"
#include "video.h"
#include "menu.h"
#include "preferences.h"
#include "fileop.h"
#include "dvd.h"

static u8 * SysArea = NULL;

static void InitCardSystem()
{
	CARD_Init ("SNES", "00");
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 * Returns TRUE if found.
 ***************************************************************************/
static int
CardFileExists (char *filename, int slot)
{
	card_dir CardDir;
	int CardError;

	CardError = CARD_FindFirst (slot, &CardDir, TRUE);
	while (CardError != CARD_ERROR_NOFILE)
	{
		CardError = CARD_FindNext (&CardDir);

		if (strcmp ((char *) CardDir.filename, filename) == 0)
			return 1;
	}

	return 0;
}

/****************************************************************************
 * MountCard
 *
 * Mounts the memory card in the given slot.
 * Returns the result of the last attempted CARD_Mount command.
 ***************************************************************************/
static int MountCard(int cslot, bool silent)
{
	int ret = -1;
	int tries = 0;

	#ifdef HW_DOL
	uselessinquiry ();
	#endif

	// Mount the card
	while ( tries < 10 && ret != 0)
	{
		EXI_ProbeReset ();
		ret = CARD_Mount (cslot, SysArea, NULL);
		VIDEO_WaitVSync ();
		tries++;
	}
	return ret;
}

/****************************************************************************
 * TestCard
 *
 * Checks to see if a card is in the card slot specified
 ***************************************************************************/
bool TestCard(int slot, bool silent)
{
	// Memory Cards do not work in Wii mode - disable
	#ifdef HW_RVL
	return false;
	#endif

	bool ret;

	/*** Initialize Card System ***/
	SysArea = (u8 *)memalign(32, CARD_WORKAREA);
	memset (SysArea, 0, CARD_WORKAREA);
	InitCardSystem();

	/*** Try to mount the card ***/
	if (MountCard(slot, silent) == 0)
	{
		// Mount successful!
		if(!silent)
		{
			if (slot == CARD_SLOTA)
				ErrorPrompt("Mounted Slot A Memory Card!");
			else
				ErrorPrompt("Mounted Slot B Memory Card!");
		}
		CARD_Unmount (slot);
		ret = true;
	}
	else
	{
		if(!silent)
		{
			if (slot == CARD_SLOTA)
				ErrorPrompt("Unable to Mount Slot A Memory Card!");
			else
				ErrorPrompt("Unable to Mount Slot B Memory Card!");
		}

		ret = false;
	}
	free(SysArea);
	return ret;
}

/****************************************************************************
 * Verify Memory Card file against buffer
 ***************************************************************************/
static int
VerifyMCFile (char *buf, int slot, char *filename, int datasize)
{
	card_file CardFile;
	unsigned char verifybuffer[65536] ATTRIBUTE_ALIGN (32);
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
    int bytesleft = 0;
    int bytesread = 0;
    int ret = 0;

    memset (verifybuffer, 0, 65536);

	/*** Get Sector Size ***/
	CARD_GetSectorSize (slot, &SectorSize);

	if (!CardFileExists (filename, slot))
	{
		ErrorPrompt("Unable to open file!");
	}
	else
	{
		memset (&CardFile, 0, sizeof (CardFile));
		CardError = CARD_Open (slot, filename, &CardFile);

		if(CardError)
		{
			ErrorPrompt("Unable to open file!");
		}
		else
		{
			blocks = CardFile.len;

			if (blocks < SectorSize)
				blocks = SectorSize;

			if (blocks % SectorSize)
				blocks += SectorSize;

			if (blocks > (unsigned int)datasize)
				blocks = datasize;

			bytesleft = blocks;
			bytesread = 0;
			while (bytesleft > 0)
			{
				CARD_Read (&CardFile, verifybuffer, SectorSize, bytesread);
				if ( memcmp (buf + bytesread, verifybuffer, (unsigned int)bytesleft < SectorSize ? bytesleft : SectorSize) )
				{
					ErrorPrompt("File integrity could not be verified!");
					break;
				}

				bytesleft -= SectorSize;
				bytesread += SectorSize;

				ShowProgress ("Verifying...", bytesread, blocks);
			}
			CARD_Close (&CardFile);
			CancelAction();
		}
	}
	return ret;
}

/****************************************************************************
 * LoadMCFile
 * Load savebuffer from Memory Card file
 ***************************************************************************/
int
LoadMCFile (char *buf, int slot, char *filename, bool silent)
{
	card_file CardFile;
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
    int bytesleft = 0;
    int bytesread = 0;

	/*** Initialize Card System ***/
    SysArea = (u8 *)memalign(32, CARD_WORKAREA);
	memset (SysArea, 0, CARD_WORKAREA);
	InitCardSystem();

	/*** Try to mount the card ***/
	CardError = MountCard(slot, NOTSILENT);

	if (CardError == 0)
	{
		/*** Get Sector Size ***/
		CARD_GetSectorSize (slot, &SectorSize);

		if (!CardFileExists (filename, slot))
		{
			if (!silent)
				ErrorPrompt("Unable to open file!");
		}
		else
		{
			memset (&CardFile, 0, sizeof (CardFile));
			CardError = CARD_Open (slot, filename, &CardFile);

			if(CardError)
			{
				ErrorPrompt("Unable to open file!");
			}
			else
			{
				blocks = CardFile.len;

				if (blocks < SectorSize)
					blocks = SectorSize;

				if (blocks % SectorSize)
					blocks += SectorSize;

				bytesleft = blocks;
				bytesread = 0;
				while (bytesleft > 0)
				{
					CardError = CARD_Read (&CardFile, buf + bytesread, SectorSize, bytesread);

					if(CardError)
					{
						ErrorPrompt("Error loading file!");
						bytesread = 0;
						break;
					}

					bytesleft -= SectorSize;
					bytesread += SectorSize;
					ShowProgress ("Loading...", bytesread, blocks);
				}
				CARD_Close (&CardFile);
				CancelAction();
			}
		}
		CARD_Unmount(slot);
	}
	else
	{
		if (slot == CARD_SLOTA)
			ErrorPrompt("Unable to Mount Slot A Memory Card!");
		else
			ErrorPrompt("Unable to Mount Slot B Memory Card!");
	}

	free(SysArea);
	return bytesread;
}

/****************************************************************************
 * SaveMCFile
 * Write savebuffer to Memory Card file
 ***************************************************************************/
int
SaveMCFile (char *buf, int slot, char *filename, int datasize, bool silent)
{
	card_file CardFile;
	card_stat CardStatus;
	int CardError;
	unsigned int blocks;
	unsigned int SectorSize;
	int byteswritten = 0;
	int bytesleft = 0;

	if(datasize <= 0)
		return 0;

	/*** Initialize Card System ***/
	SysArea = (u8 *)memalign(32, CARD_WORKAREA);
	memset (SysArea, 0, CARD_WORKAREA);
	InitCardSystem();

	/*** Try to mount the card ***/
	CardError = MountCard(slot, NOTSILENT);

	if (CardError == 0)
	{
		/*** Get Sector Size ***/
		CARD_GetSectorSize (slot, &SectorSize);

		/*** Calculate number of blocks required ***/
		blocks = (datasize / SectorSize) * SectorSize;
		if (datasize % SectorSize)
			blocks += SectorSize;

		/*** Does this file exist ? ***/
		if (CardFileExists (filename, slot))
		{
			/*** Try to open the file ***/
			CardError = CARD_Open (slot, filename, &CardFile);
			if (CardError)
			{
				ErrorPrompt("Unable to open card file!");
				goto done;
			}
			else
			{
				if ( (s32)blocks > CardFile.len )  /*** new data is longer ***/
				{
					CARD_Close (&CardFile);

					/*** Try to create temp file to check available space ***/
					CardError = CARD_Create (slot, "TEMPFILE", blocks, &CardFile);
					if (CardError)
					{
						ErrorPrompt("Insufficient space to update file!");
						goto done;
					}

					/*** Delete the temporary file ***/
					CARD_Close (&CardFile);
					CardError = CARD_Delete(slot, "TEMPFILE");
					if (CardError)
					{
						ErrorPrompt("Unable to delete temporary file!");
						goto done;
					}

					/*** Delete the existing shorter file ***/
					CardError = CARD_Delete(slot, filename);
					if (CardError)
					{
						ErrorPrompt("Unable to delete existing file!");
						goto done;
					}

					/*** Create new, longer file ***/
					CardError = CARD_Create (slot, filename, blocks, &CardFile);
					if (CardError)
					{
						ErrorPrompt("Unable to create updated card file!");
						goto done;
					}
				}
			}
		}
		else  /*** no file existed, create new one ***/
		{
			/*** Create new file ***/
			CardError = CARD_Create (slot, filename, blocks, &CardFile);
			if (CardError)
			{
				if (CardError == CARD_ERROR_INSSPACE)
					ErrorPrompt("Insufficient space to create file!");
				else
					ErrorPrompt("Unable to create card file!");
				goto done;
			}
		}

		/*** Now, have an open file handle, ready to send out the data ***/
		CARD_GetStatus (slot, CardFile.filenum, &CardStatus);
		CardStatus.icon_addr = 0x0;
		CardStatus.icon_fmt = 2;
		CardStatus.icon_speed = 1;
		CardStatus.comment_addr = 2048;
		CARD_SetStatus (slot, CardFile.filenum, &CardStatus);

		bytesleft = blocks;

		while (bytesleft > 0)
		{
			CardError =
				CARD_Write (&CardFile, buf + byteswritten,
							SectorSize, byteswritten);

			if(CardError)
			{
				ErrorPrompt("Error writing file!");
				byteswritten = 0;
				break;
			}

			bytesleft -= SectorSize;
			byteswritten += SectorSize;

			ShowProgress ("Saving...", byteswritten, blocks);
		}
		CARD_Close (&CardFile);
		CancelAction();

		if (byteswritten > 0 && GCSettings.VerifySaves)
		{
			// Verify the written file
			if (!VerifyMCFile (buf, slot, filename, byteswritten) )
				byteswritten = 0;
		}
done:
		CARD_Unmount (slot);
	}
	else
	{
		if (slot == CARD_SLOTA)
			ErrorPrompt("Unable to Mount Slot A Memory Card!");
		else
			ErrorPrompt("Unable to Mount Slot B Memory Card!");
	}
	free(SysArea);
	return byteswritten;
}
