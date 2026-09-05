/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Time.h
 *
 * Platform-agnostic monotonic timing.
 ***************************************************************************/
#pragma once

#include <cstdint>

#if defined(__WIIU__)
#include <coreinit/time.h>
#else
#include <ogc/lwp_watchdog.h>
#endif

//!An opaque monotonic timestamp, returned by SystemTime::now(). Not
//!wall-clock time, has no defined epoch.
typedef uint64_t Ticks;

class SystemTime
{
	public:
		//!\return the current monotonic timestamp
		static inline Ticks now()
		{
			#if defined(__WIIU__)
			return (Ticks)OSGetSystemTime();
			#else
			return (Ticks)gettime();
			#endif
		}

		//!\return whole seconds elapsed between two now() samples
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffSecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToSeconds((int64_t)(end - start));
			#else
			return diff_sec(start, end);
			#endif
		}

		//!\return whole milliseconds elapsed between two now() samples
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffMillisecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToMilliseconds((int64_t)(end - start));
			#else
			return diff_msec(start, end);
			#endif
		}

		//!\return whole microseconds elapsed between two now() samples.
		//!This is the granularity an emulator core's frame-sync loop
		//!actually needs.
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffMicrosecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToMicroseconds((int64_t)(end - start));
			#else
			return diff_usec(start, end);
			#endif
		}
};
