/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#ifdef HW_RVL
extern "C" {
#include <di/di.h>
}
#endif

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
#include "cheats.h"

#include "snes9xGX.h"
#include "video.h"
#include "filebrowser.h"
#include "gcunzip.h"
#include "networkop.h"
#include "memcardop.h"
#include "fileop.h"

#include "dvd.h"
#include "s9xconfig.h"
#include "sram.h"
#include "freeze.h"
#include "preferences.h"
#include "button_mapping.h"
#include "cheatmgr.h"
#include "input.h"
#include "patch.h"
#include "filter.h"

#include "filelist.h"
#include "GRRLIB.h"
#include "gui/gui.h"
#include "menu.h"

int rumbleRequest[4] = {0,0,0,0};
static int rumbleCount[4] = {0,0,0,0};
static GuiTrigger userInput[4];
static GuiImageData * pointer[4];
static GuiImage * gameScreenImg = NULL;
static GuiWindow * mainWindow = NULL;
static GuiText * settingText = NULL;

static lwp_t guithread = LWP_THREAD_NULL;
static bool guiReady = false;
static bool guiHalt = true;
static lwp_t progressthread = LWP_THREAD_NULL;
static bool showProgress = false;

static char progressTitle[100];
static char progressMsg[200];
static int progressDone = 0;
static int progressTotal = 0;

static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

static void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(50);
}

/****************************************************************************
 * ShutoffRumble
 ***************************************************************************/

void ShutoffRumble()
{
	for(int i=0;i<4;i++)
	{
		WPAD_Rumble(i, 0);
		rumbleCount[i] = 0;
	}
}

/****************************************************************************
 * WindowPrompt
 ***************************************************************************/

int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,10);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 0xff});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 0xff});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(25, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 0xff});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-25, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	guiReady = true;

	while(choice == -1)
	{
		VIDEO_WaitVSync();

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}
	guiReady = false;
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	guiReady = true;
	return choice;
}

/****************************************************************************
 * UpdateGUI
 ***************************************************************************/

static u32 arena1mem = 0;
static u32 arena2mem = 0;
static char mem[150] = { 0 };
static GuiText * memTxt;

static void *
UpdateGUI (void *arg)
{
	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else if(!guiReady)
		{
			VIDEO_WaitVSync();
		}
		else
		{
			arena1mem = (u32)SYS_GetArena1Hi() - (u32)SYS_GetArena1Lo();
			#ifdef HW_RVL
			arena2mem = (u32)SYS_GetArena2Hi() - (u32)SYS_GetArena2Lo();
			#endif
			sprintf(mem, "A1: %u / A2: %u", arena1mem, arena2mem);
			memTxt->SetText(mem);

			mainWindow->Draw();

			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				#ifdef HW_RVL
				memcpy(&userInput[i].wpad, WPAD_Data(i), sizeof(WPADData));

				if(userInput[i].wpad.ir.valid)
				{
					GRRLIB_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48, 96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
				}
				#endif

				userInput[i].chan = i;
				userInput[i].pad.btns_d = PAD_ButtonsDown(i);
				userInput[i].pad.btns_u = PAD_ButtonsUp(i);
				userInput[i].pad.btns_h = PAD_ButtonsHeld(i);
				userInput[i].pad.stickX = PAD_StickX(i);
				userInput[i].pad.stickY = PAD_StickY(i);
				userInput[i].pad.substickX = PAD_SubStickX(i);
				userInput[i].pad.substickY = PAD_SubStickY(i);
				userInput[i].pad.triggerL = PAD_TriggerL(i);
				userInput[i].pad.triggerR = PAD_TriggerR(i);

				mainWindow->Update(&userInput[i]);

				#ifdef HW_RVL
				if(rumbleRequest[i] && rumbleCount[i] < 3)
				{
					WPAD_Rumble(i, 1); // rumble on
					rumbleCount[i]++;
				}
				else if(rumbleRequest[i])
				{
					rumbleCount[i] = 12;
					rumbleRequest[i] = 0;
				}
				else
				{
					if(rumbleCount[i])
						rumbleCount[i]--;
					WPAD_Rumble(i, 0); // rumble off
				}
				#endif
			}

			GRRLIB_Render();

			if(ExitRequested)
				ExitToLoader();

			#ifdef HW_RVL
			if(updateFound)
			{
				updateFound = WindowPrompt(
					"Update Available",
					"An update is available!",
					"Update now",
					"Update later");
				if(updateFound)
					if(DownloadUpdate())
						ExitToLoader();
			}

			if(ShutdownRequested)
				ShutdownWii();
			#endif
		}
	}
	return NULL;
}

/****************************************************************************
 * ProgressWindow
 ***************************************************************************/

static void
ProgressWindow(char *title, char *msg)
{
	usleep(300000); // wait to see if progress flag changes soon
	if(!showProgress)
		return;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	throbberImg.SetPosition(0, 40);

	GuiText titleTxt(title, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,10);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 0xff});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&throbberImg);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	guiReady = true;

	float angle = 0;

	u32 count = 0;

	while(showProgress)
	{
		VIDEO_WaitVSync();
		count++;

		if(count % 5 == 0)
		{
			angle+=45;
			if(angle >= 360)
				angle = 0;
			throbberImg.SetAngle(angle);
		}
	}

	guiReady = false;
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	guiReady = true;
}

