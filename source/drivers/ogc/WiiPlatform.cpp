/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * WiiPlatform.cpp
 ***************************************************************************/
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/iosupport.h>
#include <ogc/lwp_threads.h>

#include "WiiPlatform.h"
#include "WiiSystemEvents.h"
#include "OgcDebugOutput.h"

extern "C" {
extern void __exception_setreload(int t);
}

extern bool isWiiVC;

/****************************************************************************
 * Shutdown/reset
 ***************************************************************************/
static bool shutdownRequestedFlag = false;

void NotifyWiiShutdownRequested() { shutdownRequestedFlag = true; }

SystemEvent WiiPlatform::getSystemEvent()
{
	if(shutdownRequestedFlag)
		return SystemEvent::ShutdownRequested;

	static bool wasResetDown = false;
	bool isResetDown = SYS_ResetButtonDown();
	bool justPressed = isResetDown && !wasResetDown;
	wasResetDown = isResetDown;

	if(justPressed)
		return SystemEvent::ResetRequested;

	return SystemEvent::None;
}

/****************************************************************************
 * IOS Check
 ***************************************************************************/
static inline bool IsDolphinEmulator() {
	s32 fd = IOS_Open("/dev/dolphin", 0);

	if (fd >= 0) {
		IOS_Close(fd);
		return true;
	}

	return false;
}

bool SupportedIOS(uint32_t ios)
{
	if(IsDolphinEmulator()) {
		return true;
	}

	if(ios == 58 || ios == 61)
		return true;

	return false;
}

bool SaneIOS(uint32_t ios)
{
	if(IsDolphinEmulator()) {
		return true;
	}

	bool res = false;
	u32 num_titles=0;
	u32 tmd_size;

	if(ios > 200)
		return false;

	if (ES_GetNumTitles(&num_titles) < 0)
		return false;

	if(num_titles < 1)
		return false;

	u64 *titles = (u64 *)memalign(32, num_titles * sizeof(u64) + 32);

	if(!titles)
		return false;

	if (ES_GetTitles(titles, num_titles) < 0)
	{
		free(titles);
		return false;
	}

	u32 *tmdbuffer = (u32 *)memalign(32, MAX_SIGNED_TMD_SIZE);

	if(!tmdbuffer)
	{
		free(titles);
		return false;
	}

	for(u32 n=0; n < num_titles; n++)
	{
		if((titles[n] & 0xFFFFFFFF) != ios)
			continue;

		if (ES_GetStoredTMDSize(titles[n], &tmd_size) < 0)
			break;

		if (tmd_size > 4096)
			break;

		if (ES_GetStoredTMD(titles[n], (signed_blob *)tmdbuffer, tmd_size) < 0)
			break;

		if (tmdbuffer[1] || tmdbuffer[2])
		{
			res = true;
			break;
		}
	}
	free(tmdbuffer);
	free(titles);
	return res;
}

/****************************************************************************
 * init/shutdown
 ***************************************************************************/
void WiiPlatform::init(int width, int height)
{
	L2Enhance();

	u32 ios = IOS_GetVersion();

	if(!SupportedIOS(ios))
	{
		s32 preferred = IOS_GetPreferredVersion();

		if(SupportedIOS(preferred))
			IOS_ReloadIOS(preferred);
	}

	__exception_setreload(8);

	this->threadDriver = new OgcThreadDriver();
	this->threadDriver->init();

	this->videoDriver = new OgcVideoDriver();
	this->videoDriver->init(width, height);

	this->audioDriver = new WiiAudioDriver();
	this->audioDriver->init();

	this->inputDriver = new OgcInputDriver();
	this->inputDriver->init();

	this->fileSystemDriver = new WiiFileSystemDriver();
	this->fileSystemDriver->init();
}

void WiiPlatform::shutdown()
{
	if (fileSystemDriver) {
		fileSystemDriver->shutdown();
		delete fileSystemDriver;
		fileSystemDriver = nullptr;
	}

	if (inputDriver) {
		inputDriver->shutdown();
		delete inputDriver;
		inputDriver = nullptr;
	}

	if (audioDriver) {
		audioDriver->shutdown();
		delete audioDriver;
		audioDriver = nullptr;
	}

	if (videoDriver) {
		videoDriver->shutdown();
		delete videoDriver;
		videoDriver = nullptr;
	}

	if (threadDriver) {
		threadDriver->shutdown();
		delete threadDriver;
		threadDriver = nullptr;
	}
}

/****************************************************************************
 * Console/memory info
 ***************************************************************************/
