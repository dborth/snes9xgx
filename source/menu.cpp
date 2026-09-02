/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <ogcsys.h>
#include <ogc/cond.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <memory>

#include "snes9xgx.h"
#include "memmanager.h"
#include "system.h"
#include "video.h"
#include "filebrowser.h"
#include "gcunzip.h"
#include "networkop.h"
#include "fileop.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "input.h"
#include "drivers/ogc/videofilters.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "menu.h"

#include "utils/pngcodec.h"
#include "drivers/ogc/wiidrc.h"

#include "snes9x/port.h"
#include "snes9x/snes9x.h"
#include "snes9x/fxemu.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"
#include "snes9x/cheats.h"

extern SCheatData Cheat;
extern void ToggleCheat(uint32);

#define THREAD_SLEEP 100

#ifdef HW_RVL
static GuiImageData * pointer[4];
static GuiImage cursorImg[4];
#endif

static GuiTrigger * trigA = nullptr;

#ifdef HW_RVL
static GuiButton * batteryBtn[4];
#endif
static uint8_t * gameScreenTexture = nullptr;
static GuiImage * gameScreenImg = nullptr;
static GuiSound * bgMusic = nullptr;
static GuiSound * enterSound = nullptr;
static GuiSound * exitSound = nullptr;
static GuiText * settingText = nullptr;
static GuiText * settingText2 = nullptr;
static int lastMenu = MENU_NONE;
static int mapMenuCtrl = 0;
static int mapMenuCtrlSNES = 0;

static int currentLanguage = -1;

uint8_t * bg_music;
uint32_t bg_music_size;

struct Menu;
static Menu * menu = nullptr;

static volatile int showProgress = 0;

static mutex_t progMutex      = LWP_MUTEX_NULL;
static cond_t  progIdleCond   = LWP_COND_NULL; // signalled when the overlay has been fully torn down
static bool    progIdle       = true;          // protected by progMutex - true when no overlay is showing/pending

static char progressTitle[101];
static char progressMsg[201];
static int progressDone = 0;
static int progressTotal = 0;
static bool buttonMappingCancelled = false;

static lwp_t mainThreadId = LWP_THREAD_NULL;

static bool IsMainThread()
{
	return LWP_GetSelf() == mainThreadId;
}

static mutex_t promptMutex        = LWP_MUTEX_NULL;
static cond_t  promptDoneCond     = LWP_COND_NULL; // main -> background: result is ready
static bool    promptPending      = false; // protected by promptMutex
static bool    promptResultReady  = false; // protected by promptMutex
static int     promptResult       = 0;
static const char * promptPendingTitle;
static const char * promptPendingMsg;
static const char * promptPendingBtn1;
static const char * promptPendingBtn2;

/****************************************************************************
 * InitGUIThreads
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	mainThreadId = LWP_GetSelf();

	LWP_MutexInit(&progMutex, false);
	LWP_CondInit(&progIdleCond);

	LWP_MutexInit(&promptMutex, false);
	LWP_CondInit(&promptDoneCond);
}

/****************************************************************************
 * UpdateProgressOverlay
 *
 * Reflects the current progress/action state into the GUI
 ***************************************************************************/
struct ProgressOverlayState {
	GuiWindow progressWindow;
	GuiImageData dialogBox;
	GuiImage dialogBoxImg;
	GuiImageData progressbarOutline;
	GuiImage progressbarOutlineImg;
	GuiImageData progressbarEmpty;
	GuiImage progressbarEmptyImg;
	GuiImageData progressbar;
	GuiImage progressbarImg;
	GuiImageData throbber;
	GuiImage throbberImg;
	GuiText titleTxt;
	GuiText msgTxt;

	bool overlayShown;
	bool waitingToShow;
	u64 pendingStart;
	STATE oldState;
	float angle;
	uint32_t count;

	ProgressOverlayState() :
		progressWindow(448, 288),
		dialogBox(dialogue_box_png), dialogBoxImg(&dialogBox),
		progressbarOutline(progressbar_outline_png), progressbarOutlineImg(&progressbarOutline),
		progressbarEmpty(progressbar_empty_png), progressbarEmptyImg(&progressbarEmpty),
		progressbar(progressbar_png), progressbarImg(&progressbar),
		throbber(throbber_png), throbberImg(&throbber),
		titleTxt(nullptr, 26, (PixelColor){255, 255, 255, 255}),
		msgTxt(nullptr, 26, (PixelColor){0, 0, 0, 255}),
		overlayShown(false), waitingToShow(false), pendingStart(0),
		oldState(STATE::DEFAULT), angle(0), count(0)
	{
		progressWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		progressWindow.setPosition(0, -10);
		progressWindow.append(&dialogBoxImg);

		titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		titleTxt.setPosition(0, 14);
		progressWindow.append(&titleTxt);

		msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		msgTxt.setPosition(0, 80);
		progressWindow.append(&msgTxt);

		progressbarOutlineImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		progressbarOutlineImg.setPosition(25, 40);

		progressbarEmptyImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		progressbarEmptyImg.setPosition(25, 40);
		progressbarEmptyImg.setTile(100);

		progressbarImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		progressbarImg.setPosition(25, 40);

		throbberImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		throbberImg.setPosition(0, 40);
	}

	void update();
};

struct Menu {
	GuiWindow mainWindow;

	GuiImageData bgTop;
	GuiImage bgTopImg;
	GuiImageData bgBottom;
	GuiImage bgBottomImg;
	GuiImageData logo;
	GuiImage logoImg;
	GuiImageData logoOver;
	GuiImage logoImgOver;
	GuiText logoTxt;
	GuiButton btnLogo;

	GuiSound btnSoundOver;
	GuiSound btnSoundClick;

	ProgressOverlayState progressOverlayState;

	Menu() :
		mainWindow(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
		bgTop(bg_top_png), bgTopImg(&bgTop),
		bgBottom(bg_bottom_png), bgBottomImg(&bgBottom),
		logo(logo_png), logoImg(&logo),
		logoOver(logo_over_png), logoImgOver(&logoOver),
		logoTxt(APPVERSION, 18, (PixelColor){255, 255, 255, 255}),
		btnLogo(logoImg.getWidth(), logoImg.getHeight()),
		btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM),
		btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM)
	{
		bgBottomImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);

		logoTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
		logoTxt.setPosition(0, 4);

		btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
		btnLogo.setPosition(-50, 24);
		btnLogo.setImage(&logoImg);
		btnLogo.setImageOver(&logoImgOver);
		btnLogo.setLabel(&logoTxt);
		btnLogo.setSoundOver(&btnSoundOver);
		btnLogo.setSoundClick(&btnSoundClick);
		btnLogo.setTrigger(trigA);

		mainWindow.append(gameScreenImg);
		mainWindow.append(&bgTopImg);
		mainWindow.append(&bgBottomImg);
		mainWindow.append(&btnLogo);
	}
};

void ProgressOverlayState::update() {
	if(!menu) return;

	LWP_MutexLock(progMutex);
	int progress = showProgress;
	int done = progressDone;
	int total = progressTotal;
	char title[101]; snprintf(title, sizeof(title), "%s", progressTitle);
	char msg[201]; snprintf(msg, sizeof(msg), "%s", progressMsg);
	LWP_MutexUnlock(progMutex);

	if(!progress)
	{
		waitingToShow = false;

		if(overlayShown)
		{
			menu->mainWindow.remove(&progressWindow);
			menu->mainWindow.setState(oldState);
			overlayShown = false;
		}

		LWP_MutexLock(progMutex);
		if(!progIdle)
		{
			progIdle = true;
			LWP_CondBroadcast(progIdleCond);
		}
		LWP_MutexUnlock(progMutex);
	}
	else if(!overlayShown)
	{
		if(!waitingToShow)
		{
			waitingToShow = true;
			pendingStart = gettime();
		}
		else if(ticks_to_millisecs(diff_ticks(pendingStart, gettime())) >= 400)
		{
			titleTxt.setText(title);
			msgTxt.setText(msg);

			progressWindow.remove(&progressbarEmptyImg);
			progressWindow.remove(&progressbarImg);
			progressWindow.remove(&progressbarOutlineImg);
			progressWindow.remove(&throbberImg);

			if(progress == 1)
			{
				progressWindow.append(&progressbarEmptyImg);
				progressWindow.append(&progressbarImg);
				progressWindow.append(&progressbarOutlineImg);
			}
			else
			{
				progressWindow.append(&throbberImg);
			}

			oldState = menu->mainWindow.getState();
			menu->mainWindow.setState(STATE::DISABLED);
			menu->mainWindow.append(&progressWindow);
			menu->mainWindow.changeFocus(&progressWindow);

			overlayShown = true;
			waitingToShow = false;
			angle = 0;
			count = 0;
		}
	}

	if(overlayShown)
	{
		if(progress == 1 && total > 0)
		{
			progressbarImg.setTile(100*done/total);
		}
		else if(progress == 2)
		{
			if(count % 5 == 0)
			{
				angle += 45.0f;
				if(angle >= 360.0f)
					angle = 0;
				throbberImg.setAngle(angle);
			}
			++count;
		}
	}
}

static void ProcessGuiInput() {
	UpdatePads();

	menu->mainWindow.update(userInput[3]);
	menu->mainWindow.update(userInput[2]);
	menu->mainWindow.update(userInput[1]);
	menu->mainWindow.update(userInput[0]);
}

static void DrawGui() {
	menu->mainWindow.draw();

	#ifdef HW_RVL
	int i = 3;
	do
	{
		if(userInput[i]->getPadData().validPointer) {
			cursorImg[i].setPosition(userInput[i]->getPadData().cursor_x-48, userInput[i]->getPadData().cursor_y-48);
			cursorImg[i].setAngle(userInput[i]->getPadData().cursor_angle);
			cursorImg[i].draw();
		}
		--i;
	} while(i>=0);
	#endif

	platform->getVideo()->renderMenu();
}

/****************************************************************************
 * CreditsWindow
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/

static void CreditsWindow()
{
	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	creditsWindowBox.append(&creditsBoxImg);

	int numEntries = 24;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: https://github.com/dborth/snes9xgx", 20, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=40;

	GuiText::setPresets(20, (PixelColor){0, 0, 0, 255}, 0, GUI_TEXT_JUSTIFY_LEFT | GUI_TEXT_ALIGN_TOP, ALIGN_H::LEFT, ALIGN_V::TOP);
	txt[i] = new GuiText("Coding & menu design");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("Tantric");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("Additional improvements");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("Zopenko, michniewski");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("InfiniteBlue, others");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("Menu artwork");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("the3seashells");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("Menu sound");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("Peter de Man");
	txt[i]->setPosition(350,y); i++; y+=48;

	txt[i] = new GuiText("Snes9x GX GameCube");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("SoftDev, crunchy2,");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("eke-eke, others");
	txt[i]->setPosition(350,y); i++; y+=24;
	txt[i] = new GuiText("Snes9x");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("Snes9x Team");
	txt[i]->setPosition(350,y); i++; y+=24;

	txt[i] = new GuiText("libogc / devkitPPC");
	txt[i]->setPosition(60,y); i++;
	txt[i] = new GuiText("shagkur & WinterMute");
	txt[i]->setPosition(350,y); i++; y+=24;

	char consoleDetails[40];
	char memoryFreeInfo[50];
	char controllerInfo[100];

	sprintf(consoleDetails, getConsoleDetails());
	sprintf(memoryFreeInfo, getMemoryFreeInfo());

#ifdef HW_RVL
	sprintf(controllerInfo, GetUSBControllerInfo());
#endif

	txt[i] = new GuiText(consoleDetails, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-90); i++;
	txt[i] = new GuiText(memoryFreeInfo, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-76); i++;
	txt[i] = new GuiText(controllerInfo, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(20,-52); i++;

	GuiText::setPresets(12, (PixelColor){0, 0, 0, 255}, 0, GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::BOTTOM);

	txt[i] = new GuiText("Snes9x - Copyright (c) Snes9x Team 1996 - 2023");
	txt[i]->setPosition(0,-44); i++;
	txt[i] = new GuiText("This software is open source and may be copied, distributed, or modified ");
	txt[i]->setPosition(0,-32); i++;
	txt[i] = new GuiText("under the terms of the GNU General Public License (GPL) Version 2.");
	txt[i]->setPosition(0,-20);

	for(i=0; i < numEntries; i++)
		creditsWindowBox.append(txt[i]);

	STATE oldState = menu->mainWindow.getState();
	menu->mainWindow.setState(STATE::DISABLED);
	menu->mainWindow.append(&creditsWindowBox);
	menu->mainWindow.changeFocus(&creditsWindowBox);

	auto buttonPressed = [&]()-> bool {
		return userInput[0]->getPadData().buttons_d || userInput[1]->getPadData().buttons_d ||
			   userInput[2]->getPadData().buttons_d || userInput[3]->getPadData().buttons_d; };

	// debounce - wait for button to be unpressed
	while(buttonPressed())
	{
		ProcessGuiInput();
		DrawGui();
	}

	// credits open - wait for button to be pressed
	while(!buttonPressed())
	{
		ProcessGuiInput();
		DrawGui();
	}

	menu->mainWindow.remove(&creditsWindowBox);

	for(i=0; i < numEntries; i++)
		delete txt[i];

	// credits closed - wait for button to be unpressed (so we don't just reopen credits)
	while(buttonPressed())
	{
		ProcessGuiInput();
		DrawGui();
	}
	menu->mainWindow.setState(oldState);
}

static void ServicePendingCreditsWindowRequest() {
	if(menu->btnLogo.getState() == STATE::CLICKED)
	{
		menu->btnLogo.resetState();
		CreditsWindow();
	}
}

/****************************************************************************
 * Cross-thread WindowPrompt() support
 ***************************************************************************/
static int WindowPrompt(const char *, const char *, const char *, const char *);

static void ServicePendingWindowPromptRequest()
{
	LWP_MutexLock(promptMutex);
	bool pending = promptPending;
	LWP_MutexUnlock(promptMutex);

	if(!pending)
		return;

	int result = WindowPrompt(promptPendingTitle, promptPendingMsg, promptPendingBtn1, promptPendingBtn2);

	LWP_MutexLock(promptMutex);
	promptResult = result;
	promptResultReady = true;
	promptPending = false;
	LWP_CondBroadcast(promptDoneCond);
	LWP_MutexUnlock(promptMutex);
}

/****************************************************************************
 * UpdateGui
 *
 * The single shared frame-step primitive. Performs exactly one frame:
 * scans input for all active controllers, applies that input to the
 * element tree, reflects the progress overlay state, draws the tree, and
 * presents/renders
 *
 * Returns true if the caller should keep looping, or false if the caller
 * must stop immediately and return control up the call stack
 ***************************************************************************/

static bool UpdateGui()
{
	static bool exiting = false;
	if(exiting)
		return false;

	ProcessGuiInput();

	menu->progressOverlayState.update();
	ServicePendingWindowPromptRequest();
	ServicePendingCreditsWindowRequest();

	DrawGui();

	if(ExitRequested || ShutdownRequested)
	{
		for(int a = 0; a <= 255; a += 15)
		{
			menu->mainWindow.draw();
			platform->getVideo()->getImageRenderer()->drawRectangle(0,0,platform->getVideo()->getScreenWidth(),platform->getVideo()->getScreenHeight(),(PixelColor){0, 0, 0, (uint8_t)a});
			platform->getVideo()->renderMenu();
		}
		exiting = true;
		return false;
	}

	return true;
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice.
 ***************************************************************************/
static int WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	if(!menu)
		return 0;

	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	promptWindow.setPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_prompt_png);
	GuiImageData btnOutlineOver(button_prompt_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);
	GuiText msgTxt(msg, 26, (PixelColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	msgTxt.setPosition(0,-20);
	msgTxt.setWrap(true, 430);

	GuiText btn1Txt(btn1Label, 22, (PixelColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.getWidth(), btnOutline.getHeight());

	if(btn2Label)
	{
		btn1.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
		btn1.setPosition(20, -25);
	}
	else
	{
		btn1.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
		btn1.setPosition(0, -25);
		btn1.setTrigger(&trigB);
	}

	btn1.setLabel(&btn1Txt);
	btn1.setImage(&btn1Img);
	btn1.setImageOver(&btn1ImgOver);
	btn1.setSoundOver(&btnSoundOver);
	btn1.setSoundClick(&btnSoundClick);
	btn1.setTrigger(trigA);
	btn1.setState(STATE::SELECTED);
	btn1.setEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (PixelColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.getWidth(), btnOutline.getHeight());
	btn2.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	btn2.setPosition(-20, -25);
	btn2.setLabel(&btn2Txt);
	btn2.setImage(&btn2Img);
	btn2.setImageOver(&btn2ImgOver);
	btn2.setSoundOver(&btnSoundOver);
	btn2.setSoundClick(&btnSoundClick);
	btn2.setTrigger(trigA);
	btn2.setEffectGrow();

	promptWindow.append(&dialogBoxImg);
	promptWindow.append(&titleTxt);
	promptWindow.append(&msgTxt);
	promptWindow.append(&btn1);

	if(btn2Label)
	{
		promptWindow.append(&btn2);
		btn2.setTrigger(&trigB);
	}

	promptWindow.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 50);
	CancelAction();
	menu->mainWindow.setState(STATE::DISABLED);
	menu->mainWindow.appendWithAutoRemove(&promptWindow);
	menu->mainWindow.changeFocus(&promptWindow);
	if(btn2Label)
	{
		btn1.resetState();
		btn2.setState(STATE::SELECTED);
	}

	while(choice == -1)
	{
		if(!UpdateGui()) return 0;

		if(btn1.getState() == STATE::CLICKED)
			choice = 1;
		else if(btn2.getState() == STATE::CLICKED)
			choice = 0;
	}

	promptWindow.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 50);
	while(promptWindow.getEffect() > 0)
	{
		if(!UpdateGui()) return choice;
	}
	menu->mainWindow.setState(STATE::DEFAULT);
	return choice;
}

