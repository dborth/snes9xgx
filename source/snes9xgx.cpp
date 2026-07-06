/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * snes9xgx.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include "snes9xgx.h"
#include "system.h"
#include "s9xsupport.h"
#include "video.h"
#include "videofilters.h"
#include "audio.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "mem2.h"

#ifdef USE_VM
	#include "vmalloc.h"
#endif

#include "snes9x/snes9x.h"
#include "snes9x/fxemu.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

int ScreenshotRequested = 0;
int ConfigRequested = 0;
char appPath[1024] = { 0 };
static bool firstRun = true;
static bool autoboot = false;

int main(int argc, char *argv[])
{
	DefaultSettings (); // Set defaults
	InitializeSnes9x(); // ensure Snes9x memory is in MEM1 for Wii
	SystemInit();
	ResetVideo_Menu (); // change to menu video mode
	S9xInitSync(); // initialize frame sync

#ifdef HW_RVL
	savebuffer = (unsigned char *)mem2_malloc(SAVEBUFFERSIZE);
	browserList = (BROWSERENTRY *)mem2_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#else
	savebuffer = (unsigned char *)memalign(32,SAVEBUFFERSIZE);
#ifdef USE_VM
	browserList = (BROWSERENTRY *)vm_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#else
	browserList = (BROWSERENTRY *)memalign(32,sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#endif
#endif
	InitGUIThreads();

#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);

	if(argc > 2 && argv[1] != NULL && argv[2] != NULL) {
		LoadPrefs();
		if(strncmp(argv[1], "sd", 2) == 0)
		{
			GCSettings.SaveMethod = DEVICE_SD;
			GCSettings.LoadMethod = DEVICE_SD;
		}
		else if(strncmp(argv[1], "usb", 3) == 0)
		{
			GCSettings.SaveMethod = DEVICE_USB;
			GCSettings.LoadMethod = DEVICE_USB;
		}
		SavePrefs(SILENT);

		GCSettings.AutoloadGame = AutoloadGame(argv[1], argv[2]);
		autoboot = GCSettings.AutoloadGame;
	}
#endif

	while (1) // main loop
	{
		if(!autoboot) {
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			ResumeDeviceThread();

			SwitchAudioMode(1);

			if(SNESROMSize == 0)
				MainMenu(MENU_GAMESELECTION);
			else
				MainMenu(MENU_GAME);
		}

		if (firstRun)
		{
			firstRun = false;
			switch (GCSettings.sfxOverclock)
			{
				case 0: Settings.SuperFXSpeedPerLine = 5823405; break;
				case 1: Settings.SuperFXSpeedPerLine = 0.417 * 20.5e6; break;
				case 2: Settings.SuperFXSpeedPerLine = 0.417 * 40.5e6; break;
				case 3: Settings.SuperFXSpeedPerLine = 0.417 * 60.5e6; break;
				case 4: Settings.SuperFXSpeedPerLine = 0.417 * 80.5e6; break;
				case 5: Settings.SuperFXSpeedPerLine = 0.417 * 100.5e6; break;
				case 6: Settings.SuperFXSpeedPerLine = 0.417 * 120.5e6; break;
			}

			if (GCSettings.sfxOverclock > 0)
			S9xResetSuperFX();
			S9xReset();

			switch (GCSettings.Interpolation)
			{
			case 0: Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN; break;
			case 1: Settings.InterpolationMethod = DSP_INTERPOLATION_LINEAR; break;
			case 2: Settings.InterpolationMethod = DSP_INTERPOLATION_CUBIC; break;
			case 3: Settings.InterpolationMethod = DSP_INTERPOLATION_SINC; break;
			case 4: Settings.InterpolationMethod = DSP_INTERPOLATION_NONE; break;
			}
		}
		
		autoboot = false;		
		ConfigRequested = 0;
		ScreenshotRequested = 0;
		SwitchAudioMode(0);

		Settings.Mute = GCSettings.MuteAudio;
		Settings.SupportHiRes = (GCSettings.HiResolution == 1);
		Settings.MaxSpriteTilesPerLine = (GCSettings.SpriteLimit ? 34 : 128);
		Settings.SkipFrames = (GCSettings.FrameSkip ? AUTO_FRAMERATE : 0);
		Settings.AutoDisplayMessages = (Settings.DisplayFrameRate || Settings.DisplayTime ? true : false);
		Settings.MultiPlayer5Master = (GCSettings.Controller == CTRL_PAD4 ? true : false);
		Settings.SuperScopeMaster = (GCSettings.Controller == CTRL_SCOPE ? true : false);
		Settings.MouseMaster = (GCSettings.Controller == CTRL_MOUSE || GCSettings.Controller == CTRL_MOUSE_PORT2 || GCSettings.Controller == CTRL_MOUSE_BOTH_PORTS);
		Settings.JustifierMaster = (GCSettings.Controller == CTRL_JUST ? true : false);
		SetControllers ();

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceThread();

		AudioStart ();

		FrameTimer = 0;
		setFrameTimerMethod (); // set frametimer method every time a ROM is loaded

		CheckVideo = 2;		// force video update
		prevRenderedFrameCount = IPPU.RenderedFramesCount;
		SelectFilterMethod(GCSettings.FilterMethod); // Initialize / Re-evaluate active filter

		while(1) // emulation loop
		{
			S9xMainLoop ();
			ReportButtons ();
			ClearButtonsReported ();

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

void ExitApp() {
	SavePrefs(SILENT);

	if (SNESROMSize > 0 && !ConfigRequested && GCSettings.AutoSave == AUTOSAVE_SRAM)
		SaveSRAMAuto(SILENT);

	SystemExit(GCSettings.ExitAction, autoboot);
}
