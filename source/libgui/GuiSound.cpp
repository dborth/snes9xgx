/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiSound.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

/**
 * Constructor for the GuiSound class.
 */
GuiSound::GuiSound(const u8 * s, s32 l, SOUND t)
{
	sound = s;
	length = l;
	type = t;
	voice = -1;
	volume = 100;
	loop = false;
}

/**
 * Destructor for the GuiSound class.
 */
GuiSound::~GuiSound()
{
	#ifndef NO_SOUND
	if(type == SOUND::OGG)
		StopOgg();
	#endif
}

void GuiSound::play()
{
	#ifndef NO_SOUND
	int vol;

	switch(type)
	{
		case SOUND::PCM:
		vol = 0.0255f*(volume*GCSettings.SFXVolume);
		voice = ASND_GetFirstUnusedVoice();
		if(voice >= 0)
			ASND_SetVoice(voice, VOICE_STEREO_16BIT, 48000, 0,
				(u8 *)sound, length, vol, vol, nullptr);
		break;

		case SOUND::OGG:
		voice = 0;
		if(loop)
			PlayOgg((char *)sound, length, 0, OGG_INFINITE_TIME);
		else
			PlayOgg((char *)sound, length, 0, OGG_ONE_TIME);
		SetVolumeOgg(2.55f*(volume));
		break;
	}
	#endif
}

void GuiSound::stop()
{
	#ifndef NO_SOUND
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
		ASND_StopVoice(voice);
		break;

		case SOUND::OGG:
		StopOgg();
		break;
	}
	#endif
}

void GuiSound::pause()
{
	#ifndef NO_SOUND
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
		ASND_PauseVoice(voice, 1);
		break;

		case SOUND::OGG:
		PauseOgg(1);
		break;
	}
	#endif
}

void GuiSound::resume()
{
	#ifndef NO_SOUND
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
		ASND_PauseVoice(voice, 0);
		break;

		case SOUND::OGG:
		PauseOgg(0);
		break;
	}
	#endif
}

bool GuiSound::isPlaying()
{
	#ifndef NO_SOUND
	if(ASND_StatusVoice(voice) == SND_WORKING || ASND_StatusVoice(voice) == SND_WAITING)
		return true;
	#endif
	return false;
}

void GuiSound::setVolume(int vol)
{
	#ifndef NO_SOUND
	volume = vol;

	if(voice < 0)
		return;

	int newvol = 0.0255f*(volume*GCSettings.SFXVolume);

	switch(type)
	{
		case SOUND::PCM:
		ASND_ChangeVolumeVoice(voice, newvol, newvol);
		break;

		case SOUND::OGG:
		SetVolumeOgg(2.55f*(volume));
		break;
	}
	#endif
}

void GuiSound::setLoop(bool l)
{
	loop = l;
}
