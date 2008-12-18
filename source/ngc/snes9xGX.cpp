/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric September 2008
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

#ifdef WII_DVD
extern "C" {
#include <di/di.h>
}
#endif

#include "snes9x.h"
#include "memmap.h"
#include "debug.h"
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
#include "smbop.h"
#include "video.h"
#include "menudraw.h"
#include "s9xconfig.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "fileop.h"
#include "input.h"

#include "gui.h"

int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
char appPath[1024];
FILE* debughandle;

extern int FrameTimer;

extern long long prev;
extern unsigned int timediffallowed;

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

void ExitCleanup()
{
	UnmountAllFAT();
	CloseShare();

#ifdef HW_RVL
	DI_Close();
#endif
}

#ifdef HW_DOL
	#define PSOSDLOADID 0x7c6000a6
	int *psoid = (int *) 0x80001800;
	void (*PSOReload) () = (void (*)()) 0x80001800;
#endif

void Reboot()
{
	ExitCleanup();
#ifdef HW_RVL
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
	#define SOFTRESET_ADR ((volatile u32*)0xCC003024)
	*SOFTRESET_ADR = 0x00000000;
#endif
}

void ExitToLoader()
{
	ExitCleanup();
	// Exit to Loader
	#ifdef HW_RVL
		exit(0);
	#else	// gamecube
		if (psoid[0] == PSOSDLOADID)
			PSOReload ();
	#endif
}

#ifdef HW_RVL
void ShutdownCB()
{
	ConfigRequested = 1;
	ShutdownRequested = 1;
}
void ResetCB()
{
	ResetRequested = 1;
}
void ShutdownWii()
{
	ExitCleanup();
	SYS_ResetSystem(SYS_POWEROFF, 0, 0);
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
extern u8 vmode_60hz;
int timerstyle;

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
		//timediffallowed = Settings.FrameTimePAL;
	} else {
		if(vmode_60hz == 1)
			timerstyle = 0;
		else
			timerstyle = 1;
		//timediffallowed = Settings.FrameTimeNTSC;
	}
	return;
}

/****************************************************************************
 * Emulation loop
 ***************************************************************************/
extern void S9xInitSync();
bool CheckVideo = 0; // for forcing video reset in video.cpp
extern uint32 prevRenderedFrameCount;

void
emulate ()
{
	S9xSetSoundMute (TRUE);
	AudioStart ();
	S9xInitSync(); // Eke-Eke: initialize frame Sync

	setFrameTimerMethod(); // set frametimer method every time a ROM is loaded

	while (1)
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
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			LWP_ResumeThread (devicethread);

			// change to menu video mode
			ResetVideo_Menu ();

			if ( GCSettings.AutoSave == 1 )
			{
				SaveSRAM ( GCSettings.SaveMethod, SILENT );
			}
			else if ( GCSettings.AutoSave == 2 )
			{
				if ( WaitPromptChoice ("Save Freeze State?", "Don't Save", "Save") )
					NGCFreezeGame ( GCSettings.SaveMethod, SILENT );
			}
			else if ( GCSettings.AutoSave == 3 )
			{
				if ( WaitPromptChoice ("Save SRAM and Freeze State?", "Don't Save", "Save") )
				{
					SaveSRAM(GCSettings.SaveMethod, SILENT );
					NGCFreezeGame ( GCSettings.SaveMethod, SILENT );
				}
			}

			#ifdef HW_RVL
			if(ShutdownRequested)
				ShutdownWii();
			#endif

			// GUI Stuff
			/*
			gui_alloc();
			gui_makebg ();
			gui_clearscreen();
			gui_draw ();
			gui_showscreen ();
			//gui_savescreen ();
			*/

			MainMenu (2); // go to game menu

			// save zoom level
			SavePrefs(SILENT);

			FrameTimer = 0;
			setFrameTimerMethod (); // set frametimer method every time a ROM is loaded

			Settings.SuperScopeMaster = (GCSettings.Superscope > 0 ? true : false);
			Settings.MouseMaster = (GCSettings.Mouse > 0 ? true : false);
			Settings.JustifierMaster = (GCSettings.Justifier > 0 ? true : false);
			SetControllers ();
			//S9xReportControllers ();

			ConfigRequested = 0;

			#ifdef _DEBUG_VIDEO
			// log stuff
			fprintf(debughandle, "\n\nPlaying ROM: %s", Memory.ROMFilename);
			fprintf(debughandle, "\nrender: %u", GCSettings.render);
			#endif

			CheckVideo = 1;	// force video update
			prevRenderedFrameCount = IPPU.RenderedFramesCount;

			// stop checking if devices were removed/inserted
			// since we're starting emulation again
			LWP_SuspendThread (devicethread);
		}//if ConfigRequested

	}//while
}

void CreateAppPath(char origpath[])
{
#ifdef HW_DOL
	sprintf(appPath, GCSettings.SaveFolder);
#else
	char path[1024];
	strcpy(path, origpath); // make a copy so we don't mess up original

	char * loc;
	int pos = -1;

	loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	loc = strchr(path,'/'); // looking for / from fat:/
	if (loc != NULL)
		pos = loc - path + 1;

	if(pos >= 0 && pos < 1024)
		sprintf(appPath, &(path[pos]));
#endif
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
	#ifdef HW_DOL
	ipl_set_config(6); // disable Qoob modchip
	#endif

	#ifdef WII_DVD
	DI_Init();	// first
	#endif

	int selectedMenu = -1;

	// Initialise video
	InitGCVideo ();

	// Controllers
	#ifdef HW_RVL
	WPAD_Init();
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
	#endif

	PAD_Init ();

	// Wii Power/Reset buttons
	#ifdef HW_RVL
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);
	#endif

	// Audio
	AUDIO_Init (NULL);

	// GC Audio RAM (for ROM and backdrop storage)
	AR_Init (NULL, 0);

	// GameCube only - Injected ROM
	// Before going any further, let's copy any injected ROM image
	// We'll put it in ARAM for safe storage

	#ifdef HW_DOL
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

	// Initialise freetype font system
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}

	unpackbackdrop ();

	// Set defaults
	DefaultSettings ();

	// store path app was loaded from
	sprintf(appPath, "snes9x");
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);

	S9xUnmapAllControls ();
	SetDefaultButtonMap ();

	// Allocate SNES Memory
	if (!Memory.Init ())
		while (1);

	// Allocate APU
	if (!S9xInitAPU ())
		while (1);

	// Set Pixel Renderer to match 565
	S9xSetRenderPixelFormat (RGB565);

	// Initialise Snes Sound System
	S9xInitSound (5, TRUE, 1024);

	// Initialise Graphics
	setGFX ();
	if (!S9xGraphicsInit ())
		while (1);

	// Initialize libFAT for SD and USB
	MountAllFAT();
	InitDeviceThread();

	// Initialize DVD subsystem (GameCube only)
	#ifdef HW_DOL
	DVD_Init ();
	#endif

	// Check if DVD drive belongs to a Wii
	SetDVDDriveType();

	// Load preferences
	if(!LoadPrefs())
	{
		WaitPrompt("Preferences reset - check settings!");
		selectedMenu = 1; // change to preferences menu
	}

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

	// Get the user to load a ROM
	while (SNESROMSize <= 0)
	{
		MainMenu (selectedMenu);
	}

	// Emulate
	emulate ();

	// NO! - We're never leaving here!
	while (1);
		return 0;
}