static void * ProgressThread (void *arg)
{
	while(1)
	{
		if(!showProgress)
			LWP_SuspendThread (progressthread);

		ProgressWindow(progressTitle, progressMsg);
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 ***************************************************************************/

void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
	LWP_CreateThread (&progressthread, ProgressThread, NULL, NULL, 0, 40);
}

/****************************************************************************
 * Progress Bar
 *
 * Show the user what's happening
 ***************************************************************************/
void
ShowProgress (const char *msg, int done, int total)
{
	if(total < (256*1024))
		return;
	else if(done > total) // this shouldn't happen
		done = total;

	progressTotal = total;
	progressDone = done;
	showProgress = true;
	LWP_ResumeThread (progressthread);
}

void
ShowAction (const char *msg)
{
	strncpy(progressMsg, msg, 200);
	sprintf(progressTitle, "Please Wait");
	progressDone = 0;
	progressTotal = 0;
	showProgress = true;
	LWP_ResumeThread (progressthread);
}

void
CancelAction()
{
	showProgress = false;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(progressthread))
		usleep(50);
}

void ErrorPrompt(const char *msg)
{
	showProgress = false;
	WindowPrompt("Error", msg, "OK", NULL);
}

void InfoPrompt(const char *msg)
{
	showProgress = false;
	WindowPrompt("Information", msg, "OK", NULL);
}

void AutoSave()
{
	if (GCSettings.AutoSave == 1)
	{
		SaveSRAMAuto(GCSettings.SaveMethod, SILENT);
	}
	else if (GCSettings.AutoSave == 2)
	{
		if (WindowPrompt("Save", "Save Snapshot?", "Save", "Don't Save") )
			NGCFreezeGameAuto(GCSettings.SaveMethod, SILENT);
	}
	else if (GCSettings.AutoSave == 3)
	{
		if (WindowPrompt("Save", "Save SRAM and Snapshot?", "Save", "Don't Save") )
		{
			SaveSRAMAuto(GCSettings.SaveMethod, SILENT);
			NGCFreezeGameAuto(GCSettings.SaveMethod, SILENT);
		}
	}
}

static void OnScreenKeyboard(char * var)
{
	int save = -1;

	GuiKeyboard keyboard(var);

	GuiWindow keyboardWindow(560,400);
	keyboardWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	keyboardWindow.Append(&keyboard);

	GuiWindow buttonWindow(500,200);
	buttonWindow.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	buttonWindow.SetPosition(0, -100);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);

	buttonWindow.Append(&okBtn);
	buttonWindow.Append(&cancelBtn);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboardWindow);
	mainWindow->Append(&buttonWindow);
	mainWindow->ChangeFocus(&keyboardWindow);
	guiReady = true;

	while(save == -1)
	{
		VIDEO_WaitVSync();

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		strncpy(var, keyboard.kbtextstr, 100);
		var[100] = 0;
	}

	guiReady = false;
	mainWindow->Remove(&keyboardWindow);
	mainWindow->Remove(&buttonWindow);
	mainWindow->SetState(STATE_DEFAULT);
	guiReady = true;
}

static int
SettingWindow(const char * title, GuiWindow * w)
{
	int save = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,10);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&okBtn);
	promptWindow.Append(&cancelBtn);

	guiReady = false;
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->Append(w);
	mainWindow->ChangeFocus(w);
	guiReady = true;

	while(save == -1)
	{
		VIDEO_WaitVSync();

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}
	guiReady = false;
	mainWindow->Remove(&promptWindow);
	mainWindow->Remove(w);
	mainWindow->SetState(STATE_DEFAULT);
	guiReady = true;
	return save;
}

/****************************************************************************
 * MenuGameSelection
 ***************************************************************************/

static int MenuGameSelection()
{
	// populate initial directory listing
	int method = GCSettings.LoadMethod;

	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	int num = OpenGameList(method);

	if(num == 0)
	{
		int choice = WindowPrompt(
		"Error",
		"Game directory not found on selected load device.",
		"Retry",
		"Change Settings");

		if(choice)
			return MENU_GAMESELECTION;
		else
			return MENU_SETTINGS_FILE;
	}

	int menu = MENU_NONE;

	GuiText titleTxt("Choose Game", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData iconHome(icon_home_png);
	GuiImageData iconSettings(icon_settings_png);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText settingsBtnTxt("Settings", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage settingsBtnIcon(&iconSettings);
	settingsBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	settingsBtnIcon.SetPosition(20,0);
	GuiImage settingsBtnImg(&btnOutline);
	GuiImage settingsBtnImgOver(&btnOutlineOver);
	GuiButton settingsBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	settingsBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	settingsBtn.SetPosition(100, -35);
	settingsBtn.SetLabel(&settingsBtnTxt);
	settingsBtn.SetIcon(&settingsBtnIcon);
	settingsBtn.SetImage(&settingsBtnImg);
	settingsBtn.SetImageOver(&settingsBtnImgOver);
	settingsBtn.SetSoundOver(&btnSoundOver);
	settingsBtn.SetTrigger(&trigA);

	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage exitBtnIcon(&iconHome);
	exitBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	exitBtnIcon.SetPosition(20,0);
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	exitBtn.SetPosition(-100, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetIcon(&exitBtnIcon);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.Append(&settingsBtn);
	buttonWindow.Append(&exitBtn);

	GuiFileBrowser gameBrowser(424, 248);
	gameBrowser.SetPosition(50, 108);

	guiReady = false;
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&gameBrowser);
	mainWindow->Append(&buttonWindow);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync();

		// update gameWindow based on arrow buttons
		// set MENU_EXIT if A button pressed on a game
		for(int i=0; i<PAGESIZE; i++)
		{
			if(gameBrowser.gameList[i]->GetState() == STATE_CLICKED)
			{
				gameBrowser.gameList[i]->ResetState();
				// check corresponding browser entry
				if(browserList[i].isdir)
				{
					BrowserChangeFolder(method);
					gameBrowser.ResetState();
					gameBrowser.gameList[0]->SetState(STATE_SELECTED);
					gameBrowser.TriggerUpdate();
				}
				else
				{
					#ifdef HW_RVL
					ShutoffRumble();
					#endif
					if(BrowserLoadFile(method))
						menu = MENU_EXIT;
				}
			}
		}

		if(settingsBtn.GetState() == STATE_CLICKED)
			menu = MENU_SETTINGS;
		else if(exitBtn.GetState() == STATE_CLICKED)
			ExitRequested = 1;
	}
	guiReady = false;
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&buttonWindow);
	mainWindow->Remove(&gameBrowser);
	return menu;
}

