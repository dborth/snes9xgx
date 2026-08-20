/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/cond.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef HW_RVL
#include <di/di.h>
#include <wiiuse/wpad.h>
#endif

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
#include "videofilters.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "menu.h"

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
#endif

static GuiTrigger * trigA = NULL;

static GuiButton * btnLogo = NULL;
#ifdef HW_RVL
static GuiButton * batteryBtn[4];
#endif
static u8 * gameScreenTexture = NULL;
static GuiImage * gameScreenImg = NULL;
static GuiImage * bgTopImg = NULL;
static GuiImage * bgBottomImg = NULL;
static GuiSound * bgMusic = NULL;
static GuiSound * enterSound = NULL;
static GuiSound * exitSound = NULL;
static GuiWindow * mainWindow = NULL;
static GuiText * settingText = NULL;
static GuiText * settingText2 = NULL;
static int lastMenu = MENU_NONE;
static int mapMenuCtrl = 0;
static int mapMenuCtrlSNES = 0;

static lwp_t guithread = LWP_THREAD_NULL;
static lwp_t progressthread = LWP_THREAD_NULL;
static volatile bool guiHalt = true;
static volatile int showProgress = 0;

// GUI thread synchronization
static mutex_t guiMutex    = LWP_MUTEX_NULL;
static cond_t  guiHaltCond = LWP_COND_NULL; // GUI thread -> main: halted
static cond_t  guiWakeCond = LWP_COND_NULL; // main -> GUI thread: resume
static bool    guiHalted   = false;          // protected by guiMutex

// progress thread synchronization
static mutex_t progMutex      = LWP_MUTEX_NULL;
static cond_t  progActiveCond = LWP_COND_NULL; // main -> progress: work available
static cond_t  progIdleCond   = LWP_COND_NULL; // progress -> main: now idle
static bool    progIdle       = true;           // protected by progMutex
static bool showCredits = false;

static char progressTitle[101];
static char progressMsg[201];
static int progressDone = 0;
static int progressTotal = 0;
static bool buttonMappingCancelled = false;

u8 * bg_music;
u32 bg_music_size;

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	LWP_MutexLock(guiMutex);
	guiHalt = false;
	LWP_CondSignal(guiWakeCond);
	LWP_MutexUnlock(guiMutex);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void
HaltGui()
{
	LWP_MutexLock(guiMutex);
	guiHalt = true;
	while(!guiHalted)
		LWP_CondWait(guiHaltCond, guiMutex);
	LWP_MutexUnlock(guiMutex);
}

static bool LoadLanguage()
{
	char line[200];
	char *lastID = NULL;

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

	if(mainWindow)
	{
		HaltGui();
		mainWindow->resetText();
		ResumeGui();
	}
}

static int currentLanguage = -1;

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
			HaltGui();
			delete fontSystem;
			fontSystem = new GuiTextRenderer(ext_font_ttf, fileSize, glyphRenderer);
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
		if(ext_font_ttf != NULL) {
			HaltGui();
			delete fontSystem;
			extmem_free(ext_font_ttf);
			ext_font_ttf = NULL;
			fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size, glyphRenderer);
		}
	}
