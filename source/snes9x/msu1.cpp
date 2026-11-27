/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "snes9x.h"
#include "memmap.h"
#include "display.h"
#include "msu1.h"
#include "msu1audio.h"
#include "msu1prefetch.h"
#include "apu/blargg_endian.h"
#include <fstream>
#include <sys/stat.h>

STREAM dataStream = NULL;
size_t partial_frames;

// Audio is served by a background prefetcher (see msu1prefetch.h). msu1.cpp only
// pulls frames; the track format lives behind MSU1AudioDecoder (msu1audio.h).
#ifndef MSU1_PREFETCH_BLOCKS
#if defined(HW_DOL)
#define MSU1_PREFETCH_BLOCKS 4	// 128 KB: GameCube has no spare MEM2
#else
#define MSU1_PREFETCH_BLOCKS 8	// 256 KB, ~1.5 s of audio; the worst stall measured on hardware was 123 ms
#endif
#endif

static MSU1Prefetch audioPrefetch;
static bool audioTrackOpen = false;	// a track's decoder has been handed to audioPrefetch

// Menu transitions: files are closed while the menu runs and reopened on return.
static bool msuSuspended = false, resumeData = false, resumeAudio = false;

// Sample buffer
int16 *bufPos, *bufBegin, *bufEnd;

#ifdef UNZIP_SUPPORT
static int unzFindExtension(unzFile &file, const char *ext, bool restart = TRUE, bool print = TRUE, bool allowExact = FALSE)
{
    unz_file_info	info;
    int				port, l = strlen(ext), e = allowExact ? 0 : 1;

    if (restart)
        port = unzGoToFirstFile(file);
    else
        port = unzGoToNextFile(file);

    while (port == UNZ_OK)
    {
        int		len;
        char	name[132];

        unzGetCurrentFileInfo(file, &info, name, 128, NULL, 0, NULL, 0);
        len = strlen(name);

        if (len >= l + e && strcasecmp(name + len - l, ext) == 0 && unzOpenCurrentFile(file) == UNZ_OK)
        {
            if (print)
                printf("Using msu file %s", name);

            return (port);
        }

        port = unzGoToNextFile(file);
    }

    return (port);
}
#endif

STREAM S9xMSU1OpenFile(const char *msu_ext, bool skip_unpacked)
{
#ifdef SNES9XGX
	char filename[1024];
	sprintf(filename, "%s%s%s", Memory.ROMFilePath, Memory.ROMFilename, msu_ext);
#else
	const char *filename = S9xGetFilename(msu_ext, ROMFILENAME_DIR); 
#endif
    
	STREAM file = 0;

	if (!skip_unpacked)
	{
		file = OPEN_STREAM(filename, "rb");
		if (file)
			printf("Using msu file %s.\n", filename);
	}

#ifdef UNZIP_SUPPORT
    // look for msu1 pack file in the rom or patch dir if msu data file not found in rom dir
    if (!file)
    {
        const char *zip_filename = S9xGetFilename(".msu1", ROMFILENAME_DIR);
		unzFile	unzFile = unzOpen(zip_filename);

		if (!unzFile)
		{
			zip_filename = S9xGetFilename(".msu1", PATCH_DIR);
			unzFile = unzOpen(zip_filename);
		}

        if (unzFile)
        {
            int	port = unzFindExtension(unzFile, msu_ext, true, true, true);
            if (port == UNZ_OK)
            {
                printf(" in %s.\n", zip_filename);
                file = new unzStream(unzFile);
            }
            else
                unzClose(unzFile);
        }
    }
#endif

    return file;
}

//! The one place that decides which decoder serves a track. Add new formats here.
MSU1AudioDecoder * S9xMSU1OpenAudioDecoder(uint16_t track)
{
	MSU1AudioDecoder * decoder = NULL;

	if (!decoder)
		decoder = S9xMSU1OpenPcmDecoder(track);

	return decoder;
}

static inline uint32 PosToFrame(uint32 pos)	// MSU1_AUDIO_POS is a byte offset into a .pcm: 8-byte header, 4 bytes/frame
{
	return pos >= 8 ? (pos - 8) >> 2 : 0;
}

//! Drop the current track. The worker closes its file, not us (asynchronously: no waiting).
static void AudioClose()
{
	if (audioTrackOpen)
	{
		audioPrefetch.setSource(NULL, 0);
		audioTrackOpen = false;
	}
}

static bool AudioOpen()
{
	MSU1.MSU1_STATUS |= AudioError;

	AudioClose();

	MSU1AudioDecoder * decoder = S9xMSU1OpenAudioDecoder(MSU1.MSU1_CURRENT_TRACK);
	if (!decoder)
		return false;

	if (!audioPrefetch.start(MSU1_PREFETCH_BLOCKS))
	{
		delete decoder;		// no memory for the ring: report an audio error
		return false;
	}

	audioPrefetch.setSource(decoder, 0);
	audioTrackOpen = true;

	MSU1.MSU1_AUDIO_POS = 8;
	MSU1.MSU1_STATUS &= ~AudioError;
	return true;
}