static void ControllerWindowUpdate(void * ptr, int dir)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.Controller += dir;

		if(GCSettings.Controller > CTRL_LENGTH-1)
			GCSettings.Controller = 0;
		else if(GCSettings.Controller < 0)
			GCSettings.Controller = CTRL_LENGTH-1;

		settingText->SetText(ctrlName[GCSettings.Controller]);
		b->ResetState();
	}
}

static void ControllerWindowLeftClick(void * ptr) { ControllerWindowUpdate(ptr, -1); }
static void ControllerWindowRightClick(void * ptr) { ControllerWindowUpdate(ptr, +1); }

static void ControllerWindow()
{
	GuiWindow * w = new GuiWindow(250,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ControllerWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ControllerWindowRightClick);

	settingText = new GuiText(ctrlName[GCSettings.Controller], 22, (GXColor){0, 0, 0, 0xff});

	int currentController = GCSettings.Controller;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	if(!SettingWindow("Controller",w))
		GCSettings.Controller = currentController; // undo changes

	delete(w);
	delete(settingText);
}

/****************************************************************************
 * MenuGame
 ***************************************************************************/

static int MenuGame()
{
	int menu = MENU_NONE;

	GuiText titleTxt((char *)Memory.ROMFilename, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_close_png);
	GuiImageData btnCloseOutlineOver(button_close_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText saveBtnTxt("Save", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage saveBtnImg(&btnLargeOutline);
	GuiImage saveBtnImgOver(&btnLargeOutlineOver);
	GuiButton saveBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	saveBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	saveBtn.SetPosition(50, 120);
	saveBtn.SetLabel(&saveBtnTxt);
	saveBtn.SetImage(&saveBtnImg);
	saveBtn.SetImageOver(&saveBtnImgOver);
	saveBtn.SetSoundOver(&btnSoundOver);
	saveBtn.SetTrigger(&trigA);

	GuiText loadBtnTxt("Load", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage loadBtnImg(&btnLargeOutline);
	GuiImage loadBtnImgOver(&btnLargeOutlineOver);
	GuiButton loadBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	loadBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	loadBtn.SetPosition(0, 120);
	loadBtn.SetLabel(&loadBtnTxt);
	loadBtn.SetImage(&loadBtnImg);
	loadBtn.SetImageOver(&loadBtnImgOver);
	loadBtn.SetSoundOver(&btnSoundOver);
	loadBtn.SetTrigger(&trigA);

	GuiText resetBtnTxt("Reset", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage resetBtnImg(&btnLargeOutline);
	GuiImage resetBtnImgOver(&btnLargeOutlineOver);
	GuiButton resetBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	resetBtn.SetPosition(-50, 120);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);

	GuiText controllerBtnTxt("Controller", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage controllerBtnImg(&btnLargeOutline);
	GuiImage controllerBtnImgOver(&btnLargeOutlineOver);
	GuiButton controllerBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	controllerBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	controllerBtn.SetPosition(-125, 250);
	controllerBtn.SetLabel(&controllerBtnTxt);
	controllerBtn.SetImage(&controllerBtnImg);
	controllerBtn.SetImageOver(&controllerBtnImgOver);
	controllerBtn.SetSoundOver(&btnSoundOver);
	controllerBtn.SetTrigger(&trigA);

	GuiText cheatsBtnTxt("Cheats", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage cheatsBtnImg(&btnLargeOutline);
	GuiImage cheatsBtnImgOver(&btnLargeOutlineOver);
	GuiButton cheatsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	cheatsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	cheatsBtn.SetPosition(125, 250);
	cheatsBtn.SetLabel(&cheatsBtnTxt);
	cheatsBtn.SetImage(&cheatsBtnImg);
	cheatsBtn.SetImageOver(&cheatsBtnImgOver);
	cheatsBtn.SetSoundOver(&btnSoundOver);
	cheatsBtn.SetTrigger(&trigA);

	GuiText backBtnTxt("Main Menu", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	backBtn.SetPosition(0, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-30, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);

	for(int i=0; i < 4; i++)
	{
		if(userInput[i].wpad.err == WPAD_ERR_NONE) // controller connected
		{

		}
		else // controller not connected
		{

		}
	}

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&saveBtn);
	w.Append(&loadBtn);
	w.Append(&resetBtn);
	w.Append(&controllerBtn);
	w.Append(&cheatsBtn);

	w.Append(&backBtn);
	w.Append(&closeBtn);

	mainWindow->Append(&w);

	guiReady = true;
	AutoSave();

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync();

		if(saveBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_SAVE;
		}
		else if(loadBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_LOAD;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			S9xSoftReset ();
			menu = MENU_EXIT;
		}
		else if(controllerBtn.GetState() == STATE_CLICKED)
		{
			ControllerWindow();
		}
		else if(cheatsBtn.GetState() == STATE_CLICKED)
		{
			cheatsBtn.ResetState();
			if(Cheat.num_cheats > 0)
				menu = MENU_GAME_CHEATS;
			else
				InfoPrompt("Cheats file not found!");
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			if(gameScreenImg)
			{
				mainWindow->Remove(gameScreenImg);
				delete gameScreenImg;
				gameScreenImg = NULL;
				free(gameScreenTex);
				gameScreenTex = NULL;
			}

			menu = MENU_GAMESELECTION;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
		}
	}
	guiReady = false;
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuGameSaves
 ***************************************************************************/

static int MenuGameSaves(int action)
{
	int menu = MENU_NONE;
	int ret;
	int i, len, len2;
	int j = 0;
	SaveList saves;
	char filepath[1024];

	memset(&saves, 0, sizeof(saves));

	strncpy(browser.dir, GCSettings.SaveFolder, 200);

	if(ParseDirectory() > 0)
	{
		for(i=0; i < browser.numEntries; i++)
		{
			len = strlen(Memory.ROMFilename);
			len2 = strlen(browserList[i].filename);

			// find matching files
			if(len2 > 5 && strncmp(browserList[i].filename, Memory.ROMFilename, len) == 0)
			{
				if(strncmp(&browserList[i].filename[len2-4], ".srm", 4) == 0)
					saves.type[j] = FILE_SRAM;
				else if(strncmp(&browserList[i].filename[len2-4], ".frz", 4) == 0)
					saves.type[j] = FILE_SNAPSHOT;
				else
					saves.type[j] = -1;

				if(saves.type[j] != -1)
				{
					int n = -1;
					char tmp[300];
					strncpy(tmp, browserList[i].filename, 255);
					tmp[len2-4] = 0;

					if(len2 - len == 7)
						n = atoi(&tmp[len2-5]);
					else if(len2 - len == 6)
						n = atoi(&tmp[len2-6]);

					if(n > 0 && n < 100)
						saves.files[saves.type[j]][n] = 1;

					strncpy(saves.filename[j], browserList[i].filename, 255);
					strftime(saves.date[j], 20, "%a %b %d", &browserList[j].mtime);
					strftime(saves.time[j], 10, "%I:%M %p", &browserList[j].mtime);
					j++;
				}
			}
		}
	}

	saves.length = j;

	if(saves.length == 0 && action == 0)
	{
		InfoPrompt("No game saves found.");
		return MENU_GAME;
	}

	GuiText titleTxt(NULL, 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	if(action == 0)
		titleTxt.SetText("Load Game");
	else
		titleTxt.SetText("Save Game");

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiSaveBrowser saveBrowser(552, 248, &saves, action);
	saveBrowser.SetPosition(0, 108);
	saveBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&saveBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		ret = saveBrowser.GetClickedSave();

		// load or save game
		if(ret >= 0)
		{
			int result = 0;

			if(action == 0) // load
			{
				sprintf(filepath, "%s/%s", GCSettings.SaveFolder, saves.filename[ret]);

				switch(saves.type[ret])
				{
					case FILE_SRAM:
						result = LoadSRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
						break;
					case FILE_SNAPSHOT:
						result = NGCUnfreezeGame (filepath, GCSettings.SaveMethod, NOTSILENT);
						break;
				}
				if(result)
					menu = MENU_EXIT;
			}
			else // save
			{
				if(ret == 0) // new SRAM
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_SRAM][i] == 0)
							break;

					if(i < 100)
					{
						sprintf(filepath, "%s/%s %i.srm", GCSettings.SaveFolder, Memory.ROMFilename, i);
						SaveSRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else if(ret == 1) // new Snapshot
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_SNAPSHOT][i] == 0)
							break;

					if(i < 100)
					{
						sprintf(filepath, "%s/%s %i.frz", GCSettings.SaveFolder, Memory.ROMFilename, i);
						NGCFreezeGame (filepath, GCSettings.SaveMethod, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else // overwrite SRAM/Snapshot
				{
					sprintf(filepath, "%s/%s", GCSettings.SaveFolder, saves.filename[ret-2]);

					switch(saves.type[ret-2])
					{
						case FILE_SRAM:
							SaveSRAM(filepath, GCSettings.SaveMethod, NOTSILENT);
							break;
						case FILE_SNAPSHOT:
							NGCFreezeGame (filepath, GCSettings.SaveMethod, NOTSILENT);
							break;
					}
					menu = MENU_GAME_SAVE;
				}
			}
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
	}
	guiReady = false;
	mainWindow->Remove(&saveBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuGameCheats
 ***************************************************************************/

static int MenuGameCheats()
{
	int menu = MENU_NONE;
	int ret;
	u16 i = 0;
	OptionList options;

	for(i=0; i < Cheat.num_cheats; i++)
		sprintf (options.name[i], "%s", Cheat.c[i].name);

	options.length = i;

	GuiText titleTxt("Cheats", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		for(i=0; i < Cheat.num_cheats; i++)
			sprintf (options.value[i], "%s", Cheat.c[i].enabled == true ? "On" : "Off");

		ret = optionBrowser.GetClickedOption();

		if(Cheat.c[ret].enabled)
			S9xDisableCheat(ret);
		else
			S9xEnableCheat(ret);

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
	}
	guiReady = false;
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettings
 ***************************************************************************/

static int MenuSettings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Settings", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText mappingBtnTxt("Button Mapping", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage mappingBtnImg(&btnLargeOutline);
	GuiImage mappingBtnImgOver(&btnLargeOutlineOver);
	GuiButton mappingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	mappingBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	mappingBtn.SetPosition(50, 120);
	mappingBtn.SetLabel(&mappingBtnTxt);
	mappingBtn.SetImage(&mappingBtnImg);
	mappingBtn.SetImageOver(&mappingBtnImgOver);
	mappingBtn.SetSoundOver(&btnSoundOver);
	mappingBtn.SetTrigger(&trigA);

	GuiText videoBtnTxt("Video", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage videoBtnImg(&btnLargeOutline);
	GuiImage videoBtnImgOver(&btnLargeOutlineOver);
	GuiButton videoBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	videoBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	videoBtn.SetPosition(0, 120);
	videoBtn.SetLabel(&videoBtnTxt);
	videoBtn.SetImage(&videoBtnImg);
	videoBtn.SetImageOver(&videoBtnImgOver);
	videoBtn.SetSoundOver(&btnSoundOver);
	videoBtn.SetTrigger(&trigA);

	GuiText savingBtnTxt("Saving / Loading", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage savingBtnImg(&btnLargeOutline);
	GuiImage savingBtnImgOver(&btnLargeOutlineOver);
	GuiButton savingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	savingBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	savingBtn.SetPosition(-50, 120);
	savingBtn.SetLabel(&savingBtnTxt);
	savingBtn.SetImage(&savingBtnImg);
	savingBtn.SetImageOver(&savingBtnImgOver);
	savingBtn.SetSoundOver(&btnSoundOver);
	savingBtn.SetTrigger(&trigA);

	GuiText menuBtnTxt("Menu", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage menuBtnImg(&btnLargeOutline);
	GuiImage menuBtnImgOver(&btnLargeOutlineOver);
	GuiButton menuBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	menuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	menuBtn.SetPosition(-125, 250);
	menuBtn.SetLabel(&menuBtnTxt);
	menuBtn.SetImage(&menuBtnImg);
	menuBtn.SetImageOver(&menuBtnImgOver);
	menuBtn.SetSoundOver(&btnSoundOver);
	menuBtn.SetTrigger(&trigA);

	GuiText networkBtnTxt("Network", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage networkBtnImg(&btnLargeOutline);
	GuiImage networkBtnImgOver(&btnLargeOutlineOver);
	GuiButton networkBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	networkBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	networkBtn.SetPosition(125, 250);
	networkBtn.SetLabel(&networkBtnTxt);
	networkBtn.SetImage(&networkBtnImg);
	networkBtn.SetImageOver(&networkBtnImgOver);
	networkBtn.SetSoundOver(&btnSoundOver);
	networkBtn.SetTrigger(&trigA);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiText resetBtnTxt("Reset Settings", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	resetBtn.SetPosition(-100, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&mappingBtn);
	w.Append(&videoBtn);
	w.Append(&savingBtn);
	w.Append(&menuBtn);
	w.Append(&networkBtn);

	w.Append(&backBtn);
	w.Append(&resetBtn);

	mainWindow->Append(&w);

	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		if(mappingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MAPPINGS;
		}
		else if(videoBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_VIDEO;
		}
		else if(savingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_FILE;
		}
		else if(menuBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MENU;
		}
		else if(networkBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_NETWORK;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESELECTION;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();
			int choice = WindowPrompt(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");

			if(choice == 1)
				DefaultSettings ();
		}
	}
	SavePrefs(SILENT);
	guiReady = false;
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuSettingsMappings
 ***************************************************************************/

static int MenuSettingsMappings()
{
	return MENU_EXIT;
}

/****************************************************************************
 * MenuSettingsVideo
 ***************************************************************************/

static void ScreenZoomWindowUpdate(void * ptr, float amount)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.ZoomLevel += amount;

		char zoom[10];
		sprintf(zoom, "%.2f%%", GCSettings.ZoomLevel*100);
		settingText->SetText(zoom);
		b->ResetState();
	}
}

static void ScreenZoomWindowLeftClick(void * ptr) { ScreenZoomWindowUpdate(ptr, -0.01); }
static void ScreenZoomWindowRightClick(void * ptr) { ScreenZoomWindowUpdate(ptr, +0.01); }

static void ScreenZoomWindow()
{
	GuiWindow * w = new GuiWindow(250,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenZoomWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenZoomWindowRightClick);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 0xff});
	char zoom[10];
	sprintf(zoom, "%.2f%%", GCSettings.ZoomLevel*100);
	settingText->SetText(zoom);

	float currentZoom = GCSettings.ZoomLevel;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	if(!SettingWindow("Screen Zoom",w))
		GCSettings.ZoomLevel = currentZoom; // undo changes

	delete(w);
	delete(settingText);
}

static void ScreenPositionWindowUpdate(void * ptr, int x, int y)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.xshift += x;
		GCSettings.yshift += y;

		char shift[10];
		sprintf(shift, "%i, %i", GCSettings.xshift, GCSettings.yshift);
		settingText->SetText(shift);
		b->ResetState();
	}
}

static void ScreenPositionWindowLeftClick(void * ptr) { ScreenPositionWindowUpdate(ptr, -1, 0); }
static void ScreenPositionWindowRightClick(void * ptr) { ScreenPositionWindowUpdate(ptr, +1, 0); }
static void ScreenPositionWindowUpClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, -1); }
static void ScreenPositionWindowDownClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, +1); }

static void ScreenPositionWindow()
{
	GuiWindow * w = new GuiWindow(150,150);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	w->SetPosition(0, -10);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiTrigger trigUp;
	trigUp.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP);

	GuiTrigger trigDown;
	trigDown.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenPositionWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenPositionWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	arrowUpBtn.SetTrigger(0, &trigA);
	arrowUpBtn.SetTrigger(1, &trigUp);
	arrowUpBtn.SetSelectable(false);
	arrowUpBtn.SetUpdateCallback(ScreenPositionWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	arrowDownBtn.SetTrigger(0, &trigA);
	arrowDownBtn.SetTrigger(1, &trigDown);
	arrowDownBtn.SetSelectable(false);
	arrowDownBtn.SetUpdateCallback(ScreenPositionWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 0xff});
	char shift[10];
	sprintf(shift, "%i, %i", GCSettings.xshift, GCSettings.yshift);
	settingText->SetText(shift);

	int currentX = GCSettings.xshift;
	int currentY = GCSettings.yshift;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(&arrowUpBtn);
	w->Append(&arrowDownBtn);
	w->Append(&screenPositionImg);
	w->Append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		GCSettings.xshift = currentX; // undo changes
		GCSettings.yshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static int MenuSettingsVideo()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	OptionList options;

	sprintf(options.name[i++], "Rendering");
	sprintf(options.name[i++], "Scaling");
	sprintf(options.name[i++], "Filtering");
	sprintf(options.name[i++], "Screen Zoom");
	sprintf(options.name[i++], "Screen Position");
	options.length = i;

	GuiText titleTxt("Settings - Video", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		// don't allow original render mode if progressive video mode detected
		if (GCSettings.render==0 && progressive)
			GCSettings.render++;

		if (GCSettings.render == 0)
			sprintf (options.value[0], "Original");
		else if (GCSettings.render == 1)
			sprintf (options.value[0], "Filtered");
		else if (GCSettings.render == 2)
			sprintf (options.value[0], "Unfiltered");

		if(GCSettings.widescreen)
			sprintf (options.value[1], "16:9 Correction");
		else
			sprintf (options.value[1], "Default");

		sprintf (options.value[2], "%s", GetFilterName((RenderFilter)GCSettings.FilterMethod));

		sprintf (options.value[3], "%.2f%%", GCSettings.ZoomLevel*100);

		sprintf (options.value[4], "%d, %d", GCSettings.xshift, GCSettings.yshift);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.render++;
				if (GCSettings.render > 2 || GCSettings.FilterMethod != FILTER_NONE)
					GCSettings.render = 0;
				break;

			case 1:
				GCSettings.widescreen ^= 1;
				break;

			case 2:
				GCSettings.FilterMethod++;
				if (GCSettings.FilterMethod >= NUM_FILTERS)
					GCSettings.FilterMethod = 0;
				SelectFilterMethod();
				break;

			case 3:
				ScreenZoomWindow();
				break;

			case 4:
				ScreenPositionWindow();
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	guiReady = false;
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
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
	OptionList options;
	sprintf(options.name[i++], "Load Method");
	sprintf(options.name[i++], "Load Folder");
	sprintf(options.name[i++], "Save Method");
	sprintf(options.name[i++], "Save Folder");
	sprintf(options.name[i++], "Auto Load");
	sprintf(options.name[i++], "Auto Save");
	sprintf(options.name[i++], "Verify MC Saves");
	options.length = i;

	GuiText titleTxt("Settings - Saving/Loading", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		// some load/save methods are not implemented - here's where we skip them
		// they need to be skipped in the order they were enumerated in snes9xGX.h

		// no USB ports on GameCube
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_USB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_USB)
			GCSettings.SaveMethod++;
		#endif

		// saving to DVD is impossible
		if(GCSettings.SaveMethod == METHOD_DVD)
			GCSettings.SaveMethod++;

		// disable SMB in GC mode (stalls out)
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_SMB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_SMB)
			GCSettings.SaveMethod++;
		#endif

		// disable MC saving in Wii mode - does not work for some reason!
		#ifdef HW_RVL
		if(GCSettings.SaveMethod == METHOD_MC_SLOTA)
			GCSettings.SaveMethod++;
		if(GCSettings.SaveMethod == METHOD_MC_SLOTB)
			GCSettings.SaveMethod++;
		options.name[6][0] = 0;
		#endif

		// correct load/save methods out of bounds
		if(GCSettings.LoadMethod > 4)
			GCSettings.LoadMethod = 0;
		if(GCSettings.SaveMethod > 6)
			GCSettings.SaveMethod = 0;

		if (GCSettings.LoadMethod == METHOD_AUTO) sprintf (options.value[0],"Auto");
		else if (GCSettings.LoadMethod == METHOD_SD) sprintf (options.value[0],"SD");
		else if (GCSettings.LoadMethod == METHOD_USB) sprintf (options.value[0],"USB");
		else if (GCSettings.LoadMethod == METHOD_DVD) sprintf (options.value[0],"DVD");
		else if (GCSettings.LoadMethod == METHOD_SMB) sprintf (options.value[0],"Network");

		sprintf (options.value[1], "%s", GCSettings.LoadFolder);

		if (GCSettings.SaveMethod == METHOD_AUTO) sprintf (options.value[2],"Auto");
		else if (GCSettings.SaveMethod == METHOD_SD) sprintf (options.value[2],"SD");
		else if (GCSettings.SaveMethod == METHOD_USB) sprintf (options.value[2],"USB");
		else if (GCSettings.SaveMethod == METHOD_SMB) sprintf (options.value[2],"Network");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTA) sprintf (options.value[2],"MC Slot A");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTB) sprintf (options.value[2],"MC Slot B");

		sprintf (options.value[3], "%s", GCSettings.SaveFolder);

		if (GCSettings.AutoLoad == 0) sprintf (options.value[4],"Off");
		else if (GCSettings.AutoLoad == 1) sprintf (options.value[4],"SRAM");
		else if (GCSettings.AutoLoad == 2) sprintf (options.value[4],"Snapshot");

		if (GCSettings.AutoSave == 0) sprintf (options.value[5],"Off");
		else if (GCSettings.AutoSave == 1) sprintf (options.value[5],"SRAM");
		else if (GCSettings.AutoSave == 2) sprintf (options.value[5],"Snapshot");
		else if (GCSettings.AutoSave == 3) sprintf (options.value[5],"Both");

		sprintf (options.value[6], "%s", GCSettings.VerifySaves == true ? "On" : "Off");

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod ++;
				break;

			case 1:
				OnScreenKeyboard(GCSettings.LoadFolder);
				break;

			case 2:
				GCSettings.SaveMethod ++;
				break;

			case 3:
				OnScreenKeyboard(GCSettings.SaveFolder);
				break;

			case 4:
				GCSettings.AutoLoad ++;
				if (GCSettings.AutoLoad > 2)
					GCSettings.AutoLoad = 0;
				break;

			case 5:
				GCSettings.AutoSave ++;
				if (GCSettings.AutoSave > 3)
					GCSettings.AutoSave = 0;
				break;

			case 6:
				GCSettings.VerifySaves ^= 1;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	guiReady = false;
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsMenu
 ***************************************************************************/

static int MenuSettingsMenu()
{
	return MENU_EXIT;
}

/****************************************************************************
 * MenuSettingsNetwork
 ***************************************************************************/

static int MenuSettingsNetwork()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	OptionList options;
	sprintf(options.name[i++], "SMB Share IP");
	sprintf(options.name[i++], "SMB Share Name");
	sprintf(options.name[i++], "SMB Share Username");
	sprintf(options.name[i++], "SMB Share Password");
	options.length = i;

	GuiText titleTxt("Settings - Network", 22, (GXColor){255, 255, 255, 0xff});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_mp3, button_over_mp3_size);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 0xff});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetTrigger(&trigA);

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	guiReady = false;
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	guiReady = true;

	while(menu == MENU_NONE)
	{
		VIDEO_WaitVSync ();

		strncpy (options.value[0], GCSettings.smbip, 15);
		strncpy (options.value[1], GCSettings.smbshare, 19);
		strncpy (options.value[2], GCSettings.smbuser, 19);
		strncpy (options.value[3], GCSettings.smbpwd, 19);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(GCSettings.smbip);
				break;

			case 1:
				OnScreenKeyboard(GCSettings.smbshare);
				break;

			case 2:
				OnScreenKeyboard(GCSettings.smbuser);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.smbpwd);
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	guiReady = false;
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/

