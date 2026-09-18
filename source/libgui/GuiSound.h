/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiSound.h
 ***************************************************************************/
#pragma once

enum class SOUND {
	NONE,
	PCM,
	OGG
};

enum class VOLUME_TYPE {
	MUSIC,
	SFX
};

//!Sound conversion and playback. Generic -- delegates to audioSystem for
//!everything platform-specific.
class GuiSound
{
	public:
		//!Constructor
		//!\param s Pointer to the sound data
		//!\param l Length of sound data
		//!\param t Sound format type (PCM or OGG)
		GuiSound(const uint8_t * s, int32_t l, SOUND t);
		//!Constructor
		GuiSound();
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

		//!Set global default volume for a volume category (music or sound effects)
		//!\param t Volume category (MUSIC or SFX)
		//!\param v Volume (0-100)
		static void setDefaultVolume(VOLUME_TYPE t, int v);

	protected:
		const uint8_t * sound = nullptr; //!< Pointer to the sound data
		SOUND type = SOUND::NONE; //!< Sound format type (PCM or OGG)
		int32_t length = 0; //!< Length of sound data
		int32_t voice = -1; //!< Backend-assigned voice handle (PCM only)
		int32_t volume = 100; //!< Sound volume (0-100)
		bool loop = false; //!< Loop sound playback

		static int defaultMusicVolume; //!< Global music volume (0-100) -- applies to any looping sound
		static int defaultSfxVolume; //!< Global sound effects volume (0-100) -- applies to any non-looping sound

		//!Pointer to the GuiSound instance currently occupying the single shared hardware OGG stream.
		static GuiSound* playingOGG;

		//!Pointer to the last GuiSound with loop==true that was told to play, ie: the "background music" track
		static GuiSound* activeMusic;
};
