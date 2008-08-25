/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 *
 * snes9xGX.h
 *
 * This file controls overall program flow. Most things start and end here!
 ****************************************************************************/

#ifndef _SNES9XGX_H_
#define _SNES9XGX_H_

#include <gccore.h>
#include "snes9x.h"

#define VERSIONNUM "005"
#define VERSIONSTR "Snes9x GX 005"

#define NOTSILENT 0
#define SILENT 1

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB
};

struct SGCSettings{
    int		AutoLoad;
    int		AutoSave;
    int		LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, SMB
	char	LoadFolder[200]; // Path to game files
	char	SaveFolder[200]; // Path to save files
	char	CheatFolder[200]; // Path to cheat files
	char	gcip[16];
	char	gwip[16];
	char	mask[16];
	char	smbip[16];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbgcid[20];
	char	smbsvid[20];
	char	smbshare[20];
    int		NGCZoom; // 0 - off, 1 - on
    int		VerifySaves;
	int		render;		// 0 - original, 1 - filtered
	int		Superscope;
	int		Mouse;
	int		Justifier;
	int		widescreen;	// 0 - 4:3 aspect, 1 - 16:9 aspect
};

START_EXTERN_C

extern struct SGCSettings GCSettings;
extern bool8 isWii;

END_EXTERN_C

#endif