/****************************************************************************
 * CancelAction
 *
 * Cancels any progress/action overlay currently showing (or about to show),
 * and waits for it to be fully torn down before returning.
 ***************************************************************************/
void CancelAction()
{
	if(!menu) return;

	LWP_MutexLock(progMutex);
	showProgress = 0;
	LWP_MutexUnlock(progMutex);

	if(IsMainThread())
	{
		menu->progressOverlayState.update(); // synchronously reflect showProgress==0 right now
	}
	else
	{
		LWP_MutexLock(progMutex);

		while(!progIdle)
			LWP_CondWait(progIdleCond, progMutex);

		LWP_MutexUnlock(progMutex);
	}
}

/****************************************************************************
 * ShowProgress
 *
 * Updates the variables used by the progress window for drawing a progress
 * bar. Safe to call from a background worker thread - see CancelAction().
 ***************************************************************************/
void ShowProgress (const char *msg, int done, int total)
{
	if(!menu)
		return;

	if(total < (256*1024))
		return;
	else if(done > total) // this shouldn't happen
		done = total;

	if(done/total > 0.99)
		done = total;

	if(showProgress != 1)
		CancelAction(); // wait for previous progress window to finish

	LWP_MutexLock(progMutex);
	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 1;
	progressTotal = total;
	progressDone = done;
	progIdle = false;
	LWP_MutexUnlock(progMutex);
}

/****************************************************************************
 * ShowAction
 *
 * Shows that an action is underway. Safe to call from a background worker
 * thread - see CancelAction().
 ***************************************************************************/
void ShowAction (const char *msg)
{
	if(!menu)
		return;

	if(showProgress != 0)
		CancelAction(); // wait for previous progress window to finish

	LWP_MutexLock(progMutex);
	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressDone = 0;
	progressTotal = 0;
	progIdle = false;
	LWP_MutexUnlock(progMutex);
}

static int WindowPromptRequest(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	if(IsMainThread())
		return WindowPrompt(title, msg, btn1Label, btn2Label);

	if(!menu)
		return 0;

	LWP_MutexLock(promptMutex);
	promptPendingTitle = title;
	promptPendingMsg = msg;
	promptPendingBtn1 = btn1Label;
	promptPendingBtn2 = btn2Label;
	promptResultReady = false;
	promptPending = true;
	while(!promptResultReady)
		LWP_CondWait(promptDoneCond, promptMutex);
	int result = promptResult;
	LWP_MutexUnlock(promptMutex);
	return result;
}

void ErrorPrompt(const char *msg)
{
	WindowPromptRequest("Error", msg, "OK", nullptr);
}

int ErrorPromptRetry(const char *msg)
{
	return WindowPromptRequest("Error", msg, "Retry", "Cancel");
}

void InfoPrompt(const char *msg)
{
	WindowPromptRequest("Information", msg, "OK", nullptr);
}

/****************************************************************************
 * AutoSave
 *
 * Automatically saves SRAM/state when returning from in-game to the menu
 ***************************************************************************/
static void AutoSave()
{
	if (GCSettings.AutoSave == AUTOSAVE_SRAM)
	{
		SaveSRAMAuto(SILENT);
	}
	else if (GCSettings.AutoSave == AUTOSAVE_STATE)
	{
		if (WindowPrompt("Save", "Save State?", "Save", "Don't Save") )
			SaveSnapshotAuto(NOTSILENT);
	}
	else if (GCSettings.AutoSave == AUTOSAVE_BOTH)
	{
		if (WindowPrompt("Save", "Save SRAM and State?", "Save", "Don't Save") )
		{
			SaveSRAMAuto(NOTSILENT);
			SaveSnapshotAuto(NOTSILENT);
		}
	}
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, uint32_t maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiText okBtnTxt("OK", 22, (PixelColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.getWidth(), btnOutline.getHeight());

	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(25, -25);

	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setImageOver(&okBtnImgOver);
	okBtn.setSoundOver(&btnSoundOver);
	okBtn.setSoundClick(&btnSoundClick);
	okBtn.setTrigger(trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (PixelColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.getWidth(), btnOutline.getHeight());
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-25, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setImageOver(&cancelBtnImgOver);
	cancelBtn.setSoundOver(&btnSoundOver);
	cancelBtn.setSoundClick(&btnSoundClick);
	cancelBtn.setTrigger(trigA);
	cancelBtn.setEffectGrow();

	keyboard.append(&okBtn);
	keyboard.append(&cancelBtn);

	menu->mainWindow.setState(STATE::DISABLED);
	menu->mainWindow.appendWithAutoRemove(&keyboard);
	menu->mainWindow.changeFocus(&keyboard);

	while(save == -1)
	{
		if(!UpdateGui()) return;

		if(okBtn.getState() == STATE::CLICKED)
			save = 1;
		else if(cancelBtn.getState() == STATE::CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	menu->mainWindow.setState(STATE::DEFAULT);
}

/****************************************************************************
 * SettingWindow
 *
 * Opens a new window, with the specified window element appended. Allows
 * for a customizable prompted setting.
 ***************************************************************************/
static int
SettingWindow(const char * title, GuiWindow * w)
{
	int save = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);

	GuiText okBtnTxt("OK", 22, (PixelColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.getWidth(), btnOutline.getHeight());

	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(20, -25);

	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setImageOver(&okBtnImgOver);
	okBtn.setSoundOver(&btnSoundOver);
	okBtn.setSoundClick(&btnSoundClick);
	okBtn.setTrigger(trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (PixelColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.getWidth(), btnOutline.getHeight());
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-20, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setImageOver(&cancelBtnImgOver);
	cancelBtn.setSoundOver(&btnSoundOver);
	cancelBtn.setSoundClick(&btnSoundClick);
	cancelBtn.setTrigger(trigA);
	cancelBtn.setEffectGrow();

	promptWindow.append(&dialogBoxImg);
	promptWindow.append(&titleTxt);
	promptWindow.append(&okBtn);
	promptWindow.append(&cancelBtn);

	menu->mainWindow.setState(STATE::DISABLED);
	menu->mainWindow.appendWithAutoRemove(&promptWindow);
	menu->mainWindow.appendWithAutoRemove(w);
	menu->mainWindow.changeFocus(w);

	while(save == -1)
	{
		if(!UpdateGui()) return 0;

		if(okBtn.getState() == STATE::CLICKED)
			save = 1;
		else if(cancelBtn.getState() == STATE::CLICKED)
			save = 0;
	}
	menu->mainWindow.setState(STATE::DEFAULT);
	return save;
}

/****************************************************************************
 * MenuGameSelection
 *
 * Displays a list of games on the specified load device, and allows the user
 * to browse and select from this list.
 ***************************************************************************/
static char* getImageFolder()
{
	switch(GCSettings.PreviewImage)
	{
		case PREVIEWIMAGE_SCREENSHOT : return GCSettings.ScreenshotsFolder;
		case PREVIEWIMAGE_COVER : return GCSettings.CoverFolder;
		case PREVIEWIMAGE_ARTWORK : return GCSettings.ArtworkFolder;
		default : return GCSettings.CoverFolder;
	}
}

static int BrowserLoadFileTask(void * arg) { return BrowserLoadFile(); }

struct ChangeInterfaceArgs
{
	int device;
	bool silent;
};

static int ChangeInterfaceTask(void * arg) {
	ChangeInterfaceArgs * a = (ChangeInterfaceArgs *)arg;
	return ChangeInterface(a->device, a->silent) ? 1 : 0;
}

static int MenuGameSelection()
{
	int selection = MENU_NONE;
	bool res;
	int i;

	GuiText titleTxt("Choose Game", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData iconHome(icon_home_png);
	GuiImageData iconSettings(icon_settings_png);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);
	GuiImageData bgPreviewImg(bg_preview_png);

	GuiTrigger trigHome;
	trigHome.setButtonOnlyTrigger(-1, GUI_BTN_HOME);

	GuiText settingsBtnTxt("Settings", 22, (PixelColor){0, 0, 0, 255});
	GuiImage settingsBtnIcon(&iconSettings);
	settingsBtnIcon.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	settingsBtnIcon.setPosition(14,0);
	GuiImage settingsBtnImg(&btnOutline);
	GuiImage settingsBtnImgOver(&btnOutlineOver);
	GuiButton settingsBtn(btnOutline.getWidth(), btnOutline.getHeight());
	settingsBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	settingsBtn.setPosition(90, -35);
	settingsBtn.setLabel(&settingsBtnTxt);
	settingsBtn.setIcon(&settingsBtnIcon);
	settingsBtn.setImage(&settingsBtnImg);
	settingsBtn.setImageOver(&settingsBtnImgOver);
	settingsBtn.setSoundOver(&btnSoundOver);
	settingsBtn.setSoundClick(&btnSoundClick);
	settingsBtn.setTrigger(trigA);
	settingsBtn.setEffectGrow();

	GuiText exitBtnTxt("Exit", 22, (PixelColor){0, 0, 0, 255});
	GuiImage exitBtnIcon(&iconHome);
	exitBtnIcon.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	exitBtnIcon.setPosition(14,0);
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.getWidth(), btnOutline.getHeight());
	exitBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	exitBtn.setPosition(-90, -35);
	exitBtn.setLabel(&exitBtnTxt);
	exitBtn.setIcon(&exitBtnIcon);
	exitBtn.setImage(&exitBtnImg);
	exitBtn.setImageOver(&exitBtnImgOver);
	exitBtn.setSoundOver(&btnSoundOver);
	exitBtn.setSoundClick(&btnSoundClick);
	exitBtn.setTrigger(trigA);
	exitBtn.setTrigger(&trigHome);
	exitBtn.setEffectGrow();

	GuiWindow buttonWindow(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	buttonWindow.append(&settingsBtn);
	buttonWindow.append(&exitBtn);

	GuiFileBrowser gameBrowser(330, 268);
	gameBrowser.setPosition(20, 98);
	ResetBrowser();
	
	GuiImage bgPreview(&bgPreviewImg);
	bgPreview.setPosition(365, 98);
	int previousPreviewImg = GCSettings.PreviewImage;
	
	GuiImageData previewImageData;
	GuiImage preview;
	preview.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	preview.setPosition(174, -8);

	std::unique_ptr<uint8_t, decltype(&free)> pngFileBuffer((uint8_t *)malloc(PNG_FILE_BUFFER_SIZE), free);

	int  previousBrowserIndex = -1;
	char imagePath[MAXJOLIET + 1];
	
	menu->btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	menu->btnLogo.setPosition(-50, 24);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	menu->mainWindow.appendWithAutoRemove(&gameBrowser);
	menu->mainWindow.appendWithAutoRemove(&buttonWindow);
	menu->mainWindow.appendWithAutoRemove(&bgPreview);
	menu->mainWindow.appendWithAutoRemove(&preview);

	// populate initial directory listing
	selectLoadedFile = 1;
	OpenGameList();

	gameBrowser.resetState();
	gameBrowser.fileList[0]->setState(STATE::SELECTED);
	gameBrowser.triggerUpdate();
	titleTxt.setText(inSz ? szname : "Choose Game");
			
	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;
		
		if(selectLoadedFile == 2)
		{
			selectLoadedFile = 0;
			menu->mainWindow.changeFocus(&gameBrowser);
			gameBrowser.triggerUpdate();
		}

		// update gameWindow based on arrow buttons
		// set MENU_EXIT if A button pressed on a game
		for(i=0; i < FILE_PAGESIZE; i++)
		{
			if(gameBrowser.fileList[i]->getState() == STATE::CLICKED)
			{
				gameBrowser.fileList[i]->resetState();
				
				// check corresponding browser entry
				if(browserList[browser.selIndex].isdir || IsSz())
				{	
					res = BrowserChangeFolder();
					if(res)
					{
						gameBrowser.resetState();
						gameBrowser.fileList[0]->setState(STATE::SELECTED);
						gameBrowser.triggerUpdate();
						previousBrowserIndex = -1;			
					}
					else
					{
						selection = MENU_GAMESELECTION;
						break;
					}
										
					titleTxt.setText(inSz ? szname : "Choose Game");
					
				}
				else
				{
					menu->mainWindow.setState(STATE::DISABLED);

					if(RunOnWorkerThread(BrowserLoadFileTask))
					{
						while(!IsWorkerThreadFinished())
						{
							if(!UpdateGui()) return MENU_EXIT;
						}

						if(GetWorkerThreadResult())
							selection = MENU_EXIT;
						else
							menu->mainWindow.setState(STATE::DEFAULT);
					}
					else
					{
						menu->mainWindow.setState(STATE::DEFAULT);
					}
				}
			}
		}
		
		//update gamelist image
		if(previousBrowserIndex != browser.selIndex || previousPreviewImg != GCSettings.PreviewImage)
		{
			previousBrowserIndex = browser.selIndex;
			previousPreviewImg = GCSettings.PreviewImage;

			// ensure selected index is valid
			bool loadedPreview = false;

			if(browser.dir[0] != 0 && GCSettings.LoadMethod > 0 && browser.numEntries > 0 && browser.selIndex > 0 && browser.selIndex < browser.numEntries)
			{
				snprintf(imagePath, MAXJOLIET, "%s%s/%s.png", pathPrefix[GCSettings.LoadMethod], getImageFolder(), browserList[browser.selIndex].displayname);

				if(ChangeInterface(imagePath, SILENT) &&
				   LoadFile((char *)pngFileBuffer.get(), imagePath, 0, PNG_FILE_BUFFER_SIZE, SILENT) &&
				   previewImageData.reload(pngFileBuffer.get(), 640, 480))
				{
					preview.setImage(&previewImageData);
					preview.setScale( MIN(225.0f / previewImageData.getWidth(), 235.0f / previewImageData.getHeight()) );
					loadedPreview = true;
				}
			}

			if(!loadedPreview)
				preview.setImage(nullptr);
		}

		if(settingsBtn.getState() == STATE::CLICKED)
			selection = MENU_SETTINGS;
		else if(exitBtn.getState() == STATE::CLICKED)
			ExitRequested = 1;
	}

	HaltParseThread(); // halt parsing
	ResetBrowser();
	return selection;
}

/****************************************************************************
 * ControllerWindowUpdate
 *
 * Callback for controller window. Responds to clicks on window elements.
 ***************************************************************************/
static void ControllerWindowUpdate(void * ptr, int dir)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->getState() == STATE::CLICKED)
	{
		GCSettings.Controller += dir;

		if(GCSettings.Controller > CTRL_PAD4)
			GCSettings.Controller = CTRL_SCOPE;
		if(GCSettings.Controller < CTRL_SCOPE)
			GCSettings.Controller = CTRL_PAD4;

		settingText->setText(ctrlName[GCSettings.Controller]);
		b->resetState();
	}
}

/****************************************************************************
 * ControllerWindowLeftClick / ControllerWindowRightsClick
 *
 * Callbacks for controller window arrows. Responds arrow clicks.
 ***************************************************************************/
static void ControllerWindowLeftClick(void * ptr) { ControllerWindowUpdate(ptr, -1); }
static void ControllerWindowRightClick(void * ptr) { ControllerWindowUpdate(ptr, +1); }

/****************************************************************************
 * ControllerWindow
 *
 * Opens a window to allow the user to select the controller to be used.
 ***************************************************************************/
static void ControllerWindow()
{
	GuiWindow * w = new GuiWindow(300,250);
	w->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiTrigger trigLeft;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_LEFT);

	GuiTrigger trigRight;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.getWidth(), arrowLeft.getHeight());
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setImageOver(&arrowLeftOverImg);
	arrowLeftBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(ControllerWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.getWidth(), arrowRight.getHeight());
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setImageOver(&arrowRightOverImg);
	arrowRightBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(ControllerWindowRightClick);

	settingText = new GuiText(ctrlName[GCSettings.Controller], 22, (PixelColor){0, 0, 0, 255});

	int currentController = GCSettings.Controller;

	w->append(&arrowLeftBtn);
	w->append(&arrowRightBtn);
	w->append(settingText);

	if(!SettingWindow("Controller",w))
		GCSettings.Controller = currentController; // undo changes

	delete(w);
	delete(settingText);
}

#ifdef HW_RVL
static int playerMappingChan = 0;

static void PlayerMappingWindowUpdate(void * ptr, int dir)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->getState() == STATE::CLICKED)
	{
		playerMapping[playerMappingChan] += dir;

		if(playerMapping[playerMappingChan] > 3)
			playerMapping[playerMappingChan] = 0;
		if(playerMapping[playerMappingChan] < 0)
			playerMapping[playerMappingChan] = 3;

		char playerNumber[20];
		sprintf(playerNumber, "Player %d", playerMapping[playerMappingChan]+1);

		settingText->setText(playerNumber);
		b->resetState();
	}
}

