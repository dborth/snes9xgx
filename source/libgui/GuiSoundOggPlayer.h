/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiSoundOggPlayer.h
 ***************************************************************************/
#pragma once
#include <stdint.h>
#include <tremor/ivorbisfile.h>
#include "../drivers/ThreadDriver.h"

class GuiSoundOggPlayer {
	public:
		GuiSoundOggPlayer();
		~GuiSoundOggPlayer();

		bool play(const uint8_t *data, int32_t length, int time_pos, bool loop);
		void stop();
		void pause(bool pause);

		bool isPlaying() const {
			return threadRunning;
		}
		bool isPaused() const {
			return streamPaused;
		}
		int getSampleRate() const {
			return sampleRate;
		}
		int getChannels() const {
			return channels;
		}

		const uint8_t* getReadyBuffer(int32_t *outSize);
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