typedef enum {
	CONSOLE_WII,
	CONSOLE_WIIU_VWII,
	CONSOLE_WIIU_WIIVC,
	CONSOLE_DOLPHIN
} ConsoleType;

static inline bool IsWiiU() {
	return (*(vu16*)0xCD8005A0 == 0xCAFE) || (*(vu32*)0xCD8000A0 & 0x00080000);
}

static ConsoleType GetConsoleType() {
	if (IsDolphinEmulator()) {
		return CONSOLE_DOLPHIN;
	}

	if (IsWiiU()) {
		if (isWiiVC) {
			return CONSOLE_WIIU_WIIVC;
		}
		return CONSOLE_WIIU_VWII;
	}

	return CONSOLE_WII;
}

static u32 GetCPUSpeedMHz() {
	u32 busClock = SYS_GetBusFrequency(); // ~243 MHz on Wii/vWii
	u32 multiplier = SYS_GetCoreMultiplier(); // 3x standard, 5x+ under unlocked vWii/VC

	if (busClock > 0 && multiplier > 0) {
		u64 coreClockHz = (u64)busClock * multiplier;
		return (u32)(coreClockHz / 1000000);
	}
	return 729; // Fallback
}

const char* WiiPlatform::getConsoleDetails() {
	static char description[64];
	ConsoleType type = GetConsoleType();
	u32 mhz = GetCPUSpeedMHz();

	char speedStr[16];
	if (mhz >= 1000) {
		snprintf(speedStr, sizeof(speedStr), "%.2f GHz", mhz / 1000.0f);
	} else {
		snprintf(speedStr, sizeof(speedStr), "%u MHz", mhz);
	}

	switch(type) {
		case CONSOLE_WII:
			snprintf(description, sizeof(description), "Wii (%s), IOS: %d", speedStr, IOS_GetVersion());
			break;

		case CONSOLE_WIIU_VWII:
			snprintf(description, sizeof(description), "vWii (%s), IOS: %d", speedStr, IOS_GetVersion());
			break;

		case CONSOLE_WIIU_WIIVC:
			snprintf(description, sizeof(description), "Wii U VC (%s), IOS: %d", speedStr, IOS_GetVersion());
			break;

		case CONSOLE_DOLPHIN:
			snprintf(description, sizeof(description), "Dolphin Emulator");
			break;
	}

	return description;
}

const char* WiiPlatform::getMemoryFreeInfo() {
	static char memoryFreeInfo[50];

	// Wii uses libogc2's malloc_wii split-heap mspace wrapper.
	// fordblks tracks the actual free memory inside the MEM1 pool.
	struct mallinfo mi = mallinfo();
	float mem1_mb = (float)mi.fordblks / (1024.0f * 1024.0f);

	uint32_t mem2_bytes = SYS_GetArena2Size();
	float mem2_mb = (float)mem2_bytes / (1024.0f * 1024.0f);

	snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "MEM1 free: %.2fMB, MEM2 free: %.2fMB", mem1_mb, mem2_mb);

	return memoryFreeInfo;
}

/****************************************************************************
 * Exit
 ***************************************************************************/
void WiiPlatform::requestExit(int exitAction, bool autoloadedGame)
{
	this->shutdown();

	if(shutdownRequestedFlag) {
		SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, FALSE);
	}
	else if(autoloadedGame) {
		if( !!*(u32*)0x80001800 )
		{
			// Were we launched via HBC? (or via WiiFlow's stub replacement)
			exit(1);
		}
		else
		{
			// Wii channel support
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, FALSE);
		}
	}
	else {
		if(exitAction == EXITACTION_WII_AUTO) // Auto
		{
			char * sig = (char *)0x80001804;
			if(
				sig[0] == 'S' &&
				sig[1] == 'T' &&
				sig[2] == 'U' &&
				sig[3] == 'B' &&
				sig[4] == 'H' &&
				sig[5] == 'A' &&
				sig[6] == 'X' &&
				sig[7] == 'X')
				exitAction = EXITACTION_WII_RETURN_TO_LOADER; // Exit to HBC
			else
				exitAction = EXITACTION_WII_RETURN_TO_MENU; // HBC not found
		}

		if(exitAction == EXITACTION_WII_RETURN_TO_MENU) // Exit to Menu
		{
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, FALSE);
		}
		else if(exitAction == EXITACTION_WII_POWER_OFF) // Shutdown Wii
		{
			SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, FALSE);
		}
		else // Exit to Loader
		{
			exit(0);
		}
	}
}
