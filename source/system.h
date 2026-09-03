/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * system.h
 *
 * Console support functions
 ***************************************************************************/

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stdint.h>

enum {
	EXITACTION_WII_AUTO = 0,
	EXITACTION_WII_RETURN_TO_MENU,
	EXITACTION_WII_POWER_OFF,
	EXITACTION_WII_RETURN_TO_LOADER,
	EXITACTION_WII_LENGTH
};

enum {
	EXITACTION_GC_RETURN_TO_LOADER = 0,
	EXITACTION_GC_REBOOT,
	EXITACTION_GC_LENGTH
};

void SystemInit();
void SystemExit(int exitAction, bool autoloadedGame);
void ShutdownWii();
bool SupportedIOS(uint32_t ios);
bool SaneIOS(uint32_t ios);
char * getConsoleDetails();
char * getMemoryFreeInfo();
extern int ShutdownRequested;
extern int ResetRequested;
extern int ExitRequested;

#endif
