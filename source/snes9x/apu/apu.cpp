/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <math.h>
#include "../snes9x.h"
#include "apu.h"
#include "../msu1.h"
#include "../snapshot.h"
#include "../display.h"
#include "resampler.h"

#define APU_DEFAULT_INPUT_RATE		32040
#define APU_MINIMUM_SAMPLE_COUNT	512
#define APU_MINIMUM_SAMPLE_BLOCK	128
#define APU_NUMERATOR_NTSC			15664
#define APU_DENOMINATOR_NTSC		328125
#define APU_NUMERATOR_PAL			34176
#define APU_DENOMINATOR_PAL			709379

SNES_SPC	*spc_core = NULL;

static uint8 APUROM[64] =
{
	0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
	0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
	0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
	0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
	0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
	0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
	0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
	0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

namespace spc
{
	static apu_callback	sa_callback     = NULL;
	static void			*extra_data     = NULL;

	static bool8		sound_in_sync   = TRUE;
	static bool8		sound_enabled   = FALSE;

	static int			buffer_size;
	static int			lag_master      = 0;
	static int			lag             = 0;

	static uint8		*landing_buffer = NULL;
	static uint8		*shrink_buffer  = NULL;

	static Resampler	*resampler      = NULL;

	static int32		reference_time;
	static uint32		remainder;

	static const int	timing_hack_numerator   = SNES_SPC::tempo_unit;
	static int			timing_hack_denominator = SNES_SPC::tempo_unit;
	/* Set these to NTSC for now. Will change to PAL in S9xAPUTimingSetSpeedup
	   if necessary on game load. */
	static uint32		ratio_numerator = APU_NUMERATOR_NTSC;
	static uint32		ratio_denominator = APU_DENOMINATOR_NTSC;

	static double		dynamic_rate_multiplier = 1.0;
} // namespace spc

namespace msu
{
	static int			buffer_size;
	static uint8		*landing_buffer = NULL;
	static Resampler	*resampler		= NULL;
	static int			resample_buffer_size	= -1;
	static uint8		*resample_buffer		= NULL;
} // namespace msu

static void EightBitize (uint8 *, int);
static void DeStereo (uint8 *, int);
static void ReverseStereo (uint8 *, int);
void UpdatePlaybackRate (void);
static void from_apu_to_state (uint8 **, void *, size_t);
static void to_apu_from_state (uint8 **, void *, size_t);
static void SPCSnapshotCallback (void);
static inline int S9xAPUGetClock (int32);
static inline int S9xAPUGetClockRemainder (int32);


static void EightBitize (uint8 *buffer, int sample_count)
{
	uint8	*buf8  = (uint8 *) buffer;
	int16	*buf16 = (int16 *) buffer;

	for (int i = 0; i < sample_count; i++)
		buf8[i] = (uint8) ((buf16[i] / 256) + 128);
}

static void DeStereo (uint8 *buffer, int sample_count)
{
	int16	*buf = (int16 *) buffer;
	int32	s1, s2;

	for (int i = 0; i < (sample_count >> 1); i++)
	{
		s1 = (int32) buf[2 * i];
		s2 = (int32) buf[2 * i + 1];
		buf[i] = (int16) ((s1 + s2) >> 1);
	}
}

static void ReverseStereo (uint8 *src_buffer, int sample_count)
{
	int16	*buffer = (int16 *) src_buffer;

	for (int i = 0; i < sample_count; i += 2)
	{
		buffer[i + 1] ^= buffer[i];
		buffer[i] ^= buffer[i + 1];
		buffer[i + 1] ^= buffer[i];
	}
}

bool8 S9xMixSamples (uint8 *buffer, int sample_count)
{
	static int	shrink_buffer_size = -1;
	uint8		*dest;

	if (!Settings.SixteenBitSound || !Settings.Stereo)
	{
		/* We still need both stereo samples for generating the mono sample */
		if (!Settings.Stereo)
			sample_count <<= 1;

		/* We still have to generate 16-bit samples for bit-dropping, too */
		if (shrink_buffer_size < (sample_count << 1))
		{
			delete[] spc::shrink_buffer;
			spc::shrink_buffer = new uint8[sample_count << 1];
			shrink_buffer_size = sample_count << 1;
		}

		dest = spc::shrink_buffer;
	}
	else
		dest = buffer;
    
    if (Settings.MSU1 && msu::resample_buffer_size < (sample_count << 3))
    {
        delete[] msu::resample_buffer;
        msu::resample_buffer = new uint8[sample_count << 3];
        msu::resample_buffer_size = sample_count << 3;
    }

	if (Settings.Mute)
	{
		memset(dest, 0, sample_count << 1);
		spc::resampler->clear();

		if(Settings.MSU1)
			msu::resampler->clear();

		return (FALSE);
	}
	else
	{
		if (spc::resampler->avail() >= (sample_count + spc::lag))
		{
			spc::resampler->read((short *) dest, sample_count);
			if (spc::lag == spc::lag_master)
				spc::lag = 0;

			if (Settings.MSU1)
			{
                if (msu::resampler->avail() >= sample_count)
                {
                    msu::resampler->read((short *)msu::resample_buffer, sample_count);
                    for (int32 i = 0; i < sample_count; ++i)
                        *((int16*)(dest+(i * 2))) += *((int16*)(msu::resample_buffer+(i * 2)));
				}
			}
		}
		else
		{
			memset(buffer, (Settings.SixteenBitSound ? 0 : 128), (sample_count << (Settings.SixteenBitSound ? 1 : 0)) >> (Settings.Stereo ? 0 : 1));
			if (spc::lag == 0)
				spc::lag = spc::lag_master;

			return (FALSE);
		}
	}

	if (Settings.ReverseStereo && Settings.Stereo)
		ReverseStereo(dest, sample_count);

	if (!Settings.Stereo || !Settings.SixteenBitSound)
	{
		if (!Settings.Stereo)
		{
			DeStereo(dest, sample_count);
			sample_count >>= 1;
		}

		if (!Settings.SixteenBitSound)
			EightBitize(dest, sample_count);

		memcpy(buffer, dest, (sample_count << (Settings.SixteenBitSound ? 1 : 0)));
	}

	return (TRUE);
}

int S9xGetSampleCount (void)
{
	return (spc::resampler->avail() >> (Settings.Stereo ? 0 : 1));
}

void S9xFinalizeSamples (void)
{
	bool drop_current_msu1_samples = TRUE;

	if (!Settings.Mute)
	{
		drop_current_msu1_samples = FALSE;

		if (!spc::resampler->push((short *) spc::landing_buffer, spc_core->sample_count()))
		{
			/* We weren't able to process the entire buffer. Potential overrun. */
			spc::sound_in_sync = FALSE;

			if (Settings.SoundSync && !Settings.TurboMode)
				return;

			// since we drop the current dsp samples we also want to drop generated msu1 samples
			drop_current_msu1_samples = TRUE;
		}
	}

	// only generate msu1 if we really consumed the dsp samples (sample_count() resets at end of function),
	// otherwise we will generate multiple times for the same samples - so this needs to be after all early
	// function returns
	if (Settings.MSU1)
	{
		// generate the same number of msu1 samples as dsp samples were generated
		S9xMSU1SetOutput((int16 *)msu::landing_buffer, msu::buffer_size);
		S9xMSU1Generate(spc_core->sample_count());
		if (!drop_current_msu1_samples && !msu::resampler->push((short *)msu::landing_buffer, S9xMSU1Samples()))
		{
			// should not occur, msu buffer is larger and we drop msu samples if spc buffer overruns
		}
	}

	if (!Settings.SoundSync || Settings.TurboMode || Settings.Mute)
		spc::sound_in_sync = TRUE;
	else
	if (spc::resampler->space_empty() >= spc::resampler->space_filled())
		spc::sound_in_sync = TRUE;
	else
		spc::sound_in_sync = FALSE;

	spc_core->set_output((SNES_SPC::sample_t *) spc::landing_buffer, spc::buffer_size >> 1);
}

void S9xLandSamples (void)
{
	if (spc::sa_callback != NULL)
		spc::sa_callback(spc::extra_data);
	else
		S9xFinalizeSamples();
}

void S9xClearSamples (void)
{
	spc::resampler->clear();
	if (Settings.MSU1)
		msu::resampler->clear();
	spc::lag = spc::lag_master;
}

bool8 S9xSyncSound (void)
{
	if (!Settings.SoundSync || spc::sound_in_sync)
		return (TRUE);

	S9xLandSamples();

	return (spc::sound_in_sync);
}

void S9xSetSamplesAvailableCallback (apu_callback callback, void *data)
{
	spc::sa_callback = callback;
	spc::extra_data  = data;
}

void S9xUpdateDynamicRate (double rate)
{
	if(spc::dynamic_rate_multiplier != rate) {
		spc::dynamic_rate_multiplier = rate;
		UpdatePlaybackRate();
	}
}

void UpdatePlaybackRate (void)
{
	if (Settings.SoundInputRate == 0)
		Settings.SoundInputRate = APU_DEFAULT_INPUT_RATE;

	double time_ratio = (double) Settings.SoundInputRate * spc::timing_hack_numerator / (Settings.SoundPlaybackRate * spc::timing_hack_denominator);

	if (Settings.DynamicRateControl)
	{
		time_ratio *= spc::dynamic_rate_multiplier;
	}

	spc::resampler->time_ratio(time_ratio);

	if (Settings.MSU1)
	{
		time_ratio = (44100.0 / Settings.SoundPlaybackRate) * (Settings.SoundInputRate / 32040.0);
		msu::resampler->time_ratio(time_ratio);
	}
}

bool8 S9xInitSound (int buffer_ms, int lag_ms)
{
	// buffer_ms : buffer size given in millisecond
	// lag_ms    : allowable time-lag given in millisecond

	int	sample_count     = buffer_ms * 32040 / 1000;
	int	lag_sample_count = lag_ms    * 32040 / 1000;

	spc::lag_master = lag_sample_count;
	if (Settings.Stereo)
		spc::lag_master <<= 1;
	spc::lag = spc::lag_master;

	if (sample_count < APU_MINIMUM_SAMPLE_COUNT)
		sample_count = APU_MINIMUM_SAMPLE_COUNT;

	spc::buffer_size = sample_count;
	if (Settings.Stereo)
		spc::buffer_size <<= 1;
	if (Settings.SixteenBitSound)
		spc::buffer_size <<= 1;
	msu::buffer_size = sample_count << 3; // Always 16-bit, Stereo; x2 to never overflow before dsp buffer

	printf("Sound buffer size: %d (%d samples)\n", spc::buffer_size, sample_count);

	if (spc::landing_buffer)
		delete[] spc::landing_buffer;
	spc::landing_buffer = new uint8[spc::buffer_size * 2];
	if (!spc::landing_buffer)
		return (FALSE);
	if (msu::landing_buffer)
		delete[] msu::landing_buffer;
	msu::landing_buffer = (uint8*) new uint32[msu::buffer_size / 2]; // Ensure 4-byte alignment
	if (!msu::landing_buffer)
		return (FALSE);

	/* The resampler and spc unit use samples (16-bit short) as
	   arguments. Use 2x in the resampler for buffer leveling with SoundSync */
	if (!spc::resampler)
	{
		spc::resampler = new Resampler(spc::buffer_size >> (Settings.SoundSync ? 0 : 1));
		if (!spc::resampler)
		{
			delete[] spc::landing_buffer;
			return (FALSE);
		}
	}
	else
		spc::resampler->resize(spc::buffer_size >> (Settings.SoundSync ? 0 : 1));
  

	if (!msu::resampler)
	{
		msu::resampler = new Resampler(msu::buffer_size >> (Settings.SoundSync ? 0 : 1));
		if (!msu::resampler)
		{
			delete[] msu::landing_buffer;
			return (FALSE);
		}
	}
	else
		msu::resampler->resize(msu::buffer_size);

	spc_core->set_output((SNES_SPC::sample_t *) spc::landing_buffer, spc::buffer_size >> 1);

	UpdatePlaybackRate();

	spc::sound_enabled = S9xOpenSoundDevice();

	return (spc::sound_enabled);
}

void S9xSetSoundControl (uint8 voice_switch)
{
	spc_core->dsp_set_stereo_switch(voice_switch << 8 | voice_switch);
}

void S9xSetSoundMute (bool8 mute)
{
	Settings.Mute = mute;
	if (!spc::sound_enabled)
		Settings.Mute = TRUE;
}

void S9xDumpSPCSnapshot (void)
{
	spc_core->dsp_dump_spc_snapshot();
}

static void SPCSnapshotCallback (void)
{
	S9xSPCDump(S9xGetFilenameInc((".spc"), SPC_DIR));
	printf("Dumped key-on triggered spc snapshot.\n");
}

bool8 S9xInitAPU (void)
{
	spc_core = new SNES_SPC;
	if (!spc_core)
		return (FALSE);

	spc_core->init();
	spc_core->init_rom(APUROM);

	spc_core->dsp_set_spc_snapshot_callback(SPCSnapshotCallback);

	spc::landing_buffer = NULL;
	spc::shrink_buffer  = NULL;
	spc::resampler      = NULL;
	msu::resampler		= NULL;

	return (TRUE);
}

void S9xDeinitAPU (void)
{
	if (spc_core)
	{
		delete spc_core;
		spc_core = NULL;
	}

	if (spc::resampler)
	{
		delete spc::resampler;
		spc::resampler = NULL;
	}

	if (spc::landing_buffer)
	{
		delete[] spc::landing_buffer;
		spc::landing_buffer = NULL;
	}

	if (spc::shrink_buffer)
	{
		delete[] spc::shrink_buffer;
		spc::shrink_buffer = NULL;
	}

	if (msu::resampler)
	{
		delete msu::resampler;
		msu::resampler = NULL;
	}

	if (msu::landing_buffer)
	{
		delete[] msu::landing_buffer;
		msu::landing_buffer = NULL;
	}

	if (msu::resample_buffer)
	{
		delete[] msu::resample_buffer;
		msu::resample_buffer = NULL;
	}
	
	S9xMSU1DeInit();
}

static inline int S9xAPUGetClock (int32 cpucycles)
{
	return (spc::ratio_numerator * (cpucycles - spc::reference_time) + spc::remainder) /
			spc::ratio_denominator;
}

static inline int S9xAPUGetClockRemainder (int32 cpucycles)
{
	return (spc::ratio_numerator * (cpucycles - spc::reference_time) + spc::remainder) %
			spc::ratio_denominator;
}

uint8 S9xAPUReadPort (int port)
{
	return ((uint8) spc_core->read_port(S9xAPUGetClock(CPU.Cycles), port));
}

void S9xAPUWritePort (int port, uint8 byte)
{
	spc_core->write_port(S9xAPUGetClock(CPU.Cycles), port, byte);
}

void S9xAPUSetReferenceTime (int32 cpucycles)
{
	spc::reference_time = cpucycles;
}

void S9xAPUExecute (void)
{
	/* Accumulate partial APU cycles */
	spc_core->end_frame(S9xAPUGetClock(CPU.Cycles));

	spc::remainder = S9xAPUGetClockRemainder(CPU.Cycles);

	S9xAPUSetReferenceTime(CPU.Cycles);
}

void S9xAPUEndScanline (void)
{
	S9xAPUExecute();

	if (spc_core->sample_count() >= APU_MINIMUM_SAMPLE_BLOCK || !spc::sound_in_sync)
		S9xLandSamples();
}

void S9xAPUTimingSetSpeedup (int ticks)
{
	if (ticks != 0)
		printf("APU speedup hack: %d\n", ticks);

	spc::timing_hack_denominator = SNES_SPC::tempo_unit - ticks;
	spc_core->set_tempo(spc::timing_hack_denominator);

	spc::ratio_numerator = Settings.PAL ? APU_NUMERATOR_PAL : APU_NUMERATOR_NTSC;
	spc::ratio_denominator = Settings.PAL ? APU_DENOMINATOR_PAL : APU_DENOMINATOR_NTSC;
	spc::ratio_denominator = spc::ratio_denominator * spc::timing_hack_denominator / spc::timing_hack_numerator;

	UpdatePlaybackRate();
}

void S9xAPUAllowTimeOverflow (bool allow)
{
	spc_core->spc_allow_time_overflow(allow);
}

void S9xResetAPU (void)
{
	spc::reference_time = 0;
	spc::remainder = 0;
	spc_core->reset();
	spc_core->set_output((SNES_SPC::sample_t *) spc::landing_buffer, spc::buffer_size >> 1);

	spc::resampler->clear();

	if (Settings.MSU1)
		msu::resampler->clear();
}

void S9xSoftResetAPU (void)
{
	spc::reference_time = 0;
	spc::remainder = 0;
	spc_core->soft_reset();
	spc_core->set_output((SNES_SPC::sample_t *) spc::landing_buffer, spc::buffer_size >> 1);

	spc::resampler->clear();

	if (Settings.MSU1)
		msu::resampler->clear();
}

static void from_apu_to_state (uint8 **buf, void *var, size_t size)
{
	memcpy(*buf, var, size);
	*buf += size;
}

static void to_apu_from_state (uint8 **buf, void *var, size_t size)
{
	memcpy(var, *buf, size);
	*buf += size;
}

void S9xAPUSaveState (uint8 *block)
{
	uint8	*ptr = block;

	spc_core->copy_state(&ptr, from_apu_to_state);

	SET_LE32(ptr, spc::reference_time);
	ptr += sizeof(int32);
	SET_LE32(ptr, spc::remainder);
}

void S9xAPULoadState (uint8 *block)
{
	uint8	*ptr = block;

	S9xResetAPU();

	spc_core->copy_state(&ptr, to_apu_from_state);

	spc::reference_time = GET_LE32(ptr);
	ptr += sizeof(int32);
	spc::remainder = GET_LE32(ptr);
}

bool8 S9xSPCDump (const char *filename)
{
	FILE	*fs;
	uint8	buf[SNES_SPC::spc_file_size];
	size_t	ignore;

	fs = fopen(filename, "wb");
	if (!fs)
		return (FALSE);

	S9xSetSoundMute(TRUE);

	spc_core->init_header(buf);
	spc_core->save_spc(buf);

	ignore = fwrite(buf, SNES_SPC::spc_file_size, 1, fs);

	fclose(fs);

	S9xSetSoundMute(FALSE);

	return (TRUE);
}
