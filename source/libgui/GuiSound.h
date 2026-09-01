/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiSound.h
 ***************************************************************************/
#pragma once

enum class SOUND {
	PCM,
	OGG
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

		//!Set global default volume for a specific sound type
		//!\param t Sound format type (PCM or OGG)
		//!\param v Volume (0-100)
		static void setDefaultVolume(SOUND t, int v);

	protected:
		const uint8_t * sound; //!< Pointer to the sound data
		SOUND type; //!< Sound format type (PCM or OGG)
		int32_t length; //!< Length of sound data
		int32_t voice; //!< Backend-assigned voice handle (PCM only)
		int32_t volume; //!< Sound volume (0-100)
		bool loop; //!< Loop sound playback

		static int defaultPCMVolume; //!< Global PCM volume (0-100)
		static int defaultOGGVolume; //!< Global OGG volume (0-100)
		static GuiSound* playingOGG; //!< Pointer to the active OGG instance
};
