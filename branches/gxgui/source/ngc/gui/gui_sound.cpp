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
GuiSound::GuiSound(const u8 * snd, s32 len, int t)
{
	sound = snd;
	length = len;
	type = t;
	voice = 0;
	volume = 150;
}

/**
 * Destructor for the GuiSound class.
 */
GuiSound::~GuiSound()
{
}

void GuiSound::Play()
{
	switch(type)
	{
		case SOUND_PCM:
		voice = ASND_GetFirstUnusedVoice();
		ASND_SetVoice(voice, VOICE_MONO_8BIT, 8000, 0,
		(u8 *)sound, length, volume, volume, NULL);
		break;

		case SOUND_OGG:
		voice = 0;
		PlayOgg(mem_open((char *)sound, length), 0, OGG_INFINITE_TIME);
		break;
	}
}

void GuiSound::Stop()
{
	switch(type)
	{
		case SOUND_PCM:
		ASND_StopVoice(voice);
		break;

		case SOUND_OGG:
		StopOgg();
		break;
	}
}

void GuiSound::Pause()
{
	switch(type)
	{
		case SOUND_PCM:
		ASND_PauseVoice(voice, 1);
		break;

		case SOUND_OGG:
		PauseOgg(1);
		break;
	}
}

void GuiSound::Resume()
{
	switch(type)
	{
		case SOUND_PCM:
		ASND_PauseVoice(voice, 0);
		break;

		case SOUND_OGG:
		PauseOgg(0);
		break;
	}
}