static void PlayerMappingWindowLeftClick(void * ptr) { PlayerMappingWindowUpdate(ptr, -1); }
static void PlayerMappingWindowRightClick(void * ptr) { PlayerMappingWindowUpdate(ptr, +1); }

static void PlayerMappingWindow(int chan)
{
	playerMappingChan = chan;

	GuiWindow * w = new GuiWindow(300,250);
	w->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiTrigger trigLeft;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_LEFT);

	GuiTrigger trigRight;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.getWidth(), arrowLeft.getHeight());
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setImageOver(&arrowLeftOverImg);
	arrowLeftBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(PlayerMappingWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.getWidth(), arrowRight.getHeight());
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setImageOver(&arrowRightOverImg);
	arrowRightBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(PlayerMappingWindowRightClick);
	
	char playerNumber[20];
	sprintf(playerNumber, "Player %d", playerMapping[playerMappingChan]+1);

	settingText = new GuiText(playerNumber, 22, (PixelColor){0, 0, 0, 255});

	w->append(&arrowLeftBtn);
	w->append(&arrowRightBtn);
	w->append(settingText);

	char title[50];
	sprintf(title, "Player Mapping - Controller %d", chan+1);

	int previousPlayerMapping = playerMapping[playerMappingChan];

	if(!SettingWindow(title,w))
		playerMapping[playerMappingChan] = previousPlayerMapping; // undo changes

	delete(w);
	delete(settingText);
}
#endif

/****************************************************************************
 * MenuGame
 *
 * Menu displayed when returning to the menu from in-game.
 ***************************************************************************/
static int MenuGame()
{
	int selection = MENU_NONE;
	
	GuiText titleTxt((char *)Memory.ROMFilename, 22, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,40);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconGameSettings(icon_game_settings_png);
	GuiImageData iconLoad(icon_game_load_png);
	GuiImageData iconSave(icon_game_save_png);
	GuiImageData iconDelete(icon_game_delete_png);
	GuiImageData iconReset(icon_game_reset_png);

	GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

	GuiTrigger trigHome;
	GuiTrigger trigB;
	trigHome.setButtonOnlyTrigger(-1, GUI_BTN_HOME);
	trigB.setSecondaryTrigger();

	GuiText saveBtnTxt("Save", 22, (PixelColor){0, 0, 0, 255});
	GuiImage saveBtnImg(&btnLargeOutline);
	GuiImage saveBtnImgOver(&btnLargeOutlineOver);
	GuiImage saveBtnIcon(&iconSave);
	GuiButton saveBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	saveBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	saveBtn.setPosition(-200, 120);
	saveBtn.setLabel(&saveBtnTxt);
	saveBtn.setImage(&saveBtnImg);
	saveBtn.setImageOver(&saveBtnImgOver);
	saveBtn.setIcon(&saveBtnIcon);
	saveBtn.setSoundOver(&btnSoundOver);
	saveBtn.setSoundClick(&btnSoundClick);
	saveBtn.setTrigger(trigA);
	saveBtn.setEffectGrow();

	GuiText loadBtnTxt("Load", 22, (PixelColor){0, 0, 0, 255});
	GuiImage loadBtnImg(&btnLargeOutline);
	GuiImage loadBtnImgOver(&btnLargeOutlineOver);
	GuiImage loadBtnIcon(&iconLoad);
	GuiButton loadBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	loadBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	loadBtn.setPosition(0, 120);
	loadBtn.setLabel(&loadBtnTxt);
	loadBtn.setImage(&loadBtnImg);
	loadBtn.setImageOver(&loadBtnImgOver);
	loadBtn.setIcon(&loadBtnIcon);
	loadBtn.setSoundOver(&btnSoundOver);
	loadBtn.setSoundClick(&btnSoundClick);
	loadBtn.setTrigger(trigA);
	loadBtn.setEffectGrow();

	GuiText deleteBtnTxt("Delete", 22, (PixelColor){0, 0, 0, 255});
	GuiImage deleteBtnImg(&btnLargeOutline);
	GuiImage deleteBtnImgOver(&btnLargeOutlineOver);
	GuiImage deleteBtnIcon(&iconDelete);
	GuiButton deleteBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	deleteBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	deleteBtn.setPosition(200, 120);
	deleteBtn.setLabel(&deleteBtnTxt);
	deleteBtn.setImage(&deleteBtnImg);
	deleteBtn.setImageOver(&deleteBtnImgOver);
	deleteBtn.setIcon(&deleteBtnIcon);
	deleteBtn.setSoundOver(&btnSoundOver);
	deleteBtn.setSoundClick(&btnSoundClick);
	deleteBtn.setTrigger(trigA);
	deleteBtn.setEffectGrow();
	
	GuiText resetBtnTxt("Reset", 22, (PixelColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnLargeOutline);
	GuiImage resetBtnImgOver(&btnLargeOutlineOver);
	GuiImage resetBtnIcon(&iconReset);
	GuiButton resetBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	resetBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	resetBtn.setPosition(125, 250);
	resetBtn.setLabel(&resetBtnTxt);
	resetBtn.setImage(&resetBtnImg);
	resetBtn.setImageOver(&resetBtnImgOver);
	resetBtn.setIcon(&resetBtnIcon);
	resetBtn.setSoundOver(&btnSoundOver);
	resetBtn.setSoundClick(&btnSoundClick);
	resetBtn.setTrigger(trigA);
	resetBtn.setEffectGrow();

	GuiText gameSettingsBtnTxt("Game Settings", 22, (PixelColor){0, 0, 0, 255});
	gameSettingsBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage gameSettingsBtnImg(&btnLargeOutline);
	GuiImage gameSettingsBtnImgOver(&btnLargeOutlineOver);
	GuiImage gameSettingsBtnIcon(&iconGameSettings);
	GuiButton gameSettingsBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	gameSettingsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	gameSettingsBtn.setPosition(-125, 250);
	gameSettingsBtn.setLabel(&gameSettingsBtnTxt);
	gameSettingsBtn.setImage(&gameSettingsBtnImg);
	gameSettingsBtn.setImageOver(&gameSettingsBtnImgOver);
	gameSettingsBtn.setIcon(&gameSettingsBtnIcon);
	gameSettingsBtn.setSoundOver(&btnSoundOver);
	gameSettingsBtn.setSoundClick(&btnSoundClick);
	gameSettingsBtn.setTrigger(trigA);
	gameSettingsBtn.setEffectGrow();

	GuiText mainmenuBtnTxt("Main Menu", 22, (PixelColor){0, 0, 0, 255});
	if(GCSettings.AutoloadGame) {
		mainmenuBtnTxt.setText("Exit");
	}
	GuiImage mainmenuBtnImg(&btnOutline);
	GuiImage mainmenuBtnImgOver(&btnOutlineOver);
	GuiButton mainmenuBtn(btnOutline.getWidth(), btnOutline.getHeight());
	mainmenuBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
	mainmenuBtn.setPosition(0, -35);
	mainmenuBtn.setLabel(&mainmenuBtnTxt);
	mainmenuBtn.setImage(&mainmenuBtnImg);
	mainmenuBtn.setImageOver(&mainmenuBtnImgOver);
	mainmenuBtn.setSoundOver(&btnSoundOver);
	mainmenuBtn.setSoundClick(&btnSoundClick);
	mainmenuBtn.setTrigger(trigA);
	mainmenuBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.getWidth(), btnCloseOutline.getHeight());
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setImageOver(&closeBtnImgOver);
	closeBtn.setSoundOver(&btnSoundOver);
	closeBtn.setSoundClick(&btnSoundClick);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setTrigger(&trigB);
	closeBtn.setEffectGrow();

	#ifdef HW_RVL
	int i;
	char txt[3];
	bool status[4] = { false, false, false, false };
	int level[4] = { 0, 0, 0, 0 };
	bool newStatus;
	int newLevel;
	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];

	for(i=0; i < 4; i++)
	{
		sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 20, (PixelColor){255, 255, 255, 255});
		batteryTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryImg[i]->setPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->setTile(0);
		batteryBarImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryBarImg[i]->setPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->setLabel(batteryTxt[i]);
		batteryBtn[i]->setImage(batteryImg[i]);
		batteryBtn[i]->setIcon(batteryBarImg[i]);
		batteryBtn[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
		batteryBtn[i]->setTrigger(trigA);
		batteryBtn[i]->setSoundOver(&btnSoundOver);
		batteryBtn[i]->setSoundClick(&btnSoundClick);
		batteryBtn[i]->setSelectable(false);
		batteryBtn[i]->setState(STATE::DISABLED);
		batteryBtn[i]->setAlpha(150);
	}
	
	batteryBtn[0]->setPosition(45, -65);
	batteryBtn[1]->setPosition(135, -65);
	batteryBtn[2]->setPosition(45, -40);
	batteryBtn[3]->setPosition(135, -40);
	#endif

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&saveBtn);
	w.append(&loadBtn);
	w.append(&deleteBtn);
	w.append(&resetBtn);
	w.append(&gameSettingsBtn);

	#ifdef HW_RVL
	w.append(batteryBtn[0]);
	w.append(batteryBtn[1]);
	w.append(batteryBtn[2]);
	w.append(batteryBtn[3]);
	#endif

	w.append(&mainmenuBtn);
	w.append(&closeBtn);

	menu->btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	menu->btnLogo.setPosition(-50, -40);
	menu->mainWindow.appendWithAutoRemove(&w);

	if(lastMenu == MENU_NONE)
	{
		enterSound->play();
		menu->bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		mainmenuBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		menu->bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		menu->btnLogo.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#ifdef HW_RVL
		batteryBtn[0]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[1]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[2]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[3]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#endif

		w.setEffect(EFFECT::FADE, 15);
	}

	
	if(lastMenu == MENU_NONE)
		AutoSave();

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		#ifdef HW_RVL
		for(i=0; i < 4; i++)
		{
			if(userInput[i]->getPadData().hw_connected[GUI_HW_WIIMOTE])
			{
				newStatus = true;
				newLevel = (userInput[i]->getPadData().battery_level / 100.0) * 4;
				if(newLevel > 4) newLevel = 4;
			}
			else
			{
				newStatus = false;
				newLevel = 0;
			}
			
			if(status[i] != newStatus || level[i] != newLevel)
			{
				if(newStatus == true) // controller connected
				{
					batteryBtn[i]->setAlpha(255);
					batteryBtn[i]->setState(STATE::DEFAULT);
					batteryBarImg[i]->setTile(newLevel);

					if(newLevel == 0)
						batteryImg[i]->setImage(&batteryRed);
					else
						batteryImg[i]->setImage(&battery);
				}
				else // controller not connected
				{
					batteryBtn[i]->setAlpha(150);
					batteryBtn[i]->setState(STATE::DISABLED);
					batteryBarImg[i]->setTile(0);
					batteryImg[i]->setImage(&battery);
				}
				status[i] = newStatus;
				level[i] = newLevel;
			}
		}
		#endif

		if(saveBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME_SAVE;
		}
		else if(loadBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME_LOAD;
		}
		else if(deleteBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME_DELETE;
		}
		else if(resetBtn.getState() == STATE::CLICKED)
		{
			if (WindowPrompt("Reset Game", "Are you sure that you want to reset this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				S9xSoftReset ();
				selection = MENU_EXIT;
			}
			else
			{
				resetBtn.resetState();
			}
		}
		else if(gameSettingsBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS;
		}
#ifdef HW_RVL
		else if(batteryBtn[0]->getState() == STATE::CLICKED)
		{
			PlayerMappingWindow(0);
		}
		else if(batteryBtn[1]->getState() == STATE::CLICKED)
		{
			PlayerMappingWindow(1);
		}
		else if(batteryBtn[2]->getState() == STATE::CLICKED)
		{
			PlayerMappingWindow(2);
		}
		else if(batteryBtn[3]->getState() == STATE::CLICKED)
		{
			PlayerMappingWindow(3);
		}
#endif
		else if(mainmenuBtn.getState() == STATE::CLICKED)
		{
			if (WindowPrompt("Quit Game", "Quit this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				menu->mainWindow.remove(gameScreenImg);
				delete gameScreenImg;
				if(gameScreenTexture != nullptr) {
					free(gameScreenTexture);
					gameScreenTexture = nullptr;
				}
				ClearScreenshot();
				if(GCSettings.AutoloadGame) {
					ExitApp();
				}
				else {
					gameScreenImg = new GuiImage(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight(), (PixelColor){175, 200, 215, 255});
					gameScreenImg->setStripe(10);
					menu->mainWindow.insert(gameScreenImg, 0);
					bgMusic->play();
					selection = MENU_GAMESELECTION;
				}
			}
		}
		else if(closeBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_EXIT;
			exitSound->play();
			menu->bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			mainmenuBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->btnLogo.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			#ifdef HW_RVL
			batteryBtn[0]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[1]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[2]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[3]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			#endif

			w.setEffect(EFFECT::FADE, -15);
			while(w.getEffect() > 0)
			{
				if(!UpdateGui()) return MENU_EXIT;
			}
		}
	}


	#ifdef HW_RVL
	for(i=0; i < 4; i++)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}
	#endif

	return selection;
}

/****************************************************************************
 * FindGameSaveNum
 *
 * Determines the save file number of the given file name
 * Returns -1 if none is found
 ***************************************************************************/
static int FindGameSaveNum(char * savefile)
{
	int n = -1;
	int romlen = strlen(Memory.ROMFilename);
	int savelen = strlen(savefile);

	int diff = savelen-romlen;

	if(strncmp(savefile, Memory.ROMFilename, romlen) != 0)
		return -1;

	if(savefile[romlen] == ' ')
	{
		if(diff == 5 && strncmp(&savefile[romlen+1], "Auto", 4) == 0)
			n = 0; // found Auto save
		else if(diff == 2 || diff == 3)
			n = atoi(&savefile[romlen+1]);
	}

	if(n >= 0 && n < MAX_SAVES)
		return n;
	else
		return -1;
}

/****************************************************************************
 * MenuGameSaves
 *
 * Allows the user to load or save progress.
 ***************************************************************************/
static int MenuGameSaves(int action)
{
	int selection = MENU_NONE;
	int ret;
	int i, n, type, len, len2;
	int j = 0;
	SaveList saves{};
	char filepath[1024];
	char deletepath[1024];
	char scrfile[1024];
	char tmp[MAXJOLIET+1];
	struct stat filestat;
	struct tm * timeinfo;

	static ChangeInterfaceArgs ciArgs;
	ciArgs.device = GCSettings.SaveMethod;
	ciArgs.silent = NOTSILENT;
	bool changeOk = false;

	if(RunOnWorkerThread(ChangeInterfaceTask, &ciArgs))
	{
		while(!IsWorkerThreadFinished())
		{
			if(!UpdateGui()) return MENU_EXIT;
		}
		changeOk = GetWorkerThreadResult() != 0;
	}

	if(!changeOk)
		return MENU_GAME;

	GuiText titleTxt(nullptr, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	if(action == 0)
		titleTxt.setText("Load Game");
	else if (action == 2)
		titleTxt.setText("Delete Saves");
	else
		titleTxt.setText("Save Game");

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiTrigger trigHome;
	GuiTrigger trigB;
	trigHome.setButtonOnlyTrigger(-1, GUI_BTN_HOME);
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.getWidth(), btnCloseOutline.getHeight());
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setImageOver(&closeBtnImgOver);
	closeBtn.setSoundOver(&btnSoundOver);
	closeBtn.setSoundClick(&btnSoundClick);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	w.append(&closeBtn);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	sprintf(browser.dir, "%s%s", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder);
	ParseDirectory(true, false);

	len = strlen(Memory.ROMFilename);

	// find matching files
	AllocSaveBuffer();

	for(i=0; i < browser.numEntries; i++)
	{
		len2 = strlen(browserList[i].filename);

		if(len2 < 6 || len2-len < 5)
			continue;

		if(strncmp(&browserList[i].filename[len2-4], ".srm", 4) == 0)
			type = FILE_SRAM;
		else if(strncmp(&browserList[i].filename[len2-4], ".frz", 4) == 0)
			type = FILE_STATE;
		else
			continue;

		strcpy(tmp, browserList[i].filename);
		tmp[len2-4] = 0;
		n = FindGameSaveNum(tmp);

		if(n >= 0)
		{
			saves.type[j] = type;
			saves.files[saves.type[j]][n] = 1;
			strcpy(saves.filename[j], browserList[i].filename);

			if(saves.type[j] == FILE_STATE)
			{
				sprintf(scrfile, "%s%s/%s.png", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder, tmp);

				memset(savebuffer, 0, SAVEBUFFERSIZE);
				if(LoadFile(scrfile, SILENT)) {
					auto thumb = std::make_unique<GuiImageData>(savebuffer, 64, 48);
					if(thumb->getTexture())
						saves.previewImg[j] = std::move(thumb);
				}
			}
			snprintf(filepath, 1024, "%s%s/%s", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder, saves.filename[j]);
			if (stat(filepath, &filestat) == 0)
			{
				timeinfo = localtime(&filestat.st_mtime);
				strftime(saves.date[j], 20, "%a %b %d", timeinfo);
				strftime(saves.time[j], 10, "%I:%M %p", timeinfo);
			}
			j++;
		}
	}

	FreeSaveBuffer();
	saves.length = j;

	if((saves.length == 0 && action == 0) || (saves.length == 0 && action == 2)) 
	{
		InfoPrompt("No game saves found.");
		selection = MENU_GAME;
	}

	GuiSaveBrowser saveBrowser(552, 248, &saves, action);
	saveBrowser.setPosition(0, 108);
	saveBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	menu->mainWindow.appendWithAutoRemove(&saveBrowser);
	menu->mainWindow.changeFocus(&saveBrowser);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = saveBrowser.getClickedSave();

		//load, save and delete save games
		if(ret > -3)
		{
			int result = 0;

			if(action == 0) // load
			{
				MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
				switch(saves.type[ret])
				{
					case FILE_SRAM:
						result = LoadSRAM(filepath, NOTSILENT);
						break;
					case FILE_STATE:
						result = LoadSnapshot(filepath, NOTSILENT);
						break;
				}
				if(result)
					selection = MENU_EXIT;
			}
			else if(action == 2) // delete RAM/State
			{
				if (WindowPrompt("Delete File", "Delete this save file? Deleted files can not be restored.", "OK", "Cancel"))
				{
					MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
					switch(saves.type[ret])
					{
						case FILE_SRAM:
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".srm");
							remove(deletepath); // Delete the *.srm file (Battery save file)
						break;
						case FILE_STATE:
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".png");
							remove(deletepath); // Delete the *.png file (Screenshot file)
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".frz");
							remove(deletepath); // Delete the *.frz file (Save State file)
						break;
					}							
				}
				selection = MENU_GAME_DELETE;
			}
			else // save
			{
				if(ret == -2) // new State
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_STATE][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_STATE, Memory.ROMFilename, i);
						SaveSnapshot(filepath, NOTSILENT);
						selection = MENU_GAME_SAVE;
					}
				}
				else if(ret == -1 && GCSettings.HideSRAMSaving == 0) // new SRAM
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_SRAM][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, i);
						SaveSRAM(filepath, NOTSILENT);
						selection = MENU_GAME_SAVE;
					}
				}
				else // overwrite SRAM/State
				{
					MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
					switch(saves.type[ret])
					{
						case FILE_SRAM:
							SaveSRAM(filepath, NOTSILENT);
							break;
						case FILE_STATE:
							SaveSnapshot(filepath, NOTSILENT);
							break;
					}
					selection = MENU_GAME_SAVE;
				}
			}
		}
		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME;
		}
		else if(closeBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_EXIT;

			exitSound->play();
			menu->bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			backBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->btnLogo.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);

			w.setEffect(EFFECT::FADE, -15);

			while(w.getEffect() > 0)
			{
				if(!UpdateGui()) return MENU_EXIT;
			}
		}
	}

	ResetBrowser();
	return selection;
}

