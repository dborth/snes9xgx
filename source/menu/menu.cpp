/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <new>

#ifdef HW_RVL
#include <ogc/ios.h>
#endif

#include "snes9xgx.h"
#include "memmanager.h"
#include "video.h"
#include "filebrowser.h"
#include "utils/decompress.h"
#include "fileop.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "input.h"
#include "menu.h"
#include "mappingui.h"
#include "menupopover.h"
#include "controllerpicker.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "widgets/GuiWidgets.h"
#include "menuart.h"
#include "controllerstatus.h"
#include "utils/pngcodec.h"
#include "drivers/Time.h"
#include "drivers/Thread.h"
#include "drivers/Mutex.h"
#include "drivers/Cond.h"

#if defined(HW_RVL) || defined(HW_DOL)
#include "drivers/ogc/videofilters.h"
#include "drivers/ogc/wii/WiiPlatform.h"
#include "drivers/ogc/gamecube/GameCubePlatform.h"
#endif

#ifdef __WIIU__
#include "drivers/wut/WutInputDriver.h"
#include "drivers/wut/WutUpscaleFilters.h"
#endif

#include "snes9x/port.h"
#include "snes9x/snes9x.h"
#include "snes9x/fxemu.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"
#include "snes9x/cheats.h"

extern SCheatData Cheat;
extern void ToggleCheat(uint32);

#define THREAD_SLEEP 100

#ifndef HW_DOL
static GuiImageData * pointer[4];
static GuiImage cursorImg[4];
#endif

static GuiTrigger * trigA = nullptr;

#ifndef HW_DOL
static GuiButton * batteryBtn[4];
#endif
static void * gameScreenTexture = nullptr;
static GuiElement * gameScreenImg = nullptr; // blurred game screenshot, or the plain page background
static GuiSound * bgMusic = nullptr;
static GuiSound * enterSound = nullptr;
static GuiSound * exitSound = nullptr;
static GuiText * settingText = nullptr;
static GuiText * settingText2 = nullptr;
static int lastMenu = MENU_NONE;

static int currentLanguage = -1;

uint8_t * bg_music;
uint32_t bg_music_size;

struct Menu;
static Menu * menu = nullptr;

static volatile int showProgress = 0;

// ProgressSync().idleCond signals when the overlay has been fully torn
// down; ProgressSync().workCond is unused - see ThreadSync in drivers/Thread.h.
static ThreadSync & ProgressSync() { static ThreadSync s; return s; }
static bool progIdle = true; // protected by ProgressSync().mutex - true when no overlay is showing/pending

static char progressTitle[101];
static char progressMsg[201];
static int progressDone = 0;
static int progressTotal = 0;
static void (*progressCancelFn)(void) = nullptr; // protected by ProgressSync().mutex - non-null means the overlay offers a Cancel button

static ThreadId mainThreadId;

static bool IsMainThread()
{
	return ThreadId::current() == mainThreadId;
}

// PromptSync().idleCond signals main -> background: result is ready;
// PromptSync().workCond is unused.
static ThreadSync & PromptSync() { static ThreadSync s; return s; }
static bool promptPending      = false; // protected by PromptSync().mutex
static bool promptResultReady  = false; // protected by PromptSync().mutex
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
	mainThreadId = ThreadId::current();

	// Force ProgressSync()/PromptSync()'s function-local statics to
	// construct now rather than on first use from a background thread.
	ProgressSync();
	PromptSync();
}

/****************************************************************************
 * UpdateProgressOverlay
 *
 * Reflects the current progress/action state into the GUI
 ***************************************************************************/
struct ProgressOverlayState {
	GuiWindow progressWindow;
	MenuDialogShape dialogBoxImg;
	MenuProgressBar progressBar;
	MenuSpinner spinner;
	GuiText titleTxt;
	GuiText msgTxt;
	MenuButtonShape cancelImg;
	GuiText cancelTxt;
	GuiButton cancelBtn;
	GuiTrigger cancelTrigB;

	bool cancelShown;
	bool overlayShown;
	bool waitingToShow;
	Ticks pendingStart;
	STATE oldState;
	float angle;
	uint32_t count;

	ProgressOverlayState() :
		progressWindow(448, 288),
		dialogBoxImg(MENU_DIALOG_W, MENU_DIALOG_H, MENU_DIALOG_HEADER_H),
		titleTxt(nullptr, 26, (PixelColor){255, 255, 255, 255}),
		msgTxt(nullptr, 26, (PixelColor){0, 0, 0, 255}),
		cancelImg(MenuButtonShape::NORMAL),
		cancelTxt("Cancel", 22, (PixelColor){0, 0, 0, 255}),
		cancelBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H),
		cancelShown(false),
		overlayShown(false), waitingToShow(false), pendingStart(0),
		oldState(STATE::DEFAULT), angle(0), count(0)
	{
		progressWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		progressWindow.setPosition(0, -10);
		progressWindow.append(&dialogBoxImg);

		titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		titleTxt.setPosition(0, MENU_DIALOG_TITLE_Y);
		progressWindow.append(&titleTxt);

		msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		msgTxt.setPosition(0, 80);
		progressWindow.append(&msgTxt);

		progressBar.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		progressBar.setPosition((MENU_DIALOG_W - MENU_PROGRESS_W) / 2, 40);

		spinner.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		spinner.setPosition(0, 40);

		cancelBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
		cancelBtn.setPosition(0, -25);
		cancelBtn.setLabel(&cancelTxt);
		cancelBtn.setImage(&cancelImg);
		cancelTrigB.setSecondaryTrigger();
	}

	void update();
};

struct Menu {
	GuiWindow mainWindow;

	MenuBarShape bgTopImg;
	MenuBarShape bgBottomImg;
	GuiImageData logo;
	GuiImage logoImg;
	GuiImageData logoOver;
	GuiImage logoImgOver;
	GuiText logoTxt;
	GuiButton btnLogo;

	ProgressOverlayState progressOverlayState;

	Menu() :
		mainWindow(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
		bgTopImg(MenuBarShape::TOP),
		bgBottomImg(MenuBarShape::BOTTOM),
		logo(logo_png), logoImg(&logo),
		logoOver(logo_over_png), logoImgOver(&logoOver),
		logoTxt(APPVERSION, 18, (PixelColor){255, 255, 255, 255}),
		btnLogo(logoImg.getWidth(), logoImg.getHeight())
	{
		logoTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);

		btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
		btnLogo.setPosition(-50, 24);
		btnLogo.setImage(&logoImg);
		btnLogo.setImageOver(&logoImgOver);
		btnLogo.setLabel(&logoTxt);
		btnLogo.setTrigger(trigA);

		mainWindow.append(gameScreenImg);
		mainWindow.append(&bgTopImg);
		mainWindow.append(&bgBottomImg);
		mainWindow.append(&btnLogo);
	}
};