void
MainMenu (int menu)
{
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	if(gameScreenTex)
	{
		gameScreenImg = new GuiImage(gameScreenTex, screenwidth, screenheight);
		gameScreenImg->SetAlpha(128);
		mainWindow->Append(gameScreenImg);
	}

	GuiImageData bgTop(bg_top_png);
	GuiImage bgTopImg(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	GuiImage bgBottomImg(&bgBottom);
	bgBottomImg.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	mainWindow->Append(&bgTopImg);
	mainWindow->Append(&bgBottomImg);

	// memory usage - for debugging
	memTxt = new GuiText(NULL, 22, (GXColor){255, 255, 255, 0xff});
	memTxt->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	memTxt->SetPosition(-20, 40);
	mainWindow->Append(memTxt);

	guiReady = true;
	ResumeGui();

	// Load preferences
	if(!LoadPrefs())
	{
		ErrorPrompt("Preferences reset - check your settings!");
		menu = MENU_SETTINGS_FILE;
	}

	while(menu != MENU_EXIT || SNESROMSize <= 0)
	{
		switch (menu)
		{
			case MENU_GAMESELECTION:
				menu = MenuGameSelection();
				break;
			case MENU_GAME:
				menu = MenuGame();
				break;
			case MENU_GAME_LOAD:
				menu = MenuGameSaves(0);
				break;
			case MENU_GAME_SAVE:
				menu = MenuGameSaves(1);
				break;
			case MENU_GAME_CHEATS:
				menu = MenuGameCheats();
				break;
			case MENU_SETTINGS:
				menu = MenuSettings();
				break;
			case MENU_SETTINGS_MAPPINGS:
				menu = MenuSettingsMappings();
				break;
			case MENU_SETTINGS_VIDEO:
				menu = MenuSettingsVideo();
				break;
			case MENU_SETTINGS_FILE:
				menu = MenuSettingsFile();
				break;
			case MENU_SETTINGS_MENU:
				menu = MenuSettingsMenu();
				break;
			case MENU_SETTINGS_NETWORK:
				menu = MenuSettingsNetwork();
				break;
			default: // unrecognized menu
				menu = MenuGameSelection();
				break;
		}
	}

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	CancelAction();
	HaltGui();

	delete memTxt;
	delete mainWindow;
	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];
	mainWindow = NULL;

	if(gameScreenImg)
	{
		delete gameScreenImg;
		gameScreenImg = NULL;
		free(gameScreenTex);
		gameScreenTex = NULL;
	}
}