/****************************************************************************
 * MenuGameSettings
 ***************************************************************************/
static int MenuGameSettings()
{
	int selection = MENU_NONE;
	char filepath[1024];

	GuiText titleTxt("Game Settings", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconMappings(icon_settings_mappings_png);
	GuiImageData iconVideo(icon_settings_video_png);
	GuiImageData iconEmulation(icon_settings_emulation_png);
	GuiImageData iconController(icon_game_controllers_png);
	GuiImageData iconCheats(icon_game_cheats_png);
	GuiImageData iconScreenshot(icon_settings_screenshot_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiTrigger trigHome;
	GuiTrigger trigB;
	trigHome.setButtonOnlyTrigger(-1, GUI_BTN_HOME);
	trigB.setSecondaryTrigger();

	GuiText mappingBtnTxt("Button Mappings", 22, (PixelColor){0, 0, 0, 255});
	mappingBtnTxt.setWrap(true, btnLargeOutline.getWidth()-30);
	GuiImage mappingBtnImg(&btnLargeOutline);
	GuiImage mappingBtnImgOver(&btnLargeOutlineOver);
	GuiImage mappingBtnIcon(&iconMappings);
	GuiButton mappingBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	mappingBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	mappingBtn.setPosition(-200, 120);
	mappingBtn.setLabel(&mappingBtnTxt);
	mappingBtn.setImage(&mappingBtnImg);
	mappingBtn.setImageOver(&mappingBtnImgOver);
	mappingBtn.setIcon(&mappingBtnIcon);
	mappingBtn.setSoundOver(&btnSoundOver);
	mappingBtn.setSoundClick(&btnSoundClick);
	mappingBtn.setTrigger(trigA);
	mappingBtn.setEffectGrow();
	
	GuiText emulationBtnTxt("Emulation", 22, (PixelColor){0, 0, 0, 255});
	emulationBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage emulationBtnImg(&btnLargeOutline);
	GuiImage emulationBtnImgOver(&btnLargeOutlineOver);
	GuiImage emulationBtnIcon(&iconEmulation);
	GuiButton emulationBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	emulationBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	emulationBtn.setPosition(0, 120);
	emulationBtn.setLabel(&emulationBtnTxt);
	emulationBtn.setImage(&emulationBtnImg);
	emulationBtn.setImageOver(&emulationBtnImgOver);
	emulationBtn.setIcon(&emulationBtnIcon);
	emulationBtn.setSoundOver(&btnSoundOver);
	emulationBtn.setSoundClick(&btnSoundClick);
	emulationBtn.setTrigger(trigA);
	emulationBtn.setEffectGrow();
	
	GuiText videoBtnTxt("Video", 22, (PixelColor){0, 0, 0, 255});
	videoBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage videoBtnImg(&btnLargeOutline);
	GuiImage videoBtnImgOver(&btnLargeOutlineOver);
	GuiImage videoBtnIcon(&iconVideo);
	GuiButton videoBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	videoBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	videoBtn.setPosition(200, 120);
	videoBtn.setLabel(&videoBtnTxt);
	videoBtn.setImage(&videoBtnImg);
	videoBtn.setImageOver(&videoBtnImgOver);
	videoBtn.setIcon(&videoBtnIcon);
	videoBtn.setSoundOver(&btnSoundOver);
	videoBtn.setSoundClick(&btnSoundClick);
	videoBtn.setTrigger(trigA);
	videoBtn.setEffectGrow();

	GuiText controllerBtnTxt("Controller", 22, (PixelColor){0, 0, 0, 255});
	GuiImage controllerBtnImg(&btnLargeOutline);
	GuiImage controllerBtnImgOver(&btnLargeOutlineOver);
	GuiImage controllerBtnIcon(&iconController);
	GuiButton controllerBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	controllerBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	controllerBtn.setPosition(-200, 250);
	controllerBtn.setLabel(&controllerBtnTxt);
	controllerBtn.setImage(&controllerBtnImg);
	controllerBtn.setImageOver(&controllerBtnImgOver);
	controllerBtn.setIcon(&controllerBtnIcon);
	controllerBtn.setSoundOver(&btnSoundOver);
	controllerBtn.setSoundClick(&btnSoundClick);
	controllerBtn.setTrigger(trigA);
	controllerBtn.setEffectGrow();

	GuiText screenshotBtnTxt("Screenshot", 22, (PixelColor){0, 0, 0, 255});
	GuiImage screenshotBtnImg(&btnLargeOutline);
	GuiImage screenshotBtnImgOver(&btnLargeOutlineOver);
	GuiImage screenshotBtnIcon(&iconScreenshot);
	GuiButton screenshotBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	screenshotBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	screenshotBtn.setPosition(0, 250);
	screenshotBtn.setLabel(&screenshotBtnTxt);
	screenshotBtn.setImage(&screenshotBtnImg);
	screenshotBtn.setImageOver(&screenshotBtnImgOver);
	screenshotBtn.setIcon(&screenshotBtnIcon);
	screenshotBtn.setSoundOver(&btnSoundOver);
	screenshotBtn.setSoundClick(&btnSoundClick);
	screenshotBtn.setTrigger(trigA);
	screenshotBtn.setEffectGrow();
	
	GuiText cheatsBtnTxt("Cheats", 22, (PixelColor){0, 0, 0, 255});
	GuiImage cheatsBtnImg(&btnLargeOutline);
	GuiImage cheatsBtnImgOver(&btnLargeOutlineOver);
	GuiImage cheatsBtnIcon(&iconCheats);
	GuiButton cheatsBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	cheatsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	cheatsBtn.setPosition(200, 250);
	cheatsBtn.setLabel(&cheatsBtnTxt);
	cheatsBtn.setImage(&cheatsBtnImg);
	cheatsBtn.setImageOver(&cheatsBtnImgOver);
	cheatsBtn.setIcon(&cheatsBtnIcon);
	cheatsBtn.setSoundOver(&btnSoundOver);
	cheatsBtn.setSoundClick(&btnSoundClick);
	cheatsBtn.setTrigger(trigA);
	cheatsBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.getWidth(), btnCloseOutline.getHeight());
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setImageOver(&closeBtnImgOver);
	closeBtn.setSoundOver(&btnSoundOver);
	closeBtn.setSoundClick(&btnSoundClick);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&mappingBtn);
	w.append(&videoBtn);
	w.append(&emulationBtn);
	w.append(&controllerBtn);
	w.append(&screenshotBtn);
	w.append(&cheatsBtn);
	w.append(&closeBtn);
	w.append(&backBtn);
	
	menu->mainWindow.appendWithAutoRemove(&w);


	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(mappingBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS;
		}
		else if(videoBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_VIDEO;
		}
		else if(emulationBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_EMULATION;
		}
		else if(controllerBtn.getState() == STATE::CLICKED)
		{
			ControllerWindow();
		}
		else if(cheatsBtn.getState() == STATE::CLICKED)
		{
			cheatsBtn.resetState();

			if(Cheat.g.size() > 0) {
				selection = MENU_GAMESETTINGS_CHEATS;
			}
			else {
				InfoPrompt("Cheats file not found!");
			}
		}
		else if(screenshotBtn.getState() == STATE::CLICKED)
		{
			if (WindowPrompt("Preview Screenshot", "Save a new Preview Screenshot? Current Screenshot image will be overwritten.", "OK", "Cancel"))
			{
				snprintf(filepath, 1024, "%s%s/%s", pathPrefix[GCSettings.LoadMethod], GCSettings.ScreenshotsFolder, Memory.ROMFilename);
				SavePreviewImg(filepath, NOTSILENT); 
			}
		}
		else if(closeBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_EXIT;

			exitSound->play();
			menu->bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			backBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			menu->btnLogo.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);

			w.setEffect(EFFECT::FADE, -15);

			while(w.getEffect() > 0)
			{
				if(!UpdateGui()) return MENU_EXIT;
			}
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME;
		}
	}

	return selection;
}

/****************************************************************************
 * MenuGameCheats
 *
 * Displays a list of cheats available, and allows the user to enable/disable
 * them.
 ***************************************************************************/
static int MenuGameCheats()
{
	int selection = MENU_NONE;
	int ret;
	uint16_t i = 0;
	OptionList options;

	for(i=0; i < Cheat.g.size(); i++)
	{
		snprintf (options.name[i], 50, "%s", Cheat.g[i].name);
		sprintf (options.value[i], "%s", Cheat.g[i].enabled == true ? "On" : "Off");
	}

	options.length = i;

	GuiText titleTxt("Game Settings - Cheats", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	optionBrowser.setCol2Position(475);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		if(ret >= 0)
		{
			ToggleCheat(ret);
			sprintf (options.value[ret], "%s", Cheat.g[ret].enabled == true ? "On" : "Off");
			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS;
		}
	}
	return selection;
}

/****************************************************************************
 * MenuSettingsMappings
 ***************************************************************************/
static int MenuSettingsMappings()
{
	int selection = MENU_NONE;

	GuiText titleTxt("Game Settings - Button Mappings", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconSNESController(icon_settings_snescontroller_png);
	GuiImageData iconSuperscope(icon_settings_superscope_png);
	GuiImageData iconJustifier(icon_settings_justifier_png);
	GuiImageData iconMouse(icon_settings_mouse_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText snesBtnTxt("SNES Controller", 22, (PixelColor){0, 0, 0, 255});
	snesBtnTxt.setWrap(true, btnLargeOutline.getWidth()-40);
	GuiImage snesBtnImg(&btnLargeOutline);
	GuiImage snesBtnImgOver(&btnLargeOutlineOver);
	GuiImage snesBtnIcon(&iconSNESController);
	GuiButton snesBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	snesBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	snesBtn.setPosition(-125, 120);
	snesBtn.setLabel(&snesBtnTxt);
	snesBtn.setImage(&snesBtnImg);
	snesBtn.setImageOver(&snesBtnImgOver);
	snesBtn.setIcon(&snesBtnIcon);
	snesBtn.setSoundOver(&btnSoundOver);
	snesBtn.setSoundClick(&btnSoundClick);
	snesBtn.setTrigger(trigA);
	snesBtn.setEffectGrow();

	GuiText superscopeBtnTxt("Super Scope", 22, (PixelColor){0, 0, 0, 255});
	superscopeBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage superscopeBtnImg(&btnLargeOutline);
	GuiImage superscopeBtnImgOver(&btnLargeOutlineOver);
	GuiImage superscopeBtnIcon(&iconSuperscope);
	GuiButton superscopeBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	superscopeBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	superscopeBtn.setPosition(125, 120);
	superscopeBtn.setLabel(&superscopeBtnTxt);
	superscopeBtn.setImage(&superscopeBtnImg);
	superscopeBtn.setImageOver(&superscopeBtnImgOver);
	superscopeBtn.setIcon(&superscopeBtnIcon);
	superscopeBtn.setSoundOver(&btnSoundOver);
	superscopeBtn.setSoundClick(&btnSoundClick);
	superscopeBtn.setTrigger(trigA);
	superscopeBtn.setEffectGrow();

	GuiText mouseBtnTxt("SNES Mouse", 22, (PixelColor){0, 0, 0, 255});
	mouseBtnTxt.setWrap(true, btnLargeOutline.getWidth()-55);
	GuiImage mouseBtnImg(&btnLargeOutline);
	GuiImage mouseBtnImgOver(&btnLargeOutlineOver);
	GuiImage mouseBtnIcon(&iconMouse);
	GuiButton mouseBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	mouseBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	mouseBtn.setPosition(-200, 250);
	mouseBtn.setLabel(&mouseBtnTxt);
	mouseBtn.setImage(&mouseBtnImg);
	mouseBtn.setImageOver(&mouseBtnImgOver);
	mouseBtn.setIcon(&mouseBtnIcon);
	mouseBtn.setSoundOver(&btnSoundOver);
	mouseBtn.setSoundClick(&btnSoundClick);
	mouseBtn.setTrigger(trigA);
	mouseBtn.setEffectGrow();

	GuiText justifierBtnTxt("Justifier", 22, (PixelColor){0, 0, 0, 255});
	GuiImage justifierBtnImg(&btnLargeOutline);
	GuiImage justifierBtnImgOver(&btnLargeOutlineOver);
	GuiImage justifierBtnIcon(&iconJustifier);
	GuiButton justifierBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	justifierBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	justifierBtn.setPosition(0, 250);
	justifierBtn.setLabel(&justifierBtnTxt);
	justifierBtn.setImage(&justifierBtnImg);
	justifierBtn.setImageOver(&justifierBtnImgOver);
	justifierBtn.setIcon(&justifierBtnIcon);
	justifierBtn.setSoundOver(&btnSoundOver);
	justifierBtn.setSoundClick(&btnSoundClick);
	justifierBtn.setTrigger(trigA);
	justifierBtn.setEffectGrow();

	GuiText otherBtnTxt("Other Mappings", 22, (PixelColor){0, 0, 0, 255});
	GuiImage otherBtnImg(&btnLargeOutline);
	GuiImage otherBtnImgOver(&btnLargeOutlineOver);
	GuiButton otherBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	otherBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	otherBtn.setPosition(200, 250);
	otherBtn.setLabel(&otherBtnTxt);
	otherBtn.setImage(&otherBtnImg);
	otherBtn.setImageOver(&otherBtnImgOver);
	otherBtn.setSoundOver(&btnSoundOver);
	otherBtn.setSoundClick(&btnSoundClick);
	otherBtn.setTrigger(trigA);
	otherBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&snesBtn);
	w.append(&superscopeBtn);
	w.append(&mouseBtn);
	w.append(&justifierBtn);
	w.append(&otherBtn);

	w.append(&backBtn);

	menu->mainWindow.appendWithAutoRemove(&w);


	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(snesBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_PAD;
		}
		else if(superscopeBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_SCOPE;
		}
		else if(mouseBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_MOUSE;
		}
		else if(justifierBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_JUST;
		}
		else if(otherBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_OTHER;
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS;
		}
	}
	return selection;
}