void ProgressOverlayState::update() {
	if(!menu) return;

	ProgressSync().mutex.lock();
	int progress = showProgress;
	int done = progressDone;
	int total = progressTotal;
	void (*cancelFn)(void) = progressCancelFn;
	char title[101]; snprintf(title, sizeof(title), "%s", progressTitle);
	char msg[201]; snprintf(msg, sizeof(msg), "%s", progressMsg);
	ProgressSync().mutex.unlock();

	if(!progress)
	{
		waitingToShow = false;

		if(overlayShown)
		{
			menu->mainWindow.remove(&progressWindow);
			menu->mainWindow.setState(oldState);
			overlayShown = false;
			cancelShown = false;
		}

		ProgressSync().mutex.lock();
		if(!progIdle)
		{
			progIdle = true;
			ProgressSync().idleCond.signal();
		}
		ProgressSync().mutex.unlock();
	}
	else if(!overlayShown)
	{
		if(!waitingToShow)
		{
			waitingToShow = true;
			pendingStart = SystemTime::now();
		}
		else if(SystemTime::diffMillisecs(pendingStart, SystemTime::now()) >= 400)
		{
			titleTxt.setText(title);
			msgTxt.setText(msg);

			progressWindow.remove(&progressBar);
			progressWindow.remove(&spinner);
			progressWindow.remove(&cancelBtn);
			spinner.setPosition(0, 40);

			if(progress == 1)
			{
				progressBar.setProgress(0);
				progressWindow.append(&progressBar);
			}
			else
			{
				progressWindow.append(&spinner);

				if(cancelFn)
				{
					// Move the throbber up so the button fits below
					spinner.setPosition(0, 15);
					cancelBtn.setTrigger(0, trigA);
					cancelBtn.setTrigger(1, &cancelTrigB);
					cancelBtn.setState(STATE::SELECTED);
					progressWindow.append(&cancelBtn);
					cancelShown = true;
				}
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
			progressBar.setProgress((int)(100*done/total));
		}
		else if(progress == 2)
		{
			if(count % 5 == 0)
			{
				angle += 45.0f;
				if(angle >= 360.0f)
					angle = 0;
				spinner.setPhase((int)(angle / 45.0f));
			}
			++count;
		}

		if(cancelShown && cancelBtn.getState() == STATE::CLICKED)
		{
			cancelBtn.resetState();
			progressWindow.remove(&cancelBtn);
			cancelShown = false;
			msgTxt.setText("Cancelling...");

			if(cancelFn)
				cancelFn();
		}
	}
}

static void ProcessInputData() {
	platform->getInput()->update();

	for(int i = 3; i >= 0; i--)
		menu->mainWindow.update(controller[i]);
}

static void DrawGui() {
	menu->mainWindow.draw();

	#ifndef HW_DOL
	for(int i = 3; i >= 0; i--)
	{
		if(controller[i]->getPadData().validPointer) {
			cursorImg[i].setPosition(controller[i]->getPadData().cursor_x - cursorImg[i].getWidth()/2, controller[i]->getPadData().cursor_y - cursorImg[i].getHeight()/2);
			cursorImg[i].setAngle(controller[i]->getPadData().cursor_angle);
			cursorImg[i].draw();
		}
	}
	#endif

	platform->getVideo()->renderMenu();
}

/****************************************************************************
 * CreditsWindow
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/

static bool UpdateGui();

static void CreditsWindow()
{
	int i = 0;
	int y = 14;

	GuiWindow creditsWindowBox(MENU_CREDITS_W, MENU_CREDITS_H);
	creditsWindowBox.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	MenuDialogShape creditsBoxImg(MENU_CREDITS_W, MENU_CREDITS_H, MENU_CREDITS_HEADER_H);
	creditsWindowBox.append(&creditsBoxImg);

	int numEntries = 22;
	GuiText * txt[numEntries];

	// the title sits on the dialog's chrome band, so it is white; the credits start below the band
	txt[i] = new GuiText("Credits", 30, (PixelColor){255, 255, 255, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0, MENU_CREDITS_TITLE_Y); i++; y+=56;

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
	char controllerInfo[128] = { 0 };

	snprintf(consoleDetails, sizeof(consoleDetails), "%s", platform->getConsoleDetails());
	snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "%s", platform->getMemoryFreeInfo());

#ifdef HW_RVL
	snprintf(controllerInfo, sizeof(controllerInfo), "%s", GetUSBControllerInfo());
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
		return controller[0]->getPadData().buttons_d || controller[1]->getPadData().buttons_d ||
			   controller[2]->getPadData().buttons_d || controller[3]->getPadData().buttons_d; };

	bool running = true;

	// debounce - wait for button to be unpressed
	while(running && buttonPressed())
		running = UpdateGui();

	// credits open - wait for button to be pressed
	while(running && !buttonPressed())
		running = UpdateGui();

	menu->mainWindow.remove(&creditsWindowBox);

	for(i=0; i < numEntries; i++)
		delete txt[i];

	// credits closed - wait for button to be unpressed (so we don't just reopen credits)
	while(running && buttonPressed())
		running = UpdateGui();

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
	PromptSync().mutex.lock();
	bool pending = promptPending;
	const char * title = promptPendingTitle;
	const char * msg = promptPendingMsg;
	const char * btn1 = promptPendingBtn1;
	const char * btn2 = promptPendingBtn2;
	// Reentrant call handling - clear promptPending now, before the blocking WindowPrompt() call below
	if(pending)
		promptPending = false;
	PromptSync().mutex.unlock();

	if(!pending)
		return;

	int result = WindowPrompt(title, msg, btn1, btn2);

	PromptSync().mutex.lock();
	promptResult = result;
	promptResultReady = true;
	PromptSync().idleCond.signal();
	PromptSync().mutex.unlock();
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

// Set once the OS has asked the app to quit and the fade-out has played
static volatile bool guiExiting = false;

// A background thread can be parked waiting for the GUI thread to acknowledge
// an overlay (CancelAction) or answer a prompt (WindowPromptRequest)
static void ReleaseBackgroundWaiters()
{
	ProgressSync().mutex.lock();
	ProgressSync().idleCond.signal();
	ProgressSync().mutex.unlock();

	PromptSync().mutex.lock();
	PromptSync().idleCond.signal();
	PromptSync().mutex.unlock();
}

static bool UpdateGui()
{
	if(guiExiting)
		return false;

	ProcessInputData();

	menu->progressOverlayState.update();
	ServicePendingWindowPromptRequest();
	ServicePendingCreditsWindowRequest();

	DrawGui();

	if(platform->shouldExit())
	{
		for(int a = 0; a <= 255; a += 15)
		{
			menu->mainWindow.draw();
			platform->getVideo()->getImageRenderer()->drawRectangle(0,0,platform->getVideo()->getScreenWidth(),platform->getVideo()->getScreenHeight(),(PixelColor){0, 0, 0, (uint8_t)a});
			platform->getVideo()->renderMenu();
		}
		guiExiting = true;
		ReleaseBackgroundWaiters();
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
	if(!menu || platform->getStatus() != Status::Running) // no UI without the foreground
		return 0;

	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	promptWindow.setPosition(0, -10);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	MenuDialogShape dialogBoxImg(MENU_DIALOG_W, MENU_DIALOG_H, MENU_DIALOG_HEADER_H);

	GuiText titleTxt(title, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0, MENU_DIALOG_TITLE_Y);
	GuiText msgTxt(msg, 26, (PixelColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	msgTxt.setPosition(0,-20);
	msgTxt.setWrap(true, 430);

	GuiText btn1Txt(btn1Label, 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape btn1Img(MenuButtonShape::NORMAL);
	GuiButton btn1(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);

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
	btn1.setTrigger(trigA);
	btn1.setState(STATE::SELECTED);
	btn1.setEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape btn2Img(MenuButtonShape::NORMAL);
	GuiButton btn2(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	btn2.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	btn2.setPosition(-20, -25);
	btn2.setLabel(&btn2Txt);
	btn2.setImage(&btn2Img);
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

	ProgressSync().mutex.lock();
	showProgress = 0;
	progressCancelFn = nullptr;
	ProgressSync().mutex.unlock();

	if(IsMainThread())
	{
		menu->progressOverlayState.update(); // synchronously reflect showProgress==0 right now
	}
	else
	{
		ProgressSync().mutex.lock();

		while(!progIdle && !guiExiting) // nobody acknowledges once the GUI thread has stopped
			ProgressSync().idleCond.wait(ProgressSync().mutex);

		ProgressSync().mutex.unlock();
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

	ProgressSync().mutex.lock();
	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 1;
	progressCancelFn = nullptr;
	progressTotal = total;
	progressDone = done;
	progIdle = false;
	ProgressSync().mutex.unlock();
}

/****************************************************************************
 * ShowAction
 *
 * Shows that an action is underway. Safe to call from a background worker
 * thread - see CancelAction().
 ***************************************************************************/
void ShowAction (const char *msg, void (*onCancel)(void))
{
	if(!menu)
		return;

	if(showProgress != 0)
		CancelAction(); // wait for previous progress window to finish

	ProgressSync().mutex.lock();
	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressCancelFn = onCancel;
	progressDone = 0;
	progressTotal = 0;
	progIdle = false;
	ProgressSync().mutex.unlock();
}

static int WindowPromptRequest(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	if(IsMainThread())
		return WindowPrompt(title, msg, btn1Label, btn2Label);

	if(!menu || guiExiting || platform->getStatus() != Status::Running)
		return 0;

	PromptSync().mutex.lock();
	promptPendingTitle = title;
	promptPendingMsg = msg;
	promptPendingBtn1 = btn1Label;
	promptPendingBtn2 = btn2Label;
	promptResultReady = false;
	promptPending = true;
	while(!promptResultReady && !guiExiting) // nobody answers once the GUI thread has stopped
		PromptSync().idleCond.wait(PromptSync().mutex);
	int result = promptResultReady ? promptResult : 0;
	promptPending = false;
	PromptSync().mutex.unlock();
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
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, uint32_t maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiText okBtnTxt("OK", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape okBtnImg(MenuButtonShape::NORMAL);
	GuiButton okBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);

	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(25, -25);

	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setTrigger(trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape cancelBtnImg(MenuButtonShape::NORMAL);
	GuiButton cancelBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-25, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setTrigger(trigA);
	cancelBtn.setEffectGrow();

	MenuKeyboardBox keyboardBox;
	keyboard.insert(&keyboardBox, 0);
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

	MenuDialogShape dialogBoxImg(MENU_DIALOG_W, MENU_DIALOG_H, MENU_DIALOG_HEADER_H);

	GuiText titleTxt(title, 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0, MENU_DIALOG_TITLE_Y);

	GuiText okBtnTxt("OK", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape okBtnImg(MenuButtonShape::NORMAL);
	GuiButton okBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);

	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(20, -25);

	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setTrigger(trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape cancelBtnImg(MenuButtonShape::NORMAL);
	GuiButton cancelBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-20, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
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
	switch(EmuSettings.previewImage)
	{
		case PREVIEWIMAGE_SCREENSHOT : return EmuSettings.screenshotsFolder;
		case PREVIEWIMAGE_COVER : return EmuSettings.coverFolder;
		case PREVIEWIMAGE_ARTWORK : return EmuSettings.artworkFolder;
		default : return EmuSettings.coverFolder;
	}
}

static bool ResolvePreviewImagePath(void *, int index, char * outPath, size_t outPathSize)
{
	if(browser.dir[0] == 0 || EmuSettings.loadDevice <= 0 ||
	   browser.numEntries <= 0 || index <= 0 || index >= browser.numEntries)
		return false;

	char imageBase[MAXJOLIET + 1];
	if(browserList[index].isdir)
		snprintf(imageBase, sizeof(imageBase), "%s", browserList[index].filename);
	else
		StripExt(imageBase, browserList[index].filename);

	char imageFile[MAXJOLIET + 16];
	snprintf(imageFile, sizeof(imageFile), "%s.png", imageBase);
	platform->getFileSystem()->getPath(outPath, outPathSize, EmuSettings.loadDevice, getImageFolder(), imageFile);
	return true;
}

// Fits the preview image inside its card (with a margin) and sizes the shadow behind it to match
static void OnPreviewImageChanged(void * context, GuiImage * target, bool isDefault)
{
	MenuCoverShadow * shadow = (MenuCoverShadow *)context;
	int w = 0, h = 0;

	if(target->getWidth() > 0 && target->getHeight() > 0)
	{
		const float scale = std::min((MENU_PREVIEW_W - 36.0f) / target->getWidth(), (MENU_PREVIEW_H - 30.0f) / target->getHeight());
		target->setScale(scale);
		w = (int)(target->getWidth() * scale + 0.5f);
		h = (int)(target->getHeight() * scale + 0.5f);
	}
	shadow->fit(isDefault ? 0 : w, isDefault ? 0 : h); // the default image has no drop shadow
}

static int BrowserLoadFileTask(void *) { return BrowserLoadFile(); }
static int BrowserChangeFolderTask(void * arg) { return BrowserChangeFolder(); }

struct ChangeInterfaceArgs
{
	int device;
	bool silent;
};

static int ChangeInterfaceTask(void * arg) {
	ChangeInterfaceArgs * a = (ChangeInterfaceArgs *)arg;
	return ChangeInterface(a->device, a->silent) ? 1 : 0;
}

static int OpenGameListTask(void *) { return OpenGameList(); }

static int AutoDetectDevicesTask(void *) { AutoDetectDevices(); return 0; }

/****************************************************************************
 * RunWithGuiUpdates
 * Runs a task on the worker thread while the GUI keeps drawing (and the main
 * window ignores input).
 * If the app is asked to quit while waiting the task may still be running, so
 * anything it uses through arg must outlive the caller's stack frame (static
 * or heap storage).
 * \return false if the app was asked to quit while waiting.
 ***************************************************************************/
static bool RunWithGuiUpdates(BgTaskFn task, int * result = nullptr, void * arg = nullptr)
{
	menu->mainWindow.setState(STATE::DISABLED);

	int r;
	if(RunOnWorkerThread(task, arg))
	{
		while(!IsWorkerThreadFinished())
		{
			if(!UpdateGui()) return false;
		}
		r = GetWorkerThreadResult();
	}
	else
	{
		r = task(arg);
	}

	menu->mainWindow.setState(STATE::DEFAULT);

	if(result)
		*result = r;

	return true;
}

/****************************************************************************
 * WaitForQueuedTasks
 * Lets queued background work (eg. the auto-save of the game that was just
 * left) finish before something that reads the files it writes. The GUI keeps
 * drawing meanwhile, and it returns straight away when nothing is queued.
 * \return false if the app was asked to quit while waiting.
 ***************************************************************************/
static bool WaitForQueuedTasks()
{
	while(!BackgroundTasksIdle())
	{
		if(!UpdateGui()) return false;
	}
	return true;
}

/****************************************************************************
 * AutoSave
 *
 * Automatically saves SRAM/state when returning from in-game to the menu.
 *
 * Everything that has to happen while the game is still loaded - asking the
 * user (when set to prompt) and capturing the SRAM/state - is done right here
 * on the main thread. Writing it to the storage device is then queued as a
 * silent background task, so the menu never waits on the device and nothing
 * on screen is disabled or covered while it happens.
 * \return false if the app was asked to quit
 ***************************************************************************/
struct AutoSaveJob
{
	SRAMSnapshot * sram;
	StateSnapshot * state;
};

// Runs on the worker thread, and owns the job
static int AutoSaveTask(void * arg)
{
	AutoSaveJob * job = (AutoSaveJob *)arg;

	if(job->sram)
		WriteSRAMSnapshot(job->sram, SILENT);
	if(job->state)
		WriteStateSnapshot(job->state, SILENT);

	FreeSRAMSnapshot(job->sram);
	FreeStateSnapshot(job->state);
	delete job;
	return 0;
}

static bool AutoSave()
{
	bool saveSram = false;
	bool saveState = false;

	if (EmuSettings.autoSave == AUTOSAVE_SRAM)
	{
		saveSram = true;
	}
	else if (EmuSettings.autoSave == AUTOSAVE_STATE)
	{
		saveState = WindowPrompt("Save", "Save State?", "Save", "Don't Save");
	}
	else if (EmuSettings.autoSave == AUTOSAVE_BOTH)
	{
		if (WindowPrompt("Save", "Save SRAM and State?", "Save", "Don't Save") )
		{
			saveSram = true;
			saveState = true;
		}
	}

	if(guiExiting) // asked to quit while the prompt was up
		return false;

	if(!saveSram && !saveState)
		return true; // nothing to save

	// Capture everything now - the worker may not get to it until the user has
	// already loaded another game
	AutoSaveJob * job = new (std::nothrow) AutoSaveJob();

	if(!job)
		return true;

	job->sram = saveSram ? SnapshotSRAMAuto() : nullptr;
	job->state = saveState ? SnapshotStateAuto() : nullptr;

	if(!job->sram && !job->state)
	{
		delete job;
		return true;
	}

	if(QueueBackgroundTask(AutoSaveTask, job))
		return true;

	// Worker unavailable or its queue is full (unlikely) - don't lose the save,
	// wait for it instead
	return RunWithGuiUpdates(AutoSaveTask, nullptr, job);
}

static int MenuGameSelection()
{
	int selection = MENU_NONE;
	bool res;
	int i;

	GuiText titleTxt("Choose Game", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiImageData iconHome(icon_home_png);
	GuiImageData iconSettings(icon_settings_png);

	GuiTrigger trigHome;
	trigHome.setButtonOnlyTrigger(-1, INPUT_BTN_HOME);

	GuiText settingsBtnTxt("Settings", 22, (PixelColor){0, 0, 0, 255});
	GuiImage settingsBtnIcon(&iconSettings);
	settingsBtnIcon.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	settingsBtnIcon.setPosition(14,0);
	MenuButtonShape settingsBtnImg(MenuButtonShape::LONG);
	GuiButton settingsBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	settingsBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	settingsBtn.setPosition(90, -35);
	settingsBtn.setLabel(&settingsBtnTxt);
	settingsBtn.setIcon(&settingsBtnIcon);
	settingsBtn.setImage(&settingsBtnImg);
	settingsBtn.setTrigger(trigA);
	settingsBtn.setEffectGrow();

	GuiText exitBtnTxt("Exit", 22, (PixelColor){0, 0, 0, 255});
	GuiImage exitBtnIcon(&iconHome);
	exitBtnIcon.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	exitBtnIcon.setPosition(14,0);
	MenuButtonShape exitBtnImg(MenuButtonShape::LONG);
	GuiButton exitBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	exitBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	exitBtn.setPosition(-90, -35);
	exitBtn.setLabel(&exitBtnTxt);
	exitBtn.setIcon(&exitBtnIcon);
	exitBtn.setImage(&exitBtnImg);
	exitBtn.setTrigger(trigA);

	#if defined(HW_RVL) || defined(HW_DOL)
	exitBtn.setTrigger(&trigHome);
	#endif

	exitBtn.setEffectGrow();

	GuiWindow buttonWindow(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	buttonWindow.append(&settingsBtn);
	buttonWindow.append(&exitBtn);

	#ifdef __WIIU__
	GuiButton hiddenHomeBtn;
	hiddenHomeBtn.setSoundOver(nullptr);
	hiddenHomeBtn.setSoundClick(nullptr);
	hiddenHomeBtn.setTrigger(&trigHome);
	hiddenHomeBtn.setSelectable(false);
	buttonWindow.append(&hiddenHomeBtn);
	#endif

	DeviceCheckingScope deviceChecking;

	GuiGameBrowser gameBrowser(MENU_LIST_W, MENU_LIST_H);
	gameBrowser.setPosition(MENU_LIST_X, MENU_LIST_Y);
	ResetBrowser();

	MenuCoverShadow previewShadow;
	previewShadow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	previewShadow.setPosition(MENU_PREVIEW_X + MENU_PREVIEW_W/2 - platform->getVideo()->getScreenWidth()/2,
		MENU_LIST_Y + MENU_PREVIEW_H/2 - platform->getVideo()->getScreenHeight()/2);

	GuiImage preview;
	preview.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	preview.setPosition(MENU_PREVIEW_X + MENU_PREVIEW_W/2 - platform->getVideo()->getScreenWidth()/2,
		MENU_LIST_Y + MENU_PREVIEW_H/2 - platform->getVideo()->getScreenHeight()/2);

#if defined(HW_RVL) || defined(HW_DOL)
	gameBrowser.setPreviewImage(&preview, ResolvePreviewImagePath, nullptr, 1, 0, 640, 480, OnPreviewImageChanged, &previewShadow);
#else
	gameBrowser.setPreviewImage(&preview, ResolvePreviewImagePath, nullptr, 6, 2, 640, 480, OnPreviewImageChanged, &previewShadow);
#endif

	menu->btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	menu->btnLogo.setPosition(-50, 24);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	menu->mainWindow.appendWithAutoRemove(&gameBrowser);
	menu->mainWindow.appendWithAutoRemove(&previewShadow);
	menu->mainWindow.appendWithAutoRemove(&preview);
	menu->mainWindow.appendWithAutoRemove(&buttonWindow);

	// populate initial directory listing
	selectLoadedFile = 1;
	if(!RunWithGuiUpdates(OpenGameListTask)) return MENU_EXIT;

	gameBrowser.resetState();
	gameBrowser.fileList[0]->setState(STATE::SELECTED);
	gameBrowser.triggerUpdate();
	titleTxt.setText(inSz ? szname : "Choose Game");

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		deviceChecking.update(); // starts device checking once its delay has elapsed

		// A device the currently-open folder lives on just disappeared - the cached browserList is stale
		const uint32_t removedMask = __atomic_exchange_n(&removedDeviceMask, 0, __ATOMIC_RELAXED);
		if(removedMask != 0)
		{
			if(browser.dir[0] != 0)
			{
				int currentDevice;
				if(!FindDevice(browser.dir, &currentDevice) || (removedMask & (1u << currentDevice)))
				{
					browser.dir[0] = 0;
					browserDeviceListChanged = true;
				}
			}
		}

		// A device appeared/disappeared since the last check  - refresh the device listing
		if(browserDeviceListChanged)
		{
			browserDeviceListChanged = false;

			if(browser.dir[0] == 0)
			{
				ResetBrowser();
				browser.numEntries = AddDeviceListing();
				gameBrowser.resetState();
				if(browser.numEntries > 0)
					gameBrowser.fileList[0]->setState(STATE::SELECTED);
				gameBrowser.triggerUpdate();
			}
		}

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
					gameBrowser.setState(STATE::DISABLED);

					if(!RunOnWorkerThread(BrowserChangeFolderTask))
					{
						gameBrowser.setState(STATE::DEFAULT);
						continue;
					}

					while(!IsWorkerThreadFinished())
					{
						if(!UpdateGui()) return MENU_EXIT;
						gameBrowser.setState(STATE::DISABLED);
					}

					res = GetWorkerThreadResult() != 0;

					if(res)
					{
						gameBrowser.resetState();
						gameBrowser.fileList[0]->setState(STATE::SELECTED);
						gameBrowser.triggerUpdate();
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

					if(!WaitForQueuedTasks()) return MENU_EXIT; // eg. auto-save of the last game

					deviceChecking.leave();

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

		if(settingsBtn.getState() == STATE::CLICKED)
			selection = MENU_SETTINGS;
		else if(exitBtn.getState() == STATE::CLICKED)
			platform->triggerExit();

		#ifdef __WIIU__
		if(hiddenHomeBtn.getState() == STATE::CLICKED) {
			hiddenHomeBtn.resetState();
			static_cast<WutInputDriver*>(platform->getInput())->openHomeButtonOverlay();
		}
		#endif
	}

	HaltParseThread(); // halt parsing
	ResetBrowser();
	return selection;
}

#ifndef HW_DOL
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
	trigLeft.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_LEFT);

	GuiTrigger trigRight;
	trigRight.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_RIGHT);

	MenuArrowImage arrowLeftImg(SHAPE::TRIANGLE_LEFT);
	GuiButton arrowLeftBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowLeftBtn.setSoundOver(nullptr);
	arrowLeftBtn.setSoundClick(nullptr);
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(PlayerMappingWindowLeftClick);

	MenuArrowImage arrowRightImg(SHAPE::TRIANGLE_RIGHT);
	GuiButton arrowRightBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowRightBtn.setSoundOver(nullptr);
	arrowRightBtn.setSoundClick(nullptr);
	arrowRightBtn.setImage(&arrowRightImg);
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

	if(lastMenu == MENU_NONE) // coming from the game
	{
		// Auto-save first, before the (slow) building of this screen below, so
		// that a prompt appears right away. The menu frame slides in behind it.
		enterSound->play();
		menu->btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
		menu->btnLogo.setPosition(-50, -40);
		menu->bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		menu->bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		menu->btnLogo.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);

		if(!AutoSave())
			return MENU_EXIT;
	}

	char romTitle[512];
	platform->getFileSystem()->nameToDisplay(Memory.ROMFilename, romTitle, sizeof(romTitle)); // raw name -> UTF-8 for display only
	GuiText titleTxt(romTitle, 22, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,40);

	MenuIconData iconMappings(icon_settings_mappings_png, -4.0f);
	MenuIconData iconSave(icon_game_save_png);
	MenuIconData iconReset(icon_game_reset_png);

	GuiTrigger trigHome;
	GuiTrigger trigB;
	trigHome.setButtonOnlyTrigger(-1, INPUT_BTN_HOME);
	trigB.setSecondaryTrigger();
	
	GuiText mappingBtnTxt("Button Mappings", 22, (PixelColor){0, 0, 0, 255});
	mappingBtnTxt.setWrap(true, MENU_BTN_LARGE_W-30);
	MenuButtonShape mappingBtnImg(MenuButtonShape::LARGE);
	GuiImage mappingBtnIcon(&iconMappings);
	GuiButton mappingBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	mappingBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	mappingBtn.setPosition(-200, 150);
	mappingBtn.setLabel(&mappingBtnTxt);
	mappingBtn.setImage(&mappingBtnImg);
	mappingBtn.setIcon(&mappingBtnIcon);
	mappingBtn.setTrigger(trigA);
	mappingBtn.setEffectGrow();

	GuiText saveBtnTxt("Saves", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape saveBtnImg(MenuButtonShape::LARGE);
	GuiImage saveBtnIcon(&iconSave);
	GuiButton saveBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	saveBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	saveBtn.setPosition(0, 150);
	saveBtn.setLabel(&saveBtnTxt);
	saveBtn.setImage(&saveBtnImg);
	saveBtn.setIcon(&saveBtnIcon);
	saveBtn.setTrigger(trigA);
	saveBtn.setEffectGrow();

	GuiText resetBtnTxt("Reset", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape resetBtnImg(MenuButtonShape::LARGE);
	GuiImage resetBtnIcon(&iconReset);
	GuiButton resetBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	resetBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	resetBtn.setPosition(200, 150);
	resetBtn.setLabel(&resetBtnTxt);
	resetBtn.setImage(&resetBtnImg);
	resetBtn.setIcon(&resetBtnIcon);
	resetBtn.setTrigger(trigA);
	resetBtn.setEffectGrow();

	// Second row: the controller chip (a popover) and Game Settings, centred as a pair
	UiScope ui;
	bool controllerRequest = false;
	GuiCard& controllerChip = MenuPopover_Chip(ui, MENU_BTN_NORMAL_W, "CONTROLLER", []{ return ControllerChipValue(); });
	controllerChip.onClick([&controllerRequest]{ controllerRequest = true; });
	controllerChip.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	controllerChip.setPosition(-120, 296);

	GuiText gameSettingsBtnTxt("Game Settings", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape gameSettingsBtnImg(MenuButtonShape::NORMAL);
	GuiButton gameSettingsBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	gameSettingsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	gameSettingsBtn.setPosition(120, 290);
	gameSettingsBtn.setLabel(&gameSettingsBtnTxt);
	gameSettingsBtn.setImage(&gameSettingsBtnImg);
	gameSettingsBtn.setTrigger(trigA);
	gameSettingsBtn.setEffectGrow();

	GuiText mainmenuBtnTxt("Main Menu", 22, (PixelColor){0, 0, 0, 255});
	if(EmuSettings.autoloadGame) {
		mainmenuBtnTxt.setText("Exit");
	}
	MenuButtonShape mainmenuBtnImg(MenuButtonShape::NORMAL);
	GuiButton mainmenuBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	mainmenuBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
	mainmenuBtn.setPosition(0, -35);
	mainmenuBtn.setLabel(&mainmenuBtnTxt);
	mainmenuBtn.setImage(&mainmenuBtnImg);
	mainmenuBtn.setTrigger(trigA);
	mainmenuBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	MenuButtonShape closeBtnImg(MenuButtonShape::SMALL);
	GuiButton closeBtn(MENU_BTN_SMALL_W, MENU_BTN_SMALL_H);
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setTrigger(&trigB);
	closeBtn.setEffectGrow();

	#ifndef HW_DOL
	ControllerStatus controllerStatus(trigA);
	int controllerClicked;
	#endif

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&saveBtn);
	w.append(&mappingBtn);
	w.append(&resetBtn);
	w.append(&controllerChip);
	w.append(&gameSettingsBtn);

	#ifndef HW_DOL
	controllerStatus.appendTo(w);
	#endif

	w.append(&mainmenuBtn);
	w.append(&closeBtn);

	ui.attachTo(w);

	menu->btnLogo.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	menu->btnLogo.setPosition(-50, -40);
	menu->mainWindow.appendWithAutoRemove(&w);

	if(lastMenu == MENU_NONE)
	{
		closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		mainmenuBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#ifndef HW_DOL
		controllerStatus.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#endif

		w.setEffect(EFFECT::FADE, 15);
	}

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		#ifndef HW_DOL
		controllerStatus.update();
		#endif

		if(saveBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAME_SAVE;
		}
		else if(mappingBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_MAPPINGS;
		}
		else if(controllerRequest)
		{
			controllerRequest = false;
			// a popover beside the chip, over this screen, which stays drawn (dimmed) where it is
			w.setState(STATE::DISABLED);
			ChooseController(GuiPopover::Anchor(controllerChip));
			w.setState(STATE::DEFAULT);
			menu->mainWindow.changeFocus(&w);
			w.resetState();
			controllerChip.select();
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
#ifndef HW_DOL
		else if((controllerClicked = controllerStatus.clicked()) >= 0)
		{
			PlayerMappingWindow(controllerClicked);
		}
#endif
		else if(mainmenuBtn.getState() == STATE::CLICKED)
		{
			if (WindowPrompt("Quit Game", "Quit this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				menu->mainWindow.remove(gameScreenImg);
				delete gameScreenImg;
				if(gameScreenTexture != nullptr) {
					platform->getVideo()->getImageRenderer()->destroyTexture(gameScreenTexture);
					gameScreenTexture = nullptr;
				}
				ClearScreenshot();
				if(EmuSettings.autoloadGame) {
					ExitApp();
				}
				else {
					gameScreenImg = new MenuBackgroundShape();
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
			#ifndef HW_DOL
			controllerStatus.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			#endif

			w.setEffect(EFFECT::FADE, -15);

			while(w.getEffect() > 0)
			{
				if(!UpdateGui()) return MENU_EXIT;
			}
		}
	}


	return selection;
}

/****************************************************************************
 * FindGameSaveNum
 *
 * Determines the save file number of the given file name
 * Returns -1 if none is found
 ***************************************************************************/
static int FindGameSaveNum(char * savefile, bool exactIsAuto = false)
{
	int n = -1;
	int romlen = strlen(Memory.ROMFilename);
	int savelen = strlen(savefile);

	int diff = savelen-romlen;

	if(strncmp(savefile, Memory.ROMFilename, romlen) != 0)
		return -1;

	// The auto slot has no "Auto" in its name when the setting for that is off, and a game's save file may have
	// always been named that way. Only the caller knows whether that name is the auto slot.
	if(diff == 0)
		return exactIsAuto ? 0 : -1;

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
 * SaveScreenData / SaveListTask
 *
 * Finds this game's save files and decodes their screenshots on the worker
 * thread. Every save costs several requests to the storage device, and that
 * is slow on some platforms. Turning the decoded screenshots into
 * textures has to wait until we are back on the main thread.
 ***************************************************************************/
struct SaveScreenData
{
	SaveList saves{};
	GuiImageData::DecodedImage thumbs[MAX_SAVES + 1];
};

static int SaveListTask(void * arg)
{
	SaveScreenData * data = (SaveScreenData *)arg;
	SaveList & saves = data->saves;
	char filepath[1024];
	char scrfile[1024];
	char tmp[MAXJOLIET+1];
	struct stat filestat;
	struct tm * timeinfo;
	int i, n, type, len2;
	int j = 0;

	int len = strlen(Memory.ROMFilename);

	platform->getFileSystem()->getPath(browser.dir, EmuSettings.saveDevice, EmuSettings.saveFolder);
	ParseDirectory(true, false, Memory.ROMFilename); // only this game's files - the folder holds every game's saves

	// find matching files
	AllocSaveBuffer();

	for(i=0; i < browser.numEntries && j <= MAX_SAVES; i++)
	{
		len2 = strlen(browserList[i].filename);

		if(len2 < 6 || len2-len < 4)
			continue;

		if(strncmp(&browserList[i].filename[len2-4], ".srm", 4) == 0)
			type = FILE_SRAM;
		else if(strncmp(&browserList[i].filename[len2-4], ".frz", 4) == 0)
			type = FILE_STATE;
		else
			continue;

		strcpy(tmp, browserList[i].filename);
		tmp[len2-4] = 0;
		n = FindGameSaveNum(tmp, type == FILE_SRAM || !EmuSettings.appendAuto);

		if(n >= 0)
		{
			saves.type[j] = type;
			saves.files[saves.type[j]][n] = 1;
			strcpy(saves.filename[j], browserList[i].filename);

			if(saves.type[j] == FILE_STATE)
			{
				char scrname[MAXJOLIET+1];
				snprintf(scrname, sizeof(scrname), "%s.png", tmp);
				platform->getFileSystem()->getPath(scrfile, EmuSettings.saveDevice, EmuSettings.saveFolder, scrname);

				memset(savebuffer, 0, SAVEBUFFERSIZE);
				if(LoadFile(scrfile, SILENT))
					data->thumbs[j] = GuiImageData::decodeToRgba(savebuffer, 64, 48);
			}
			platform->getFileSystem()->getPath(filepath, EmuSettings.saveDevice, EmuSettings.saveFolder, saves.filename[j]);
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
	return j;
}

/****************************************************************************
 * SaveOpTask
 *
 * Loads, saves or deletes a single save file from the saves screen, on the
 * worker thread.
 ***************************************************************************/
enum { SAVEOP_LOAD, SAVEOP_SAVE, SAVEOP_DELETE };

struct SaveOpArgs
{
	int op;
	int type; // FILE_SRAM / FILE_STATE
	bool silent; // saving only: no "Save successful" box - the screen shows its own note
	char filepath[1024];
};
static SaveOpArgs saveOpArgs; // static: the worker may still be running if the app exits mid-operation

static int SaveOpTask(void * arg)
{
	SaveOpArgs * a = (SaveOpArgs *)arg;
	char deletepath[1024];
	int result = 0;

	switch(a->op)
	{
		case SAVEOP_LOAD:
			if(a->type == FILE_SRAM)
				result = LoadSRAM(a->filepath, NOTSILENT);
			else if(a->type == FILE_STATE)
				result = LoadSnapshot(a->filepath, NOTSILENT);
			break;

		case SAVEOP_SAVE:
			if(a->type == FILE_SRAM)
				result = SaveSRAM(a->filepath, a->silent);
			else if(a->type == FILE_STATE)
				result = SaveSnapshot(a->filepath, a->silent);
			break;

		case SAVEOP_DELETE:
			if(a->type == FILE_SRAM)
			{
				snprintf(deletepath, sizeof(deletepath), "%s", a->filepath);
				deletepath[strlen(deletepath)-4] = 0;
				strcat(deletepath, ".srm");
				remove(deletepath); // Delete the *.srm file (Battery save file)
			}
			else if(a->type == FILE_STATE)
			{
				snprintf(deletepath, sizeof(deletepath), "%s", a->filepath);
				deletepath[strlen(deletepath)-4] = 0;
				strcat(deletepath, ".png");
				remove(deletepath); // Delete the *.png file (Screenshot file)
				snprintf(deletepath, sizeof(deletepath), "%s", a->filepath);
				deletepath[strlen(deletepath)-4] = 0;
				strcat(deletepath, ".frz");
				remove(deletepath); // Delete the *.frz file (Save State file)
			}
			break;
	}
	return result;
}

// \return false if the app was asked to quit
static bool RunSaveOp(int op, int type, const char * filepath, int * result, bool silent = false)
{
	menu->mainWindow.setState(STATE::DISABLED);
	if(!WaitForQueuedTasks()) return false;

	saveOpArgs.op = op;
	saveOpArgs.type = type;
	saveOpArgs.silent = silent;
	snprintf(saveOpArgs.filepath, sizeof(saveOpArgs.filepath), "%s", filepath);
	return RunWithGuiUpdates(SaveOpTask, result, &saveOpArgs);
}

/****************************************************************************
 * MenuGameSaves
 *
 * One screen for everything saved: a grid of snapshots (the whole game at
 * one moment, a "state") and under it the game's own save file ("SRAM"),
 * which a game with no battery doesn't have. Choosing a card opens a popover
 * beside it with what can be done with that card, so the D-pad, the pointer
 * and touch all work the same way. The snapshot the game was started from
 * (or last saved to) is pinned first and tagged, and is what "Update" offers
 * to overwrite. The save file's backups are one step further: its popover
 * has a Backups row that opens a list of them.
 ***************************************************************************/
enum { SAVEACT_LOAD, SAVEACT_UPDATE, SAVEACT_DELETE, SAVEACT_SAVENOW, SAVEACT_BACKUP, SAVEACT_BACKUPS, SAVEACT_RESTORE };

#define SAVE_LIST_Y		116		// the grid's top, under the header bar
#define SAVE_LIST_W		540
#define SAVE_CARD_W		250	// a card of the browser, and the pitch from one column to the next: both are the
#define SAVE_CARD_H		72	// browser's, repeated here for the note that stands in the empty column
#define SAVE_COL_PITCH	258
#define SAVE_MAX_BACKUPS	6	// the most the Backups list shows: a popover doesn't scroll
#define SAVE_TOAST_W	224
#define SAVE_TOAST_H	30
#define SAVE_TOAST_FRAMES	120

// What a card stands for
struct SaveCard
{
	int save;		// index into the SaveList; -1 when there is no file yet (a "new" card)
	int type;		// FILE_STATE / FILE_SRAM
	int slot;		// the number in the file's name; 0 is the auto slot; -1 for a new snapshot
	bool main;		// the game's own save file (slot 0 of the save file kind)
	bool playing;	// the snapshot the game was started from
};

// The slot number in a save file's name (0 for the auto slot), or -1
static int SaveSlotOf(const char * filename, int type)
{
	char tmp[MAXJOLIET+1];
	const int len = strlen(filename);

	if(len < 5 || len > MAXJOLIET)
		return -1;

	snprintf(tmp, sizeof(tmp), "%s", filename);
	tmp[len-4] = 0; // the extension
	return FindGameSaveNum(tmp, type == FILE_SRAM || !EmuSettings.appendAuto);
}

static std::string SaveNumbered(const char * what, int slot)
{
	char text[64];
	snprintf(text, sizeof(text), "%s %d", what, slot);
	return text;
}

static std::string SaveWhen(SaveList & saves, int i)
{
	std::string when = saves.date[i];
	if(saves.time[i][0])
		when += (when.empty() ? "" : ", ") + std::string(saves.time[i]);
	return when;
}

// The files of one kind, the auto slot first and then the numbered ones, newest slot first
static void SortSaveFiles(SaveList & saves, int type, std::vector<int> & found)
{
	std::sort(found.begin(), found.end(), [&saves, type](int a, int b)
	{
		const int sa = SaveSlotOf(saves.filename[a], type);
		const int sb = SaveSlotOf(saves.filename[b], type);
		if((sa == 0) != (sb == 0))
			return sa == 0;
		return sa > sb;
	});
}

// The game's own save file, then its backups, as indexes into the SaveList
static void FindSramFiles(SaveList & saves, std::vector<int> & found)
{
	found.clear();
	for(int i = 0; i < saves.length; i++)
		if(saves.type[i] == FILE_SRAM && SaveSlotOf(saves.filename[i], FILE_SRAM) >= 0)
			found.push_back(i);

	// a save file from before the slots, named for the game itself, is the game's save: other slot 0 files are not
	const size_t romLen = strlen(Memory.ROMFilename);
	bool legacy = false;
	for(size_t k = 0; k < found.size(); k++)
		legacy = legacy || (SaveSlotOf(saves.filename[found[k]], FILE_SRAM) == 0 && strlen(saves.filename[found[k]]) - 4 == romLen);
	if(legacy)
		found.erase(std::remove_if(found.begin(), found.end(), [&saves, romLen](int i)
		{
			return SaveSlotOf(saves.filename[i], FILE_SRAM) == 0 && strlen(saves.filename[i]) - 4 != romLen;
		}), found.end());

	SortSaveFiles(saves, FILE_SRAM, found);
}

// The cards: the snapshot we started from, a New Snapshot card, the auto snapshot, then the numbered ones newest
// first - and, when the game has a save of its own, its pinned card last. The backups of that save are not cards:
// they are listed in `backups` (indexes into the SaveList), for its popover.
static void BuildSaveCards(SaveList & saves, bool gameSave, std::vector<SaveEntry> & entries, std::vector<SaveCard> & cards, std::vector<int> & backups)
{
	entries.clear();
	cards.clear();
	backups.clear();

	const char * playing = GetPlayingFrom();
	char path[1024];
	std::vector<int> found;
	int playingIdx = -1;

	for(int i = 0; i < saves.length; i++)
	{
		if(saves.type[i] != FILE_STATE || SaveSlotOf(saves.filename[i], FILE_STATE) < 0)
			continue;

		if(playing[0] && MakeFilePath(path, FILE_STATE, saves.filename[i]) && strcmp(path, playing) == 0)
			playingIdx = i;
		else
			found.push_back(i);
	}
	SortSaveFiles(saves, FILE_STATE, found);

	auto addSnapshot = [&](int i, bool isPlaying)
	{
		SaveEntry e;
		SaveCard c;
		c.save = i;
		c.type = FILE_STATE;
		c.slot = SaveSlotOf(saves.filename[i], FILE_STATE);
		c.main = false;
		c.playing = isPlaying;
		e.kind = SaveEntry::SAVE;
		e.title = c.slot == 0 ? std::string("Auto Snapshot") : SaveNumbered("Snapshot", c.slot);
		e.subtitle = SaveWhen(saves, i);
		e.preview = saves.previewImg[i].get();
		e.placeholder = "No image";
		e.playing = isPlaying;
		e.id = i;
		entries.push_back(e);
		cards.push_back(c);
	};

	if(playingIdx >= 0)
		addSnapshot(playingIdx, true);

	{
		SaveEntry e;
		SaveCard c;
		e.kind = SaveEntry::NEW;
		e.title = "New Snapshot";
		e.subtitle = "Save this moment";
		e.placeholder = "New";
		c.save = -1; c.type = FILE_STATE; c.slot = -1; c.main = false; c.playing = false;
		entries.push_back(e);
		cards.push_back(c);
	}

	for(size_t k = 0; k < found.size(); k++)
		addSnapshot(found[k], false);

	if(!gameSave)
		return;

	std::vector<int> sram;
	FindSramFiles(saves, sram);
	const bool haveMain = !sram.empty() && SaveSlotOf(saves.filename[sram[0]], FILE_SRAM) == 0;

	SaveEntry e;
	SaveCard c;
	e.kind = haveMain ? SaveEntry::SAVE : SaveEntry::NEW;	// with no file yet it is a card for writing it the first time
	e.pinned = true;
	e.title = "Game Save File";
	e.subtitle = haveMain ? "Saved " + SaveWhen(saves, sram[0]) : std::string("Not saved yet");
	// Auto-save looks after this file when it is set to SRAM or both; set to snapshots only, or off, it does not
	const bool autoSaved = EmuSettings.autoSave == AUTOSAVE_SRAM || EmuSettings.autoSave == AUTOSAVE_BOTH;
	e.chip = autoSaved ? "Auto-save on" : "Auto-save off";
	e.chipOn = autoSaved;
	e.id = haveMain ? sram[0] : -1;
	c.save = e.id; c.type = FILE_SRAM; c.slot = 0; c.main = true; c.playing = false;
	entries.push_back(e);
	cards.push_back(c);

	for(size_t k = haveMain ? 1 : 0; k < sram.size(); k++)
		backups.push_back(sram[k]);
}

static SaveCard BackupCard(SaveList & saves, int index)
{
	SaveCard c;
	c.save = index;
	c.type = FILE_SRAM;
	c.slot = SaveSlotOf(saves.filename[index], FILE_SRAM);
	c.main = false;
	c.playing = false;
	return c;
}

// What can be done with a card: the rows of its popover and, for each, the action it stands for. The row the
// cursor starts on is the likeliest and never the destructive one - except Update on the snapshot we started from.
static void SaveCardMenu(const SaveCard & card, int backupCount, std::vector<PickerItem> & items, std::vector<int> & acts)
{
	items.clear();
	acts.clear();

	auto add = [&items, &acts](int act, const char * name, const std::string & about, bool preferred, bool divider)
	{
		PickerItem it;
		it.name = name;
		it.hint = about;
		it.current = false;
		it.preferred = preferred;
		it.divider = divider;
		items.push_back(it);
		acts.push_back(act);
	};

	if(card.type == FILE_STATE)
	{
		if(card.playing)
		{
			add(SAVEACT_UPDATE, "Update", "Replace with your progress now", true, false);
			add(SAVEACT_LOAD, "Load", "Jump back to this moment", false, true);
		}
		else
		{
			add(SAVEACT_LOAD, "Load", "Jump to this moment", true, false);
			add(SAVEACT_UPDATE, "Replace", "Overwrite with your progress", false, true);
		}
		add(SAVEACT_DELETE, "Delete", "Remove this snapshot", false, false);
	}
	else if(card.main)
	{
		add(SAVEACT_SAVENOW, "Save Now", "Write the save to its file", true, false);
		add(SAVEACT_BACKUP, "Make Backup", "Keep a copy, just in case", false, backupCount > 0);
		if(backupCount > 0)
		{
			add(SAVEACT_BACKUPS, "Backups", GuiFormat("%d saved", backupCount), false, false);
			items.back().chevron = true;
		}
	}
	else
	{
		add(SAVEACT_RESTORE, "Restore", "Use this as the game's save", true, true);
		add(SAVEACT_DELETE, "Delete", "Remove this backup", false, false);
	}
}

static int MenuGameSaves()
{
	int selection = MENU_NONE;
	char filepath[1024];

	static ChangeInterfaceArgs ciArgs;
	ciArgs.device = EmuSettings.saveDevice;
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

	// A game with no battery has no save file of its own, and the setting can hide it: then there is
	// nothing under the snapshots, and they get the room
	const bool gameSave = !EmuSettings.hideSramSaving && GetSRAMSaveSize() > 0;

	const MenuPalette & pal = MenuPaletteCurrent();
	const PixelColor white = (PixelColor){255, 255, 255, 255};

	char romTitle[512];
	platform->getFileSystem()->nameToDisplay(Memory.ROMFilename, romTitle, sizeof(romTitle)); // raw name -> UTF-8 for display only

	GuiText titleTxt("Saves", 26, white);
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,24);

	GuiText gameTxt(romTitle, 20, white);
	gameTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	gameTxt.setPosition(50,60);

	GuiTrigger trigHome;
	trigHome.setButtonOnlyTrigger(-1, INPUT_BTN_HOME);

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	MenuButtonShape closeBtnImg(MenuButtonShape::SMALL);
	GuiButton closeBtn(MENU_BTN_SMALL_W, MENU_BTN_SMALL_H);
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setEffectGrow();

	// Shown beside a lone "New Snapshot" card, in the grid's second column. It is a card of its own - dark and
	// translucent, the size of the cards, not clickable - so the words read over any picture, and it looks
	// like a note next to the white cards you can choose.
	const int emptyX = (platform->getVideo()->getScreenWidth() - SAVE_LIST_W) / 2 + SAVE_COL_PITCH;
	GuiShape emptyCard(SAVE_CARD_W, SAVE_CARD_H);
	emptyCard.setCorners(GuiCorners(GuiTheme::current().radius));
	emptyCard.setPaint(GuiPaint().color(MenuRGBA(0, 0, 0, 150)).stroke(1.5f, MenuRGBA(255, 255, 255, 80)));
	emptyCard.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	emptyCard.setPosition(emptyX, SAVE_LIST_Y);
	emptyCard.setVisible(false);
	GuiText emptyTitle("No snapshots yet", 16, white);
	GuiText emptyLine0("Remembers the exact moment, so you", 13, white);
	GuiText emptyLine1("can jump back to it any time.", 13, white);
	GuiText * emptyTexts[3] = { &emptyTitle, &emptyLine0, &emptyLine1 };
	const int emptyTextY[3] = { 11, 35, 52 };
	for(int k = 0; k < 3; k++)
	{
		emptyTexts[k]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		emptyTexts[k]->setPosition(emptyX + 16, SAVE_LIST_Y + emptyTextY[k]);
		emptyTexts[k]->setVisible(false);
	}

	// the note that says what a save did, over the bottom of the list
	GuiText toastTxt(nullptr, 16, white);
	toastTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	toastTxt.setPosition(14, 0);
	MenuCheckIcon toastCheck(20, white, pal.accentDark);
	toastCheck.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	toastCheck.setPosition(10, 0);
	GuiShape toastShape(SAVE_TOAST_W, SAVE_TOAST_H);
	toastShape.setCorners(GuiCorners(SAVE_TOAST_H / 2.0f));
	toastShape.setStyle(GuiShapeStyle(GuiPaint().gradient(pal.accent, pal.accentDark).stroke(2.0f, pal.glow)));
	GuiButton toastBtn(SAVE_TOAST_W, SAVE_TOAST_H);
	toastBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
	toastBtn.setPosition(0, -(MENU_BAR_BOTTOM_H + 4));
	toastBtn.setLabel(&toastTxt);
	toastBtn.setImage(&toastShape);
	toastBtn.setIcon(&toastCheck);
	toastBtn.setSelectable(false);
	toastBtn.setVisible(false);
	int toastFrames = 0;

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	w.append(&closeBtn);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&titleTxt);
	menu->mainWindow.appendWithAutoRemove(&gameTxt);
	menu->mainWindow.appendWithAutoRemove(&emptyCard);
	for(int k = 0; k < 3; k++)
		menu->mainWindow.appendWithAutoRemove(emptyTexts[k]);

	// two rows of snapshots with the save file under them, or three rows when there is no save file
	GuiSaveBrowser saveBrowser(SAVE_LIST_W, gameSave ? 2 : 3);
	saveBrowser.setPosition(0, SAVE_LIST_Y);
	saveBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	menu->mainWindow.appendWithAutoRemove(&saveBrowser);
	menu->mainWindow.appendWithAutoRemove(&toastBtn);

	// Look up the saves on the worker thread so the GUI keeps running. The data is on
	// the heap because the task may still be using it if the app is asked to quit.
	std::unique_ptr<SaveScreenData> data;
	auto loadSaves = [&data]() -> bool
	{
		std::unique_ptr<SaveScreenData> fresh(new SaveScreenData);

		if(!RunWithGuiUpdates(SaveListTask, nullptr, fresh.get()))
		{
			fresh.release(); // deliberately leaked - the worker may still be writing to it
			return false;
		}

		// create the screenshot textures (main thread only)
		for(int i = 0; i < fresh->saves.length; i++)
		{
			if(fresh->thumbs[i].valid())
			{
				auto thumb = std::make_unique<GuiImageData>();
				if(thumb->uploadDecoded(std::move(fresh->thumbs[i])))
					fresh->saves.previewImg[i] = std::move(thumb);
			}
		}
		data = std::move(fresh);
		return true;
	};

	std::vector<SaveEntry> entries;
	std::vector<SaveCard> cards;
	std::vector<int> backups;

	// Shows the cards, with the cursor on the card of kind `selType` (FILE_STATE / FILE_SRAM) and slot `selSlot`, or with
	// `selType` -1 on the card at `selPos` (or the last one of the grid, when that is gone)
	auto showCards = [&](int selType, int selSlot, int selPos)
	{
		BuildSaveCards(data->saves, gameSave, entries, cards, backups);

		const int gridCount = (int)entries.size() - ((!entries.empty() && entries.back().pinned) ? 1 : 0);
		int select = selPos;
		for(size_t k = 0; selType >= 0 && k < cards.size(); k++)
			if(cards[k].type == selType && cards[k].slot == selSlot)
				select = (int)k;
		if(selType < 0 && select >= gridCount)
			select = gridCount - 1;
		if(select >= (int)entries.size())
			select = (int)entries.size() - 1;
		if(select < 0)
			select = 0;

		saveBrowser.setEntries(entries, select);

		const bool onlyNew = (gridCount == 1);
		emptyCard.setVisible(onlyNew);
		for(int k = 0; k < 3; k++)
			emptyTexts[k]->setVisible(onlyNew);

		menu->mainWindow.changeFocus(&saveBrowser);
	};

	auto showToast = [&](const std::string & text)
	{
		toastTxt.setText(text.c_str());
		toastBtn.setVisible(true);
		toastFrames = SAVE_TOAST_FRAMES;
	};

	// the first free slot (1-99) of a kind of file, or -1
	auto freeSlot = [&](int type) -> int
	{
		for(int s = 1; s < 100; s++)
			if(data->saves.files[type][s] == 0)
				return s;
		return -1;
	};

	// What to do with a card, asked in popovers beside it. The save file's popover leads on to its backups: a list
	// of them, then what to do with the one picked (which then becomes `card`). B steps back to the one before.
	// The action chosen (a SAVEACT_*), or -1.
	auto askAction = [&](SaveCard & card, SaveEntry & entry, const GuiPopover::Anchor & anchor) -> int
	{
		std::vector<PickerItem> items;
		std::vector<int> acts;

		while(true)
		{
			SaveCardMenu(card, (int)backups.size(), items, acts);
			const int chosen = MenuPopover_Pick("", items, anchor);
			if(chosen < 0 || (size_t)chosen >= acts.size())
				return -1;
			if(acts[(size_t)chosen] != SAVEACT_BACKUPS)
				return acts[(size_t)chosen];

			while(true)
			{
				std::vector<PickerItem> list;
				const size_t shown = std::min(backups.size(), (size_t)SAVE_MAX_BACKUPS);
				for(size_t k = 0; k < shown; k++)
				{
					PickerItem it;
					it.name = SaveNumbered("Backup", SaveSlotOf(data->saves.filename[backups[k]], FILE_SRAM));
					it.hint = SaveWhen(data->saves, backups[k]);
					it.current = false;
					list.push_back(it);
				}

				const int picked = MenuPopover_Pick("", list, anchor);
				if(picked < 0 || (size_t)picked >= shown)
					break;	// back to the save file's popover

				const SaveCard backup = BackupCard(data->saves, backups[(size_t)picked]);
				std::vector<PickerItem> what;
				std::vector<int> whatActs;
				SaveCardMenu(backup, 0, what, whatActs);
				const int doing = MenuPopover_Pick("", what, anchor);
				if(doing < 0 || (size_t)doing >= whatActs.size())
					continue;	// back to the list of backups

				card = backup;
				entry.title = SaveNumbered("Backup", backup.slot);
				return whatActs[(size_t)doing];
			}
		}
	};

	if(!loadSaves())
		return MENU_EXIT;
	showCards(-1, -1, 0);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(toastFrames > 0 && --toastFrames == 0)
			toastBtn.setVisible(false);

		const int pos = saveBrowser.getClickedSave();

		if(pos >= 0 && pos < (int)cards.size())
		{
			SaveCard card = cards[(size_t)pos];
			SaveEntry entry = entries[(size_t)pos];
			int act = -1;

			if(card.type == FILE_STATE && card.save < 0)
			{
				act = SAVEACT_UPDATE; // New Snapshot: it only ever adds one, so there is nothing to ask
			}
			else
			{
				// popovers beside the card, over this screen, which keeps the card highlighted but takes no input
				w.setState(STATE::DISABLED);
				saveBrowser.setState(STATE::DISABLED);
				act = askAction(card, entry, saveBrowser.getEntryAnchor(pos));
				saveBrowser.setState(STATE::DEFAULT);
				w.setState(STATE::DEFAULT);
				saveBrowser.resetState();
				menu->mainWindow.changeFocus(&saveBrowser);
			}

			if(act >= 0)
			{
				int result = 0;
				int newSlot = -1;
				bool changed = false;	// the list of files changed: look again
				const char * saved = nullptr;

				if(card.save >= 0)
					MakeFilePath(filepath, card.type, data->saves.filename[card.save]);

				switch(act)
				{
					case SAVEACT_LOAD:
					case SAVEACT_RESTORE:
						if(!RunSaveOp(SAVEOP_LOAD, card.type, filepath, &result))
							return MENU_EXIT;
						if(result)
							selection = MENU_EXIT;	// back to the game
						break;

					case SAVEACT_UPDATE: // Update / Replace / a New Snapshot
					{
						if(card.save < 0)
						{
							newSlot = freeSlot(FILE_STATE);
							if(newSlot < 0)
							{
								ErrorPrompt("All 99 snapshot slots are in use. Delete one to make room.");
								break;
							}
							MakeFilePath(filepath, FILE_STATE, Memory.ROMFilename, newSlot);
						}
						else if(!card.playing && !WindowPrompt(("Replace " + entry.title + "?").c_str(),
							"It will be overwritten with your progress right now.", "Replace", "Cancel"))
							break;

						if(!RunSaveOp(SAVEOP_SAVE, FILE_STATE, filepath, &result, true))
							return MENU_EXIT;

						if(result)
						{
							changed = true;
							if(newSlot >= 0)
							{
								saved = "saved";
							}
							else
							{
								newSlot = card.slot;
								saved = "updated";
							}
						}
						else
							ErrorPrompt("Save failed!");
						break;
					}

					case SAVEACT_SAVENOW:
					case SAVEACT_BACKUP:
					{
						int slot = 0;
						if(act == SAVEACT_BACKUP)
						{
							slot = freeSlot(FILE_SRAM);
							if(slot < 0 || (int)backups.size() >= SAVE_MAX_BACKUPS)
							{
								ErrorPrompt(GuiFormat("All %d backup slots are in use. Delete one to make room.", SAVE_MAX_BACKUPS).c_str());
								break;
							}
						}
						if(act == SAVEACT_BACKUP || card.save < 0)	// Save Now on a file that exists writes that very file
							MakeFilePath(filepath, FILE_SRAM, Memory.ROMFilename, slot);

						if(!RunSaveOp(SAVEOP_SAVE, FILE_SRAM, filepath, &result, true))
							return MENU_EXIT;

						if(result)
						{
							changed = true;
							newSlot = slot;
							saved = (act == SAVEACT_BACKUP) ? "saved" : "updated";
						}
						else
							ErrorPrompt("Save failed!");
						break;
					}

					case SAVEACT_DELETE:
					{
						std::string question = "Delete " + entry.title + "?";
						std::string detail = "This can't be undone.";
						if(card.playing)
							detail = "This is the snapshot you started from. " + detail;

						if(!WindowPrompt(question.c_str(), detail.c_str(), "Delete", "Cancel"))
							break;

						if(!RunSaveOp(SAVEOP_DELETE, card.type, filepath, &result))
							return MENU_EXIT;

						if(card.type == FILE_STATE && strcmp(filepath, GetPlayingFrom()) == 0)
							ClearPlayingFrom(); // or the next snapshot to take this slot would look like the one we started from

						changed = true;
						break;
					}
				}

				if(changed && selection == MENU_NONE)
				{
					if(!loadSaves())
						return MENU_EXIT;

					// the cursor stays where it was: on the save file for anything done to it or its backups, on the
					// snapshot just written, or where a deleted snapshot was
					if(card.type == FILE_SRAM)
						showCards(FILE_SRAM, 0, pos);
					else if(act == SAVEACT_DELETE)
						showCards(-1, -1, pos);
					else
						showCards(FILE_STATE, newSlot, pos);

					if(saved)
					{
						std::string what = (card.type == FILE_SRAM)
							? (newSlot == 0 ? std::string("Game save file") : SaveNumbered("Backup", newSlot))
							: SaveNumbered("Snapshot", newSlot);
						showToast(what + " " + saved);
					}
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

	MenuIconData iconVideo(icon_settings_video_png);
	MenuIconData iconEmulation(icon_settings_emulation_png);
	MenuIconData iconCheats(icon_game_cheats_png);
	MenuIconData iconScreenshot(icon_settings_screenshot_png);

	GuiTrigger trigHome;
	GuiTrigger trigB;
	trigHome.setButtonOnlyTrigger(-1, INPUT_BTN_HOME);
	trigB.setSecondaryTrigger();

	GuiText emulationBtnTxt("Emulation", 22, (PixelColor){0, 0, 0, 255});
	emulationBtnTxt.setWrap(true, MENU_BTN_LARGE_W-20);
	MenuButtonShape emulationBtnImg(MenuButtonShape::LARGE);
	GuiImage emulationBtnIcon(&iconEmulation);
	GuiButton emulationBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	emulationBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	emulationBtn.setPosition(-100, 120);
	emulationBtn.setLabel(&emulationBtnTxt);
	emulationBtn.setImage(&emulationBtnImg);
	emulationBtn.setIcon(&emulationBtnIcon);
	emulationBtn.setTrigger(trigA);
	emulationBtn.setEffectGrow();
	
	GuiText videoBtnTxt("Video", 22, (PixelColor){0, 0, 0, 255});
	videoBtnTxt.setWrap(true, MENU_BTN_LARGE_W-20);
	MenuButtonShape videoBtnImg(MenuButtonShape::LARGE);
	GuiImage videoBtnIcon(&iconVideo);
	GuiButton videoBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	videoBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	videoBtn.setPosition(100, 120);
	videoBtn.setLabel(&videoBtnTxt);
	videoBtn.setImage(&videoBtnImg);
	videoBtn.setIcon(&videoBtnIcon);
	videoBtn.setTrigger(trigA);
	videoBtn.setEffectGrow();

	GuiText screenshotBtnTxt("Screenshot", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape screenshotBtnImg(MenuButtonShape::LARGE);
	GuiImage screenshotBtnIcon(&iconScreenshot);
	GuiButton screenshotBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	screenshotBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	screenshotBtn.setPosition(-100, 250);
	screenshotBtn.setLabel(&screenshotBtnTxt);
	screenshotBtn.setImage(&screenshotBtnImg);
	screenshotBtn.setIcon(&screenshotBtnIcon);
	screenshotBtn.setTrigger(trigA);
	screenshotBtn.setEffectGrow();
	
	GuiText cheatsBtnTxt("Cheats", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape cheatsBtnImg(MenuButtonShape::LARGE);
	GuiImage cheatsBtnIcon(&iconCheats);
	GuiButton cheatsBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	cheatsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	cheatsBtn.setPosition(100, 250);
	cheatsBtn.setLabel(&cheatsBtnTxt);
	cheatsBtn.setImage(&cheatsBtnImg);
	cheatsBtn.setIcon(&cheatsBtnIcon);
	cheatsBtn.setTrigger(trigA);
	cheatsBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 20, (PixelColor){0, 0, 0, 255});
	MenuButtonShape closeBtnImg(MenuButtonShape::SMALL);
	GuiButton closeBtn(MENU_BTN_SMALL_W, MENU_BTN_SMALL_H);
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setTrigger(trigA);
	closeBtn.setTrigger(&trigHome);
	closeBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&titleTxt);
	w.append(&videoBtn);
	w.append(&emulationBtn);
	w.append(&screenshotBtn);
	w.append(&cheatsBtn);
	w.append(&closeBtn);
	w.append(&backBtn);
	
	menu->mainWindow.appendWithAutoRemove(&w);


	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		if(videoBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_VIDEO;
		}
		else if(emulationBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_GAMESETTINGS_EMULATION;
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
				platform->getFileSystem()->getPath(filepath, EmuSettings.loadDevice, EmuSettings.screenshotsFolder, Memory.ROMFilename);
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

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
 * What screens built outside this file need of the menu (see menu.h)
 ***************************************************************************/
GuiWindow& MenuMainWindow()
{
	return menu->mainWindow;
}

bool MenuUpdateGui()
{
	return UpdateGui();
}

bool MenuConfirm(const char * title, const char * msg, const char * yes, const char * no)
{
	return WindowPrompt(title, msg, yes, no) == 1;
}

/****************************************************************************
 * MenuSettingsVideo
 ***************************************************************************/

static void ScreenZoomWindowUpdate(void * ptr, float h, float v)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->getState() == STATE::CLICKED)
	{
		EmuSettings.videoZoomHor += h;
		EmuSettings.videoZoomVert += v;

		char zoom[10];
		sprintf(zoom, "%.2f%%", EmuSettings.videoZoomHor*100);
		settingText->setText(zoom);
		sprintf(zoom, "%.2f%%", EmuSettings.videoZoomVert*100);
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
	trigLeft.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_LEFT);

	GuiTrigger trigRight;
	trigRight.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_RIGHT);

	GuiTrigger trigUp;
	trigUp.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_UP);

	GuiTrigger trigDown;
	trigDown.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_DOWN);

	MenuArrowImage arrowLeftImg(SHAPE::TRIANGLE_LEFT);
	GuiButton arrowLeftBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowLeftBtn.setSoundOver(nullptr);
	arrowLeftBtn.setSoundClick(nullptr);
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowLeftBtn.setPosition(50, 0);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(ScreenZoomWindowLeftClick);

	MenuArrowImage arrowRightImg(SHAPE::TRIANGLE_RIGHT);
	GuiButton arrowRightBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowRightBtn.setSoundOver(nullptr);
	arrowRightBtn.setSoundClick(nullptr);
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowRightBtn.setPosition(164, 0);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(ScreenZoomWindowRightClick);

	MenuArrowImage arrowUpImg(SHAPE::TRIANGLE_UP);
	GuiButton arrowUpBtn(MENU_ARROW_HIT_LONG, MENU_ARROW_HIT_SHORT);
	arrowUpBtn.setSoundOver(nullptr);
	arrowUpBtn.setSoundClick(nullptr);
	arrowUpBtn.setImage(&arrowUpImg);
	arrowUpBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpBtn.setPosition(-76, -27);
	arrowUpBtn.setTrigger(trigA);
	arrowUpBtn.setTrigger(&trigUp);
	arrowUpBtn.setSelectable(false);
	arrowUpBtn.setUpdateCallback(ScreenZoomWindowUpClick);

	MenuArrowImage arrowDownImg(SHAPE::TRIANGLE_DOWN);
	GuiButton arrowDownBtn(MENU_ARROW_HIT_LONG, MENU_ARROW_HIT_SHORT);
	arrowDownBtn.setSoundOver(nullptr);
	arrowDownBtn.setSoundClick(nullptr);
	arrowDownBtn.setImage(&arrowDownImg);
	arrowDownBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownBtn.setPosition(-76, 27);
	arrowDownBtn.setTrigger(trigA);
	arrowDownBtn.setTrigger(&trigDown);
	arrowDownBtn.setSelectable(false);
	arrowDownBtn.setUpdateCallback(ScreenZoomWindowDownClick);

	MenuScreenShape screenPositionImg;
	screenPositionImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	screenPositionImg.setPosition(0, 0);

	settingText = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	settingText2 = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	char zoom[10];
	sprintf(zoom, "%.2f%%", EmuSettings.videoZoomHor*100);
	settingText->setText(zoom);
	settingText->setPosition(108, 0);
	sprintf(zoom, "%.2f%%", EmuSettings.videoZoomVert*100);
	settingText2->setText(zoom);
	settingText2->setPosition(-76, 0);

	float currentZoomHor = EmuSettings.videoZoomHor;
	float currentZoomVert = EmuSettings.videoZoomVert;

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
		EmuSettings.videoZoomHor = currentZoomHor;
		EmuSettings.videoZoomVert = currentZoomVert;
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
		EmuSettings.videoXshift += x;
		EmuSettings.videoYshift += y;

		if(!(EmuSettings.videoXshift > -50 && EmuSettings.videoXshift < 50))
			EmuSettings.videoXshift = 0;
		if(!(EmuSettings.videoYshift > -50 && EmuSettings.videoYshift < 50))
			EmuSettings.videoYshift = 0;

		char shift[10];
		sprintf(shift, "%hd, %hd", EmuSettings.videoXshift, EmuSettings.videoYshift);
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
	trigLeft.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_LEFT);

	GuiTrigger trigRight;
	trigRight.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_RIGHT);

	GuiTrigger trigUp;
	trigUp.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_UP);

	GuiTrigger trigDown;
	trigDown.setButtonOnlyInFocusTrigger(-1, INPUT_BTN_DOWN);

	MenuArrowImage arrowLeftImg(SHAPE::TRIANGLE_LEFT);
	GuiButton arrowLeftBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowLeftBtn.setSoundOver(nullptr);
	arrowLeftBtn.setSoundClick(nullptr);
	arrowLeftBtn.setImage(&arrowLeftImg);
	arrowLeftBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	arrowLeftBtn.setTrigger(trigA);
	arrowLeftBtn.setTrigger(&trigLeft);
	arrowLeftBtn.setSelectable(false);
	arrowLeftBtn.setUpdateCallback(ScreenPositionWindowLeftClick);

	MenuArrowImage arrowRightImg(SHAPE::TRIANGLE_RIGHT);
	GuiButton arrowRightBtn(MENU_ARROW_HIT_SHORT, MENU_ARROW_HIT_LONG);
	arrowRightBtn.setSoundOver(nullptr);
	arrowRightBtn.setSoundClick(nullptr);
	arrowRightBtn.setImage(&arrowRightImg);
	arrowRightBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
	arrowRightBtn.setTrigger(trigA);
	arrowRightBtn.setTrigger(&trigRight);
	arrowRightBtn.setSelectable(false);
	arrowRightBtn.setUpdateCallback(ScreenPositionWindowRightClick);

	MenuArrowImage arrowUpImg(SHAPE::TRIANGLE_UP);
	GuiButton arrowUpBtn(MENU_ARROW_HIT_LONG, MENU_ARROW_HIT_SHORT);
	arrowUpBtn.setSoundOver(nullptr);
	arrowUpBtn.setSoundClick(nullptr);
	arrowUpBtn.setImage(&arrowUpImg);
	arrowUpBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	arrowUpBtn.setTrigger(trigA);
	arrowUpBtn.setTrigger(&trigUp);
	arrowUpBtn.setSelectable(false);
	arrowUpBtn.setUpdateCallback(ScreenPositionWindowUpClick);

	MenuArrowImage arrowDownImg(SHAPE::TRIANGLE_DOWN);
	GuiButton arrowDownBtn(MENU_ARROW_HIT_LONG, MENU_ARROW_HIT_SHORT);
	arrowDownBtn.setSoundOver(nullptr);
	arrowDownBtn.setSoundClick(nullptr);
	arrowDownBtn.setImage(&arrowDownImg);
	arrowDownBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
	arrowDownBtn.setTrigger(trigA);
	arrowDownBtn.setTrigger(&trigDown);
	arrowDownBtn.setSelectable(false);
	arrowDownBtn.setUpdateCallback(ScreenPositionWindowDownClick);

	MenuScreenShape screenPositionImg;
	screenPositionImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	settingText = new GuiText(nullptr, 20, (PixelColor){0, 0, 0, 255});
	char shift[10];
	sprintf(shift, "%i, %i", EmuSettings.videoXshift, EmuSettings.videoYshift);
	settingText->setText(shift);

	int currentX = EmuSettings.videoXshift;
	int currentY = EmuSettings.videoYshift;

	w->append(&arrowLeftBtn);
	w->append(&arrowRightBtn);
	w->append(&arrowUpBtn);
	w->append(&arrowDownBtn);
	w->append(&screenPositionImg);
	w->append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		// undo changes
		EmuSettings.videoXshift = currentX;
		EmuSettings.videoYshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static int MenuSettingsGeneralMappings()
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

	UiScope ui;
	GuiLabel& title = ui.label("General Mappings").fontSize(UI_FONT_TITLE - 2).textColor(MenuChromeText());
	GuiLabel& hint = ui.label("Turbo mode, the menu toggle and ABXY on the right stick").fontSize(UI_FONT_SMALL + 2).textColor(MenuChromeTextMuted());
	hint.setMaxWidth(176);
	hint.setScroll(SCROLL::HORIZONTAL);
	GuiLayout& titleBlock = ui.column().gap(4).size(176, 0).add(title).add(hint);
	GuiLayout& header = ui.row().size(platform->getVideo()->getScreenWidth(), MENU_BAR_TOP_H).pad(MENU_PAGE_MARGIN, 0)
		.align(LAYOUT_ALIGN::CENTRE).add(titleBlock);
	header.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	header.setPosition(0, 0);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
	optionBrowser.setCol2Position(240);
	optionBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	w.append(&backBtn);
	menu->mainWindow.appendWithAutoRemove(&optionBrowser);
	menu->mainWindow.appendWithAutoRemove(&w);
	menu->mainWindow.appendWithAutoRemove(&header);

	while(selection == MENU_NONE)
	{
		if(!UpdateGui()) return MENU_EXIT;

		ret = optionBrowser.getClickedOption();

		switch (ret)
		{
			case 0:
				EmuSettings.turboModeEnabled = !EmuSettings.turboModeEnabled;
				break;

			case 1:
				EmuSettings.turboModeButton++;
				if (EmuSettings.turboModeButton > 14)
					EmuSettings.turboModeButton = 0;
				break;

			case 2:
				EmuSettings.gamepadMenuToggle++;
				if (EmuSettings.gamepadMenuToggle >= GAMEPAD_MENU_TOGGLE_LENGTH)
					EmuSettings.gamepadMenuToggle = GAMEPAD_MENU_TOGGLE_DEFAULT;
				break;

			case 3:
				EmuSettings.mapAbxyRightStick = !EmuSettings.mapAbxyRightStick;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			sprintf (options.value[0], "%s", EmuSettings.turboModeEnabled ? "On" : "Off");
			
			switch(EmuSettings.turboModeButton)
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

			switch(EmuSettings.gamepadMenuToggle)
			{
				case GAMEPAD_MENU_TOGGLE_DEFAULT:
					sprintf (options.value[2], "Default (All Enabled)"); break;
				case GAMEPAD_MENU_TOGGLE_HOME_RIGHTSTICK:
					sprintf (options.value[2], "Home / Right Stick"); break;
				case GAMEPAD_MENU_TOGGLE_LRSTART_12PLUS:
					sprintf (options.value[2], "L+R+Start / 1+2+Plus"); break;
			}

			sprintf (options.value[3], "%s", EmuSettings.mapAbxyRightStick ? "On" : "Off");

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
#if defined(HW_RVL) || defined(HW_DOL)
	sprintf(options.name[i++], "Output Mode");
#else
	options.name[i++][0] = 0; // Output Mode is GameCube / Wii only
#endif
	sprintf(options.name[i++], "Aspect Ratio Correction");
	sprintf(options.name[i++], "Bilinear Filtering");
#if defined(HW_RVL) || defined(HW_DOL)
	sprintf(options.name[i++], "Hardware Softening");
#else
	options.name[i++][0] = 0; // Hardware Softening is GameCube / Wii only
#endif
#if defined(HW_RVL) || defined(HW_DOL) || defined(__WIIU__)
	sprintf(options.name[i++], "Upscaling");
#else
	options.name[i++][0] = 0; // upscaling filters not available on this platform
#endif
	sprintf(options.name[i++], "Scanline Overlay");
	sprintf(options.name[i++], "Screen Zoom");
	sprintf(options.name[i++], "Screen Position");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Game Settings - Video", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
				EmuSettings.videoMode++;
				if(EmuSettings.videoMode >= VIDEOMODE_LENGTH)
					EmuSettings.videoMode = VIDEOMODE_AUTO;
				break;

			case 1:
				EmuSettings.videoAspectRatioCorrection++;
				if(EmuSettings.videoAspectRatioCorrection >= VIDEO_ASPECT_RATIO_CORRECTION_LENGTH)
					EmuSettings.videoAspectRatioCorrection = VIDEO_ASPECT_RATIO_CORRECTION_NONE;
				break;

			case 2:
				EmuSettings.videoBilinearFilter = !EmuSettings.videoBilinearFilter;
				break;

			case 3:
				EmuSettings.videoHardwareSoften++;
				if(EmuSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_LENGTH)
					EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_OFF;
				break;

#if defined(HW_RVL) || defined(HW_DOL) || defined(__WIIU__)
			case 4:
				EmuSettings.videoUpscalingFilter++;
				if (EmuSettings.videoUpscalingFilter >= NUM_UPSCALE_FILTERS)
					EmuSettings.videoUpscalingFilter = UPSCALE_NONE;
				break;
#endif

			case 5:
				EmuSettings.videoScanlines = !EmuSettings.videoScanlines;
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

			switch(EmuSettings.videoMode)
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

			switch(EmuSettings.videoAspectRatioCorrection)
			{
				case VIDEO_ASPECT_RATIO_CORRECTION_NONE:
					sprintf (options.value[1], "None"); break;
				case VIDEO_ASPECT_RATIO_CORRECTION_16_9:
					sprintf (options.value[1], "16:9"); break;
				case VIDEO_ASPECT_RATIO_CORRECTION_16_9_FIXED:
					sprintf (options.value[1], "16:9 (Fixed Pixel Ratio)"); break;
			}

#ifdef __WIIU__
			if (EmuSettings.videoUpscalingFilter == UPSCALE_SHARP_BILINEAR)
				sprintf (options.value[2], "N/A"); // Sharp Bilinear does its own filtering
			else
#endif
			sprintf (options.value[2], "%s", EmuSettings.videoBilinearFilter ? "On" : "Off");

			switch(EmuSettings.videoHardwareSoften)
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

#if defined(HW_RVL) || defined(HW_DOL) || defined(__WIIU__)
			sprintf (options.value[4], "%s", GetUpscaleFilterName(EmuSettings.videoUpscalingFilter));
#endif
			sprintf (options.value[5], "%s", EmuSettings.videoScanlines ? "On" : "Off");
			sprintf (options.value[6], "%.2f%%, %.2f%%", EmuSettings.videoZoomHor*100, EmuSettings.videoZoomVert*100);
			sprintf (options.value[7], "%d, %d", EmuSettings.videoXshift, EmuSettings.videoYshift);

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

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::NORMAL);
	GuiButton backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(50, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
				EmuSettings.hiResolution = !EmuSettings.hiResolution;
				break;

			case 1:
				EmuSettings.spriteLimit = !EmuSettings.spriteLimit;
				break;

			case 2:
				#ifndef HW_DOL
				EmuSettings.sfxOverclock++;
				if (EmuSettings.sfxOverclock >= SFXOVERCLOCK_LENGTH) {
					EmuSettings.sfxOverclock = SFXOVERCLOCK_OFF;
				}
				#else
				EmuSettings.sfxOverclock++;
				if (EmuSettings.sfxOverclock > SFXOVERCLOCK_60MHZ) {
					EmuSettings.sfxOverclock = SFXOVERCLOCK_OFF;
				}
				#endif
				switch(EmuSettings.sfxOverclock)
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
				EmuSettings.interpolation++;
				if (EmuSettings.interpolation > 4) {
					EmuSettings.interpolation = 0;
				}
				switch(EmuSettings.interpolation)
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
				EmuSettings.muteAudio = !EmuSettings.muteAudio;
				break;

			case 5:
				EmuSettings.frameSkip = !EmuSettings.frameSkip;
				break;

			case 6:
				EmuSettings.crosshair = !EmuSettings.crosshair;
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

			sprintf (options.value[0], "%s", EmuSettings.hiResolution ? "On" : "Off");
			sprintf (options.value[1], "%s", EmuSettings.spriteLimit ? "On" : "Off");
			
			switch(EmuSettings.sfxOverclock)
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

			switch(EmuSettings.interpolation)
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

			sprintf (options.value[4], "%s", EmuSettings.muteAudio ? "On" : "Off");
			sprintf (options.value[5], "%s", EmuSettings.frameSkip ? "On" : "Off");
			sprintf (options.value[6], "%s", EmuSettings.crosshair ? "On" : "Off");
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

	MenuIconData iconFile(icon_settings_file_png);
	MenuIconData iconMenu(icon_settings_menu_png);
	MenuIconData iconNetwork(icon_settings_network_png);
	MenuIconData iconCredits(icon_credits_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText savingBtnTxt1("Saving", 22, (PixelColor){0, 0, 0, 255});
	GuiText savingBtnTxt2("&", 18, (PixelColor){0, 0, 0, 255});
	GuiText savingBtnTxt3("Loading", 22, (PixelColor){0, 0, 0, 255});
	savingBtnTxt1.setPosition(0, -20);
	savingBtnTxt3.setPosition(0, +20);
	MenuButtonShape savingBtnImg(MenuButtonShape::LARGE);
	GuiImage fileBtnIcon(&iconFile);
	GuiButton savingBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	savingBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	savingBtn.setPosition(-125, 120);
	savingBtn.setLabel(&savingBtnTxt1, 0);
	savingBtn.setLabel(&savingBtnTxt2, 1);
	savingBtn.setLabel(&savingBtnTxt3, 2);
	savingBtn.setImage(&savingBtnImg);
	savingBtn.setIcon(&fileBtnIcon);
	savingBtn.setTrigger(trigA);
	savingBtn.setEffectGrow();

	GuiText menuBtnTxt("Menu", 22, (PixelColor){0, 0, 0, 255});
	menuBtnTxt.setWrap(true, MENU_BTN_LARGE_W-30);
	MenuButtonShape menuBtnImg(MenuButtonShape::LARGE);
	GuiImage menuBtnIcon(&iconMenu);
	GuiButton menuBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	menuBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	menuBtn.setPosition(125, 120);
	menuBtn.setLabel(&menuBtnTxt);
	menuBtn.setImage(&menuBtnImg);
	menuBtn.setIcon(&menuBtnIcon);
	menuBtn.setTrigger(trigA);
	menuBtn.setEffectGrow();

	GuiText networkBtnTxt("Network Share", 22, (PixelColor){0, 0, 0, 255});
	networkBtnTxt.setWrap(true, MENU_BTN_LARGE_W-30);
	MenuButtonShape networkBtnImg(MenuButtonShape::LARGE);
	GuiImage networkBtnIcon(&iconNetwork);
	GuiButton networkBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	networkBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	networkBtn.setPosition(-125, 250);
	networkBtn.setLabel(&networkBtnTxt);
	networkBtn.setImage(&networkBtnImg);
	networkBtn.setIcon(&networkBtnIcon);
	networkBtn.setTrigger(trigA);
	networkBtn.setEffectGrow();

	GuiText creditsBtnTxt("Credits", 22, (PixelColor){0, 0, 0, 255});
	creditsBtnTxt.setWrap(true, MENU_BTN_LARGE_W-20);
	MenuButtonShape creditsBtnImg(MenuButtonShape::LARGE);
	GuiImage creditsBtnIcon(&iconCredits);
	GuiButton creditsBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	creditsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	creditsBtn.setPosition(125, 250);
	creditsBtn.setLabel(&creditsBtnTxt);
	creditsBtn.setImage(&creditsBtnImg);
	creditsBtn.setIcon(&creditsBtnIcon);
	creditsBtn.setTrigger(trigA);
	creditsBtn.setEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::LONG);
	GuiButton backBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiText resetBtnTxt("Reset Settings", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape resetBtnImg(MenuButtonShape::LONG);
	GuiButton resetBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	resetBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	resetBtn.setPosition(-90, -35);
	resetBtn.setLabel(&resetBtnTxt);
	resetBtn.setImage(&resetBtnImg);
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
				RunWithGuiUpdates(AutoDetectDevicesTask);
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

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::LONG);
	GuiButton backBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
				EmuSettings.loadDevice = getNextLoadDevice(EmuSettings.loadDevice);
				break;

			case 1:
				EmuSettings.saveDevice = getNextSaveDevice(EmuSettings.saveDevice);
				break;

			case 2:
				OnScreenKeyboard(EmuSettings.loadFolder, MAXPATHLEN);
				break;

			case 3:
				OnScreenKeyboard(EmuSettings.saveFolder, MAXPATHLEN);
				break;

			case 4:
				OnScreenKeyboard(EmuSettings.cheatFolder, MAXPATHLEN);
				break;
				
			case 5:
				OnScreenKeyboard(EmuSettings.screenshotsFolder, MAXPATHLEN);
				break;
				
			case 6:
				OnScreenKeyboard(EmuSettings.coverFolder, MAXPATHLEN);
				break;

			case 7:
				OnScreenKeyboard(EmuSettings.artworkFolder, MAXPATHLEN);
				break;
				
			case 8:
				EmuSettings.autoLoad++;
				if (EmuSettings.autoLoad > AUTOLOAD_STATE)
					EmuSettings.autoLoad = AUTOLOAD_OFF;
				break;

			case 9:
				EmuSettings.autoSave++;
				if (EmuSettings.autoSave > AUTOSAVE_BOTH)
					EmuSettings.autoSave = AUTOSAVE_OFF;
				break;

			case 10:
				EmuSettings.appendAuto = !EmuSettings.appendAuto;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			if (EmuSettings.loadDevice == DEVICE_AUTO) sprintf (options.value[0],"Auto Detect");
			else if (EmuSettings.loadDevice == DEVICE_SD) sprintf (options.value[0],"SD");
			else if (EmuSettings.loadDevice == DEVICE_USB) sprintf (options.value[0],"USB");
			else if (EmuSettings.loadDevice == DEVICE_DVD) sprintf (options.value[0],"DVD");
			else if (EmuSettings.loadDevice == DEVICE_SMB) sprintf (options.value[0],"Network Share");
			else if (EmuSettings.loadDevice == DEVICE_SD_SLOTA) sprintf (options.value[0],"SD Gecko Slot A");
			else if (EmuSettings.loadDevice == DEVICE_SD_SLOTB) sprintf (options.value[0],"SD Gecko Slot B");
			else if (EmuSettings.loadDevice == DEVICE_SD_PORT2) sprintf (options.value[0],"SD in SP2");
			else if (EmuSettings.loadDevice == DEVICE_SD_GCLOADER) sprintf (options.value[0],"GC Loader");

			if (EmuSettings.saveDevice == DEVICE_AUTO) sprintf (options.value[1],"Auto Detect");
			else if (EmuSettings.saveDevice == DEVICE_SD) sprintf (options.value[1],"SD");
			else if (EmuSettings.saveDevice == DEVICE_USB) sprintf (options.value[1],"USB");
			else if (EmuSettings.saveDevice == DEVICE_SMB) sprintf (options.value[1],"Network Share");
			else if (EmuSettings.saveDevice == DEVICE_SD_SLOTA) sprintf (options.value[1],"SD Gecko Slot A");
			else if (EmuSettings.saveDevice == DEVICE_SD_SLOTB) sprintf (options.value[1],"SD Gecko Slot B");
			else if (EmuSettings.saveDevice == DEVICE_SD_PORT2) sprintf (options.value[1],"SD in SP2");
			else if (EmuSettings.saveDevice == DEVICE_SD_GCLOADER) sprintf (options.value[1],"GC Loader");

			snprintf (options.value[2], 35, "%s", EmuSettings.loadFolder);
			snprintf (options.value[3], 35, "%s", EmuSettings.saveFolder);
			snprintf (options.value[4], 35, "%s", EmuSettings.cheatFolder);
			snprintf (options.value[5], 35, "%s", EmuSettings.screenshotsFolder);
			snprintf (options.value[6], 35, "%s", EmuSettings.coverFolder);
			snprintf (options.value[7], 35, "%s", EmuSettings.artworkFolder);

			if (EmuSettings.autoLoad == AUTOLOAD_OFF) sprintf (options.value[8],"Off");
			else if (EmuSettings.autoLoad == AUTOLOAD_SRAM) sprintf (options.value[8],"SRAM");
			else if (EmuSettings.autoLoad == AUTOLOAD_STATE) sprintf (options.value[8],"State");

			if (EmuSettings.autoSave == AUTOSAVE_OFF) sprintf (options.value[9],"Off");
			else if (EmuSettings.autoSave == AUTOSAVE_SRAM) sprintf (options.value[9],"SRAM");
			else if (EmuSettings.autoSave == AUTOSAVE_STATE) sprintf (options.value[9],"State");
			else if (EmuSettings.autoSave == AUTOSAVE_BOTH) sprintf (options.value[9],"Both");

			if (!EmuSettings.appendAuto) sprintf (options.value[10], "Off");
			else sprintf (options.value[10], "On");

			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			selection = MENU_SETTINGS;
			RunWithGuiUpdates(AutoDetectDevicesTask);
		}
	}
	return selection;
}

static bool LoadLanguage()
{
	const uint8_t *buffer;
	size_t size;

	switch(EmuSettings.language)
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

#ifndef HW_DOL
static void GetFontFilePath(char *filepath, size_t size, const char *fontName)
{
	snprintf(filepath, size, "%s/%s", appPath, fontName);
#ifdef __WIIU__
	struct stat st;
	if(appPath[0] != 0 && stat(filepath, &st) == 0)
		return;

	static const char * const folders[] = { "wiiu/apps/" APPFOLDER, APPFOLDER };

	for(const char * folder : folders)
	{
		char candidate[MAXPATHLEN];
		platform->getFileSystem()->getPath(candidate, DEVICE_SD, folder, fontName);

		if(stat(candidate, &st) == 0)
		{
			snprintf(filepath, size, "%s", candidate);
			return;
		}
	}
#endif
}
#endif

void ChangeLanguage() {
	if(currentLanguage == EmuSettings.language) {
		return;
	}

	if(EmuSettings.language == LANG_JAPANESE || EmuSettings.language == LANG_KOREAN || EmuSettings.language == LANG_SIMP_CHINESE) {
#ifndef HW_DOL
		char filepath[MAXPATHLEN];

		switch(EmuSettings.language) {
			case LANG_KOREAN:
				GetFontFilePath(filepath, sizeof(filepath), "ko.ttf");
				break;
			case LANG_JAPANESE:
				GetFontFilePath(filepath, sizeof(filepath), "jp.ttf");
				break;
			case LANG_SIMP_CHINESE:
				GetFontFilePath(filepath, sizeof(filepath), "zh.ttf");
				break;
		}

		size_t fileSize = LoadFont(filepath);

		if(fileSize > 0) {
			if(fontSystem) delete fontSystem;
			fontSystem = new GuiTextRenderer(ext_font_ttf, fileSize, platform->getVideo()->getGlyphRenderer(), platform->getVideo()->getUIScale());
		}
		else {
			EmuSettings.language = (currentLanguage >= 0) ? currentLanguage : LANG_ENGLISH;
		}
#else
	EmuSettings.language = (currentLanguage >= 0) ? currentLanguage : LANG_ENGLISH;
	ErrorPrompt("Unsupported language!");
#endif
	}
#ifndef HW_DOL
	else {
		if(ext_font_ttf != nullptr) {
			if(fontSystem) delete fontSystem;
			extmem_free(ext_font_ttf);
			ext_font_ttf = nullptr;
			fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size, platform->getVideo()->getGlyphRenderer(), platform->getVideo()->getUIScale());
		}
	}
#endif
	ResetText();
	currentLanguage = EmuSettings.language;
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
	currentLanguage = EmuSettings.language;

#if defined(HW_RVL) || defined(HW_DOL)
	sprintf(options.name[i++], "Exit Action");
#else
	options.name[i++][0] = 0; // Exit Action not available on this platform
#endif
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

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::LONG);
	GuiButton backBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
				EmuSettings.exitAction++;
				#ifdef HW_RVL
				if(EmuSettings.exitAction >= EXITACTION_WII_LENGTH)
					EmuSettings.exitAction = EXITACTION_WII_AUTO;
				#elif HW_DOL
				if(EmuSettings.exitAction >= EXITACTION_GC_LENGTH)
					EmuSettings.exitAction = EXITACTION_GC_RETURN_TO_LOADER;
				#endif
				break;
			case 1:
				EmuSettings.wiimoteOrientation++;
				if(EmuSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_LENGTH)
					EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_VERTICAL;
				platform->getInput()->setWiimoteOrientation(EmuSettings.wiimoteOrientation);
				break;
			case 2:
				EmuSettings.musicVolume += 10;
				if(EmuSettings.musicVolume > 100)
					EmuSettings.musicVolume = 0;
				GuiSound::setDefaultVolume(VOLUME_TYPE::MUSIC, EmuSettings.musicVolume);
				break;
			case 3:
				EmuSettings.sfxVolume += 10;
				if(EmuSettings.sfxVolume > 100)
					EmuSettings.sfxVolume = 0;
				GuiSound::setDefaultVolume(VOLUME_TYPE::SFX, EmuSettings.sfxVolume);
				break;
			case 4:
				EmuSettings.rumble = !EmuSettings.rumble;
				platform->getInput()->setRumbleEnabled(EmuSettings.rumble);
				break;
			case 5:
				EmuSettings.language++;
				
				if(EmuSettings.language == LANG_TRAD_CHINESE) // skip (not supported)
					EmuSettings.language = LANG_KOREAN;
				else if(EmuSettings.language >= LANG_LENGTH)
					EmuSettings.language = LANG_JAPANESE;
				break;
			case 6:
				EmuSettings.previewImage++;
				if(EmuSettings.previewImage >= PREVIEWIMAGE_LENGTH)
					EmuSettings.previewImage = PREVIEWIMAGE_SCREENSHOT;
				break;
			case 7:
				EmuSettings.hideSramSaving = !EmuSettings.hideSramSaving;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			#ifdef HW_RVL
			if (EmuSettings.exitAction == EXITACTION_WII_RETURN_TO_MENU)
				sprintf (options.value[0], "Return to Wii Menu");
			else if (EmuSettings.exitAction == EXITACTION_WII_POWER_OFF)
				sprintf (options.value[0], "Power Off Wii");
			else if (EmuSettings.exitAction == EXITACTION_WII_RETURN_TO_LOADER)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Auto");
			#elif HW_DOL // GameCube
			if (EmuSettings.exitAction == EXITACTION_GC_RETURN_TO_LOADER)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Reboot");

			options.name[1][0] = 0; // Wiimote
			options.name[2][0] = 0; // Music
			options.name[3][0] = 0; // Sound Effects
			#endif

			if (EmuSettings.wiimoteOrientation == WIIMOTE_ORIENTATION_HORIZONTAL)
				sprintf (options.value[1], "Horizontal");
			else
				sprintf (options.value[1], "Vertical");

			if(EmuSettings.musicVolume > 0)
				sprintf(options.value[2], "%d%%", EmuSettings.musicVolume);
			else
				sprintf(options.value[2], "Mute");

			if(EmuSettings.sfxVolume > 0)
				sprintf(options.value[3], "%d%%", EmuSettings.sfxVolume);
			else
				sprintf(options.value[3], "Mute");

			if (EmuSettings.rumble)
				sprintf (options.value[4], "Enabled");
			else
				sprintf (options.value[4], "Disabled");
			
			if (EmuSettings.hideSramSaving)
				sprintf (options.value[7], "On");
			else
				sprintf (options.value[7], "Off");

			switch(EmuSettings.language)
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
			
			switch(EmuSettings.previewImage)
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
	sprintf(options.name[i++], "IP");
	sprintf(options.name[i++], "Name");
	sprintf(options.name[i++], "Username");
	sprintf(options.name[i++], "Password");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Network Share", 26, (PixelColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape backBtnImg(MenuButtonShape::LONG);
	GuiButton backBtn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(90, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setTrigger(trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser optionBrowser(MENU_OPTIONS_W, MENU_OPTIONS_H, &options);
	optionBrowser.setPosition(0, MENU_LIST_Y);
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
				OnScreenKeyboard(EmuSettings.smbShare.host, sizeof(EmuSettings.smbShare.host));
				break;

			case 1:
				OnScreenKeyboard(EmuSettings.smbShare.share, sizeof(EmuSettings.smbShare.share));
				break;

			case 2:
				OnScreenKeyboard(EmuSettings.smbShare.user, sizeof(EmuSettings.smbShare.user));
				break;

			case 3:
				OnScreenKeyboard(EmuSettings.smbShare.password, sizeof(EmuSettings.smbShare.password));
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			snprintf (options.value[0], 25, "%s", EmuSettings.smbShare.host);
			snprintf (options.value[1], 19, "%s", EmuSettings.smbShare.share);
			snprintf (options.value[2], 19, "%s", EmuSettings.smbShare.user);
			snprintf (options.value[3], 19, "%s", EmuSettings.smbShare.password);
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

// Context passed to BlurredOverlayPixelSource() via fillTexture()'s userdata.
// Holds everything needed to compute the final (post vertical-blur, post
// overlay) color of a single destination pixel, without ever having a
// full-screen plain RGBA8 buffer materialized anywhere.
struct BlurredGameTextureContext {
	const uint8_t * scaledImg; // horizontally-blurred crop, cropWidth x cropHeight, RGBA8
	int cropWidth;
	int cropHeight;
	int drawX;
	int drawY;
	int blurAmount;
	int div;
	PixelColor overlayColor;
	uint8_t bgR, bgG, bgB, bgA; // precomputed flat background (black + overlay)
};

// ImageRenderer::PixelSourceFn implementation: does the vertical blur pass
// and overlay blend for destination pixel (x,y), sourcing from the
// horizontally-blurred crop buffer, or returns the flat background color
// for anything outside the crop/screen bounds.
static void BlurredOverlayPixelSource(int x, int y, PixelColor * outColor, void * userdata)
{
	BlurredGameTextureContext * ctx = (BlurredGameTextureContext *)userdata;

	if (x >= ctx->drawX && x < ctx->drawX + ctx->cropWidth &&
		y >= ctx->drawY && y < ctx->drawY + ctx->cropHeight) {

		int cx = x - ctx->drawX;
		int cy = y - ctx->drawY;

		int sumR = 0, sumG = 0, sumB = 0;
		for (int k = -ctx->blurAmount; k <= ctx->blurAmount; ++k) {
			int ny = cy + k;
			if (ny < 0) ny = 0;
			if (ny >= ctx->cropHeight) ny = ctx->cropHeight - 1;

			int idx = (ny * ctx->cropWidth + cx) * 4;
			sumR += ctx->scaledImg[idx + 0];
			sumG += ctx->scaledImg[idx + 1];
			sumB += ctx->scaledImg[idx + 2];
		}

		uint8_t blurredR = sumR / ctx->div;
		uint8_t blurredG = sumG / ctx->div;
		uint8_t blurredB = sumB / ctx->div;

		int alphaIn = ctx->overlayColor.a;
		int invAlpha = 255 - alphaIn;

		outColor->r = (uint8_t)((blurredR * invAlpha + ctx->overlayColor.r * alphaIn) / 255);
		outColor->g = (uint8_t)((blurredG * invAlpha + ctx->overlayColor.g * alphaIn) / 255);
		outColor->b = (uint8_t)((blurredB * invAlpha + ctx->overlayColor.b * alphaIn) / 255);
		outColor->a = 255;
	} else {
		outColor->r = ctx->bgR;
		outColor->g = ctx->bgG;
		outColor->b = ctx->bgB;
		outColor->a = ctx->bgA;
	}
}

static void * CreateBlurredGameTexture() {
	if(gameScreenPng.size == 0) {
		return nullptr;
	}

	int blurAmount = 4; // blur amount
	PixelColor blurOverlayColor = (PixelColor){50, 50, 50, 160};

	int screenWidth = platform->getVideo()->getScreenWidth();
	int screenHeight = platform->getVideo()->getScreenHeight();

	int scaledWidth = (int)(gameScreenPng.width * gameScreenPng.scaleX);
	int scaledHeight = (int)(gameScreenPng.height * gameScreenPng.scaleY);

	// Failsafe for invalid scale metrics
	if (scaledWidth <= 0 || scaledHeight <= 0) {
		return nullptr;
	}

	// Calculate the absolute top-left starting pixel of the scaled image.
	int targetCenterX = (screenWidth / 2) + gameScreenPng.xoffset;
	int targetCenterY = (screenHeight / 2) + gameScreenPng.yoffset;

	int trueOffsetX = targetCenterX - (scaledWidth / 2);
	int trueOffsetY = targetCenterY - (scaledHeight / 2);

	// --- VIEWABLE AREA CLAMPING LOGIC ---
	// Determine where to start drawing on the screen bounds
	int drawX = trueOffsetX < 0 ? 0 : trueOffsetX;
	int drawY = trueOffsetY < 0 ? 0 : trueOffsetY;

	// Determine the max visible boundaries clipped to screen dimensions
	int endX = (trueOffsetX + scaledWidth > screenWidth) ? screenWidth : (trueOffsetX + scaledWidth);
	int endY = (trueOffsetY + scaledHeight > screenHeight) ? screenHeight : (trueOffsetY + scaledHeight);

	// Calculate the dimensions of the viewable (cropped) area
	int cropWidth = endX - drawX;
	int cropHeight = endY - drawY;

	// Failsafe if the image is pushed entirely off-screen
	if (cropWidth <= 0 || cropHeight <= 0) {
		return nullptr;
	}

	// Determine the starting offset within the theoretical scaled image
	int cropStartX = trueOffsetX < 0 ? -trueOffsetX : 0;
	int cropStartY = trueOffsetY < 0 ? -trueOffsetY : 0;

	// Allocate texture before scratch memory to avoid fragmentation
	void * texture = platform->getVideo()->getImageRenderer()->createTexture(screenWidth, screenHeight);
	if (!texture) {
		return nullptr;
	}

	uint8_t *src = DecodePNGToRGBA8(gameScreenPng.buffer, gameScreenPng.width, gameScreenPng.height);
	if(!src) {
		platform->getVideo()->getImageRenderer()->destroyTexture(texture);
		return nullptr;
	}

	// Allocate scratch space ONLY for the viewable cropped portion
	uint8_t *scaledImg = (uint8_t *)extmem_malloc(cropWidth * cropHeight * 4);
	uint8_t *rowBuf    = (uint8_t *)extmem_malloc(cropWidth * 4);

	if (!scaledImg || !rowBuf) {
		if (scaledImg) extmem_free(scaledImg);
		if (rowBuf) extmem_free(rowBuf);
		extmem_free(src);
		platform->getVideo()->getImageRenderer()->destroyTexture(texture);
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

	// The horizontally-blurred crop (scaledImg) and rowBuf are all we need from
	// here on.
	extmem_free(rowBuf);
	extmem_free(src);

	// Precalculate flat background color (Solid Black + Overlay) once, rather
	// than per background pixel.
	int alphaIn = blurOverlayColor.a;
	int invAlpha = 255 - alphaIn;

	BlurredGameTextureContext ctx;
	ctx.scaledImg = scaledImg;
	ctx.cropWidth = cropWidth;
	ctx.cropHeight = cropHeight;
	ctx.drawX = drawX;
	ctx.drawY = drawY;
	ctx.blurAmount = blurAmount;
	ctx.div = div;
	ctx.overlayColor = blurOverlayColor;
	ctx.bgR = (uint8_t)((0 * invAlpha + blurOverlayColor.r * alphaIn) / 255);
	ctx.bgG = (uint8_t)((0 * invAlpha + blurOverlayColor.g * alphaIn) / 255);
	ctx.bgB = (uint8_t)((0 * invAlpha + blurOverlayColor.b * alphaIn) / 255);
	ctx.bgA = 255;

	// Vertical blur + overlay compositing happens here, in platform-agnostic
	// pixel math (BlurredOverlayPixelSource). The platform driver's only job
	// is to place each resulting pixel into its own native texture layout -
	// this function still only ever holds the small cropped scratch buffer,
	// never a full-screen plain RGBA8 intermediate.
	platform->getVideo()->getImageRenderer()->fillTexture(texture, screenWidth, screenHeight, BlurredOverlayPixelSource, &ctx);

	extmem_free(scaledImg);
	return texture;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
static int FirstRunTask(void *) {
	LoadPrefs();
	AutoDetectDevices();
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
		#ifndef HW_DOL
		pointer[0] = new GuiImageData(player1_point_png);
		pointer[1] = new GuiImageData(player2_point_png);
		pointer[2] = new GuiImageData(player3_point_png);
		pointer[3] = new GuiImageData(player4_point_png);
		for(int i = 0; i < 4; i++)
			cursorImg[i].setImage(pointer[i]);
		#endif

		trigA = new GuiTrigger;
		trigA->setPrimaryTrigger();

		// the sounds every button plays; registered once, so pages don't build their own
		#ifndef HW_DOL
		static GuiSound buttonSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
		static GuiSound buttonSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
		GuiSound::setDefaultSounds(&buttonSoundOver, &buttonSoundClick);
		#endif
	}

	if(selection == MENU_GAME)
	{
		gameScreenTexture = CreateBlurredGameTexture();
		if(gameScreenTexture != nullptr) {
			gameScreenImg = new GuiImage((uint8_t *)gameScreenTexture, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
		}
	}

	if(gameScreenImg == nullptr) {
		gameScreenImg = new MenuBackgroundShape();
	}

	std::unique_ptr<Menu> menuInstance = std::make_unique<Menu>();
	menu = menuInstance.get();

	if(firstRun) {
		firstRun = false;

		if(RunOnWorkerThread(FirstRunTask)) {
			while(!IsWorkerThreadFinished()) {
				if(!UpdateGui()) break;
			}
		}

		#ifdef HW_RVL
		uint32_t ios = IOS_GetVersion();

		if(!SupportedIOS(ios))
			ErrorPrompt("The current IOS is unsupported. Functionality and/or stability may be adversely affected.");
		else if(!SaneIOS(ios))
			ErrorPrompt("The current IOS has been altered (fake-signed). Functionality and/or stability may be adversely affected.");
		#endif

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
				currentMenu = MenuGameSaves();
				break;
			case MENU_GAME_SAVE:
				currentMenu = MenuGameSaves();
				break;
			case MENU_GAME_DELETE:
				currentMenu = MenuGameSaves();
				break;	
			case MENU_GAMESETTINGS:
				currentMenu = MenuGameSettings();
				break;
			case MENU_GAMESETTINGS_MAPPINGS:
				currentMenu = MappingScreen();
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
			case MENU_GAMESETTINGS_MAPPINGS_GENERAL:
				currentMenu = MenuSettingsGeneralMappings();
				break;
			default: // unrecognized menu
				currentMenu = MenuGameSelection();
				break;
		}
		lastMenu = currentMenu;

		if(guiExiting)
			break;
	}

	CancelAction();

	if(gameScreenImg != nullptr) {
		menuInstance->mainWindow.remove(gameScreenImg);
		delete gameScreenImg;
		gameScreenImg = nullptr;
	}

	if(gameScreenTexture != nullptr) {
		platform->getVideo()->getImageRenderer()->destroyTexture(gameScreenTexture);
		gameScreenTexture = nullptr;
	}

	ClearScreenshot();

	// wait for keys to be depressed - but not when leaving the app
	while(isMenuRequested() && !platform->isExiting())
	{
		platform->getInput()->update();
		usleep(THREAD_SLEEP);
	}
	
	menu = nullptr;

	// Give back the shape meshes the menu cached (including the video backend's copies) before
	// the game runs
	menuInstance.reset();
	GuiMeshCache::flush();
}
