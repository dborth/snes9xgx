/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * system.cpp
 *
 * Console support functions
 ***************************************************************************/

#include <gccore.h>
#include <sys/iosupport.h>

#ifdef HW_RVL
#include <di/di.h>
#include <wiiuse/wpad.h>
#endif

#include "system.h"
#include "video.h"
#include "audio.h"
#include "fileop.h"
#include "input.h"
#include "mem2.h"
#include "font_ttf.h"
#include "utils/wiidrc.h"
#include "utils/FreeTypeGX.h"

#ifdef USE_VM
	#include "vmalloc.h"
#endif

extern "C" {
extern void __exception_setreload(int t);
}

extern "C" {
	s32 __STM_Close();
	s32 __STM_Init();
}

int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
static bool isWiiVC = false;

/****************************************************************************
 * USB Gecko Debugging
 ***************************************************************************/

static bool gecko = false;
static mutex_t gecko_mutex = 0;

static ssize_t __out_write(struct _reent *r, void* fd, const char *ptr, size_t len)
{
	if (!gecko || len == 0)
		return len;

	if(!ptr || len < 0)
		return -1;

	u32 level;
	LWP_MutexLock(gecko_mutex);
	level = IRQ_Disable();
	usb_sendbuffer(1, ptr, len);
	IRQ_Restore(level);
	LWP_MutexUnlock(gecko_mutex);
	return len;
}

const devoptab_t gecko_out = {
	"stdout",	// device name
	0,			// size of file structure
	NULL,		// device open
	NULL,		// device close
	__out_write,// device write
	NULL,		// device read
	NULL,		// device seek
	NULL,		// device fstat
	NULL,		// device stat
	NULL,		// device link
	NULL,		// device unlink
	NULL,		// device chdir
	NULL,		// device rename
	NULL,		// device mkdir
	0,			// dirStateSize
	NULL,		// device diropen_r
	NULL,		// device dirreset_r
	NULL,		// device dirnext_r
	NULL,		// device dirclose_r
	NULL		// device statvfs_r
};

static void USBGeckoOutput()
{
	gecko = usb_isgeckoalive(1);
	LWP_MutexInit(&gecko_mutex, false);

	devoptab_list[STD_OUT] = &gecko_out;
	devoptab_list[STD_ERR] = &gecko_out;
}

/****************************************************************************
 * Startup / Shutdown / Reboot / Exit
 ***************************************************************************/

#ifdef HW_RVL
void ShutdownCB()
{
	ShutdownRequested = 1;
}
void ResetCB()
{
	ResetRequested = 1;
}
#endif

#ifdef HW_DOL
/****************************************************************************
 * ipl_set_config
 * lowlevel Qoob Modchip disable
 ***************************************************************************/

static void ipl_set_config(unsigned char c)
{
	volatile unsigned long* exi = (volatile unsigned long*)0xCC006800;
	unsigned long val,addr;
	addr=0xc0000000;
	val = c << 24;
	exi[0] = ((((exi[0]) & 0x405) | 256) | 48);	//select IPL
	//write addr of IPL
	exi[0 * 5 + 4] = addr;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);
	//write the ipl we want to send
	exi[0 * 5 + 4] = val;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);

	exi[0] &= 0x405;	//deselect IPL
}
#endif

void SystemInit() {
	#ifdef HW_RVL
	L2Enhance();

	u32 ios = IOS_GetVersion();

	if(!SupportedIOS(ios))
	{
		s32 preferred = IOS_GetPreferredVersion();

		if(SupportedIOS(preferred))
			IOS_ReloadIOS(preferred);
	}
	#else
	ipl_set_config(6); // disable Qoob modchip
	#endif

	USBGeckoOutput();
	__exception_setreload(8);

	InitVideo(); // Initialize video
	InitAudio();

	#ifdef HW_RVL
	// Wii Power/Reset buttons
	__STM_Close();
	__STM_Init();
	__STM_Close();
	__STM_Init();
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);

	WiiDRC_Init();
	isWiiVC = WiiDRC_Inited();
	WPAD_Init();
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	DI_Init();
	USBStorage_Initialize();
	#else
	DVD_Init (); // Initialize DVD subsystem (GameCube only)
	#endif

	SetupPads();
	InitDeviceThread();
	MountAllFAT(); // Initialize libFAT for SD and USB

	#ifdef HW_RVL
	InitMem2Manager();
	#endif

	InitFreeType((u8*)font_ttf, font_ttf_size); // Initialize font system
}

static void ExitCleanup()
{
	ShutdownAudio();
	StopGX();

	HaltDeviceThread();
	UnmountAllFAT();

#ifdef HW_RVL
	DI_Close();
#endif
}

#ifdef HW_DOL
	#define PSOSDLOADID 0x7c6000a6
	int *psoid = (int *) 0x80001800;
	void (*PSOReload) () = (void (*)()) 0x80001800;