static int MenuSettingsMappingsController()
{
	int selection = MENU_NONE;
	char menuTitle[100];
	char menuSubtitle[100];

	sprintf(menuTitle, "Game Settings - Button Mappings");
	GuiText titleTxt(menuTitle, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	sprintf(menuSubtitle, "%s", ctrlName[mapMenuCtrlSNES]);
	GuiText subtitleTxt(menuSubtitle, 20, (PixelColor){255, 255, 255, 255});
	subtitleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	subtitleTxt.setPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconWiimote(icon_settings_wiimote_png);
	GuiImageData iconClassic(icon_settings_classic_png);
	GuiImageData iconGamecube(icon_settings_gamecube_png);
	GuiImageData iconNunchuk(icon_settings_nunchuk_png);
	GuiImageData iconWiiupro(icon_settings_wiiupro_png);
	GuiImageData iconDrc(icon_settings_drc_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();
	
	GuiText gamecubeBtnTxt("GameCube Controller", 22, (PixelColor){0, 0, 0, 255});
	gamecubeBtnTxt.setWrap(true, btnLargeOutline.getWidth()-30);
	GuiImage gamecubeBtnImg(&btnLargeOutline);
	GuiImage gamecubeBtnImgOver(&btnLargeOutlineOver);
	GuiImage gamecubeBtnIcon(&iconGamecube);
	GuiButton gamecubeBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	gamecubeBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	gamecubeBtn.setPosition(-125, 120);
	gamecubeBtn.setLabel(&gamecubeBtnTxt);
	gamecubeBtn.setImage(&gamecubeBtnImg);
	gamecubeBtn.setImageOver(&gamecubeBtnImgOver);
	gamecubeBtn.setIcon(&gamecubeBtnIcon);
	gamecubeBtn.setSoundOver(&btnSoundOver);
	gamecubeBtn.setSoundClick(&btnSoundClick);
	gamecubeBtn.setTrigger(trigA);
	gamecubeBtn.setEffectGrow();

	GuiText wiimoteBtnTxt("Wiimote", 22, (PixelColor){0, 0, 0, 255});
	GuiImage wiimoteBtnImg(&btnLargeOutline);
	GuiImage wiimoteBtnImgOver(&btnLargeOutlineOver);
	GuiImage wiimoteBtnIcon(&iconWiimote);
	GuiButton wiimoteBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	wiimoteBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	wiimoteBtn.setPosition(125, 120);
	wiimoteBtn.setLabel(&wiimoteBtnTxt);
	wiimoteBtn.setImage(&wiimoteBtnImg);
	wiimoteBtn.setImageOver(&wiimoteBtnImgOver);
	wiimoteBtn.setIcon(&wiimoteBtnIcon);
	wiimoteBtn.setSoundOver(&btnSoundOver);
	wiimoteBtn.setSoundClick(&btnSoundClick);
	wiimoteBtn.setTrigger(trigA);
	wiimoteBtn.setEffectGrow();

	GuiText drcBtnTxt("Wii U GamePad", 22, (PixelColor){0, 0, 0, 255});
	drcBtnTxt.setWrap(true, btnLargeOutline.getWidth()-30);
	GuiImage drcBtnImg(&btnLargeOutline);
	GuiImage drcBtnImgOver(&btnLargeOutlineOver);
	GuiImage drcBtnIcon(&iconDrc);
	GuiButton drcBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	drcBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	drcBtn.setPosition(200, 120);
	drcBtn.setLabel(&drcBtnTxt);
	drcBtn.setImage(&drcBtnImg);
	drcBtn.setImageOver(&drcBtnImgOver);
	drcBtn.setIcon(&drcBtnIcon);
	drcBtn.setSoundOver(&btnSoundOver);
	drcBtn.setSoundClick(&btnSoundClick);
	drcBtn.setTrigger(trigA);
	drcBtn.setEffectGrow();

	GuiText classicBtnTxt("Classic Controller", 22, (PixelColor){0, 0, 0, 255});
	classicBtnTxt.setWrap(true, btnLargeOutline.getWidth()-30);
	GuiImage classicBtnImg(&btnLargeOutline);
	GuiImage classicBtnImgOver(&btnLargeOutlineOver);
	GuiImage classicBtnIcon(&iconClassic);
	GuiButton classicBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	classicBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	classicBtn.setPosition(-200, 250);
	classicBtn.setLabel(&classicBtnTxt);
	classicBtn.setImage(&classicBtnImg);
	classicBtn.setImageOver(&classicBtnImgOver);
	classicBtn.setIcon(&classicBtnIcon);
	classicBtn.setSoundOver(&btnSoundOver);
	classicBtn.setSoundClick(&btnSoundClick);
	classicBtn.setTrigger(trigA);
	classicBtn.setEffectGrow();

	GuiText nunchukBtnTxt1("Wiimote", 22, (PixelColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt2("&", 18, (PixelColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt3("Nunchuk", 22, (PixelColor){0, 0, 0, 255});
	nunchukBtnTxt1.setPosition(0, -20);
	nunchukBtnTxt3.setPosition(0, +20);
	GuiImage nunchukBtnImg(&btnLargeOutline);
	GuiImage nunchukBtnImgOver(&btnLargeOutlineOver);
	GuiImage nunchukBtnIcon(&iconNunchuk);
	GuiButton nunchukBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	nunchukBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	nunchukBtn.setPosition(0, 250);
	nunchukBtn.setLabel(&nunchukBtnTxt1, 0);
	nunchukBtn.setLabel(&nunchukBtnTxt2, 1);
	nunchukBtn.setLabel(&nunchukBtnTxt3, 2);
	nunchukBtn.setImage(&nunchukBtnImg);
	nunchukBtn.setImageOver(&nunchukBtnImgOver);
	nunchukBtn.setIcon(&nunchukBtnIcon);
	nunchukBtn.setSoundOver(&btnSoundOver);
	nunchukBtn.setSoundClick(&btnSoundClick);
	nunchukBtn.setTrigger(trigA);
	nunchukBtn.setEffectGrow();

	GuiText wiiuproBtnTxt("Wii U Pro Controller", 22, (PixelColor){0, 0, 0, 255});
	wiiuproBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage wiiuproBtnImg(&btnLargeOutline);
	GuiImage wiiuproBtnImgOver(&btnLargeOutlineOver);
	GuiImage wiiuproBtnIcon(&iconWiiupro);
	GuiButton wiiuproBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	wiiuproBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	wiiuproBtn.setPosition(200, 250);
	wiiuproBtn.setLabel(&wiiuproBtnTxt);
	wiiuproBtn.setImage(&wiiuproBtnImg);
	wiiuproBtn.setImageOver(&wiiuproBtnImgOver);
	wiiuproBtn.setIcon(&wiiuproBtnIcon);
	wiiuproBtn.setSoundOver(&btnSoundOver);
	wiiuproBtn.setSoundClick(&btnSoundClick);
	wiiuproBtn.setTrigger(trigA);
	wiiuproBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&subtitleTxt);

	w.append(&gamecubeBtn);
#ifdef HW_RVL
	w.append(&wiimoteBtn);

	if(mapMenuCtrlSNES == CTRL_PAD)
	{
		if(WiiDRC_Inited() && WiiDRC_Connected()) {
			gamecubeBtn.setPosition(-200, 120);
			wiimoteBtn.setPosition(0, 120);
			w.append(&drcBtn);
		}
	
		w.append(&classicBtn);
		w.append(&nunchukBtn);
		w.append(&wiiuproBtn);
	}
#endif
	w.append(&backBtn);

	menu->mainWindow.appendWithAutoRemove(&w);


	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(wiimoteBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_WIIMOTE;
		}
		else if(nunchukBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_NUNCHUK;
		}
		else if(classicBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_CLASSIC;
		}
		else if(wiiuproBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_WUPC;
		}
		else if(drcBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_DRC;
		}
		else if(gamecubeBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_GAMECUBE;
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS;
		}
	}
	return selection;
}

/****************************************************************************
 * ButtonMappingWindow
 ***************************************************************************/
static uint32_t ButtonMappingWindow()
{
	GuiWindow promptWindow(448,288);
	promptWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	promptWindow.setPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt("Button Mapping", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);

	char msg[200];

	switch(mapMenuCtrl)
	{
		case GUI_HW_GAMECUBE:
			sprintf(msg, "Press any button on the GameCube Controller now. Press Home or the C-Stick in any direction to clear the existing mapping.");
			break;
		case GUI_HW_WIIMOTE:
			sprintf(msg, "Press any button on the Wiimote now. Press Home to clear the existing mapping.");
			break;
		case GUI_HW_CLASSIC:
			sprintf(msg, "Press any button on the Classic Controller now. Press Home to clear the existing mapping.");
			break;
		case GUI_HW_WUPC:
			sprintf(msg, "Press any button on the Wii U Pro Controller now. Press Home to clear the existing mapping.");
			break;
		case GUI_HW_DRC:
			sprintf(msg, "Press any button on the Wii U GamePad now. Press Home to clear the existing mapping.");
			break;
		case GUI_HW_NUNCHUK:
			sprintf(msg, "Press any button on the Wiimote or Nunchuk now. Press Home to clear the existing mapping.");
			break;
		default:
			sprintf(msg, "Press any button to map. Press Home to clear.");
			break;
	}

	GuiText msgTxt(msg, 26, (PixelColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	msgTxt.setPosition(0,-20);
	msgTxt.setWrap(true, 430);

	promptWindow.append(&dialogBoxImg);
	promptWindow.append(&titleTxt);
	promptWindow.append(&msgTxt);

	menu->mainWindow.setState(STATE::DISABLED);
	menu->mainWindow.appendWithAutoRemove(&promptWindow);
	menu->mainWindow.changeFocus(&promptWindow);

	uint32_t pressed = 0;
	buttonMappingCancelled = false;

	while(pressed == 0 && !buttonMappingCancelled)
	{
		if(!UpdateGui()) return 0;

		if(!userInput[0]) continue;

		const GuiInputPadData& pad = userInput[0]->getPadData();

		// Listen strictly to the specific hardware profile being mapped
		pressed = pad.hw_buttons_d[mapMenuCtrl];

		// C-Stick clear for GameCube specifically
		if(mapMenuCtrl == GUI_HW_GAMECUBE)
		{
			if(pad.hw_substickX[GUI_HW_GAMECUBE] < -0.55f || pad.hw_substickX[GUI_HW_GAMECUBE] > 0.55f ||
			   pad.hw_substickY[GUI_HW_GAMECUBE] < -0.55f || pad.hw_substickY[GUI_HW_GAMECUBE] > 0.55f)
			{
				pressed = GUI_BTN_HOME;
			}
		}

		// Normalize Home button press to clear mapping
		if (pressed & GUI_BTN_HOME) {
			pressed = GUI_BTN_HOME;
		}

		// If no button was pressed on the target hardware, check if the user
		// hit the generic "Cancel" button (B/1) on ANY controller to back out.
		if(pressed == 0)
		{
			if(userInput[0]->isSecondaryPressed()) {
				buttonMappingCancelled = true;
			}
		}
	}

	// GUI_BTN_HOME explicitly clears the mapped button
	if(pressed == GUI_BTN_HOME) {
		pressed = 0;
	}

	menu->mainWindow.setState(STATE::DEFAULT);

	return pressed;
}

static int MenuSettingsMappingsMap()
{
	int selection = MENU_NONE;
	int ret,i,j;
	bool firstRun = true;
	OptionList options;

	char menuTitle[100];
	char menuSubtitle[100];
	sprintf(menuTitle, "Game Settings - Button Mappings");

	GuiText titleTxt(menuTitle, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	sprintf(menuSubtitle, "%s - %s", textTranslator->getText(ctrlName[mapMenuCtrlSNES]), textTranslator->getText(ctrlrName[mapMenuCtrl]));
	GuiText subtitleTxt(menuSubtitle, 20, (PixelColor){255, 255, 255, 255});
	subtitleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	subtitleTxt.setPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnShortOutline(button_short_png);
	GuiImageData btnShortOutlineOver(button_short_over_png);

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setEffectGrow();

	GuiText resetBtnTxt("Reset Mappings", 22, (PixelColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnShortOutline);
	GuiImage resetBtnImgOver(&btnShortOutlineOver);
	GuiButton resetBtn(btnShortOutline.getWidth(), btnShortOutline.getHeight());
	resetBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	resetBtn.setPosition(260, -35);
	resetBtn.setLabel(&resetBtnTxt);
	resetBtn.setImage(&resetBtnImg);
	resetBtn.setImageOver(&resetBtnImgOver);
	resetBtn.setSoundOver(&btnSoundOver);
	resetBtn.setSoundClick(&btnSoundClick);
	resetBtn.setTrigger(trigA);
	resetBtn.setEffectGrow();

	i=0;

	switch(mapMenuCtrlSNES)
	{
		case CTRL_PAD:
			sprintf(options.name[i++], "A");
			sprintf(options.name[i++], "B");
			sprintf(options.name[i++], "X");
			sprintf(options.name[i++], "Y");
			sprintf(options.name[i++], "L");
			sprintf(options.name[i++], "R");
			sprintf(options.name[i++], "Start");
			sprintf(options.name[i++], "Select");
			sprintf(options.name[i++], "Up");
			sprintf(options.name[i++], "Down");
			sprintf(options.name[i++], "Left");
			sprintf(options.name[i++], "Right");
			options.length = i;
			break;
		case CTRL_SCOPE:
			sprintf(options.name[i++], "Fire");
			sprintf(options.name[i++], "Aim Offscreen");
			sprintf(options.name[i++], "Cursor");
			sprintf(options.name[i++], "Turbo On");
			sprintf(options.name[i++], "Turbo Off");
			sprintf(options.name[i++], "Pause");
			options.length = i;
			break;
		case CTRL_MOUSE:
			sprintf(options.name[i++], "Left Button");
			sprintf(options.name[i++], "Right Button");
			options.length = i;
			break;
		case CTRL_JUST:
			sprintf(options.name[i++], "Fire");
			sprintf(options.name[i++], "Aim Offscreen");
			sprintf(options.name[i++], "Start");
			options.length = i;
			break;
	};

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	optionBrowser.setCol2Position(215);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	w.append(&resetBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	menu->mainWindow.appendWithAutoRemove(&subtitleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS_CTRL;
		}
		else if(resetBtn.getState() == STATE::CLICKED)
		{
			resetBtn.resetState();

			int choice = WindowPrompt(
				"Reset Mappings",
				"Are you sure that you want to reset your mappings?",
				"Yes",
				"No");

			if(choice == 1)
			{
				ResetControls(mapMenuCtrlSNES, mapMenuCtrl);
				firstRun = true;
			}
		}

		ret = optionBrowser.getClickedOption();

		if(ret >= 0)
		{
			int buttonPressed = ButtonMappingWindow();
			
			if (!buttonMappingCancelled)
			{
				// get a button selection from user if the remap wasn't cancelled
				btnmap[mapMenuCtrlSNES][mapMenuCtrl][ret] = buttonPressed;
			}
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			for(i=0; i < options.length; i++)
			{
				for(j=0; j < ctrlr_def[mapMenuCtrl].num_btns; j++)
				{
					if(btnmap[mapMenuCtrlSNES][mapMenuCtrl][i] == 0)
					{
						options.value[i][0] = 0;
					}
					else if(btnmap[mapMenuCtrlSNES][mapMenuCtrl][i] ==
						ctrlr_def[mapMenuCtrl].map[j].btn)
					{
						if(strcmp(options.value[i], ctrlr_def[mapMenuCtrl].map[j].name) != 0)
							sprintf(options.value[i], ctrlr_def[mapMenuCtrl].map[j].name);
						break;
					}
				}
			}
			optionBrowser.triggerUpdate();
		}
	}

	return selection;
}

/****************************************************************************
 * MenuSettingsVideo
 ***************************************************************************/

static void ScreenZoomWindowUpdate(void * ptr, float h, float v)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->getState() == STATE::CLICKED)
	{
		GCSettings.videoZoomHor += h;
		GCSettings.videoZoomVert += v;

		char zoom[10];
		sprintf(zoom, "%.2f%%", GCSettings.videoZoomHor*100);
		settingText->setText(zoom);
		sprintf(zoom, "%.2f%%", GCSettings.videoZoomVert*100);
		settingText2->setText(zoom);
		b->resetState();
	}
}

static void ScreenZoomWindowLeftClick(void * ptr) { ScreenZoomWindowUpdate(ptr, -0.01, 0); }
static void ScreenZoomWindowRightClick(void * ptr) { ScreenZoomWindowUpdate(ptr, +0.01, 0); }
static void ScreenZoomWindowUpClick(void * ptr) { ScreenZoomWindowUpdate(ptr, 0, +0.01); }
static void ScreenZoomWindowDownClick(void * ptr) { ScreenZoomWindowUpdate(ptr, 0, -0.01); }

static void ScreenZoomWindow()
{
	GuiWindow * w = new GuiWindow(200,200);
	w->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiTrigger trigLeft;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_LEFT);

	GuiTrigger trigRight;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_RIGHT);

	GuiTrigger trigUp;
	trigUp.setButtonOnlyInFocusTrigger(-1, GUI_BTN_UP);

	GuiTrigger trigDown;
	trigDown.setButtonOnlyInFocusTrigger(-1, GUI_BTN_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.getWidth(), arrowLeft.getHeight());
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setImageOver(&arrowLeftOverImg);
	arrowLeftBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowLeftBtn.setPosition(50, 0);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(ScreenZoomWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.getWidth(), arrowRight.getHeight());
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setImageOver(&arrowRightOverImg);
	arrowRightBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowRightBtn.setPosition(164, 0);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(ScreenZoomWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.getWidth(), arrowUp.getHeight());
	arrowUpBtn.setImage(&arrowUpImg);
	arrowUpBtn.setImageOver(&arrowUpOverImg);
	arrowUpBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpBtn.setPosition(-76, -27);
	arrowUpBtn.setTrigger(trigA);
	arrowUpBtn.setTrigger(&trigUp);
	arrowUpBtn.setSelectable(false);
	arrowUpBtn.setUpdateCallback(ScreenZoomWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.getWidth(), arrowDown.getHeight());
	arrowDownBtn.setImage(&arrowDownImg);
	arrowDownBtn.setImageOver(&arrowDownOverImg);
	arrowDownBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownBtn.setPosition(-76, 27);
	arrowDownBtn.setTrigger(trigA);
	arrowDownBtn.setTrigger(&trigDown);
	arrowDownBtn.setSelectable(false);
	arrowDownBtn.setUpdateCallback(ScreenZoomWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	screenPositionImg.setPosition(0, 0);

	settingText = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	settingText2 = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	char zoom[10];
	sprintf(zoom, "%.2f%%", GCSettings.videoZoomHor*100);
	settingText->setText(zoom);
	settingText->setPosition(108, 0);
	sprintf(zoom, "%.2f%%", GCSettings.videoZoomVert*100);
	settingText2->setText(zoom);
	settingText2->setPosition(-76, 0);

	float currentZoomHor = GCSettings.videoZoomHor;
	float currentZoomVert = GCSettings.videoZoomVert;

	w->append(&arrowLeftBtn);
	w->append(&arrowRightBtn);
	w->append(&arrowUpBtn);
	w->append(&arrowDownBtn);
	w->append(&screenPositionImg);
	w->append(settingText);
	w->append(settingText2);

	if(!SettingWindow("Screen Zoom",w))
	{
		// undo changes
		GCSettings.videoZoomHor = currentZoomHor;
		GCSettings.videoZoomVert = currentZoomVert;
	}

	delete(w);
	delete(settingText);
	delete(settingText2);
}

static void ScreenPositionWindowUpdate(void * ptr, int x, int y)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->getState() == STATE::CLICKED)
	{
		GCSettings.videoXshift += x;
		GCSettings.videoYshift += y;

		if(!(GCSettings.videoXshift > -50 && GCSettings.videoXshift < 50))
			GCSettings.videoXshift = 0;
		if(!(GCSettings.videoYshift > -50 && GCSettings.videoYshift < 50))
			GCSettings.videoYshift = 0;

		char shift[10];
		sprintf(shift, "%hd, %hd", GCSettings.videoXshift, GCSettings.videoYshift);
		settingText->setText(shift);
		b->resetState();
	}
}

static void ScreenPositionWindowLeftClick(void * ptr) { ScreenPositionWindowUpdate(ptr, -1, 0); }
static void ScreenPositionWindowRightClick(void * ptr) { ScreenPositionWindowUpdate(ptr, +1, 0); }
static void ScreenPositionWindowUpClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, -1); }
static void ScreenPositionWindowDownClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, +1); }

