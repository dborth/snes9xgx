/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * crunchy2 April 2007-July 2007
 *
 * preferences.cpp
 *
 * Preferences save/load preferences utilities
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>

#include "snes9x.h"
#include "snes9xGx.h"
#include "memmap.h"
#include "srtc.h"
#include "ftfont.h"
#include "mcsave.h"
#include "sdload.h"
#include "smbload.h"

extern unsigned char savebuffer[];
extern int currconfig[4];

// button map configurations
extern unsigned int gcpadmap[];
extern unsigned int wmpadmap[];
extern unsigned int ccpadmap[];
extern unsigned int ncpadmap[];

#define PREFSVERSTRING "Snes9x GX 004 Prefs"

char prefscomment[2][32] = { {PREFSVERSTRING}, {"Preferences"} };

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving to a memory card.
 ****************************************************************************/
int
preparePrefsData ()
{
  int offset = sizeof (saveicon);
  int size;

  memset (savebuffer, 0, SAVEBUFFERSIZE);

	/*** Copy in save icon ***/
  memcpy (savebuffer, saveicon, offset);

	/*** And the prefscomments ***/
  memcpy (savebuffer + offset, prefscomment, 64);
  offset += 64;

  /*** Save all settings ***/
  size = sizeof (Settings);
  memcpy (savebuffer + offset, &Settings, size);
  offset += size;

  /*** Save GC specific settings ***/
  size = sizeof (GCSettings);
  memcpy (savebuffer + offset, &GCSettings, size);
  offset += size;
  
  /*** Save buttonmaps ***/
  size = sizeof (unsigned int) *12;	// this size applies to all padmaps
  memcpy (savebuffer + offset, &gcpadmap, size);
  offset += size;
  memcpy (savebuffer + offset, &wmpadmap, size);
  offset += size;
  memcpy (savebuffer + offset, &ccpadmap, size);
  offset += size;
  memcpy (savebuffer + offset, &ncpadmap, size);
  offset += size;
  
  return offset;
}


/****************************************************************************
 * Decode Preferences Data
 ****************************************************************************/
void
decodePrefsData ()
{
  int offset;
  char prefscomment[32];
  int size;
  
    offset = sizeof (saveicon);
    memcpy (prefscomment, savebuffer + offset, 32);
        
	if ( strcmp (prefscomment, PREFSVERSTRING) == 0 )
	{
	  offset += 64;
	  memcpy (&Settings, savebuffer + offset, sizeof (Settings));
	  offset += sizeof (Settings);
	  memcpy (&GCSettings, savebuffer + offset, sizeof (GCSettings));
	  offset += sizeof (GCSettings);
	  // load padmaps (order important)
	  size = sizeof (unsigned int) *12;
	  memcpy (&gcpadmap, savebuffer + offset, size);
	  offset += size;
	  memcpy (&wmpadmap, savebuffer + offset, size);
	  offset += size;
	  memcpy (&ccpadmap, savebuffer + offset, size);
	  offset += size;
	  memcpy (&ncpadmap, savebuffer + offset, size);
	}
    else
	  WaitPrompt((char*) "Preferences reset - check settings!");
}

void quickLoadPrefs (bool8 silent)
{
	switch ( QUICK_SAVE_SLOT )
	{
		case CARD_SLOTA:
		case CARD_SLOTB:
			LoadPrefsFromMC(QUICK_SAVE_SLOT, silent);
			break;
		
		case CARD_SLOTA+2:
		case CARD_SLOTB+2:
			LoadPrefsFromSD(silent);
			break;
		
		case CARD_SLOTA+4:
			LoadPrefsFromSMB(silent);
			break;
	}
}

void quickSavePrefs (bool8 silent)
{
	switch ( QUICK_SAVE_SLOT )
	{
		case CARD_SLOTA:
		case CARD_SLOTB:
			SavePrefsToMC(QUICK_SAVE_SLOT, silent);
			break;
		case CARD_SLOTA+2:
		case CARD_SLOTB+2:
			SavePrefsToSD(silent);
			break;
		case CARD_SLOTA+4:
			SavePrefsToSMB(silent);
			break;
	}
}
