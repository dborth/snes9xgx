#ifndef GUISOUND_H
#define GUISOUND_H

#include "gui.h"

enum
{
	SOUND_PCM,
	SOUND_OGG
};

//!Sound conversion and playback. A wrapper for other sound libraries - ASND, libmad, ltremor, etc
class GuiSound
{
	public:
		//!Constructor
		//!\param s Pointer to the sound data
		//!\param l Length of sound data
		//!\param t Sound format type (SOUND_PCM or SOUND_OGG)
		GuiSound(const u8 * s, s32 l, int t);
		//!Destructor
		~GuiSound();
		//!Start sound playback
		void play();
		//!Stop sound playback
		void stop();
		//!Pause sound playback
		void pause();
		//!Resume sound playback
		void resume();
		//!Checks if the sound is currently playing
		//!\return true if sound is playing, false otherwise
		bool isPlaying();
		//!Set sound volume
		//!\param v Sound volume (0-100)
		void setVolume(int v);
		//!Set the sound to loop playback (only applies to OGG)
		//!\param l Loop (true to loop)
		void setLoop(bool l);
	protected:
		const u8 * sound; //!< Pointer to the sound data
		int type; //!< Sound format type (SOUND_PCM or SOUND_OGG)
		s32 length; //!< Length of sound data
		s32 voice; //!< Currently assigned ASND voice channel
		s32 volume; //!< Sound volume (0-100)
		bool loop; //!< Loop sound playback
};

#endif // GUISOUND_H