static void ScreenPositionWindow()
{
	GuiWindow * w = new GuiWindow(150,150);
	w->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	w->setPosition(0, -10);

	GuiTrigger trigLeft;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_LEFT);

	GuiTrigger trigRight;
	trigLeft.setButtonOnlyInFocusTrigger(-1, GUI_BTN_RIGHT);

	GuiTrigger trigUp;
	trigUp.setButtonOnlyInFocusTrigger(-1, GUI_BTN_UP);

	GuiTrigger trigDown;
	trigDown.setButtonOnlyInFocusTrigger(-1, GUI_BTN_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.getWidth(), arrowLeft.getHeight());
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setImageOver(&arrowLeftOverImg);
	arrowLeftBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(ScreenPositionWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.getWidth(), arrowRight.getHeight());
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setImageOver(&arrowRightOverImg);
	arrowRightBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(ScreenPositionWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.getWidth(), arrowUp.getHeight());
	arrowUpBtn.setImage(&arrowUpImg);
	arrowUpBtn.setImageOver(&arrowUpOverImg);
	arrowUpBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	arrowUpBtn.setTrigger(trigA);
	arrowUpBtn.setTrigger(&trigUp);
	arrowUpBtn.setSelectable(false);
	arrowUpBtn.setUpdateCallback(ScreenPositionWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.getWidth(), arrowDown.getHeight());
	arrowDownBtn.setImage(&arrowDownImg);
	arrowDownBtn.setImageOver(&arrowDownOverImg);
	arrowDownBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
	arrowDownBtn.setTrigger(trigA);
	arrowDownBtn.setTrigger(&trigDown);
	arrowDownBtn.setSelectable(false);
	arrowDownBtn.setUpdateCallback(ScreenPositionWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	settingText = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	char shift[10];
	sprintf(shift, "%i, %i", GCSettings.videoXshift, GCSettings.videoYshift);
	settingText->setText(shift);

	int currentX = GCSettings.videoXshift;
	int currentY = GCSettings.videoYshift;

	w->append(&arrowLeftBtn);
	w->append(&arrowRightBtn);
	w->append(&arrowUpBtn);
	w->append(&arrowDownBtn);
	w->append(&screenPositionImg);
	w->append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		// undo changes
		GCSettings.videoXshift = currentX;
		GCSettings.videoYshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static int MenuSettingsOtherMappings()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Turbo Mode");
	sprintf(options.name[i++], "Turbo Mode Button");
	sprintf(options.name[i++], "Menu Toggle");
	sprintf(options.name[i++], "Map ABXY to Right Stick");

	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Game Settings - Button Mappings", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	GuiText subtitleTxt("Other Mappings", 20, (PixelColor){255, 255, 255, 255});
	subtitleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	subtitleTxt.setPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setCol2Position(200);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	w.append(&subtitleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.TurboModeEnabled = !GCSettings.TurboModeEnabled;
				break;

			case 1:
				GCSettings.TurboModeButton++;
				if (GCSettings.TurboModeButton > 14)
					GCSettings.TurboModeButton = 0;
				break;

			case 2:
				GCSettings.GamepadMenuToggle++;
				if (GCSettings.GamepadMenuToggle >= GAMEPAD_MENU_TOGGLE_LENGTH)
					GCSettings.GamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
				break;

			case 3:
				GCSettings.MapABXYRightStick = !GCSettings.MapABXYRightStick;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			sprintf (options.value[0], "%s", GCSettings.TurboModeEnabled ? "On" : "Off");
			
			switch(GCSettings.TurboModeButton)
			{
				case 0:
					sprintf (options.value[1], "Default (Right Stick)"); break;
				case 1:
					sprintf (options.value[1], "A"); break;
				case 2:
					sprintf (options.value[1], "B"); break;
				case 3:
					sprintf (options.value[1], "X"); break;
				case 4:
					sprintf (options.value[1], "Y"); break;
				case 5:
					sprintf (options.value[1], "L"); break;
				case 6:
					sprintf (options.value[1], "R"); break;
				case 7:
					sprintf (options.value[1], "ZL"); break;
				case 8:
					sprintf (options.value[1], "ZR"); break;
				case 9:
					sprintf (options.value[1], "Z"); break;
				case 10:
					sprintf (options.value[1], "C"); break;
				case 11:
					sprintf (options.value[1], "1"); break;
				case 12:
					sprintf (options.value[1], "2"); break;
				case 13:
					sprintf (options.value[1], "Plus"); break;
				case 14:
					sprintf (options.value[1], "Minus"); break;
			}

			switch(GCSettings.GamepadMenuToggle)
			{
				case GAMEPAD_MENU_TOGGLE_DEFAULT:
					sprintf (options.value[2], "Default (All Enabled)"); break;
				case GAMEPAD_MENU_TOGGLE_HOME_RIGHTSTICK:
					sprintf (options.value[2], "Home / Right Stick"); break;
				case GAMEPAD_MENU_TOGGLE_LRSTART_12PLUS:
					sprintf (options.value[2], "L+R+Start / 1+2+Plus"); break;
			}

			sprintf (options.value[3], "%s", GCSettings.MapABXYRightStick ? "On" : "Off");

			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS;
		}
	}
	return selection;
}

static int MenuSettingsVideo()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Output Mode");
	sprintf(options.name[i++], "Aspect Ratio Correction");
	sprintf(options.name[i++], "Bilinear Filtering");
	sprintf(options.name[i++], "Hardware Softening");
	sprintf(options.name[i++], "Upscaling");
	sprintf(options.name[i++], "Scanline Overlay");
	sprintf(options.name[i++], "Screen Zoom");
	sprintf(options.name[i++], "Screen Position");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Game Settings - Video", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setCol2Position(200);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.videoMode++;
				if(GCSettings.videoMode >= VIDEOMODE_LENGTH)
					GCSettings.videoMode = VIDEOMODE_AUTO;
				break;

			case 1:
				GCSettings.videoAspectRatioCorrection++;
				if(GCSettings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_LENGTH)
					GCSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
				break;

			case 2:
				GCSettings.videoBilinearFilter = !GCSettings.videoBilinearFilter;
				break;

			case 3:
				GCSettings.videoHardwareSoften++;
				if(GCSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_LENGTH)
					GCSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_OFF;
				break;

			case 4:
				GCSettings.videoUpscalingFilter++;
				if (GCSettings.videoUpscalingFilter >= NUM_FILTERS)
					GCSettings.videoUpscalingFilter = FILTER_NONE;
				break;

			case 5:
				GCSettings.videoScanlines = !GCSettings.videoScanlines;
				break;

			case 6:
				ScreenZoomWindow();
				break;

			case 7:
				ScreenPositionWindow();
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			switch(GCSettings.videoMode)
			{
				case VIDEOMODE_AUTO:
					sprintf (options.value[0], "Automatic (Recommended)"); break;
				case VIDEOMODE_NTSC:
					sprintf (options.value[0], "NTSC (480i)"); break;
				case VIDEOMODE_PROGRESSIVE:
					sprintf (options.value[0], "Progressive (480p)"); break;
				case VIDEOMODE_PAL:
					sprintf (options.value[0], "PAL (50Hz)"); break;
				case VIDEOMODE_PAL60:
					sprintf (options.value[0], "PAL (60Hz)"); break;
				case VIDEOMODE_PROGRESSIVE_576P:
					sprintf (options.value[0], "Progressive (576p)"); break;
				case VIDEOMODE_ORIGINAL_240P:
					sprintf (options.value[0], "Original (240p)"); break;
			}

			switch(GCSettings.videoAspectRatioCorrection)
			{
				case VIDEO_ASPECT_RATIO_CORRECTION_NONE:
					sprintf (options.value[1], "None"); break;
				case VIDEO_ASPECT_RATIO_CORRECTION_16_9:
					sprintf (options.value[1], "16:9"); break;
				case VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED:
					sprintf (options.value[1], "16:9 (Fixed Pixel Ratio)"); break;
			}

			sprintf (options.value[2], "%s", GCSettings.videoBilinearFilter ? "On" : "Off");

			switch(GCSettings.videoHardwareSoften)
			{
				case VIDEO_HW_SOFTEN_OFF:
					sprintf (options.value[3], "Off"); break;
				case VIDEO_HW_SOFTEN_AUTO:
					sprintf (options.value[3], "Auto"); break;
				case VIDEO_HW_SOFTEN_SHARP:
					sprintf (options.value[3], "Sharp"); break;
				case VIDEO_HW_SOFTEN_SOFT:
					sprintf (options.value[3], "Soft"); break;
			}

			sprintf (options.value[4], "%s", GetFilterName(GCSettings.videoUpscalingFilter));
			sprintf (options.value[5], "%s", GCSettings.videoScanlines ? "On" : "Off");
			sprintf (options.value[6], "%.2f%%, %.2f%%", GCSettings.videoZoomHor*100, GCSettings.videoZoomVert*100);
			sprintf (options.value[7], "%d, %d", GCSettings.videoXshift, GCSettings.videoYshift);

			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS;
		}
	}
	return selection;
}

/****************************************************************************
 * MenuSettingsEmulation
 ***************************************************************************/
static int MenuSettingsEmulation()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "SNES Hi-Res Mode");
	sprintf(options.name[i++], "Sprites Per-Line Limit");
	sprintf(options.name[i++], "SuperFX Overclock");
	sprintf(options.name[i++], "Audio Interpolation");
	sprintf(options.name[i++], "Mute Game Audio");
	sprintf(options.name[i++], "Frame Skipping");
	sprintf(options.name[i++], "Crosshair");
	sprintf(options.name[i++], "Show Framerate");
	sprintf(options.name[i++], "Show Local Time");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Game Settings - Emulation", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	
	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setCol2Position(200);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	
	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;
		ret = optionBrowser.getClickedOption();
		
		switch (ret)
		{
			case 0:
				GCSettings.HiResolution = !GCSettings.HiResolution;
				break;

			case 1:
				GCSettings.SpriteLimit = !GCSettings.SpriteLimit;
				break;

			case 2:
				#ifdef HW_RVL
				GCSettings.sfxOverclock++;
				if (GCSettings.sfxOverclock >= SFXOVERCLOCK_LENGTH) {
					GCSettings.sfxOverclock = SFXOVERCLOCK_OFF;
				}
				#else
				GCSettings.sfxOverclock++;
				if (GCSettings.sfxOverclock > SFXOVERCLOCK_60MHZ) {
					GCSettings.sfxOverclock = SFXOVERCLOCK_OFF;
				}
				#endif
				switch(GCSettings.sfxOverclock)
				{
					case SFXOVERCLOCK_OFF: Settings.SuperFXSpeedPerLine = 5823405; break;
					case SFXOVERCLOCK_20MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 20.5e6; break;
					case SFXOVERCLOCK_40MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 40.5e6; break;
					case SFXOVERCLOCK_60MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 60.5e6; break;
					case SFXOVERCLOCK_80MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 80.5e6; break;
					case SFXOVERCLOCK_100MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 100.5e6; break;
					case SFXOVERCLOCK_120MHZ: Settings.SuperFXSpeedPerLine = 0.417 * 120.5e6; break;
				}
				S9xResetSuperFX();
				S9xReset();
				break;

			case 3:
				GCSettings.Interpolation++;
				if (GCSettings.Interpolation > 4) {
					GCSettings.Interpolation = 0;
				}
				switch(GCSettings.Interpolation)
				{
					case 0: Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN; break;
					case 1: Settings.InterpolationMethod = DSP_INTERPOLATION_LINEAR; break;
					case 2: Settings.InterpolationMethod = DSP_INTERPOLATION_CUBIC; break;
					case 3: Settings.InterpolationMethod = DSP_INTERPOLATION_SINC; break;
					case 4: Settings.InterpolationMethod = DSP_INTERPOLATION_NONE; break;
				}
				S9xReset();
				break;

			case 4:
				GCSettings.MuteAudio = !GCSettings.MuteAudio;
				break;

			case 5:
				GCSettings.FrameSkip = !GCSettings.FrameSkip;
				break;

			case 6:
				GCSettings.crosshair = !GCSettings.crosshair;
				break;

			case 7:
				Settings.DisplayFrameRate = !Settings.DisplayFrameRate;
				break;

			case 8:
				Settings.DisplayTime = !Settings.DisplayTime;
				break;
		}
		
	if(ret >= 0 || firstRun)
		{
			firstRun = false;

			sprintf (options.value[0], "%s", GCSettings.HiResolution ? "On" : "Off");
			sprintf (options.value[1], "%s", GCSettings.SpriteLimit ? "On" : "Off");
			
			switch(GCSettings.sfxOverclock)
			{
				case SFXOVERCLOCK_OFF:
					sprintf (options.value[2], "Default"); break;
				case SFXOVERCLOCK_20MHZ:
					sprintf (options.value[2], "20 MHz"); break;
				case SFXOVERCLOCK_40MHZ:
					sprintf (options.value[2], "40 MHz"); break;
				case SFXOVERCLOCK_60MHZ:
					sprintf (options.value[2], "60 MHz"); break;
				case SFXOVERCLOCK_80MHZ:
					sprintf (options.value[2], "80 MHz"); break;
				case SFXOVERCLOCK_100MHZ:
					sprintf (options.value[2], "100 MHz"); break;
				case SFXOVERCLOCK_120MHZ:
					sprintf (options.value[2], "120 MHz"); break;
			}

			switch(GCSettings.Interpolation)
			{
				case 0:
					sprintf (options.value[3], "Gaussian (Accurate)"); break;
				case 1:
					sprintf (options.value[3], "Linear"); break;
				case 2:
					sprintf (options.value[3], "Cubic"); break;
				case 3:
					sprintf (options.value[3], "Sinc"); break;
				case 4:
					sprintf (options.value[3], "None"); break;
			}

			sprintf (options.value[4], "%s", GCSettings.MuteAudio ? "On" : "Off");
			sprintf (options.value[5], "%s", GCSettings.FrameSkip ? "On" : "Off");
			sprintf (options.value[6], "%s", GCSettings.crosshair ? "On" : "Off");
			sprintf (options.value[7], "%s", Settings.DisplayFrameRate ? "On" : "Off");
			sprintf (options.value[8], "%s", Settings.DisplayTime ? "On" : "Off");

			optionBrowser.triggerUpdate();
		}
		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS;
		}
	}
	return selection;
}

/****************************************************************************
 * MenuSettings
 ***************************************************************************/