//! Reopen the current track and continue from a byte position
static void AudioReopenAt(uint32 savedPosition)
{
	if (AudioOpen())
	{
		MSU1.MSU1_AUDIO_POS = savedPosition;
		audioPrefetch.seek(PosToFrame(savedPosition));
	}
	else
	{
		MSU1.MSU1_STATUS &= ~(AudioPlaying | AudioRepeating);
		MSU1.MSU1_STATUS |= AudioError;
	}
}

static void DataClose()
{
	if (dataStream)
	{
		CLOSE_STREAM(dataStream);
		dataStream = NULL;
	}
}

static bool DataOpen()
{
	DataClose();

    dataStream = S9xMSU1OpenFile(".msu");

	if(!dataStream)
		dataStream = S9xMSU1OpenFile("msu1.rom");

	return dataStream != NULL;
}

void S9xResetMSU(void)
{
	MSU1.MSU1_STATUS		= 0;
	MSU1.MSU1_DATA_SEEK		= 0;
	MSU1.MSU1_DATA_POS		= 0;
	MSU1.MSU1_TRACK_SEEK	= 0;
	MSU1.MSU1_CURRENT_TRACK = 0;
	MSU1.MSU1_RESUME_TRACK	= 0;
	MSU1.MSU1_VOLUME		= 0;
	MSU1.MSU1_CONTROL		= 0;
	MSU1.MSU1_AUDIO_POS		= 0;
	MSU1.MSU1_RESUME_POS	= 0;


	bufPos				= 0;
	bufBegin			= 0;
	bufEnd				= 0;

	partial_frames = 0;

	DataClose();

	AudioClose();

	msuSuspended = resumeData = resumeAudio = false; // a new/reset game has nothing to resume

	Settings.MSU1 = S9xMSU1ROMExists();
}

void S9xMSU1Init(void)
{
	DataOpen();
}

void S9xMSU1DeInit(void)
{
	DataClose();
	AudioClose();
}

//! Leaving emulation for the menu: close the files. Positions live in MSU1.* so nothing else needs saving.
void S9xMSU1Suspend(void)
{
	if (msuSuspended)
		return;

	msuSuspended = true;
	resumeData = (dataStream != NULL);
	resumeAudio = audioTrackOpen;

	DataClose();
	audioPrefetch.detach();	// waits (at most one in-flight read) until the worker has closed the file
	audioTrackOpen = false;
	// The worker thread and the ring stay allocated, parked: nothing is created or freed per menu trip.
}

//! Back from the menu: reopen.
void S9xMSU1Resume(void)
{
	if (!msuSuspended)
		return;
	msuSuspended = false;

	if (resumeData && DataOpen())
		REVERT_STREAM(dataStream, MSU1.MSU1_DATA_POS, 0);

	if (resumeAudio)
		AudioReopenAt(MSU1.MSU1_AUDIO_POS);

	resumeData = resumeAudio = false;
}

bool S9xMSU1ROMExists(void)
{
    STREAM s = S9xMSU1OpenFile(".msu");
	if (s)
	{
		CLOSE_STREAM(s);
		return true;
	}
#ifdef UNZIP_SUPPORT
	char drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], def[_MAX_FNAME + 1], ext[_MAX_EXT + 1];
	_splitpath(Memory.ROMFilename, drive, dir, def, ext);
	if (!strcasecmp(ext, ".msu1"))
		return true;

	unzFile unzFile = unzOpen(S9xGetFilename(".msu1", ROMFILENAME_DIR));

	if(!unzFile)
		unzFile = unzOpen(S9xGetFilename(".msu1", PATCH_DIR));

	if (unzFile)
	{
		unzClose(unzFile);
		return true;
	}
#endif
    return false;
}

void S9xMSU1Generate(size_t sample_count)
{
	partial_frames += 4410 * (sample_count / 2);

	uint32 bufferingFrames = 0;
	audioPrefetch.setRepeat((MSU1.MSU1_STATUS & AudioRepeating) != 0);

	while ((bufPos < (bufEnd - 2)) && partial_frames >= 3204)
	{
		if ((MSU1.MSU1_STATUS & AudioPlaying) && audioTrackOpen)
		{
			int16 left, right;

			switch (audioPrefetch.pop(left, right))
			{
				case MSU1Prefetch::POP_FRAME:
					*(bufPos++) = (int32)left * MSU1.MSU1_VOLUME / 255;
					*(bufPos++) = (int32)right * MSU1.MSU1_VOLUME / 255;
					MSU1.MSU1_AUDIO_POS = 8 + (audioPrefetch.position() << 2);
					partial_frames -= 3204;
					break;

				case MSU1Prefetch::POP_BUFFERING:
					// Read-ahead not ready: silence, and the position does not advance
					*(bufPos++) = 0;
					*(bufPos++) = 0;
					partial_frames -= 3204;
					bufferingFrames++;
					break;

				case MSU1Prefetch::POP_ENDED:
					// End of a non-repeating track. The stream has already rewound to the start.
					MSU1.MSU1_STATUS &= ~(AudioPlaying | AudioRepeating);
					MSU1.MSU1_AUDIO_POS = 8;
					break;
			}
		}
		else
		{
			MSU1.MSU1_STATUS &= ~(AudioPlaying | AudioRepeating);
			partial_frames -= 3204;
			*(bufPos++) = 0;
			*(bufPos++) = 0;
		}
	}
}


