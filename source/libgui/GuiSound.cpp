/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiSound.cpp
 *
 * Generic - Everything platform-specific lives behind audioSystem
 ***************************************************************************/

#include "Gui.h"

int GuiSound::defaultPCMVolume = 100;
int GuiSound::defaultOGGVolume = 100;
GuiSound* GuiSound::playingOGG = nullptr;

GuiSound::GuiSound() { }

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
	if(type == SOUND::OGG) {
		platform->getAudio()->stopStream();
		if (playingOGG == this) {
			playingOGG = nullptr;
		}
	}
}

void GuiSound::play()
{
	int typeVol = (type == SOUND::PCM) ? defaultPCMVolume : defaultOGGVolume;
	int vol = 255 * (volume / 100.0) * (typeVol / 100.0);

	switch(type)
	{
		case SOUND::PCM:
			voice = platform->getAudio()->playVoice(sound, length, vol);
			break;
		case SOUND::OGG:
			playingOGG = this;
			voice = 0;
			platform->getAudio()->playStream(sound, length, loop, vol);
			break;
		case SOUND::NONE:
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
			if (playingOGG == this) {
				playingOGG = nullptr;
			}
			break;
		case SOUND::NONE:
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
		case SOUND::NONE:
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
		case SOUND::NONE:
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
		case SOUND::NONE:
			return false;
	}

	return false;
}

void GuiSound::setVolume(int vol)
{
	volume = vol;

	if(voice < 0)
		return;

	int typeVol = (type == SOUND::PCM) ? defaultPCMVolume : defaultOGGVolume;
	int newvol = 255 * (volume / 100.0) * (typeVol / 100.0);

	switch(type)
	{
		case SOUND::PCM:
			platform->getAudio()->setVoiceVolume(voice, newvol);
			break;
		case SOUND::OGG:
			platform->getAudio()->setStreamVolume(newvol);
			break;
		case SOUND::NONE:
			break;
	}
}

void GuiSound::setLoop(bool l)
{
	loop = l;
}

void GuiSound::setDefaultVolume(SOUND t, int v)
{
	if (t == SOUND::PCM) {
		defaultPCMVolume = v;
	} else if (t == SOUND::OGG) {
		defaultOGGVolume = v;

		if (playingOGG && playingOGG->isPlaying()) {
			playingOGG->setVolume(playingOGG->volume);
		}
	}
}