static int MenuSettings()
{
	int selection = MENU_NONE;

	GuiText titleTxt("Settings", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconFile(icon_settings_file_png);
	GuiImageData iconMenu(icon_settings_menu_png);
	GuiImageData iconNetwork(icon_settings_network_png);
	GuiImageData iconCredits(icon_credits_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText savingBtnTxt1("Saving", 22, (PixelColor){0, 0, 0, 255});
	GuiText savingBtnTxt2("&", 18, (PixelColor){0, 0, 0, 255});
	GuiText savingBtnTxt3("Loading", 22, (PixelColor){0, 0, 0, 255});
	savingBtnTxt1.setPosition(0, -20);
	savingBtnTxt3.setPosition(0, +20);
	GuiImage savingBtnImg(&btnLargeOutline);
	GuiImage savingBtnImgOver(&btnLargeOutlineOver);
	GuiImage fileBtnIcon(&iconFile);
	GuiButton savingBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	savingBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	savingBtn.setPosition(-125, 120);
	savingBtn.setLabel(&savingBtnTxt1, 0);
	savingBtn.setLabel(&savingBtnTxt2, 1);
	savingBtn.setLabel(&savingBtnTxt3, 2);
	savingBtn.setImage(&savingBtnImg);
	savingBtn.setImageOver(&savingBtnImgOver);
	savingBtn.setIcon(&fileBtnIcon);
	savingBtn.setSoundOver(&btnSoundOver);
	savingBtn.setSoundClick(&btnSoundClick);
	savingBtn.setTrigger(trigA);
	savingBtn.setEffectGrow();

	GuiText menuBtnTxt("Menu", 22, (PixelColor){0, 0, 0, 255});
	menuBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage menuBtnImg(&btnLargeOutline);
	GuiImage menuBtnImgOver(&btnLargeOutlineOver);
	GuiImage menuBtnIcon(&iconMenu);
	GuiButton menuBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	menuBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	menuBtn.setPosition(125, 120);
	menuBtn.setLabel(&menuBtnTxt);
	menuBtn.setImage(&menuBtnImg);
	menuBtn.setImageOver(&menuBtnImgOver);
	menuBtn.setIcon(&menuBtnIcon);
	menuBtn.setSoundOver(&btnSoundOver);
	menuBtn.setSoundClick(&btnSoundClick);
	menuBtn.setTrigger(trigA);
	menuBtn.setEffectGrow();

	GuiText networkBtnTxt("Network", 22, (PixelColor){0, 0, 0, 255});
	networkBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage networkBtnImg(&btnLargeOutline);
	GuiImage networkBtnImgOver(&btnLargeOutlineOver);
	GuiImage networkBtnIcon(&iconNetwork);
	GuiButton networkBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	networkBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	networkBtn.setPosition(-125, 250);
	networkBtn.setLabel(&networkBtnTxt);
	networkBtn.setImage(&networkBtnImg);
	networkBtn.setImageOver(&networkBtnImgOver);
	networkBtn.setIcon(&networkBtnIcon);
	networkBtn.setSoundOver(&btnSoundOver);
	networkBtn.setSoundClick(&btnSoundClick);
	networkBtn.setTrigger(trigA);
	networkBtn.setEffectGrow();

	GuiText creditsBtnTxt("Credits", 22, (PixelColor){0, 0, 0, 255});
	creditsBtnTxt.setWrap(true, btnLargeOutline.getWidth()-20);
	GuiImage creditsBtnImg(&btnLargeOutline);
	GuiImage creditsBtnImgOver(&btnLargeOutlineOver);
	GuiImage creditsBtnIcon(&iconCredits);
	GuiButton creditsBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	creditsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	creditsBtn.setPosition(125, 250);
	creditsBtn.setLabel(&creditsBtnTxt);
	creditsBtn.setImage(&creditsBtnImg);
	creditsBtn.setImageOver(&creditsBtnImgOver);
	creditsBtn.setIcon(&creditsBtnIcon);
	creditsBtn.setSoundOver(&btnSoundOver);
	creditsBtn.setSoundClick(&btnSoundClick);
	creditsBtn.setTrigger(trigA);
	creditsBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiText resetBtnTxt("Reset Settings", 22, (PixelColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.getWidth(), btnOutline.getHeight());
	resetBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	resetBtn.setPosition(-90, -35);
	resetBtn.setLabel(&resetBtnTxt);
	resetBtn.setImage(&resetBtnImg);
	resetBtn.setImageOver(&resetBtnImgOver);
	resetBtn.setSoundOver(&btnSoundOver);
	resetBtn.setSoundClick(&btnSoundClick);
	resetBtn.setTrigger(trigA);
	resetBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&savingBtn);
	w.append(&menuBtn);
	w.append(&networkBtn);
	w.append(&creditsBtn);
	w.append(&backBtn);
	w.append(&resetBtn);

	menu->mainWindow.appendWithAutoRemove(&w);


	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(savingBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS_FILE;
		}
		else if(menuBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS_MENU;
		}
		else if(networkBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS_NETWORK;
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESELECTION;
		}
		else if(resetBtn.getState() == STATE::CLICKED)
		{
			resetBtn.resetState();

			int choice = WindowPrompt(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");

			if(choice == 1) {
				DefaultSettings();
				ApplySettings();
				autoSaveMethod();
				autoLoadMethod();
			}
		}
		else if(creditsBtn.getState() == STATE::CLICKED)
		{
			creditsBtn.resetState();
			CreditsWindow();
		}
	}

	return selection;
}

/****************************************************************************
 * MenuSettingsFile
 ***************************************************************************/

static int MenuSettingsFile()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	sprintf(options.name[i++], "Load Device");
	sprintf(options.name[i++], "Save Device");
	sprintf(options.name[i++], "Load Folder");
	sprintf(options.name[i++], "Save Folder");
	sprintf(options.name[i++], "Cheats Folder");
	sprintf(options.name[i++], "Screenshots Folder");
	sprintf(options.name[i++], "Covers Folder");
	sprintf(options.name[i++], "Artwork Folder");
	sprintf(options.name[i++], "Auto Load");
	sprintf(options.name[i++], "Auto Save");
	sprintf(options.name[i++], "Append Auto to .SAV Files");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Saving & Loading", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	optionBrowser.setCol2Position(215);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod = getNextLoadDevice(GCSettings.LoadMethod);
				break;

			case 1:
				GCSettings.SaveMethod = getNextSaveDevice(GCSettings.SaveMethod);
				break;

			case 2:
				OnScreenKeyboard(GCSettings.LoadFolder, MAXPATHLEN);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.SaveFolder, MAXPATHLEN);
				break;

			case 4:
				OnScreenKeyboard(GCSettings.CheatFolder, MAXPATHLEN);
				break;
				
			case 5:
				OnScreenKeyboard(GCSettings.ScreenshotsFolder, MAXPATHLEN);
				break;
				
			case 6:
				OnScreenKeyboard(GCSettings.CoverFolder, MAXPATHLEN);
				break;

			case 7:
				OnScreenKeyboard(GCSettings.ArtworkFolder, MAXPATHLEN);
				break;
				
			case 8:
				GCSettings.AutoLoad++;
				if (GCSettings.AutoLoad > AUTOLOAD_STATE)
					GCSettings.AutoLoad = AUTOLOAD_OFF;
				break;

			case 9:
				GCSettings.AutoSave++;
				if (GCSettings.AutoSave > AUTOSAVE_BOTH)
					GCSettings.AutoSave = AUTOSAVE_OFF;
				break;

			case 10:
				GCSettings.AppendAuto = !GCSettings.AppendAuto;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			if (GCSettings.LoadMethod == DEVICE_AUTO) sprintf (options.value[0],"Auto Detect");
			else if (GCSettings.LoadMethod == DEVICE_SD) sprintf (options.value[0],"SD");
			else if (GCSettings.LoadMethod == DEVICE_USB) sprintf (options.value[0],"USB");
			else if (GCSettings.LoadMethod == DEVICE_DVD) sprintf (options.value[0],"DVD");
			else if (GCSettings.LoadMethod == DEVICE_SMB) sprintf (options.value[0],"Network");
			else if (GCSettings.LoadMethod == DEVICE_SD_SLOTA) sprintf (options.value[0],"SD Gecko Slot A");
			else if (GCSettings.LoadMethod == DEVICE_SD_SLOTB) sprintf (options.value[0],"SD Gecko Slot B");
			else if (GCSettings.LoadMethod == DEVICE_SD_PORT2) sprintf (options.value[0],"SD in SP2");
			else if (GCSettings.LoadMethod == DEVICE_SD_GCLOADER) sprintf (options.value[0],"GC Loader");

			if (GCSettings.SaveMethod == DEVICE_AUTO) sprintf (options.value[1],"Auto Detect");
			else if (GCSettings.SaveMethod == DEVICE_SD) sprintf (options.value[1],"SD");
			else if (GCSettings.SaveMethod == DEVICE_USB) sprintf (options.value[1],"USB");
			else if (GCSettings.SaveMethod == DEVICE_SMB) sprintf (options.value[1],"Network");
			else if (GCSettings.SaveMethod == DEVICE_SD_SLOTA) sprintf (options.value[1],"SD Gecko Slot A");
			else if (GCSettings.SaveMethod == DEVICE_SD_SLOTB) sprintf (options.value[1],"SD Gecko Slot B");
			else if (GCSettings.SaveMethod == DEVICE_SD_PORT2) sprintf (options.value[1],"SD in SP2");
			else if (GCSettings.SaveMethod == DEVICE_SD_GCLOADER) sprintf (options.value[1],"GC Loader");

			snprintf (options.value[2], 35, "%s", GCSettings.LoadFolder);
			snprintf (options.value[3], 35, "%s", GCSettings.SaveFolder);
			snprintf (options.value[4], 35, "%s", GCSettings.CheatFolder);
			snprintf (options.value[5], 35, "%s", GCSettings.ScreenshotsFolder);
			snprintf (options.value[6], 35, "%s", GCSettings.CoverFolder);
			snprintf (options.value[7], 35, "%s", GCSettings.ArtworkFolder);

			if (GCSettings.AutoLoad == AUTOLOAD_OFF) sprintf (options.value[8],"Off");
			else if (GCSettings.AutoLoad == AUTOLOAD_SRAM) sprintf (options.value[8],"SRAM");
			else if (GCSettings.AutoLoad == AUTOLOAD_STATE) sprintf (options.value[8],"State");

			if (GCSettings.AutoSave == AUTOSAVE_OFF) sprintf (options.value[9],"Off");
			else if (GCSettings.AutoSave == AUTOSAVE_SRAM) sprintf (options.value[9],"SRAM");
			else if (GCSettings.AutoSave == AUTOSAVE_STATE) sprintf (options.value[9],"State");
			else if (GCSettings.AutoSave == AUTOSAVE_BOTH) sprintf (options.value[9],"Both");

			if (!GCSettings.AppendAuto) sprintf (options.value[10], "Off");
			else sprintf (options.value[10], "On");

			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS;
			autoSaveMethod();
			autoLoadMethod();
		}
	}
	return selection;
}

static bool LoadLanguage()
{
	char line[200];
	char *lastID = nullptr;

	const uint8_t *buffer;
	size_t size;

	switch(GCSettings.language)
	{
		case LANG_JAPANESE: buffer = jp_lang; size = jp_lang_size; break;
		case LANG_ENGLISH: buffer = en_lang; size = en_lang_size; break;
		case LANG_GERMAN: buffer = de_lang; size = de_lang_size; break;
		case LANG_FRENCH: buffer = fr_lang; size = fr_lang_size; break;
		case LANG_SPANISH: buffer = es_lang; size = es_lang_size; break;
		case LANG_ITALIAN: buffer = it_lang; size = it_lang_size; break;
		case LANG_DUTCH: buffer = nl_lang; size = nl_lang_size; break;
		case LANG_SIMP_CHINESE:
		case LANG_TRAD_CHINESE: buffer = zh_lang; size = zh_lang_size; break;
		case LANG_KOREAN: buffer = ko_lang; size = ko_lang_size; break;
		case LANG_PORTUGUESE: buffer = pt_lang; size = pt_lang_size; break;
		case LANG_BRAZILIAN_PORTUGUESE: buffer = pt_br_lang; size = pt_br_lang_size; break;
		case LANG_CATALAN: buffer = ca_lang; size = ca_lang_size; break;
		case LANG_TURKISH: buffer = tr_lang; size = tr_lang_size; break;
		case LANG_SWEDISH: buffer = sv_lang; size = sv_lang_size; break;
		default: return false;
	}

	textTranslator->loadLanguage(buffer, size);
	return true;
}

static void ResetText()
{
	LoadLanguage();

	if(menu)
	{
		menu->mainWindow.resetText();
	}
}

void ChangeLanguage() {
	if(currentLanguage == GCSettings.language) {
		return;
	}

	if(GCSettings.language == LANG_JAPANESE || GCSettings.language == LANG_KOREAN || GCSettings.language == LANG_SIMP_CHINESE) {
#ifdef HW_RVL
		char filepath[MAXPATHLEN];

		switch(GCSettings.language) {
			case LANG_KOREAN:
				sprintf(filepath, "%s/ko.ttf", appPath);
				break;
			case LANG_JAPANESE:
				sprintf(filepath, "%s/jp.ttf", appPath);
				break;
			case LANG_SIMP_CHINESE:
				sprintf(filepath, "%s/zh.ttf", appPath);
				break;
		}

		size_t fileSize = LoadFont(filepath);

		if(fileSize > 0) {
			if(fontSystem) delete fontSystem;
			fontSystem = new GuiTextRenderer(ext_font_ttf, fileSize, platform->getVideo()->getGlyphRenderer());
		}
		else {
			GCSettings.language = currentLanguage;
		}
#else
	GCSettings.language = currentLanguage;
	ErrorPrompt("Unsupported language!");
#endif
	}
#ifdef HW_RVL
	else {
		if(ext_font_ttf != nullptr) {
			if(fontSystem) delete fontSystem;
			extmem_free(ext_font_ttf);
			ext_font_ttf = nullptr;
			fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size, platform->getVideo()->getGlyphRenderer());
		}
	}
#endif
	ResetText();
	currentLanguage = GCSettings.language;
}

/****************************************************************************
 * MenuSettingsMenu
 ***************************************************************************/

static int MenuSettingsMenu()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	currentLanguage = GCSettings.language;

	sprintf(options.name[i++], "Exit Action");
	sprintf(options.name[i++], "Wiimote Orientation");
	sprintf(options.name[i++], "Music Volume");
	sprintf(options.name[i++], "Sound Effects Volume");
	sprintf(options.name[i++], "Rumble");
	sprintf(options.name[i++], "Language");
	sprintf(options.name[i++], "Preview Image");
	sprintf(options.name[i++], "Hide SRAM Saving");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Menu", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	optionBrowser.setCol2Position(275);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.ExitAction++;
				#ifdef HW_RVL
				if(GCSettings.ExitAction >= EXITACTION_WII_LENGTH)
					GCSettings.ExitAction = EXITACTION_WII_AUTO;
				#else
				if(GCSettings.ExitAction >= EXITACTION_GC_LENGTH)
					GCSettings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
				#endif
				break;
			case 1:
				GCSettings.wiimoteOrientation++;
				if(GCSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_LENGTH)
					GCSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
				platform->getInput()->setWiimoteOrientation(GCSettings.wiimoteOrientation);
				break;
			case 2:
				GCSettings.MusicVolume += 10;
				if(GCSettings.MusicVolume > 100)
					GCSettings.MusicVolume = 0;
				GuiSound::setDefaultVolume(SOUND::OGG, GCSettings.MusicVolume);
				break;
			case 3:
				GCSettings.SFXVolume += 10;
				if(GCSettings.SFXVolume > 100)
					GCSettings.SFXVolume = 0;
				GuiSound::setDefaultVolume(SOUND::PCM, GCSettings.SFXVolume);
				break;
			case 4:
				GCSettings.Rumble = !GCSettings.Rumble;
				platform->getInput()->setRumbleEnabled(GCSettings.Rumble);
				break;
			case 5:
				GCSettings.language++;
				
				if(GCSettings.language == LANG_TRAD_CHINESE) // skip (not supported)
					GCSettings.language = LANG_KOREAN;
				else if(GCSettings.language >= LANG_LENGTH)
					GCSettings.language = LANG_JAPANESE;
				break;
			case 6:
				GCSettings.PreviewImage++;
				if(GCSettings.PreviewImage >= PREVIEWIMAGE_LENGTH)
					GCSettings.PreviewImage = PREVIEWIMAGE_SCREENSHOT;
				break;
			case 7:
				GCSettings.HideSRAMSaving = !GCSettings.HideSRAMSaving;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			#ifdef HW_RVL
			if (GCSettings.ExitAction == EXITACTION_WII_RETURN_TO_MENU)
				sprintf (options.value[0], "Return to Wii Menu");
			else if (GCSettings.ExitAction == EXITACTION_WII_POWER_OFF)
				sprintf (options.value[0], "Power Off Wii");
			else if (GCSettings.ExitAction == EXITACTION_WII_RETURN_TO_LOADER)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Auto");
			#else // GameCube
			if (GCSettings.ExitAction == EXITACTION_GC_RETURN_TO_LOADER)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Reboot");

			options.name[1][0] = 0; // Wiimote
			options.name[2][0] = 0; // Music
			options.name[3][0] = 0; // Sound Effects
			#endif

			if (GCSettings.wiimoteOrientation == WIIMOTE_ORIENTATION_VERTICAL)
				sprintf (options.value[1], "Vertical");
			else if (GCSettings.wiimoteOrientation == WIIMOTE_ORIENTATION_HORIZONTAL)
				sprintf (options.value[1], "Horizontal");
			else
				sprintf (options.value[1], "Auto");

			if(GCSettings.MusicVolume > 0)
				sprintf(options.value[2], "%d%%", GCSettings.MusicVolume);
			else
				sprintf(options.value[2], "Mute");

			if(GCSettings.SFXVolume > 0)
				sprintf(options.value[3], "%d%%", GCSettings.SFXVolume);
			else
				sprintf(options.value[3], "Mute");

			if (GCSettings.Rumble)
				sprintf (options.value[4], "Enabled");
			else
				sprintf (options.value[4], "Disabled");
			
			if (GCSettings.HideSRAMSaving)
				sprintf (options.value[7], "On");
			else
				sprintf (options.value[7], "Off");

			switch(GCSettings.language)
			{
				case LANG_JAPANESE:		sprintf(options.value[5], "Japanese"); break;
				case LANG_ENGLISH:		sprintf(options.value[5], "English"); break;
				case LANG_GERMAN:		sprintf(options.value[5], "German"); break;
				case LANG_FRENCH:		sprintf(options.value[5], "French"); break;
				case LANG_SPANISH:		sprintf(options.value[5], "Spanish"); break;
				case LANG_ITALIAN:		sprintf(options.value[5], "Italian"); break;
				case LANG_DUTCH:		sprintf(options.value[5], "Dutch"); break;
				case LANG_SIMP_CHINESE:	sprintf(options.value[5], "Chinese (Simplified)"); break;
				case LANG_TRAD_CHINESE:	sprintf(options.value[5], "Chinese (Traditional)"); break;
				case LANG_KOREAN:		sprintf(options.value[5], "Korean"); break;
				case LANG_PORTUGUESE:	sprintf(options.value[5], "Portuguese"); break;
				case LANG_BRAZILIAN_PORTUGUESE: sprintf(options.value[5], "Brazilian Portuguese"); break;
				case LANG_CATALAN:		sprintf(options.value[5], "Catalan"); break;
				case LANG_TURKISH:		sprintf(options.value[5], "Turkish"); break;
				case LANG_SWEDISH:		sprintf(options.value[5], "Swedish"); break;
			}
			
			switch(GCSettings.PreviewImage)
			{
				case 0:	
					sprintf(options.value[6], "Screenshots");
					break; 
				case 1:	
					sprintf(options.value[6], "Covers");
					break; 
				case 2:	
					sprintf(options.value[6], "Artwork");
					break; 
			}
			
			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS;
		}
	}
	ChangeLanguage();
	return selection;
}

