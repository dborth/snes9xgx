/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2009
 *
 * snes9xGX.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <debug.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "FreeTypeGX.h"

#include "snes9x.h"
#include "memmap.h"
#include "s9xdebug.h"
#include "cpuexec.h"
#include "ppu.h"
#include "apu.h"
#include "display.h"
#include "gfx.h"
#include "soundux.h"
#include "spc700.h"
#include "spc7110.h"
#include "controls.h"

#include "snes9xGX.h"
#include "aram.h"
#include "dvd.h"
#include "networkop.h"
#include "video.h"
#include "s9xconfig.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"

int ScreenshotRequested = 0;
int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
char appPath[1024];
int appLoadMethod = METHOD_AUTO;

extern "C" {
extern void __exception_setreload(int t);
}

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

void ExitCleanup()
{
#ifdef HW_RVL
	ShutoffRumble();
#endif
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

void ExitApp()
{
	SavePrefs(SILENT);

	if (SNESROMSize > 0 && !ConfigRequested && GCSettings.AutoSave == 1)
		SaveSRAMAuto(GCSettings.SaveMethod, SILENT);

	ExitCleanup();

	if(ShutdownRequested)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);

	#ifdef HW_RVL
	if(GCSettings.ExitAction == 0) // Auto
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
			GCSettings.ExitAction = 3; // Exit to HBC
		else
			GCSettings.ExitAction = 1; // HBC not found
	}
	#endif

	if(GCSettings.ExitAction == 1) // Exit to Menu
	{
		#ifdef HW_RVL
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		#else
			#define SOFTRESET_ADR ((volatile u32*)0xCC003024)
			*SOFTRESET_ADR = 0x00000000;
		#endif
	}
	else if(GCSettings.ExitAction == 2) // Shutdown Wii
	{
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	else // Exit to Loader
	{
		#ifdef HW_RVL
			exit(0);
		#else
			if (psoid[0] == PSOSDLOADID)
				PSOReload();
		#endif
	}
}

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

void ipl_set_config(unsigned char c)
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

/****************************************************************************
 * setFrameTimerMethod()
 * change frametimer method depending on whether ROM is NTSC or PAL
 ***************************************************************************/

void setFrameTimerMethod()
{
	/*
	Set frametimer method
	(timerstyle: 0=NTSC vblank, 1=PAL int timer)
	*/
	if ( Settings.PAL ) {
		if(vmode_60hz == 1)
			timerstyle = 1;
		else
			timerstyle = 0;
	} else {
		if(vmode_60hz == 1)
			timerstyle = 0;
		else
			timerstyle = 1;
	}
	return;
}

/****************************************************************************
 * Emulation loop
 ***************************************************************************/
extern void S9xInitSync();
extern uint32 prevRenderedFrameCount;
static int videoReset;
static int currentMode;

void
emulate ()
{
	while (1) // main loop
	{
		// go back to checking if devices were inserted/removed
		// since we're entering the menu
		ResumeDeviceThread();

		SwitchAudioMode(1);

		if(SNESROMSize == 0)
			MainMenu(MENU_GAMESELECTION);
		else
			MainMenu(MENU_GAME);

		AllocGfxMem();
		SelectFilterMethod();

		ConfigRequested = 0;
		ScreenshotRequested = 0;
		SwitchAudioMode(0);

		Settings.MultiPlayer5Master = (GCSettings.Controller == CTRL_PAD4 ? true : false);
		Settings.SuperScopeMaster = (GCSettings.Controller == CTRL_SCOPE ? true : false);
		Settings.MouseMaster = (GCSettings.Controller == CTRL_MOUSE ? true : false);
		Settings.JustifierMaster = (GCSettings.Controller == CTRL_JUST ? true : false);
		SetControllers ();

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceThread();

		AudioStart ();

		FrameTimer = 0;
		setFrameTimerMethod (); // set frametimer method every time a ROM is loaded

		CheckVideo = 1;	// force video update
		prevRenderedFrameCount = IPPU.RenderedFramesCount;

		videoReset = -1;
		currentMode = GCSettings.render;

		while(1) // emulation loop
		{
			S9xMainLoop ();
			NGCReportButtons ();

			if(ResetRequested)
			{
				S9xSoftReset (); // reset game
				ResetRequested = 0;
			}
			if (ConfigRequested)
			{
				ConfigRequested = 0;
				FreeGfxMem();
				ResetVideo_Menu();
				break;
			}
			#ifdef HW_RVL
			if(ShutdownRequested)
				ExitApp();
			#endif
		} // emulation loop
	} // main loop
}

