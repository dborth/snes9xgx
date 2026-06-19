/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric 2008-2023
 *
 * s9xsupport.cpp
 *
 * Snes9x support functions
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>

#include "snes9x/port.h"
#include "snes9xgx.h"
#include "video.h"
#include "audio.h"
#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/display.h"
#include "snes9x/apu/apu.h"
#include "snes9x/controls.h"

#define MAX_MESSAGE_LEN (36 * 3)

static long long prev;
static long long now;

/*** Miscellaneous Functions ***/
void S9xExit()
{
	ExitApp();
}

void S9xMessage(int /*type */, int /*number */, const char *message)
{
	static char buffer[MAX_MESSAGE_LEN + 1];
	snprintf(buffer, MAX_MESSAGE_LEN, "%s", message);
	S9xSetInfoString(buffer);
}

void S9xAutoSaveSRAM()
{

}

/*** Sound based functions ***/
void S9xToggleSoundChannel(int c)
{
    static int sound_switch = 255;

    if (c == 8)
        sound_switch = 255;
    else
        sound_switch ^= 1 << c;

    S9xSetSoundControl (sound_switch);
}

/****************************************************************************
 * OpenSoundDevice
 *
 * Main initialisation for Wii sound system
 ***************************************************************************/
bool8 S9xOpenSoundDevice(void)
{
	InitAudio();
	return TRUE;
}

/* eke-eke */
void S9xInitSync()
{
	FrameTimer = 0;
	prev = gettime();
}

/*** Synchronisation ***/

/*
 * Decide whether to render or skip the current frame.
 *
 * behindSchedule is true when emulation is running behind the target rate and
 * a frame may be dropped to catch up. Frames are only ever skipped up to the
 * configured limit so that the display cannot stall indefinitely.
 */
static void S9xChooseFrameToRender(bool behindSchedule, int32 skipFrms)
{
	if (behindSchedule && (IPPU.SkippedFrames < skipFrms))
	{
		IPPU.SkippedFrames++;
		IPPU.RenderThisFrame = FALSE;
	}
	else
	{
		IPPU.SkippedFrames = 0;
		IPPU.RenderThisFrame = TRUE;
	}
}

void S9xSyncSpeed () {
	const int32 skipFrms = Settings.TurboMode
		? (int32) Settings.TurboSkipFrames
		: (int32) Settings.SkipFrames;

	if (timerstyle == 0) /* use Wii vertical sync (VSYNC) with NTSC roms */
	{
		int32 pendingFrames;
		while ((pendingFrames = FrameTimer) == 0)
		{
			usleep(50);
		}

		if (pendingFrames > skipFrms)
		{
			FrameTimer = skipFrms;
			pendingFrames = skipFrms;
		}

		S9xChooseFrameToRender(pendingFrames > 1, skipFrms);
	}
	else /* use internal timer for PAL roms */
	{
		const u32 timediffallowed = Settings.TurboMode ? 0 : Settings.FrameTime;

		now = gettime();

		if (diff_usec(prev, now) < timediffallowed)
		{
			/*** Ahead - so hold up until the frame's time budget elapses ***/
			do
			{
				usleep(50);
				now = gettime();
			} while (diff_usec(prev, now) < timediffallowed);

			IPPU.RenderThisFrame = TRUE;
			IPPU.SkippedFrames = 0;
		}
		else
		{
			/* Timer has already expired - we are behind, so consider skipping. */
			S9xChooseFrameToRender(true, skipFrms);
		}

		prev = now;
	}

	if (!Settings.TurboMode)
		FrameTimer--;
}

/*** Video / Display related functions ***/
bool8 S9xInitUpdate()
{
	return (TRUE);
}

bool8 S9xDeinitUpdate(int Width, int Height)
{
	update_video(Width, Height);
	return (TRUE);
}

bool8 S9xContinueUpdate(int Width, int Height)
{
	return (TRUE);
}

/*** Input functions ***/
void S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, int16 data2)
{
	return;
}

bool S9xPollButton(uint32 id, bool * pressed)
{
	return 0;
}

bool S9xPollAxis(uint32 id, int16 * value)
{
	return 0;
}

bool S9xPollPointer(uint32 id, int16 * x, int16 * y)
{
	return 0;
}

/****************************************************************************
 * Note that these are DUMMY functions, and only allow Snes9x to
 * compile. Where possible, they will return an error signal.
 ***************************************************************************/

const char * S9xGetDirectory(enum s9x_getdirtype dirtype)
{
	ExitApp();
	return NULL;
}

const char * S9xGetFilename(const char *ex, enum s9x_getdirtype dirtype)
{
	ExitApp();
	return NULL;
}

const char * S9xGetFilenameInc(const char *e, enum s9x_getdirtype dirtype)
{
	ExitApp();
	return NULL;
}

const char * S9xBasename(const char *name)
{
	ExitApp();
	return name;
}

const char * S9xStringInput (const char * s)
{
	ExitApp();
	return s;
}

void _splitpath(char const *buf, char *drive, char *dir, char *fname, char *ext)
{
	ExitApp();
}

void _makepath(char *filename, const char *drive, const char *dir,
		const char *fname, const char *ext)
{
	ExitApp();
}

int access(const char *pathname, int mode)
{
	ExitApp();
	return 1;
}