uint8 S9xMSU1ReadPort(uint8 port)
{
	switch (port)
	{
	case 0:
		return MSU1.MSU1_STATUS | MSU1_REVISION;
	case 1:
    {
        if (MSU1.MSU1_STATUS & DataBusy)
            return 0;
        if (!dataStream)
            return 0;
        int data = GETC_STREAM(dataStream);
        if (data >= 0)
        {
            MSU1.MSU1_DATA_POS++;
            return data;
        }
        return 0;
    }
	case 2:
		return 'S';
	case 3:
		return '-';
	case 4:
		return 'M';
	case 5:
		return 'S';
	case 6:
		return 'U';
	case 7:
		return '1';
	}

	return 0;
}


void S9xMSU1WritePort(uint8 port, uint8 byte)
{
	switch (port)
	{
	case 0:
		MSU1.MSU1_DATA_SEEK &= 0xFFFFFF00;
		MSU1.MSU1_DATA_SEEK |= byte << 0;
		break;
	case 1:
		MSU1.MSU1_DATA_SEEK &= 0xFFFF00FF;
		MSU1.MSU1_DATA_SEEK |= byte << 8;
		break;
	case 2:
		MSU1.MSU1_DATA_SEEK &= 0xFF00FFFF;
		MSU1.MSU1_DATA_SEEK |= byte << 16;
		break;
	case 3:
		MSU1.MSU1_DATA_SEEK &= 0x00FFFFFF;
		MSU1.MSU1_DATA_SEEK |= byte << 24;
		MSU1.MSU1_DATA_POS = MSU1.MSU1_DATA_SEEK;
        if (dataStream)
        {
            REVERT_STREAM(dataStream, MSU1.MSU1_DATA_POS, 0);
        }
		break;
	case 4:
		MSU1.MSU1_TRACK_SEEK &= 0xFF00;
		MSU1.MSU1_TRACK_SEEK |= byte;
		break;
	case 5:
	{
		MSU1.MSU1_TRACK_SEEK &= 0x00FF;
		MSU1.MSU1_TRACK_SEEK |= (byte << 8);
		MSU1.MSU1_CURRENT_TRACK = MSU1.MSU1_TRACK_SEEK;

		MSU1.MSU1_STATUS &= ~AudioPlaying;
		MSU1.MSU1_STATUS &= ~AudioRepeating;

		bool audioOpened = AudioOpen();
		if (audioOpened)
		{
			if (MSU1.MSU1_CURRENT_TRACK == MSU1.MSU1_RESUME_TRACK)
			{
				MSU1.MSU1_AUDIO_POS = MSU1.MSU1_RESUME_POS;
				MSU1.MSU1_RESUME_POS = 0;
				MSU1.MSU1_RESUME_TRACK = ~0;
			}
			else
			{
				MSU1.MSU1_AUDIO_POS = 8;
			}

			audioPrefetch.seek(PosToFrame(MSU1.MSU1_AUDIO_POS));
		}
		break;
	}
	case 6:
		MSU1.MSU1_VOLUME = byte;
		break;
	case 7:
		if (MSU1.MSU1_STATUS & (AudioBusy | AudioError))
			break;

		MSU1.MSU1_STATUS = (MSU1.MSU1_STATUS & ~0x30) | ((byte & 0x03) << 4);
		audioPrefetch.setRepeat((MSU1.MSU1_STATUS & AudioRepeating) != 0);

		if ((byte & (Play | Resume)) == Resume)
		{
			MSU1.MSU1_RESUME_TRACK = MSU1.MSU1_CURRENT_TRACK;
			MSU1.MSU1_RESUME_POS = MSU1.MSU1_AUDIO_POS;
		}
		break;
	}
}

size_t S9xMSU1Samples(void)
{
	return bufPos - bufBegin;
}

void S9xMSU1SetOutput(int16 * out, size_t size)
{
	bufPos = bufBegin = out;
	bufEnd = out + size;
}

void S9xMSU1PostLoadState(void)
{
	if (msuSuspended)
	{
		// Loaded from the menu: nothing is open. Reopen with the loaded positions when we resume.
		resumeData = true;
		resumeAudio = (MSU1.MSU1_STATUS & AudioPlaying) != 0;
	}
	else
	{
		if (DataOpen())
		{
			REVERT_STREAM(dataStream, MSU1.MSU1_DATA_POS, 0);
		}

		if (MSU1.MSU1_STATUS & AudioPlaying)
			AudioReopenAt(MSU1.MSU1_AUDIO_POS);
	}

	bufPos = 0;
	bufBegin = 0;
	bufEnd = 0;

	partial_frames = 0;
}
