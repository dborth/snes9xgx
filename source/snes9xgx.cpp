/****************************************************************************
 * Snes9x GX
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Daryl Borth 2008-2026
 *
 * snes9xgx.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include "snes9xgx.h"
#include "s9xsupport.h"
#include "video.h"
#include "menu.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "memmanager.h"
#include "font_ttf.h"
#include "libgui/Gui.h"

#include "snes9x/snes9x.h"
#include "snes9x/fxemu.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

#include "drivers/Platform.h"
#include "drivers/Thread.h"
#if defined(HW_RVL) || defined(HW_DOL)
#include "drivers/ogc/videofilters.h"
#endif

#ifdef HW_DOL
#include "drivers/ogc/GameCubePlatform.h"
static GameCubePlatform platformInstance;
#elif HW_RVL
#include "drivers/ogc/WiiPlatform.h"
static WiiPlatform platformInstance;
#endif
Platform* platform = &platformInstance;

AppRequest appRequest = AppRequest::NONE;
char appPath[1024] = { 0 };
static bool firstRun = true;
static bool autoboot = false;

int main(int argc, char *argv[])
{
	InitMemManager();
	platform->init(640, 480);
	InitFileOpThreads();
	MountAllFAT();

	fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size, platform->getVideo()->getGlyphRenderer());
	textTranslator = new GuiTextTranslator();
	textTranslator->loadLanguage(en_lang, en_lang_size);

	DefaultSettings();
	InitializeSnes9x(); // ensure Snes9x memory is in MEM1 for Wii
	platform->getVideo()->startMenuVideo();
	S9xInitSync(); // initialize frame sync
	InitGUIThreads();

	savebuffer = (uint8_t *)extmem_malloc(SAVEBUFFERSIZE);

#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != nullptr)
		CreateAppPath(argv[0]);

	if(argc > 2 && argv[1] != nullptr && argv[2] != nullptr) {
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
		SavePrefs();

		GCSettings.AutoloadGame = AutoloadGame(argv[1], argv[2]);
		autoboot = GCSettings.AutoloadGame;
	}
#endif

	while (appRequest != AppRequest::EXIT && platform->getSystemEvent() != SystemEvent::ShutdownRequested) // main loop
	{
		if(!autoboot) {
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			ResumeDeviceCheckingThread();
			platform->getAudio()->startMenuAudio();
			SwitchMemoryModeMenu();

			if(SNESROMSize == 0)
				MainMenu(MENU_GAMESELECTION);
			else
				MainMenu(MENU_GAME);
		}

		if(appRequest == AppRequest::EXIT || platform->getSystemEvent() == SystemEvent::ShutdownRequested) {
			break;
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
		appRequest = AppRequest::NONE;
		platform->getAudio()->startEmulatorAudio();

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
		HaltDeviceCheckingThread();

		SwitchMemoryModeGame();

		platform->getVideo()->setFrameTimer(0);
		setFrameTimerMethod (); // set frametimer method every time a ROM is loaded

		platform->getVideo()->getEmulatorVideo()->forceVideoUpdate();
#if defined(HW_RVL) || defined(HW_DOL)
		SelectFilterMethod(GCSettings.videoUpscalingFilter); // Initialize / Re-evaluate active filter
#endif

		while(appRequest == AppRequest::NONE) // emulation loop
		{
			SystemEvent event = platform->getSystemEvent(); // poll exactly once per iteration
			if(event == SystemEvent::ShutdownRequested)
				break;

			S9xMainLoop ();
			ReportButtons ();
			ClearButtonsReported ();

			if(event == SystemEvent::ResetRequested)
			{
				S9xSoftReset (); // reset game
			}
			if (appRequest == AppRequest::MENU)
			{
				appRequest = AppRequest::NONE;
				SwitchMemoryModeMenu();
				TakeScreenshot();
				platform->getVideo()->startMenuVideo();
				break;
			}
		} // emulation loop
	} // main loop
	ExitApp();
}

void ExitApp() {
	SavePrefs();

	if (SNESROMSize > 0 && appRequest != AppRequest::MENU && GCSettings.AutoSave == AUTOSAVE_SRAM)
		SaveSRAMAuto(SILENT);

	HaltDeviceCheckingThread();

	// Generic safety net: stop and join every Thread still outstanding
	// (device/parse/worker) before any driver it might touch gets torn
	// down inside requestExit()/shutdown().
	Thread::JoinAll();

	platform->requestExit(GCSettings.ExitAction, autoboot);
}
