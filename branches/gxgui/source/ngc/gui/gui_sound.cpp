/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_sound.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
/**
 * Constructor for the GuiSound class.
 */
GuiSound::GuiSound(const u8 * snd, s32 len)
{
	sound = snd;
	length = len;
}

/**
 * Destructor for the GuiSound class.
 */
GuiSound::~GuiSound()
{
}

void GuiSound::Play()
{
	MP3Player_PlayBuffer(sound,length,NULL);
}