#endif

void SystemExit(int exitAction, bool autoloadedGame)
{
#ifdef HW_RVL
	ShutoffRumble();
#endif

	ExitCleanup();

#ifdef HW_RVL
	if(ShutdownRequested) {
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
#else
	if(exitAction == EXITACTION_GC_REBOOT) // Reboot
	{
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, FALSE);
	}
	else // Exit to Loader
	{
		if (psoid[0] == PSOSDLOADID)
			PSOReload();
		else
			exit(0);
	}
#endif
}

typedef enum {
#ifdef HW_DOL
    CONSOLE_GAMECUBE,
#else
    CONSOLE_WII,
	CONSOLE_WIIU_VWII,
	CONSOLE_WIIU_WIIVC,
	CONSOLE_DOLPHIN
#endif
} ConsoleType;

#ifdef HW_RVL
static inline bool IsWiiUFastCPU() {
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) && ((*(vu32*)0xCD8005B0 & 0x20) == 0));
}

static inline bool IsDolphinEmulator() {
    s32 fd = IOS_Open("/dev/dolphin", 0);

    if (fd >= 0) {
        IOS_Close(fd);
        return true;
    }

    return false;
}
#endif
static ConsoleType GetConsoleType() {
#ifdef HW_DOL
	return CONSOLE_GAMECUBE;
#else
	if (IsDolphinEmulator()) {
		return CONSOLE_DOLPHIN;
	}

	if (*(vu16*)0xCD8005A0 == 0xCAFE || isWiiVC) {
		return CONSOLE_WIIU_WIIVC;
	}

	if (*(vu32*)0xCD8000A0 & 0x00080000) {
		return CONSOLE_WIIU_VWII;
	}

	return CONSOLE_WII;
#endif
}

static const char* GetCPUSpeed(ConsoleType type) {
#ifdef HW_DOL
	return "486 MHz"; // GameCube "Gekko" CPU
#else
	switch(type) {
		case CONSOLE_WII:
		case CONSOLE_WIIU_VWII:
			return "729 MHz"; // Wii "Broadway" CPU (vWii mode is throttled to this by default)

		case CONSOLE_WIIU_WIIVC:
			if (IsWiiUFastCPU()) {
				return "1.24 GHz"; // Wii U "Espresso" CPU speed (1.24 GHz)
			}
			return "729 MHz"; // Fallback if VC launcher throttled it

		default:
			return "729 MHz";
	}
#endif
}

char * getConsoleDetails() {
    static char description[40];
    ConsoleType type = GetConsoleType();
    const char *cpu_speed = GetCPUSpeed(type);

#ifdef HW_DOL
    snprintf(description, sizeof(description), "GameCube (%s)", cpu_speed);
#else
	switch(type) {
		case CONSOLE_WII:
			snprintf(description, sizeof(description), "Wii (%s), IOS: %d", cpu_speed, IOS_GetVersion());
			break;

		case CONSOLE_WIIU_VWII:
			snprintf(description, sizeof(description), "vWii (%s), IOS: %d", cpu_speed, IOS_GetVersion());
			break;

		case CONSOLE_WIIU_WIIVC:
			snprintf(description, sizeof(description), "Wii U VC (%s), IOS: %d", cpu_speed, IOS_GetVersion());
			break;

		case CONSOLE_DOLPHIN:
			snprintf(description, sizeof(description), "Dolphin Emulator");
			break;
    }
#endif

    return description;
}

char * getMemoryFreeInfo() {
    static char memoryFreeInfo[50];
    float mem1_mb = 0.0f;

#ifdef HW_DOL
    // GameCube uses standard libogc arena allocation
    uint32_t mem1_bytes = SYS_GetArena1Size();
    mem1_mb = (float)mem1_bytes / (1024.0f * 1024.0f);

    snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "MEM1 free: %.2fMB", mem1_mb);
#else
    // Wii uses libogc2's malloc_wii split-heap mspace wrapper.
    // fordblks tracks the actual free memory inside the MEM1 pool.
    struct mallinfo mi = mallinfo();
    mem1_mb = (float)mi.fordblks / (1024.0f * 1024.0f);

    uint32_t mem2_bytes = SYS_GetArena2Size();
    float mem2_mb = (float)mem2_bytes / (1024.0f * 1024.0f);

    snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "MEM1 free: %.2fMB, MEM2 free: %.2fMB", mem1_mb, mem2_mb);
#endif

    return memoryFreeInfo;
}

/****************************************************************************
 * IOS Check
 ***************************************************************************/
#ifdef HW_RVL
bool SupportedIOS(u32 ios)
{
	if(IsDolphinEmulator()) {
		return true;
	}

	if(ios == 58 || ios == 61)
		return true;

	return false;
}

bool SaneIOS(u32 ios)
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
#endif