#endif
	ResetText();
	currentLanguage = GCSettings.language;
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
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

	GuiText titleTxt(title, 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);
	GuiText msgTxt(msg, 26, (GuiColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	msgTxt.setPosition(0,-20);
	msgTxt.setWrap(true, 430);

	GuiText btn1Txt(btn1Label, 22, (GuiColor){0, 0, 0, 255});
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

	GuiText btn2Txt(btn2Label, 22, (GuiColor){0, 0, 0, 255});
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
	HaltGui();
	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&promptWindow);
	mainWindow->changeFocus(&promptWindow);
	if(btn2Label)
	{
		btn1.resetState();
		btn2.setState(STATE::SELECTED);
	}
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.getState() == STATE::CLICKED)
			choice = 1;
		else if(btn2.getState() == STATE::CLICKED)
			choice = 0;
	}

	promptWindow.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 50);
	while(promptWindow.getEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->remove(&promptWindow);
	mainWindow->setState(STATE::DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

static void *
UpdateGUI (void *arg)
{
	int i;

	while(1)
	{
		// if halted, block here until ResumeGui wakes us; signal HaltGui we have stopped
		LWP_MutexLock(guiMutex);
		if(guiHalt)
		{
			guiHalted = true;
			LWP_CondBroadcast(guiHaltCond);
			while(guiHalt)
				LWP_CondWait(guiWakeCond, guiMutex);
			guiHalted = false;
		}
		LWP_MutexUnlock(guiMutex);

		UpdatePads();
		mainWindow->draw();

		if (mainWindow->getState() != STATE::DISABLED)
			mainWindow->drawTooltip();

		#ifdef HW_RVL
		i = 3;
		do
		{
			if(userInput[i]->getPadData().validPointer) {
				Menu_DrawImg(userInput[i]->getPadData().cursor_x-48, userInput[i]->getPadData().cursor_y-48, 96, 96, pointer[i]->getImage(), userInput[i]->getPadData().cursor_angle, 1, 1, 255);
			}
			DoRumble(i);
			--i;
		} while(i>=0);
		#endif

		Menu_Render();

		mainWindow->update(userInput[3]);
		mainWindow->update(userInput[2]);
		mainWindow->update(userInput[1]);
		mainWindow->update(userInput[0]);

		if(ExitRequested || ShutdownRequested)
		{
			for(i = 0; i <= 255; i += 15)
			{
				mainWindow->draw();
				Menu_DrawRectangle(0,0,screenwidth,screenheight,(GuiColor){0, 0, 0, (u8)i},1);
				Menu_Render();
			}
			ExitApp();
		}
		usleep(THREAD_SLEEP);
	}
	return NULL;
}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
static void
ProgressWindow(char *title, char *msg)
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

	GuiImageData progressbarOutline(progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	progressbarOutlineImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	progressbarOutlineImg.setPosition(25, 40);

	GuiImageData progressbarEmpty(progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	progressbarEmptyImg.setPosition(25, 40);
	progressbarEmptyImg.setTile(100);

	GuiImageData progressbar(progressbar_png);
	GuiImage progressbarImg(&progressbar);
	progressbarImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	progressbarImg.setPosition(25, 40);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	throbberImg.setPosition(0, 40);

	GuiText titleTxt(title, 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);
	GuiText msgTxt(msg, 26, (GuiColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	msgTxt.setPosition(0,80);

	promptWindow.append(&dialogBoxImg);
	promptWindow.append(&titleTxt);
	promptWindow.append(&msgTxt);

	if(showProgress == 1)
	{
		promptWindow.append(&progressbarEmptyImg);
		promptWindow.append(&progressbarImg);
		promptWindow.append(&progressbarOutlineImg);
	}
	else
	{
		promptWindow.append(&throbberImg);
	}

	// wait to see if progress flag changes soon
	int progsleep = 400000;

	while(progsleep > 0)
	{
		if(!showProgress)
			break;
		usleep(THREAD_SLEEP);
		progsleep -= THREAD_SLEEP;
	}

	if(!showProgress)
		return;

	HaltGui();
	STATE oldState = mainWindow->getState();
	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&promptWindow);
	mainWindow->changeFocus(&promptWindow);
	ResumeGui();

	float angle = 0;
	u32 count = 0;

	while(showProgress)
	{
		progsleep = 20000;

		while(progsleep > 0)
		{
			if(!showProgress)
				break;
			usleep(THREAD_SLEEP);
			progsleep -= THREAD_SLEEP;
		}

		if(showProgress == 1)
		{
			progressbarImg.setTile(100*progressDone/progressTotal);
		}
		else if(showProgress == 2)
		{
			if(count % 5 == 0)
			{
				angle+=45.0f;
				if(angle >= 360.0f)
					angle = 0;
				throbberImg.setAngle(angle);
			}
			++count;
		}
	}

	HaltGui();
	mainWindow->remove(&promptWindow);
	mainWindow->setState(oldState);
	ResumeGui();
}

static void * ProgressThread (void *arg)
{
	while(1)
	{
		LWP_MutexLock(progMutex);
		// sleep until ShowProgress/ShowAction signals there is work to do
		while(!showProgress)
			LWP_CondWait(progActiveCond, progMutex);

		progIdle = false;
		LWP_MutexUnlock(progMutex);

		ProgressWindow(progressTitle, progressMsg);

		LWP_MutexLock(progMutex);
		progIdle = true;
		LWP_CondBroadcast(progIdleCond); // wake CancelAction callers
		LWP_MutexUnlock(progMutex);
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	LWP_MutexInit(&guiMutex, false);
	LWP_CondInit(&guiHaltCond);
	LWP_CondInit(&guiWakeCond);

	LWP_MutexInit(&progMutex, false);
	LWP_CondInit(&progActiveCond);
	LWP_CondInit(&progIdleCond);

	LWP_CreateThread(&guithread, UpdateGUI, NULL, NULL, 24576, 70);
	LWP_CreateThread(&progressthread, ProgressThread, NULL, NULL, 8192, 80);
}

/****************************************************************************
 * CancelAction
 *
 * Signals the GUI progress window thread to halt, and waits for it to
 * finish. Prevents multiple progress window events from interfering /
 * overriding each other.
 ***************************************************************************/
void
CancelAction()
{
	LWP_MutexLock(progMutex);
	showProgress = 0;
	while(!progIdle)
		LWP_CondWait(progIdleCond, progMutex);
	LWP_MutexUnlock(progMutex);
}

/****************************************************************************
 * ShowProgress
 *
 * Updates the variables used by the progress window for drawing a progress
 * bar. Also resumes the progress window thread if it is suspended.
 ***************************************************************************/
void
ShowProgress (const char *msg, int done, int total)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
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
	LWP_CondSignal(progActiveCond);
	LWP_MutexUnlock(progMutex);
}

/****************************************************************************
 * ShowAction
 *
 * Shows that an action is underway. Also resumes the progress window thread
 * if it is suspended.
 ***************************************************************************/
void
ShowAction (const char *msg)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return;

	if(showProgress != 0)
		CancelAction(); // wait for previous progress window to finish

	LWP_MutexLock(progMutex);
	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressDone = 0;
	progressTotal = 0;
	LWP_CondSignal(progActiveCond);
	LWP_MutexUnlock(progMutex);
}

void ErrorPrompt(const char *msg)
{
	WindowPrompt("Error", msg, "OK", NULL);
}

int ErrorPromptRetry(const char *msg)
{
	return WindowPrompt("Error", msg, "Retry", "Cancel");
}

void InfoPrompt(const char *msg)
{
	WindowPrompt("Information", msg, "OK", NULL);
}

/****************************************************************************
 * AutoSave
 *
 * Automatically saves SRAM/state when returning from in-game to the menu
 ***************************************************************************/
void AutoSave()
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
static void OnScreenKeyboard(char * var, u32 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiText okBtnTxt("OK", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText cancelBtnTxt("Cancel", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&keyboard);
	mainWindow->changeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.getState() == STATE::CLICKED)
			save = 1;
		else if(cancelBtn.getState() == STATE::CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->remove(&keyboard);
	mainWindow->setState(STATE::DEFAULT);
	ResumeGui();
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

	GuiText titleTxt(title, 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	titleTxt.setPosition(0,14);

	GuiText okBtnTxt("OK", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText cancelBtnTxt("Cancel", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&promptWindow);
	mainWindow->append(w);
	mainWindow->changeFocus(w);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.getState() == STATE::CLICKED)
			save = 1;
		else if(cancelBtn.getState() == STATE::CLICKED)
			save = 0;
	}
	HaltGui();
	mainWindow->remove(&promptWindow);
	mainWindow->remove(w);
	mainWindow->setState(STATE::DEFAULT);
	ResumeGui();
	return save;
}

/****************************************************************************
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void WindowCredits(void * ptr)
{
	if(btnLogo->getState() != STATE::CLICKED && !showCredits)
		return;

	btnLogo->resetState();

	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(screenwidth,screenheight);
	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	creditsWindowBox.append(&creditsBoxImg);

	int numEntries = 24;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (GuiColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: https://github.com/dborth/snes9xgx", 20, (GuiColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=40;

	GuiText::setPresets(20, (GuiColor){0, 0, 0, 255}, 0, GUI_TEXT_JUSTIFY_LEFT | GUI_TEXT_ALIGN_TOP, ALIGN_H::LEFT, ALIGN_V::TOP);
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

	txt[i] = new GuiText(consoleDetails, 14, (GuiColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-90); i++;
	txt[i] = new GuiText(memoryFreeInfo, 14, (GuiColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-76); i++;
	txt[i] = new GuiText(controllerInfo, 14, (GuiColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(20,-52); i++;

	GuiText::setPresets(12, (GuiColor){0, 0, 0, 255}, 0, GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::BOTTOM);

	txt[i] = new GuiText("Snes9x - Copyright (c) Snes9x Team 1996 - 2023");
	txt[i]->setPosition(0,-44); i++;
	txt[i] = new GuiText("This software is open source and may be copied, distributed, or modified ");
	txt[i]->setPosition(0,-32); i++;
	txt[i] = new GuiText("under the terms of the GNU General Public License (GPL) Version 2.");
	txt[i]->setPosition(0,-20);

	for(i=0; i < numEntries; i++)
		creditsWindowBox.append(txt[i]);

	creditsWindow.append(&creditsWindowBox);

	auto buttonPressed = [&]()-> bool { return userInput[0]->getPadData().buttons_d || userInput[1]->getPadData().buttons_d ||
			   userInput[2]->getPadData().buttons_d || userInput[3]->getPadData().buttons_d; };

	while(!exit || (buttonPressed() && exit))
	{
		UpdatePads();

		gameScreenImg->draw();
		bgBottomImg->draw();
		bgTopImg->draw();
		creditsWindow.draw();

		#ifdef HW_RVL
		i = 3;
		do {	
			if(userInput[i]->getPadData().validPointer) {
				Menu_DrawImg(userInput[i]->getPadData().cursor_x-48, userInput[i]->getPadData().cursor_y-48, 96, 96, pointer[i]->getImage(), userInput[i]->getPadData().cursor_angle, 1, 1, 255);
			}
			DoRumble(i);
			--i;
		} while(i >= 0);
		#endif

		Menu_Render();

		if(buttonPressed())
		{
			exit = true;

		}
		usleep(THREAD_SLEEP);
	}

	for(i=0; i < numEntries; i++)
		delete txt[i];

	showCredits = false;
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

static int MenuGameSelection()
{
	int menu = MENU_NONE;
	bool res;
	int i;

	GuiText titleTxt("Choose Game", 26, (GuiColor){255, 255, 255, 255});
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

	GuiText settingsBtnTxt("Settings", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText exitBtnTxt("Exit", 22, (GuiColor){0, 0, 0, 255});
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

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.append(&settingsBtn);
	buttonWindow.append(&exitBtn);

	GuiFileBrowser gameBrowser(330, 268);
	gameBrowser.setPosition(20, 98);
	ResetBrowser();
	
	GuiImage bgPreview(&bgPreviewImg);
	bgPreview.setPosition(365, 98);
	int previousPreviewImg = GCSettings.PreviewImage;
	
	GuiImage preview;
	preview.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	preview.setPosition(174, -8);
	u8* imgBuffer = (u8*)memalign(32, 640 * 480 * 4);
	int  previousBrowserIndex = -1;
	char imagePath[MAXJOLIET + 1];
	
	HaltGui();
	btnLogo->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	btnLogo->setPosition(-50, 24);
	mainWindow->append(&titleTxt);
	mainWindow->append(&gameBrowser);
	mainWindow->append(&buttonWindow);
	mainWindow->append(&bgPreview);
	mainWindow->append(&preview);
	ResumeGui();

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	// populate initial directory listing
	selectLoadedFile = 1;
	OpenGameList();

	gameBrowser.resetState();
	gameBrowser.fileList[0]->setState(STATE::SELECTED);
	gameBrowser.triggerUpdate();
	titleTxt.setText(inSz ? szname : "Choose Game");
			
	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);
		
		if(selectLoadedFile == 2)
		{
			selectLoadedFile = 0;
			mainWindow->changeFocus(&gameBrowser);
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
					HaltGui();
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
						menu = MENU_GAMESELECTION;
						break;
					}
										
					titleTxt.setText(inSz ? szname : "Choose Game");
					
					ResumeGui();
				}
				else
				{
					#ifdef HW_RVL
					ShutoffRumble();
					#endif
					mainWindow->setState(STATE::DISABLED);
					if(BrowserLoadFile())
						menu = MENU_EXIT;
					else
						mainWindow->setState(STATE::DEFAULT);
				}
			}
		}
		
		//update gamelist image
		if(previousBrowserIndex != browser.selIndex || previousPreviewImg != GCSettings.PreviewImage)
		{
			previousBrowserIndex = browser.selIndex;
			previousPreviewImg = GCSettings.PreviewImage;

			// ensure selected index is valid
			if(browser.dir[0] == 0 || GCSettings.LoadMethod <= 0 || browser.numEntries <= 0 || browser.selIndex <= 0 || browser.selIndex >= browser.numEntries)
			{
				preview.setImage(NULL, 0, 0);
			}
			else
			{
				snprintf(imagePath, MAXJOLIET, "%s%s/%s.png", pathPrefix[GCSettings.LoadMethod], getImageFolder(), browserList[browser.selIndex].displayname);

				int width, height;
				if(ChangeInterface(imagePath, SILENT) && DecodePNGFromFile(imagePath, &width, &height, imgBuffer, 640, 480))
				{
					preview.setImage(imgBuffer, width, height);
					preview.setScale( MIN(225.0f / width, 235.0f / height) );
				}
				else
				{
					preview.setImage(NULL, 0, 0);
				}
			}
		}

		if(settingsBtn.getState() == STATE::CLICKED)
			menu = MENU_SETTINGS;
		else if(exitBtn.getState() == STATE::CLICKED)
			ExitRequested = 1;
	}

	HaltParseThread(); // halt parsing
	HaltGui();
	ResetBrowser();
	mainWindow->remove(&titleTxt);
	mainWindow->remove(&buttonWindow);
	mainWindow->remove(&gameBrowser);
	mainWindow->remove(&bgPreview);
	mainWindow->remove(&preview);
	free(imgBuffer);
	return menu;
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

	settingText = new GuiText(ctrlName[GCSettings.Controller], 22, (GuiColor){0, 0, 0, 255});

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

	settingText = new GuiText(playerNumber, 22, (GuiColor){0, 0, 0, 255});

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
	int menu = MENU_NONE;
	
	GuiText titleTxt((char *)Memory.ROMFilename, 22, (GuiColor){255, 255, 255, 255});
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

	GuiText saveBtnTxt("Save", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText loadBtnTxt("Load", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText deleteBtnTxt("Delete", 22, (GuiColor){0, 0, 0, 255});
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
	
	GuiText resetBtnTxt("Reset", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText gameSettingsBtnTxt("Game Settings", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText mainmenuBtnTxt("Main Menu", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText closeBtnTxt("Close", 20, (GuiColor){0, 0, 0, 255});
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

		batteryTxt[i] = new GuiText(txt, 20, (GuiColor){255, 255, 255, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
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

	btnLogo->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	btnLogo->setPosition(-50, -40);
	mainWindow->append(&w);

	if(lastMenu == MENU_NONE)
	{
		enterSound->play();
		bgTopImg->setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
		mainmenuBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		bgBottomImg->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		btnLogo->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#ifdef HW_RVL
		batteryBtn[0]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[1]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[2]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		batteryBtn[3]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
		#endif

		w.setEffect(EFFECT::FADE, 15);
	}

	ResumeGui();
	
	if(lastMenu == MENU_NONE)
		AutoSave();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		#ifdef HW_RVL
		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE)
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
			menu = MENU_GAME_SAVE;
		}
		else if(loadBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAME_LOAD;
		}
		else if(deleteBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAME_DELETE;
		}
		else if(resetBtn.getState() == STATE::CLICKED)
		{
			if (WindowPrompt("Reset Game", "Are you sure that you want to reset this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				S9xSoftReset ();
				menu = MENU_EXIT;
			}
		}
		else if(gameSettingsBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS;
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
				HaltGui();
				mainWindow->remove(gameScreenImg);
				delete gameScreenImg;
				if(gameScreenTexture != NULL) {
					free(gameScreenTexture);
					gameScreenTexture = NULL;
				}
				ClearScreenshot();
				if(GCSettings.AutoloadGame) {
					ExitApp();
				}
				else {
					gameScreenImg = new GuiImage(screenwidth, screenheight, (GuiColor){175, 200, 215, 255});
					gameScreenImg->colorStripe(10);
					mainWindow->insert(gameScreenImg, 0);
					ResumeGui();
					#ifndef NO_SOUND
					bgMusic->play(); // startup music
					#endif
					menu = MENU_GAMESELECTION;
				}
			}
		}
		else if(closeBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_EXIT;

			exitSound->play();
			bgTopImg->setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			mainmenuBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			bgBottomImg->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			btnLogo->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			#ifdef HW_RVL
			batteryBtn[0]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[1]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[2]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			batteryBtn[3]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			#endif

			w.setEffect(EFFECT::FADE, -15);
			usleep(350000); // wait for effects to finish
		}
	}

	HaltGui();

	#ifdef HW_RVL
	for(i=0; i < 4; i++)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}
	#endif

	mainWindow->remove(&w);
	return menu;
}

/****************************************************************************
 * FindGameSaveNum
 *
 * Determines the save file number of the given file name
 * Returns -1 if none is found
 ***************************************************************************/
static int FindGameSaveNum(char * savefile, int device)
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
	int menu = MENU_NONE;
	int ret;
	int i, n, type, len, len2;
	int j = 0;
	SaveList saves;
	char filepath[1024];
	char deletepath[1024];
	char scrfile[1024];
	char tmp[MAXJOLIET+1];
	struct stat filestat;
	struct tm * timeinfo;
	int device = GCSettings.SaveMethod;

	if(!ChangeInterface(device, NOTSILENT))
		return MENU_GAME;

	GuiText titleTxt(NULL, 26, (GuiColor){255, 255, 255, 255});
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

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText closeBtnTxt("Close", 20, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	w.append(&closeBtn);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	memset(&saves, 0, sizeof(saves));

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
		n = FindGameSaveNum(tmp, device);

		if(n >= 0)
		{
			saves.type[j] = type;
			saves.files[saves.type[j]][n] = 1;
			strcpy(saves.filename[j], browserList[i].filename);

			if(saves.type[j] == FILE_STATE)
			{
				sprintf(scrfile, "%s%s/%s.png", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder, tmp);

				memset(savebuffer, 0, SAVEBUFFERSIZE);
				if(LoadFile(scrfile, SILENT))
					saves.previewImg[j] = new GuiImageData(savebuffer, 64, 48);
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
		menu = MENU_GAME;
	}

	GuiSaveBrowser saveBrowser(552, 248, &saves, action);
	saveBrowser.setPosition(0, 108);
	saveBrowser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	HaltGui();
	mainWindow->append(&saveBrowser);
	mainWindow->changeFocus(&saveBrowser);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
						result = LoadSnapshot (filepath, NOTSILENT);
						break;
				}
				if(result)
					menu = MENU_EXIT;
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
				menu = MENU_GAME_DELETE;
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
						menu = MENU_GAME_SAVE;
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
						SaveSRAM (filepath, NOTSILENT);
						menu = MENU_GAME_SAVE;
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
							SaveSnapshot (filepath, NOTSILENT);
							break;
					}
					menu = MENU_GAME_SAVE;
				}
			}
		}
		if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAME;
		}
		else if(closeBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_EXIT;

			exitSound->play();
			bgTopImg->setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			backBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			bgBottomImg->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			btnLogo->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);

			w.setEffect(EFFECT::FADE, -15);

			usleep(350000); // wait for effects to finish
		}
	}

	HaltGui();

	for(i=0; i < saves.length; i++)
		if(saves.previewImg[i])
			delete saves.previewImg[i];

	mainWindow->remove(&saveBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	ResetBrowser();
	return menu;
}