static void CreateAppPath(char origpath[])
{
#ifdef HW_DOL
	sprintf(appPath, GCSettings.SaveFolder);
#else
	char path[1024];
	strncpy(path, origpath, 1024); // make a copy so we don't mess up original

	char * loc;
	int pos = -1;

	if(strncmp(path, "sd:/", 5) == 0 || strncmp(path, "fat:/", 5) == 0)
		appLoadMethod = METHOD_SD;
	else if(strncmp(path, "usb:/", 5) == 0)
		appLoadMethod = METHOD_USB;

	loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	loc = strchr(path,'/'); // looking for first / (after sd: or usb:)
	if (loc != NULL)
		pos = loc - path + 1;

	if(pos >= 0 && pos < 1024)
		sprintf(appPath, &(path[pos]));
#endif
}

/****************************************************************************
 * USB Gecko Debugging
 ***************************************************************************/

static bool gecko = false;
static mutex_t gecko_mutex = 0;

static ssize_t __out_write(struct _reent *r, int fd, const char *ptr, size_t len)
{
	u32 level;

	if (!ptr || len <= 0 || !gecko)
		return -1;

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
	LWP_MutexInit(&gecko_mutex, false);
	gecko = usb_isgeckoalive(1);
	
	devoptab_list[STD_OUT] = &gecko_out;
	devoptab_list[STD_ERR] = &gecko_out;
}

/****************************************************************************
 * MAIN
 *
 * Steps to Snes9x Emulation :
 *	1. Initialise GC Video
 *	2. Initialise libfreetype (Nice to read something)
 *	3. Set S9xSettings to standard defaults (s9xconfig.h)
 *	4. Allocate Snes9x Memory
 *	5. Allocate APU Memory
 *	6. Set Pixel format to RGB565 for GL Rendering
 *	7. Initialise Snes9x/GC Sound System
 *	8. Initialise Snes9x Graphics subsystem
 *	9. Let's Party!
 ***************************************************************************/
int
main(int argc, char *argv[])
{
	//USBGeckoOutput(); // uncomment to enable USB gecko output
	__exception_setreload(8);

	#ifdef HW_DOL
	ipl_set_config(6); // disable Qoob modchip
	#endif

	#ifdef HW_RVL
	DI_Close(); // fixes some black screen issues
	DI_Init();	// first
	#endif

	#ifdef DEBUG_WII
	//DEBUG_Init(GDBSTUB_DEVICE_USB, 1);	// init debugging
	//_break();
	#endif

	InitDeviceThread();
	VIDEO_Init();
	PAD_Init ();

	#ifdef HW_RVL
	WPAD_Init();
	#endif

	InitGCVideo(); // Initialise video
	ResetVideo_Menu (); // change to menu video mode

	#ifdef HW_RVL
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);

	// Wii Power/Reset buttons
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);
	#endif

	// GameCube only - Injected ROM
	// Before going any further, let's copy any injected ROM image
	// We'll put it in ARAM for safe storage

	#ifdef HW_DOL
	AR_Init (NULL, 0);

	int *romptr = (int *) 0x81000000; // location of injected rom

	if (memcmp ((char *) romptr, "SNESROM0", 8) == 0)
	{
		SNESROMSize = romptr[2];

		if(SNESROMSize > (1024*128) && SNESROMSize < (1024*1024*8))
		{
			romptr = (int *) 0x81000020;
			ARAMPut ((char *) romptr, (char *) AR_SNESROM, SNESROMSize);
		}
		else // not a valid ROM size
		{
			SNESROMSize = 0;
		}
	}
	#endif

	// Initialize DVD subsystem (GameCube only)
	#ifdef HW_DOL
	DVD_Init ();
	#endif

	// store path app was loaded from
	sprintf(appPath, "snes9x");
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);

	// Initialize libFAT for SD and USB
	MountAllFAT();

	// Set defaults
	DefaultSettings ();

	S9xUnmapAllControls ();
	SetDefaultButtonMap ();

	// Allocate SNES Memory
	if (!Memory.Init ())
		ExitApp();

	// Allocate APU
	if (!S9xInitAPU ())
		ExitApp();

	// Set Pixel Renderer to match 565
	S9xSetRenderPixelFormat (RGB565);

	// Initialise Snes Sound System
	S9xInitSound (5, TRUE, 1024);

	// Initialise Graphics
	setGFX ();
	if (!S9xGraphicsInit ())
		ExitApp();

	// Check if DVD drive belongs to a Wii
	SetDVDDriveType();

	S9xSetSoundMute (TRUE);
	S9xInitSync(); // initialize frame sync

	// Initialize font system
	InitFreeType((u8*)font_ttf, font_ttf_size);

	InitGUIThreads();

	// GameCube only - Injected ROM
	// Everything's been initialized, we can copy our ROM back
	// from ARAM into main memory

	#ifdef HW_DOL
	if(SNESROMSize > 0)
	{
		ARAMFetchSlow( (char *)Memory.ROM, (char *)AR_SNESROM, SNESROMSize);
		Memory.LoadROM ("BLANK.SMC");
		Memory.LoadSRAM ("BLANK");
	}
	#endif

	emulate(); // main loop
}