/****************************************************************************
 * MenuSettingsNetwork
 ***************************************************************************/

static int MenuSettingsNetwork()
{
	int selection = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	sprintf(options.name[i++], "SMB Share IP");
	sprintf(options.name[i++], "SMB Share Name");
	sprintf(options.name[i++], "SMB Share Username");
	sprintf(options.name[i++], "SMB Share Password");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Network", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.setPosition(0, 108);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	optionBrowser.setCol2Position(290);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(GCSettings.smbip, 80);
				break;

			case 1:
				OnScreenKeyboard(GCSettings.smbshare, 20);
				break;

			case 2:
				OnScreenKeyboard(GCSettings.smbuser, 20);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.smbpwd, 20);
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			snprintf (options.value[0], 25, "%s", GCSettings.smbip);
			snprintf (options.value[1], 19, "%s", GCSettings.smbshare);
			snprintf (options.value[2], 19, "%s", GCSettings.smbuser);
			snprintf (options.value[3], 19, "%s", GCSettings.smbpwd);
			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS;
		}
	}
	CloseShare();
	return selection;
}

static uint8_t * CreateBlurredGameTexture() {
	if(gameScreenPng.size == 0) {
		return nullptr;
	}

	uint8_t *src = DecodePNGToRGBA8(gameScreenPng.buffer, gameScreenPng.width, gameScreenPng.height);
	if(!src) {
		return nullptr;
	}

	int blurAmount = 4; // blur amount
	PixelColor blurOverlayColor = (PixelColor){50, 50, 50, 160};

	uint8_t * dst = (uint8_t *)memalign(32, platform->getVideo()->getScreenWidth() * platform->getVideo()->getScreenHeight() * 4);
	if(!dst) {
		extmem_free(src);
		return nullptr;
	}

	int scaledWidth = (int)(gameScreenPng.width * gameScreenPng.scaleX);
	int scaledHeight = (int)(gameScreenPng.height * gameScreenPng.scaleY);

	// Failsafe for invalid scale metrics
	if (scaledWidth <= 0 || scaledHeight <= 0) {
		extmem_free(src);
		free(dst);
		return nullptr;
	}

	// Calculate the absolute top-left starting pixel of the scaled image.
	int targetCenterX = (platform->getVideo()->getScreenWidth() / 2) + gameScreenPng.xoffset;
	int targetCenterY = (platform->getVideo()->getScreenHeight() / 2) + gameScreenPng.yoffset;

	int trueOffsetX = targetCenterX - (scaledWidth / 2);
	int trueOffsetY = targetCenterY - (scaledHeight / 2);

	// --- VIEWABLE AREA CLAMPING LOGIC ---
	// Determine where to start drawing on the screen bounds
	int drawX = trueOffsetX < 0 ? 0 : trueOffsetX;
	int drawY = trueOffsetY < 0 ? 0 : trueOffsetY;

	// Determine the max visible boundaries clipped to screen dimensions
	int endX = (trueOffsetX + scaledWidth > platform->getVideo()->getScreenWidth()) ? platform->getVideo()->getScreenWidth() : (trueOffsetX + scaledWidth);
	int endY = (trueOffsetY + scaledHeight > platform->getVideo()->getScreenHeight()) ? platform->getVideo()->getScreenHeight() : (trueOffsetY + scaledHeight);

	// Calculate the dimensions of the viewable (cropped) area
	int cropWidth = endX - drawX;
	int cropHeight = endY - drawY;

	// Failsafe if the image is pushed entirely off-screen
	if (cropWidth <= 0 || cropHeight <= 0) {
		extmem_free(src);
		free(dst);
		return nullptr;
	}

	// Determine the starting offset within the theoretical scaled image
	int cropStartX = trueOffsetX < 0 ? -trueOffsetX : 0;
	int cropStartY = trueOffsetY < 0 ? -trueOffsetY : 0;

	// Allocate scratch space ONLY for the viewable cropped portion
	uint8_t *scaledImg = (uint8_t *)extmem_malloc(cropWidth * cropHeight * 4);
	uint8_t *rowBuf    = (uint8_t *)extmem_malloc(cropWidth * 4);

	if (!scaledImg || !rowBuf) {
		if (scaledImg) extmem_free(scaledImg);
		if (rowBuf) extmem_free(rowBuf);
		extmem_free(src);
		free(dst);
		return nullptr;
	}

	// Scale the raw input PNG directly into our viewable cropped buffer
	for (int dy = 0; dy < cropHeight; ++dy) {
		int scaledImgY = cropStartY + dy;
		int sy = (scaledImgY * gameScreenPng.height) / scaledHeight;
		if (sy < 0) sy = 0;
		if (sy >= gameScreenPng.height) sy = gameScreenPng.height - 1;

		for (int dx = 0; dx < cropWidth; ++dx) {
			int scaledImgX = cropStartX + dx;
			int sx = (scaledImgX * gameScreenPng.width) / scaledWidth;
			if (sx < 0) sx = 0;
			if (sx >= gameScreenPng.width) sx = gameScreenPng.width - 1;

			int srcIdx = (sy * gameScreenPng.width + sx) * 4;
			int dstIdx = (dy * cropWidth + dx) * 4;

			scaledImg[dstIdx + 0] = src[srcIdx + 0];
			scaledImg[dstIdx + 1] = src[srcIdx + 1];
			scaledImg[dstIdx + 2] = src[srcIdx + 2];
			scaledImg[dstIdx + 3] = src[srcIdx + 3];
		}
	}

	int div = 2 * blurAmount + 1;

	// Horizontal Box Blur Pass (in-place using the small rowBuf)
	for (int y = 0; y < cropHeight; ++y) {
		memcpy(rowBuf, &scaledImg[y * cropWidth * 4], cropWidth * 4);

		for (int x = 0; x < cropWidth; ++x) {
			int sumR = 0, sumG = 0, sumB = 0;

			for (int k = -blurAmount; k <= blurAmount; ++k) {
				int nx = x + k;
				if (nx < 0) nx = 0;
				if (nx >= cropWidth) nx = cropWidth - 1;

				int idx = nx * 4;
				sumR += rowBuf[idx + 0];
				sumG += rowBuf[idx + 1];
				sumB += rowBuf[idx + 2];
			}

			int dstIdx = (y * cropWidth + x) * 4;
			scaledImg[dstIdx + 0] = sumR / div;
			scaledImg[dstIdx + 1] = sumG / div;
			scaledImg[dstIdx + 2] = sumB / div;
		}
	}

	// Precalculate flat background color (Solid Black + Overlay)
	int alphaIn = blurOverlayColor.a;
	int invAlpha = 255 - alphaIn;

	uint8_t bgR = (uint8_t)((0 * invAlpha + blurOverlayColor.r * alphaIn) / 255);
	uint8_t bgG = (uint8_t)((0 * invAlpha + blurOverlayColor.g * alphaIn) / 255);
	uint8_t bgB = (uint8_t)((0 * invAlpha + blurOverlayColor.b * alphaIn) / 255);
	uint8_t bgA = 255;

	// Vertical Blur, Overlay, & Swizzle directly to the GX Destination Layout
	int tilesX = (platform->getVideo()->getScreenWidth() + 3) / 4;
	int tilesY = (platform->getVideo()->getScreenHeight() + 3) / 4;

	for (int ty = 0; ty < tilesY; ++ty) {
		for (int tx = 0; tx < tilesX; ++tx) {
			int tileIdx = ty * tilesX + tx;
			uint8_t* destTilePtr = dst + (tileIdx * 64);

			for (int py = 0; py < 4; ++py) {
				for (int px = 0; px < 4; ++px) {
					int currX = tx * 4 + px;
					int currY = ty * 4 + py;
					int pixelIdx = (py * 4) + px;

					if (currX >= platform->getVideo()->getScreenWidth() || currY >= platform->getVideo()->getScreenHeight()) {
						destTilePtr[pixelIdx * 2 + 0] = bgA;
						destTilePtr[pixelIdx * 2 + 1] = bgR;
						destTilePtr[32 + (pixelIdx * 2 + 0)] = bgG;
						destTilePtr[32 + (pixelIdx * 2 + 1)] = bgB;
						continue;
					}

					// Check bounds against our true absolute coordinates
					if (currX >= drawX && currX < drawX + cropWidth &&
						currY >= drawY && currY < drawY + cropHeight) {

						int cx = currX - drawX;
						int cy = currY - drawY;

						int sumR = 0, sumG = 0, sumB = 0;

						for (int k = -blurAmount; k <= blurAmount; ++k) {
							int ny = cy + k;
							if (ny < 0) ny = 0;
							if (ny >= cropHeight) ny = cropHeight - 1;

							int idx = (ny * cropWidth + cx) * 4;
							sumR += scaledImg[idx + 0];
							sumG += scaledImg[idx + 1];
							sumB += scaledImg[idx + 2];
						}

						uint8_t blurredR = sumR / div;
						uint8_t blurredG = sumG / div;
						uint8_t blurredB = sumB / div;

						uint8_t finalR = (uint8_t)((blurredR * invAlpha + blurOverlayColor.r * alphaIn) / 255);
						uint8_t finalG = (uint8_t)((blurredG * invAlpha + blurOverlayColor.g * alphaIn) / 255);
						uint8_t finalB = (uint8_t)((blurredB * invAlpha + blurOverlayColor.b * alphaIn) / 255);

						destTilePtr[pixelIdx * 2 + 0] = 255;
						destTilePtr[pixelIdx * 2 + 1] = finalR;
						destTilePtr[32 + (pixelIdx * 2 + 0)] = finalG;
						destTilePtr[32 + (pixelIdx * 2 + 1)] = finalB;

					} else {
						destTilePtr[pixelIdx * 2 + 0] = bgA;
						destTilePtr[pixelIdx * 2 + 1] = bgR;
						destTilePtr[32 + (pixelIdx * 2 + 0)] = bgG;
						destTilePtr[32 + (pixelIdx * 2 + 1)] = bgB;
					}
				}
			}
		}
	}

	DCFlushRange(dst, platform->getVideo()->getScreenWidth() * platform->getVideo()->getScreenHeight() * 4);

	extmem_free(scaledImg);
	extmem_free(rowBuf);
	extmem_free(src);
	return dst;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
static int FirstRunTask(void * arg) {
	LoadPrefs();
	autoSaveMethod();
	autoLoadMethod();
	CreateMissingDirectories();
	SavePrefs();
	return 0;
}

void MainMenu (int selection)
{
	static bool firstRun = true;
	int currentMenu = selection;
	lastMenu = MENU_NONE;
	
	if(firstRun)
	{
		#ifdef HW_RVL
		pointer[0] = new GuiImageData(player1_point_png);
		pointer[1] = new GuiImageData(player2_point_png);
		pointer[2] = new GuiImageData(player3_point_png);
		pointer[3] = new GuiImageData(player4_point_png);
		for(int i = 0; i < 4; i++)
			cursorImg[i].setImage(pointer[i]);
		#endif

		trigA = new GuiTrigger;
		trigA->setPrimaryTrigger();
	}

	if(selection == MENU_GAME)
	{
		gameScreenTexture = CreateBlurredGameTexture();
		if(gameScreenTexture != nullptr) {
			gameScreenImg = new GuiImage(gameScreenTexture, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
		}
	}

	if(gameScreenImg == nullptr) {
		gameScreenImg = new GuiImage(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight(), (PixelColor){175, 200, 215, 255});
		gameScreenImg->setStripe(10);
	}

	std::unique_ptr<Menu> menuInstance = std::make_unique<Menu>();
	menu = menuInstance.get();

	if(firstRun && RunOnWorkerThread(FirstRunTask)) {
		while(!IsWorkerThreadFinished())
		{
			if(!UpdateGui()) break;
		}
	}

#ifdef HW_RVL
	if(firstRun)
	{
		uint32_t ios = IOS_GetVersion();

		if(!SupportedIOS(ios))
			ErrorPrompt("The current IOS is unsupported. Functionality and/or stability may be adversely affected.");
		else if(!SaneIOS(ios))
			ErrorPrompt("The current IOS has been altered (fake-signed). Functionality and/or stability may be adversely affected.");
	}
#endif

	if(firstRun) {
		firstRun = false;
		#ifdef HW_DOL
		bgMusic = new GuiSound();
		enterSound = new GuiSound();
		exitSound = new GuiSound();
		#else
		bgMusic = new GuiSound(bg_music, bg_music_size, SOUND::OGG);
		bgMusic->setLoop(true);
		enterSound = new GuiSound(enter_ogg, enter_ogg_size, SOUND::OGG);
		exitSound = new GuiSound(exit_ogg, exit_ogg_size, SOUND::OGG);
		#endif
	}

	if(currentMenu == MENU_GAMESELECTION)
		bgMusic->play();

	while(currentMenu != MENU_EXIT || SNESROMSize <= 0)
	{
		switch (currentMenu)
		{
			case MENU_GAMESELECTION:
				currentMenu = MenuGameSelection();
				break;
			case MENU_GAME:
				currentMenu = MenuGame();
				break;
			case MENU_GAME_LOAD:
				currentMenu = MenuGameSaves(0);
				break;
			case MENU_GAME_SAVE:
				currentMenu = MenuGameSaves(1);
				break;
			case MENU_GAME_DELETE:
				currentMenu = MenuGameSaves(2);
				break;	
			case MENU_GAMESETTINGS:
				currentMenu = MenuGameSettings();
				break;
			case MENU_GAMESETTINGS_MAPPINGS:
				currentMenu = MenuSettingsMappings();
				break;
			case MENU_GAMESETTINGS_MAPPINGS_CTRL:
				currentMenu = MenuSettingsMappingsController();
				break;
			case MENU_GAMESETTINGS_MAPPINGS_MAP:
				currentMenu = MenuSettingsMappingsMap();
				break;
			case MENU_GAMESETTINGS_VIDEO:
				currentMenu = MenuSettingsVideo();
				break;
			case MENU_GAMESETTINGS_EMULATION:
				currentMenu = MenuSettingsEmulation();
				break;
			case MENU_GAMESETTINGS_CHEATS:
				currentMenu = MenuGameCheats();
				break;
			case MENU_SETTINGS:
				currentMenu = MenuSettings();
				break;
			case MENU_SETTINGS_FILE:
				currentMenu = MenuSettingsFile();
				break;
			case MENU_SETTINGS_MENU:
				currentMenu = MenuSettingsMenu();
				break;
			case MENU_SETTINGS_NETWORK:
				currentMenu = MenuSettingsNetwork();
				break;
			case MENU_GAMESETTINGS_MAPPINGS_OTHER:
				currentMenu = MenuSettingsOtherMappings();
				break;
			default: // unrecognized menu
				currentMenu = MenuGameSelection();
				break;
		}
		lastMenu = currentMenu;

		if(!UpdateGui())
			break;
	}

	CancelAction();

	if(gameScreenImg != nullptr) {
		menuInstance->mainWindow.remove(gameScreenImg);
		delete gameScreenImg;
		gameScreenImg = nullptr;
	}

	if(gameScreenTexture != nullptr) {
		free(gameScreenTexture);
		gameScreenTexture = nullptr;
	}

	ClearScreenshot();

	// wait for keys to be depressed
	while(isMenuRequested())
	{
		UpdatePads();
		usleep(THREAD_SLEEP);
	}
	
	menu = nullptr;
}