/****************************************************************************
 * Controller Configuration
 *
 * Snes9x 1.51 uses a cmd system to work out which button has been pressed.
 * Here, I simply move the designated value to the gcpadmaps array, which
 * saves on updating the cmd sequences.
 ***************************************************************************/

/*
u32
GetInput (u16 ctrlr_type)
{
	//u32 exp_type;
	u32 pressed;
	pressed=0;
	s8 gc_px = 0;

	while( PAD_ButtonsHeld(0)
#ifdef HW_RVL
	| WPAD_ButtonsHeld(0)
#endif
	) VIDEO_WaitVSync();	// button 'debounce'

	while (pressed == 0)
	{
		VIDEO_WaitVSync();
		// get input based on controller type
		if (ctrlr_type == CTRLR_GCPAD)
		{
			pressed = PAD_ButtonsHeld (0);
			gc_px = PAD_SubStickX (0);
		}
#ifdef HW_RVL
		else
		{
		//	if ( WPAD_Probe( 0, &exp_type) == 0)	// check wiimote and expansion status (first if wiimote is connected & no errors)
		//	{
				pressed = WPAD_ButtonsHeld (0);

		//		if (ctrlr_type != CTRLR_WIIMOTE && exp_type != ctrlr_type+1)	// if we need input from an expansion, and its not connected...
		//			pressed = 0;
		//	}
		}
#endif
		// check for exit sequence (c-stick left OR home button)
		if ( (gc_px < -70) || (pressed & WPAD_BUTTON_HOME) || (pressed & WPAD_CLASSIC_BUTTON_HOME) )
			return 0;
	}	// end while
	while( pressed == (PAD_ButtonsHeld(0)
#ifdef HW_RVL
						| WPAD_ButtonsHeld(0)
#endif
						) ) VIDEO_WaitVSync();

	return pressed;
}	// end GetInput()

static int cfg_text_count = 7;
static char cfg_text[][50] = {
"Remapping          ",
"Press Any Button",
"on the",
"       ",	// identify controller
"                   ",
"Press C-Left or",
"Home to exit"
};

u32
GetButtonMap(u16 ctrlr_type, char* btn_name)
{
	u32 pressed, previous;
	char temp[50] = "";
	uint k;
	pressed = 0; previous = 1;

	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			strncpy (cfg_text[3], "NUNCHUK", 7);
			break;
		case CTRLR_CLASSIC:
			strncpy (cfg_text[3], "CLASSIC", 7);
			break;
		case CTRLR_GCPAD:
			strncpy (cfg_text[3], "GC PAD", 7);
			break;
		case CTRLR_WIIMOTE:
			strncpy (cfg_text[3], "WIIMOTE", 7);
			break;
	};

	// note which button we are remapping
	sprintf (temp, "Remapping ");
	for (k=0; k<9-strlen(btn_name); k++) strcat(temp, " "); // add whitespace padding to align text
	strncat (temp, btn_name, 9);		// snes button we are remapping
	strncpy (cfg_text[0], temp, 19);	// copy this all back to the text we wish to display

//	DrawMenu(&cfg_text[0], NULL, cfg_text_count, 1);	// display text

//	while (previous != pressed && pressed == 0);	// get two consecutive button presses (which are the same)
//	{
//		previous = pressed;
//		VIDEO_WaitVSync();	// slow things down a bit so we don't overread the pads
		pressed = GetInput(ctrlr_type);
//	}
	return pressed;
}	// end getButtonMap()

static int cfg_btns_count = 13;
static char cfg_btns_menu[][50] = {
	"A        -         ",
	"B        -         ",
	"X        -         ",
	"Y        -         ",
	"L TRIG   -         ",
	"R TRIG   -         ",
	"SELECT   -         ",
	"START    -         ",
	"UP       -         ",
	"DOWN     -         ",
	"LEFT     -         ",
	"RIGHT    -         ",
	"Return to previous"
};

void
ConfigureButtons (u16 ctrlr_type)
{
	int quit = 0;
	int ret = 0;
	int oldmenu = 0;
	char menu_title[50];
	u32 pressed;

	unsigned int* currentpadmap = 0;
	char temp[50] = "";
	int i, j;
	uint k;

	// Update Menu Title (based on controller we're configuring)
	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			sprintf(menu_title, "SNES     -  NUNCHUK");
			currentpadmap = ncpadmap;
			break;
		case CTRLR_CLASSIC:
			sprintf(menu_title, "SNES     -  CLASSIC");
			currentpadmap = ccpadmap;
			break;
		case CTRLR_GCPAD:
			sprintf(menu_title, "SNES     -   GC PAD");
			currentpadmap = gcpadmap;
			break;
		case CTRLR_WIIMOTE:
			sprintf(menu_title, "SNES     -  WIIMOTE");
			currentpadmap = wmpadmap;
			break;
	};

	while (quit == 0)
	{
		// Update Menu with Current ButtonMap
		for (i=0; i<12; i++) // snes pad has 12 buttons to config (go thru them)
		{
			// get current padmap button name to display
			for ( j=0;
					j < ctrlr_def[ctrlr_type].num_btns &&
					currentpadmap[i] != ctrlr_def[ctrlr_type].map[j].btn	// match padmap button press with button names
				; j++ );

			memset (temp, 0, sizeof(temp));
			strncpy (temp, cfg_btns_menu[i], 12);	// copy snes button information
			if (currentpadmap[i] == ctrlr_def[ctrlr_type].map[j].btn)		// check if a match was made
			{
				for (k=0; k<7-strlen(ctrlr_def[ctrlr_type].map[j].name) ;k++) strcat(temp, " "); // add whitespace padding to align text
				strncat (temp, ctrlr_def[ctrlr_type].map[j].name, 6);		// update button map display
			}
			else
				strcat (temp, "---");								// otherwise, button is 'unmapped'
			strncpy (cfg_btns_menu[i], temp, 19);	// move back updated information

		}

//		ret = RunMenu (cfg_btns_menu, cfg_btns_count, menu_title, 16);

		switch (ret)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
				// Change button map
				// wait for input
				memset (temp, 0, sizeof(temp));
				strncpy(temp, cfg_btns_menu[ret], 6);			// get the name of the snes button we're changing
				pressed = GetButtonMap(ctrlr_type, temp);	// get a button selection from user
				// FIX: check if input is valid for this controller
				if (pressed != 0)	// check if a the button was configured, or if the user exited.
					currentpadmap[ret] = pressed;	// update mapping
				break;

			case -1: // Button B
			case 12:
				// Return
				quit = 1;
				break;
		}
	}
	menu = oldmenu;
}	// end configurebuttons()
*/
