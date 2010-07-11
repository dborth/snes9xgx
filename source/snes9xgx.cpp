/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2010
 *
 * snes9xgx.cpp
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
#include <sys/iosupport.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "snes9xgx.h"
#include "networkop.h"
#include "video.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "mem2.h"
#include "utils/usb2storage.h"
#include "utils/mload.h"
#include "utils/FreeTypeGX.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"
#include "snes9x/controls.h"

int ScreenshotRequested = 0;
int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
char appPath[1024] = { 0 };
char loadedFile[1024] = { 0 };
static int currentMode;

extern "C" {
extern void __exception_setreload(int t);
}

extern void S9xInitSync();
extern uint32 prevRenderedFrameCount;

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

void ExitCleanup()
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

void ExitApp()
{
#ifdef HW_RVL
	ShutoffRumble();
#endif

	SavePrefs(SILENT);

	if (SNESROMSize > 0 && !ConfigRequested && GCSettings.AutoSave == 1)
		SaveSRAMAuto(SILENT);

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
	if(!inNetworkInit)
		ShutdownRequested = 1;
}
void ResetCB()
{
	if(!inNetworkInit)
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
 * IOS 202
 ***************************************************************************/
#ifdef HW_RVL
static bool FindIOS(u32 ios)
{
	s32 ret;
	u32 n;

	u64 *titles = NULL;
	u32 num_titles=0;

	ret = ES_GetNumTitles(&num_titles);
	if (ret < 0)
		return false;

	if(num_titles < 1) 
		return false;

	titles = (u64 *)memalign(32, num_titles * sizeof(u64) + 32);
	if (!titles)
		return false;

	ret = ES_GetTitles(titles, num_titles);
	if (ret < 0)
	{
		free(titles);
		return false;
	}
		
	for(n=0; n < num_titles; n++)
	{
		if((titles[n] & 0xFFFFFFFF)==ios) 
		{
			free(titles); 
			return true;
		}
	}
    free(titles); 
	return false;
}

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

static bool USBLANDetected()
{
	u8 dummy;
	u8 i;
	u16 vid, pid;

	USB_Initialize();
	u8 *buffer = (u8*)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - (32*1024)));
	memset(buffer, 0, 8 << 3);

	if(USB_GetDeviceList("/dev/usb/oh0", buffer, 8, 0, &dummy) < 0)
		return false;

	for(i = 0; i < 8; i++)
	{
		memcpy(&vid, (buffer + (i << 3) + 4), 2);
		memcpy(&pid, (buffer + (i << 3) + 6), 2);
		if( (vid == 0x0b95) && (pid == 0x7720))
			return true;
	}
	return false;
}
#endif
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

void USBGeckoOutput()
{
	LWP_MutexInit(&gecko_mutex, false);
	gecko = usb_isgeckoalive(1);
	
	devoptab_list[STD_OUT] = &gecko_out;
	devoptab_list[STD_ERR] = &gecko_out;
}

int
main(int argc, char *argv[])
{
	//USBGeckoOutput(); // uncomment to enable USB gecko output
	__exception_setreload(8);

	#ifdef HW_DOL
	ipl_set_config(6); // disable Qoob modchip
	#endif

	#ifdef HW_RVL
	bool usblan = USBLANDetected();

	// try to load IOS 202
	if(FindIOS(202))
		IOS_ReloadIOS(202);
	else if(IOS_GetVersion() < 61 && FindIOS(61))
		IOS_ReloadIOS(61);

	if(IOS_GetVersion() == 202)
	{
		// enable DVD and USB2
		if(mload_init() >= 0 && load_ehci_module())
		{
			int mode = 3;

			if(usblan)
			{
				int usblanport = GetUSB2LanPort();

				if(usblanport >= 0)
				{
					if(usblanport == 1)
						mode = 1;
					else
						mode = 2;

					USB2Storage_Close();
					mload_close();
					IOS_ReloadIOS(202);
					mload_init();
					load_ehci_module();
				}
			}
			SetUSB2Mode(mode);
			USB2Enable(true);
			DI_LoadDVDX(false);
			DI_Init();
		}
	}
	#endif

	InitDeviceThread();
	InitGCVideo(); // Initialise video
	ResetVideo_Menu (); // change to menu video mode
	SetupPads();
	MountAllFAT(); // Initialize libFAT for SD and USB
#ifdef HW_RVL
	InitMem2Manager();
#endif

	// Initialize DVD subsystem (GameCube only)
	#ifdef HW_DOL
	DVD_Init ();
	#endif

	#ifdef HW_RVL
	// Wii Power/Reset buttons
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);

	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);
	#endif

	DefaultSettings (); // Set defaults
	S9xUnmapAllControls ();
	SetDefaultButtonMap ();

	// Allocate SNES Memory
	if (!Memory.Init ())
		ExitApp();

	// Allocate APU
	if (!S9xInitAPU ())
		ExitApp();

	S9xSetRenderPixelFormat (RGB565); // Set Pixel Renderer to match 565
	S9xInitSound (64, 0); // Initialise Sound System

	// Initialise Graphics
	setGFX ();
	if (!S9xGraphicsInit ())
		ExitApp();
	
	AllocGfxMem();
	S9xInitSync(); // initialize frame sync
	InitFreeType((u8*)font_ttf, font_ttf_size); // Initialize font system
#ifdef HW_RVL
	savebuffer = (unsigned char *)mem2_malloc(SAVEBUFFERSIZE);
	browserList = (BROWSERENTRY *)mem2_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#else
	savebuffer = (unsigned char *)malloc(SAVEBUFFERSIZE);
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#endif
	InitGUIThreads();

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
#ifdef HW_RVL
		SelectFilterMethod();
#endif
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

		CheckVideo = 2;	// force video update
		prevRenderedFrameCount = IPPU.RenderedFramesCount;
		currentMode = GCSettings.render;

		while(1) // emulation loop
		{
			S9xMainLoop ();
			ReportButtons ();

			if(ResetRequested)
			{
				S9xSoftReset (); // reset game
				ResetRequested = 0;
			}
			if (ConfigRequested)
			{
				ConfigRequested = 0;
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
