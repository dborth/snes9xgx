/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiSoundOggPlayer.h
 ***************************************************************************/
#pragma once
#include <stdint.h>
#include <tremor/ivorbisfile.h>
#include "../drivers/ThreadDriver.h"

//!Platform-agnostic Tremor (integer OGG) decoder. Decodes into a small
//!double-buffered PCM ring on its own decode thread; the platform
//!AudioDriver that owns an instance is responsible for pulling ready
//!buffers and feeding them to hardware - this class does no audio output
//!itself.
//!\ingroup grp_core
class GuiSoundOggPlayer {
	public:
		GuiSoundOggPlayer();
		~GuiSoundOggPlayer();

		//!Starts decoding data on a new thread. Any previous playback is
		//!stopped first.
		//!\param data OGG file data. Must remain valid until stop() returns
		//!or playback finishes (the decode thread reads it directly).
		//!\param length Length of data in bytes
		//!\param time_pos Initial seek position, in milliseconds
		//!\param loop Whether to seek back to the start and keep decoding
		//!after reaching the end of the stream
		//!\return true if the decode thread was started successfully
		bool play(const uint8_t *data, int32_t length, int time_pos, bool loop);
		//!Stops playback and joins the decode thread.
		void stop();
		//!Pauses or resumes the decode thread without stopping it.
		void pause(bool pause);

		//!\return true if the decode thread is currently running (whether
		//!paused or not)
		bool isPlaying() const {
			return threadRunning;
		}
		//!\return true if the decode thread is currently paused
		bool isPaused() const {
			return streamPaused;
		}
		//!\return sample rate of the stream currently loaded, valid once play() has started decoding
		int getSampleRate() const {
			return sampleRate;
		}
		//!\return channel count (1 or 2) of the stream currently loaded, valid once play() has started decoding
		int getChannels() const {
			return channels;
		}

		//!\param outSize Filled with the ready buffer's size in bytes
		//!\return pointer to the next decoded PCM buffer, or nullptr if
		//!neither of the two ring slots is ready yet
		const uint8_t* getReadyBuffer(int32_t *outSize);
		//!Releases the buffer last returned by getReadyBuffer(), letting
		//!the decode thread reuse that ring slot.
		void consumeBuffer();

	private:
		static void* threadEntry(void *arg);
		void threadLoop();

		static size_t readOgg(void *ptr, size_t size, size_t nmemb, void *datasource);
		static int seekOgg(void *datasource, ogg_int64_t offset, int whence);
		static int closeOgg(void *datasource);
		static long tellOgg(void *datasource);

		struct MemFile {
			const uint8_t *data;
			int32_t size;
			int32_t pos;
		};

		MemFile memFile;
		OggVorbis_File vf;
		bool vfOpen; //!< true between a successful ov_open_callbacks() and its ov_clear()
		Thread decodeThread;

		volatile bool threadRunning;
		volatile bool streamPaused;
		volatile bool streamLoop;

		int sampleRate;
		int channels;

		static const int BUFFER_SIZE = 16384;
		uint8_t *pcmBuffer[2];
		int32_t pcmBufferSize[2];
		volatile bool bufferReady[2];

		volatile int decodeIndex;
		volatile int playIndex;
};
