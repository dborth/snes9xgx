/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiSoundOggPlayer.cpp
 ***************************************************************************/
#include "GuiSoundOggPlayer.h"
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include <ogc/cache.h>

GuiSoundOggPlayer::GuiSoundOggPlayer() : threadRunning(false), streamPaused(false), sampleRate(0), channels(0) {
	pcmBuffer[0] = (uint8_t*)memalign(32, BUFFER_SIZE);
	pcmBuffer[1] = (uint8_t*)memalign(32, BUFFER_SIZE);
	bufferReady[0] = false;
	bufferReady[1] = false;
	decodeIndex = 0;
	playIndex = 0;
}

GuiSoundOggPlayer::~GuiSoundOggPlayer() {
	stop();
	free(pcmBuffer[0]);
	free(pcmBuffer[1]);
}

size_t GuiSoundOggPlayer::readOgg(void* ptr, size_t size, size_t nmemb, void* datasource) {
	MemFile* mem = static_cast<MemFile*>(datasource);
	size_t bytesToRead = size * nmemb;
	if (mem->pos >= mem->size) return 0;
	if (static_cast<size_t>(mem->pos) + bytesToRead > static_cast<size_t>(mem->size)) {
		bytesToRead = static_cast<size_t>(mem->size - mem->pos);
	}
	memcpy(ptr, mem->data + mem->pos, bytesToRead);
	mem->pos += bytesToRead;
	return bytesToRead / size;
}

int GuiSoundOggPlayer::seekOgg(void* datasource, ogg_int64_t offset, int whence) {
	MemFile* mem = static_cast<MemFile*>(datasource);
	switch (whence) {
		case SEEK_SET: mem->pos = offset; break;
		case SEEK_CUR: mem->pos += offset; break;
		case SEEK_END: mem->pos = mem->size + offset; break;
	}
	if (mem->pos < 0) mem->pos = 0;
	if (mem->pos > mem->size) mem->pos = mem->size;
	return 0;
}

int GuiSoundOggPlayer::closeOgg(void*) { return 0; }
long GuiSoundOggPlayer::tellOgg(void* datasource) { return static_cast<MemFile*>(datasource)->pos; }

bool GuiSoundOggPlayer::play(const uint8_t* data, int32_t length, int time_pos, bool loop) {
	stop();

	memFile.data = data;
	memFile.size = length;
	memFile.pos = 0;

	ov_callbacks cb = { readOgg, seekOgg, closeOgg, tellOgg };
	if (ov_open_callbacks(&memFile, &vf, nullptr, 0, cb) < 0) {
		return false;
	}

	vorbis_info* vi = ov_info(&vf, -1);
	sampleRate = vi->rate;
	channels = vi->channels;
	streamLoop = loop;

	if (time_pos > 0) ov_time_seek(&vf, time_pos);

	bufferReady[0] = false;
	bufferReady[1] = false;
	decodeIndex = 0;
	playIndex = 0;

	threadRunning = true;
	streamPaused = false;

	return decodeThread.start(threadEntry, this, 16384, ThreadPriority::High);
}

void GuiSoundOggPlayer::stop() {
	if (threadRunning) {
		threadRunning = false;
		decodeThread.join();
		ov_clear(&vf);
	}
}

void GuiSoundOggPlayer::pause(bool pause) {
	streamPaused = pause;
}

const uint8_t* GuiSoundOggPlayer::getReadyBuffer(int32_t* outSize) {
	if (bufferReady[playIndex]) {
		*outSize = pcmBufferSize[playIndex];
		return pcmBuffer[playIndex];
	}
	return nullptr;
}

void GuiSoundOggPlayer::consumeBuffer() {
	bufferReady[playIndex] = false;
	playIndex ^= 1;
}

void* GuiSoundOggPlayer::threadEntry(void* arg) {
	static_cast<GuiSoundOggPlayer*>(arg)->threadLoop();
	return nullptr;
}

void GuiSoundOggPlayer::threadLoop() {
	while (threadRunning) {
		if (streamPaused) {
			usleep(10000);
			continue;
		}

		if (!bufferReady[decodeIndex]) {
			int bytesRead = 0;
			// Accumulate data until the buffer is full or the stream ends
			while (bytesRead < BUFFER_SIZE && threadRunning && !streamPaused) {
				int currentSection = 0;
				long ret = ov_read(&vf, (char*)pcmBuffer[decodeIndex] + bytesRead, BUFFER_SIZE - bytesRead, &currentSection);

				if (ret == 0) {
					if (streamLoop) {
						ov_pcm_seek(&vf, 0);
					} else {
						if (bytesRead == 0) threadRunning = false;
						break;
					}
				} else if (ret < 0) {
					if (ret != OV_HOLE && !streamLoop) {
						if (bytesRead == 0) threadRunning = false;
						break;
					}
				} else {
					bytesRead += ret;
				}
			}

			if (bytesRead > 0) {
				DCFlushRange(pcmBuffer[decodeIndex], bytesRead);
				pcmBufferSize[decodeIndex] = bytesRead;
				bufferReady[decodeIndex] = true;
				decodeIndex ^= 1;
			} else if (!threadRunning) {
				break;
			}
		} else {
			usleep(2000); // 2ms hardware wait
		}
	}
}
