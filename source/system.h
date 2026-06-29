/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * system.h
 *
 * Console support functions
 ***************************************************************************/

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

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
bool SupportedIOS(u32 ios);
bool SaneIOS(u32 ios);
extern int ShutdownRequested;
extern int ResetRequested;
extern int ExitRequested;

extern bool isWiiVC;
static inline bool IsWiiU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) || isWiiVC);
}
static inline bool IsWiiUFastCPU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) && ((*(vu32*)0xCD8005B0 & 0x20) == 0));
}

#endif
