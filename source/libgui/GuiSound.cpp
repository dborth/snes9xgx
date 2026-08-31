/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiSound.cpp
 *
 * Generic - Everything platform-specific lives behind audioSystem
 ***************************************************************************/

#include "Gui.h"

GuiSound::GuiSound(const uint8_t * s, int32_t l, SOUND t)
{
	sound = s;
	length = l;
	type = t;
	voice = -1;
	volume = 100;
	loop = false;
}

GuiSound::~GuiSound()
{
	if(type == SOUND::OGG)
		platform->getAudio()->stopStream();
}

void GuiSound::play()
{
	int vol = 255*(volume/100.0);

	switch(type)
	{
		case SOUND::PCM:
			voice = platform->getAudio()->playVoice(sound, length, vol);
			break;

		case SOUND::OGG:
			voice = 0;
			platform->getAudio()->playStream(sound, length, loop, vol);
			break;
	}
}

void GuiSound::stop()
{
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
			platform->getAudio()->stopVoice(voice);
			break;

		case SOUND::OGG:
			platform->getAudio()->stopStream();
			break;
	}
}

void GuiSound::pause()
{
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
			platform->getAudio()->pauseVoice(voice);
			break;

		case SOUND::OGG:
			platform->getAudio()->pauseStream();
			break;
	}
}

void GuiSound::resume()
{
	if(voice < 0)
		return;

	switch(type)
	{
		case SOUND::PCM:
			platform->getAudio()->resumeVoice(voice);
			break;

		case SOUND::OGG:
			platform->getAudio()->resumeStream();
			break;
	}
}

bool GuiSound::isPlaying()
{
	if(voice < 0)
		return false;

	switch(type)
	{
		case SOUND::PCM:
			return platform->getAudio()->isVoicePlaying(voice);

		case SOUND::OGG:
			return platform->getAudio()->isStreamPlaying();
	}

	return false;
}

void GuiSound::setVolume(int vol)
{
	volume = vol;

	if(voice < 0)
		return;

	int newvol = 255*(volume/100.0);

	switch(type)
	{
		case SOUND::PCM:
			platform->getAudio()->setVoiceVolume(voice, newvol);
			break;

		case SOUND::OGG:
			platform->getAudio()->setStreamVolume(newvol);
			break;
	}
}

void GuiSound::setLoop(bool l)
{
	loop = l;
}