/****************************************************************************
 * MenuGameSettings
 ***************************************************************************/
static int MenuGameSettings()
{
	int menu = MENU_NONE;
	char filepath[1024];

	GuiText titleTxt("Game Settings", 26, (GuiColor){255, 255, 255, 255});
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

	GuiText mappingBtnTxt("Button Mappings", 22, (GuiColor){0, 0, 0, 255});
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
	
	GuiText emulationBtnTxt("Emulation", 22, (GuiColor){0, 0, 0, 255});
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
	
	GuiText videoBtnTxt("Video", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText controllerBtnTxt("Controller", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText screenshotBtnTxt("Screenshot", 22, (GuiColor){0, 0, 0, 255});
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
	
	GuiText cheatsBtnTxt("Cheats", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText closeBtnTxt("Close", 20, (GuiColor){0, 0, 0, 255});
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

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&titleTxt);
	w.append(&mappingBtn);
	w.append(&videoBtn);
	w.append(&emulationBtn);
	w.append(&controllerBtn);
	w.append(&screenshotBtn);
	w.append(&cheatsBtn);
	w.append(&closeBtn);
	w.append(&backBtn);
	
	mainWindow->append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(mappingBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS;
		}
		else if(videoBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_VIDEO;
		}
		else if(emulationBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_EMULATION;
		}
		else if(controllerBtn.getState() == STATE::CLICKED)
		{
			ControllerWindow();
		}
		else if(cheatsBtn.getState() == STATE::CLICKED)
		{
			cheatsBtn.resetState();

			if(Cheat.g.size() > 0) {
				menu = MENU_GAMESETTINGS_CHEATS;
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
			menu = MENU_EXIT;

			exitSound->play();
			bgTopImg->setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			titleTxt.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			backBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			bgBottomImg->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			btnLogo->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);

			w.setEffect(EFFECT::FADE, -15);

			usleep(350000); // wait for effects to finish
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAME;
		}
	}

	HaltGui();
	mainWindow->remove(&w);
	return menu;
}

/****************************************************************************
 * MenuGameCheats
 *
 * Displays a list of cheats available, and allows the user to enable/disable
 * them.
 ***************************************************************************/
static int MenuGameCheats()
{
	int menu = MENU_NONE;
	int ret;
	u16 i = 0;
	OptionList options;

	for(i=0; i < Cheat.g.size(); i++)
	{
		snprintf (options.name[i], 50, "%s", Cheat.g[i].name);
		sprintf (options.value[i], "%s", Cheat.g[i].enabled == true ? "On" : "Off");
	}

	options.length = i;

	GuiText titleTxt("Game Settings - Cheats", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.getClickedOption();

		if(ret >= 0)
		{
			ToggleCheat(ret);
			sprintf (options.value[ret], "%s", Cheat.g[ret].enabled == true ? "On" : "Off");
			optionBrowser.triggerUpdate();
		}

		if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsMappings
 ***************************************************************************/
static int MenuSettingsMappings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Game Settings - Button Mappings", 26, (GuiColor){255, 255, 255, 255});
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

	GuiText snesBtnTxt("SNES Controller", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText superscopeBtnTxt("Super Scope", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText mouseBtnTxt("SNES Mouse", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText justifierBtnTxt("Justifier", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText otherBtnTxt("Other Mappings", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&titleTxt);
	w.append(&snesBtn);
	w.append(&superscopeBtn);
	w.append(&mouseBtn);
	w.append(&justifierBtn);
	w.append(&otherBtn);

	w.append(&backBtn);

	mainWindow->append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(snesBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_PAD;
		}
		else if(superscopeBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_SCOPE;
		}
		else if(mouseBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_MOUSE;
		}
		else if(justifierBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_CTRL;
			mapMenuCtrlSNES = CTRL_JUST;
		}
		else if(otherBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_OTHER;
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&w);
	return menu;
}

static int MenuSettingsMappingsController()
{
	int menu = MENU_NONE;
	char menuTitle[100];
	char menuSubtitle[100];

	sprintf(menuTitle, "Game Settings - Button Mappings");
	GuiText titleTxt(menuTitle, 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	sprintf(menuSubtitle, "%s", ctrlName[mapMenuCtrlSNES]);
	GuiText subtitleTxt(menuSubtitle, 20, (GuiColor){255, 255, 255, 255});
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
	
	GuiText gamecubeBtnTxt("GameCube Controller", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText wiimoteBtnTxt("Wiimote", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText drcBtnTxt("Wii U GamePad", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText classicBtnTxt("Classic Controller", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText nunchukBtnTxt1("Wiimote", 22, (GuiColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt2("&", 18, (GuiColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt3("Nunchuk", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText wiiuproBtnTxt("Wii U Pro Controller", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
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

	mainWindow->append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(wiimoteBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_WIIMOTE;
		}
		else if(nunchukBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_NUNCHUK;
		}
		else if(classicBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_CLASSIC;
		}
		else if(wiiuproBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_WUPC;
		}
		else if(drcBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_DRC;
		}
		else if(gamecubeBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = GUI_HW_GAMECUBE;
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&w);
	return menu;
}

/****************************************************************************
 * ButtonMappingWindow
 ***************************************************************************/
static u32 ButtonMappingWindow()
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

	GuiText titleTxt("Button Mapping", 26, (GuiColor){255, 255, 255, 255});
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

	GuiText msgTxt(msg, 26, (GuiColor){0, 0, 0, 255});
	msgTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	msgTxt.setPosition(0,-20);
	msgTxt.setWrap(true, 430);

	promptWindow.append(&dialogBoxImg);
	promptWindow.append(&titleTxt);
	promptWindow.append(&msgTxt);

	HaltGui();
	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&promptWindow);
	mainWindow->changeFocus(&promptWindow);
	ResumeGui();

	u32 pressed = 0;
	buttonMappingCancelled = false;

	while(pressed == 0 && !buttonMappingCancelled)
	{
		usleep(THREAD_SLEEP);

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

	HaltGui();
	mainWindow->remove(&promptWindow);
	mainWindow->setState(STATE::DEFAULT);
	ResumeGui();

	return pressed;
}

static int MenuSettingsMappingsMap()
{
	int menu = MENU_NONE;
	int ret,i,j;
	bool firstRun = true;
	OptionList options;

	char menuTitle[100];
	char menuSubtitle[100];
	sprintf(menuTitle, "Game Settings - Button Mappings");

	GuiText titleTxt(menuTitle, 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	sprintf(menuSubtitle, "%s - %s", textTranslator->getText(ctrlName[mapMenuCtrlSNES]), textTranslator->getText(ctrlrName[mapMenuCtrl]));
	GuiText subtitleTxt(menuSubtitle, 20, (GuiColor){255, 255, 255, 255});
	subtitleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	subtitleTxt.setPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnShortOutline(button_short_png);
	GuiImageData btnShortOutlineOver(button_short_over_png);

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText resetBtnTxt("Reset Mappings", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	w.append(&resetBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	mainWindow->append(&subtitleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_CTRL;
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

	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	mainWindow->remove(&subtitleTxt);
	return menu;
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

	settingText = new GuiText(NULL, 20, (GuiColor){0, 0, 0, 255});
	settingText2 = new GuiText(NULL, 20, (GuiColor){0, 0, 0, 255});
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

	settingText = new GuiText(NULL, 20, (GuiColor){0, 0, 0, 255});
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
	int menu = MENU_NONE;
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

	GuiText titleTxt("Game Settings - Button Mappings", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,30);

	GuiText subtitleTxt("Other Mappings", 20, (GuiColor){255, 255, 255, 255});
	subtitleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	subtitleTxt.setPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	w.append(&subtitleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
			menu = MENU_GAMESETTINGS_MAPPINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	mainWindow->remove(&subtitleTxt);
	return menu;
}

static int MenuSettingsVideo()
{
	int menu = MENU_NONE;
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

	GuiText titleTxt("Game Settings - Video", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsEmulation
 ***************************************************************************/
static int MenuSettingsEmulation()
{
	int menu = MENU_NONE;
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

	GuiText titleTxt("Game Settings - Emulation", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	
	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();
	
	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);
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
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettings
 ***************************************************************************/
static int MenuSettings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Settings", 26, (GuiColor){255, 255, 255, 255});
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

	GuiText savingBtnTxt1("Saving", 22, (GuiColor){0, 0, 0, 255});
	GuiText savingBtnTxt2("&", 18, (GuiColor){0, 0, 0, 255});
	GuiText savingBtnTxt3("Loading", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText menuBtnTxt("Menu", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText networkBtnTxt("Network", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText creditsBtnTxt("Credits", 22, (GuiColor){0, 0, 0, 255});
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
	creditsBtn.setUpdateCallback(WindowCredits);

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	GuiText resetBtnTxt("Reset Settings", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&titleTxt);
	w.append(&savingBtn);
	w.append(&menuBtn);
	w.append(&networkBtn);
	w.append(&creditsBtn);
	w.append(&backBtn);
	w.append(&resetBtn);

	mainWindow->append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(savingBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_SETTINGS_FILE;
		}
		else if(menuBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_SETTINGS_MENU;
		}
		else if(networkBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_SETTINGS_NETWORK;
		}
		else if(creditsBtn.getState() == STATE::CLICKED)
		{
			showCredits = true;
			creditsBtn.setState(STATE::SELECTED);
		}
		else if(backBtn.getState() == STATE::CLICKED)
		{
			menu = MENU_GAMESELECTION;
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
				autoSaveMethod(SILENT);
				autoLoadMethod(SILENT);
			}
		}
	}

	HaltGui();
	mainWindow->remove(&w);
	return menu;
}

/****************************************************************************
 * MenuSettingsFile
 ***************************************************************************/

static int MenuSettingsFile()
{
	int menu = MENU_NONE;
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

	GuiText titleTxt("Settings - Saving & Loading", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
			menu = MENU_SETTINGS;
			autoSaveMethod(SILENT);
			autoLoadMethod(SILENT);
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsMenu
 ***************************************************************************/

static int MenuSettingsMenu()
{
	int menu = MENU_NONE;
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

	GuiText titleTxt("Settings - Menu", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
				GCSettings.WiimoteOrientation ^= 1;
				break;
			case 2:
				GCSettings.MusicVolume += 10;
				if(GCSettings.MusicVolume > 100)
					GCSettings.MusicVolume = 0;
				bgMusic->setVolume(GCSettings.MusicVolume);
				break;
			case 3:
				GCSettings.SFXVolume += 10;
				if(GCSettings.SFXVolume > 100)
					GCSettings.SFXVolume = 0;
				break;
			case 4:
				GCSettings.Rumble = !GCSettings.Rumble;
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
			options.name[4][0] = 0; // Rumble
			#endif

			if (GCSettings.WiimoteOrientation == WIIMOTE_ORIENTATION_VERTICAL)
				sprintf (options.value[1], "Vertical");
			else if (GCSettings.WiimoteOrientation == WIIMOTE_ORIENTATION_HORIZONTAL)
				sprintf (options.value[1], "Horizontal");

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
			menu = MENU_SETTINGS;
		}
	}
	ChangeLanguage();
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsNetwork
 ***************************************************************************/

static int MenuSettingsNetwork()
{
	int menu = MENU_NONE;
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

	GuiText titleTxt("Settings - Network", 26, (GuiColor){255, 255, 255, 255});
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiTrigger trigB;
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (GuiColor){0, 0, 0, 255});
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

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.append(&backBtn);
	mainWindow->append(&optionBrowser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

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
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->remove(&optionBrowser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	CloseShare();
	return menu;
}

static u8 * CreateBlurredGameTexture() {
	if(gameScreenPng.size == 0) {
		return NULL;
	}

	u8 *src = DecodePNGToRGBA8(gameScreenPng.buffer, gameScreenPng.width, gameScreenPng.height);
	if(!src) {
		return NULL;
	}

	int blurAmount = 4; // blur amount
	GuiColor blurOverlayColor = (GuiColor){50, 50, 50, 160};

	u8 * dst = (u8 *)memalign(32, screenwidth * screenheight * 4);
	if(!dst) {
		return NULL;
	}

	int scaledWidth = (int)(gameScreenPng.width * gameScreenPng.scaleX);
	int scaledHeight = (int)(gameScreenPng.height * gameScreenPng.scaleY);

	// Failsafe for invalid scale metrics
	if (scaledWidth <= 0 || scaledHeight <= 0) {
		memset(dst, 0, screenwidth * screenheight * 4);
		return dst;
	}

	// Calculate the absolute top-left starting pixel of the scaled image.
	int targetCenterX = (screenwidth / 2) + gameScreenPng.xoffset;
	int targetCenterY = (screenheight / 2) + gameScreenPng.yoffset;

	int trueOffsetX = targetCenterX - (scaledWidth / 2);
	int trueOffsetY = targetCenterY - (scaledHeight / 2);

	// --- VIEWABLE AREA CLAMPING LOGIC ---
	// Determine where to start drawing on the screen bounds
	int drawX = trueOffsetX < 0 ? 0 : trueOffsetX;
	int drawY = trueOffsetY < 0 ? 0 : trueOffsetY;

	// Determine the max visible boundaries clipped to screen dimensions
	int endX = (trueOffsetX + scaledWidth > screenwidth) ? screenwidth : (trueOffsetX + scaledWidth);
	int endY = (trueOffsetY + scaledHeight > screenheight) ? screenheight : (trueOffsetY + scaledHeight);

	// Calculate the dimensions of the viewable (cropped) area
	int cropWidth = endX - drawX;
	int cropHeight = endY - drawY;

	// Failsafe if the image is pushed entirely off-screen
	if (cropWidth <= 0 || cropHeight <= 0) {
		memset(dst, 0, screenwidth * screenheight * 4);
		return dst;
	}

	// Determine the starting offset within the theoretical scaled image
	int cropStartX = trueOffsetX < 0 ? -trueOffsetX : 0;
	int cropStartY = trueOffsetY < 0 ? -trueOffsetY : 0;

	// Allocate scratch space ONLY for the viewable cropped portion
	u8 *scaledImg = (u8 *)extmem_malloc(cropWidth * cropHeight * 4);
	u8 *rowBuf    = (u8 *)extmem_malloc(cropWidth * 4);

	if (!scaledImg || !rowBuf) {
		if (scaledImg) extmem_free(scaledImg);
		if (rowBuf) extmem_free(rowBuf);
		free(dst);
		return NULL;
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

	u8 bgR = (u8)((0 * invAlpha + blurOverlayColor.r * alphaIn) / 255);
	u8 bgG = (u8)((0 * invAlpha + blurOverlayColor.g * alphaIn) / 255);
	u8 bgB = (u8)((0 * invAlpha + blurOverlayColor.b * alphaIn) / 255);
	u8 bgA = 255;

	// Vertical Blur, Overlay, & Swizzle directly to the GX Destination Layout
	int tilesX = (screenwidth + 3) / 4;
	int tilesY = (screenheight + 3) / 4;

	for (int ty = 0; ty < tilesY; ++ty) {
		for (int tx = 0; tx < tilesX; ++tx) {
			int tileIdx = ty * tilesX + tx;
			u8* destTilePtr = dst + (tileIdx * 64);

			for (int py = 0; py < 4; ++py) {
				for (int px = 0; px < 4; ++px) {
					int currX = tx * 4 + px;
					int currY = ty * 4 + py;
					int pixelIdx = (py * 4) + px;

					if (currX >= screenwidth || currY >= screenheight) {
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

						u8 blurredR = sumR / div;
						u8 blurredG = sumG / div;
						u8 blurredB = sumB / div;

						u8 finalR = (u8)((blurredR * invAlpha + blurOverlayColor.r * alphaIn) / 255);
						u8 finalG = (u8)((blurredG * invAlpha + blurOverlayColor.g * alphaIn) / 255);
						u8 finalB = (u8)((blurredB * invAlpha + blurOverlayColor.b * alphaIn) / 255);

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
	DCFlushRange(dst, screenwidth * screenheight * 4);

	extmem_free(scaledImg);
	extmem_free(rowBuf);
	extmem_free(src);
	return dst;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/

void
MainMenu (int menu)
{
	static bool firstRun = true;
	int currentMenu = menu;
	lastMenu = MENU_NONE;
	
	if(firstRun)
	{
		#ifdef HW_RVL
		pointer[0] = new GuiImageData(player1_point_png);
		pointer[1] = new GuiImageData(player2_point_png);
		pointer[2] = new GuiImageData(player3_point_png);
		pointer[3] = new GuiImageData(player4_point_png);
		#endif

		trigA = new GuiTrigger;
		trigA->setPrimaryTrigger();;
	}

	mainWindow = new GuiWindow(screenwidth, screenheight);

	if(menu == MENU_GAME)
	{
		gameScreenTexture = CreateBlurredGameTexture();
		if(gameScreenTexture != NULL) {
			gameScreenImg = new GuiImage(gameScreenTexture, screenwidth, screenheight);
		}
	}

	if(gameScreenImg == NULL) {
		gameScreenImg = new GuiImage(screenwidth, screenheight, (GuiColor){175, 200, 215, 255});
		gameScreenImg->colorStripe(10);
	}

	mainWindow->append(gameScreenImg);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData bgTop(bg_top_png);
	bgTopImg = new GuiImage(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	bgBottomImg = new GuiImage(&bgBottom);
	bgBottomImg->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	GuiImageData logo(logo_png);
	GuiImage logoImg(&logo);
	GuiImageData logoOver(logo_over_png);
	GuiImage logoImgOver(&logoOver);
	GuiText logoTxt(APPVERSION, 18, (GuiColor){255, 255, 255, 255});
	logoTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	logoTxt.setPosition(0, 4);
	btnLogo = new GuiButton(logoImg.getWidth(), logoImg.getHeight());
	btnLogo->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	btnLogo->setPosition(-50, 24);
	btnLogo->setImage(&logoImg);
	btnLogo->setImageOver(&logoImgOver);
	btnLogo->setLabel(&logoTxt);
	btnLogo->setSoundOver(&btnSoundOver);
	btnLogo->setSoundClick(&btnSoundClick);
	btnLogo->setTrigger(trigA);
	btnLogo->setUpdateCallback(WindowCredits);

	mainWindow->append(bgTopImg);
	mainWindow->append(bgBottomImg);
	mainWindow->append(btnLogo);

	if(currentMenu == MENU_GAMESELECTION)
		ResumeGui();

	if(firstRun) {
		LoadPrefs();
		autoSaveMethod(SILENT);
		autoLoadMethod(SILENT);

		CreateMissingDirectories();
		SavePrefs(SILENT);
	}

#ifdef HW_RVL
	if(firstRun)
	{
		u32 ios = IOS_GetVersion();

		if(!SupportedIOS(ios))
			ErrorPrompt("The current IOS is unsupported. Functionality and/or stability may be adversely affected.");
		else if(!SaneIOS(ios))
			ErrorPrompt("The current IOS has been altered (fake-signed). Functionality and/or stability may be adversely affected.");
	}
#endif

	#ifndef NO_SOUND
	if(firstRun) {
		bgMusic = new GuiSound(bg_music, bg_music_size, SOUND::OGG);
		bgMusic->setVolume(GCSettings.MusicVolume);
		bgMusic->setLoop(true);
		enterSound = new GuiSound(enter_ogg, enter_ogg_size, SOUND::OGG);
		enterSound->setVolume(GCSettings.SFXVolume);
		exitSound = new GuiSound(exit_ogg, exit_ogg_size, SOUND::OGG);
		exitSound->setVolume(GCSettings.SFXVolume);
	}

	if(currentMenu == MENU_GAMESELECTION)
		bgMusic->play(); // startup music
	#endif

	firstRun = false;

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
		if (btnLogo->getState() == STATE::CLICKED)
		{
			showCredits = true;
			btnLogo->resetState();
		}
		usleep(THREAD_SLEEP);
	}

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	CancelAction();
	HaltGui();

	delete btnLogo;
	delete gameScreenImg;
	delete bgTopImg;
	delete bgBottomImg;
	delete mainWindow;

	btnLogo = NULL;
	gameScreenImg = NULL;
	bgTopImg = NULL;
	bgBottomImg = NULL;
	mainWindow = NULL;

	if(gameScreenTexture != NULL) {
		free(gameScreenTexture);
		gameScreenTexture = NULL;
	}

	ClearScreenshot();

	// wait for keys to be depressed
	while(isMenuRequested())
	{
		UpdatePads();
		usleep(THREAD_SLEEP);
	}
}
