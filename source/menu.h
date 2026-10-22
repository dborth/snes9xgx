/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * menu.h
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#ifndef _MENU_H_
#define _MENU_H_

#include <stdint.h>

class GuiWindow;

void InitGUIThreads();
void MainMenu(int menuitem);
void ErrorPrompt(const char * msg);
int ErrorPromptRetry(const char * msg);
void InfoPrompt(const char * msg);
void ShowAction(const char *msg, void (*onCancel)(void) = nullptr);
void CancelAction();
void ShowProgress(const char *msg, int done, int total);
void ChangeLanguage();

// What a screen built outside menu.cpp needs of the menu: its main window, a frame of it, and the prompts
GuiWindow& MenuMainWindow();
bool MenuUpdateGui();	//!< Runs one frame of the menu. False when the application is exiting.
bool MenuConfirm(const char * title, const char * msg, const char * yes, const char * no);	//!< true if `yes` was chosen

extern uint8_t * bg_music;
extern uint32_t bg_music_size;

enum
{
	MENU_EXIT = -1,
	MENU_NONE,
	MENU_SETTINGS,
	MENU_SETTINGS_FILE,
	MENU_SETTINGS_MENU,
	MENU_SETTINGS_NETWORK,
	MENU_GAMESELECTION,
	MENU_GAME,
	MENU_GAME_SAVE,
	MENU_GAME_LOAD,
	MENU_GAME_DELETE,
	MENU_GAMESETTINGS,
	MENU_GAMESETTINGS_MAPPINGS,
	MENU_GAMESETTINGS_VIDEO,
	MENU_GAMESETTINGS_EMULATION,
	MENU_GAMESETTINGS_CHEATS,
	MENU_GAMESETTINGS_MAPPINGS_OTHER,
};

#endif
