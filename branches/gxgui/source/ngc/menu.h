/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * menu.h
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#ifndef _NGCMENU_
#define _NGCMENU_

void InitGUIThread();
void MainMenu (int menuitem);
void ShutoffRumble();

extern lwp_t guithread;

enum
{
	MENU_EXIT = -1,
	MENU_NONE,
	MENU_SETTINGS,
	MENU_GAMESELECTION,
	MENU_GAME
};

#endif
