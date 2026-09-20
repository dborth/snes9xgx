/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Trace.cpp
 *
 * See Trace.h
 ***************************************************************************/
#include "Trace.h"

#if LOGGING_ENABLED

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "Platform.h"
#include "Thread.h"
#include "Mutex.h"
#include "Time.h"

#define TRACE_MAX_THREADS   12
#define TRACE_RING_SIZE     256  // phase changes remembered, across all threads
#define TRACE_DUMP_EVENTS   64   // how many of those a stall report replays
#define TRACE_SLOW_PHASE_MS 500  // a phase this long is reported when it ends
#define TRACE_STALL_MS      3000 // a phase this long is reported while it is still going on
#define TRACE_REPEAT_MS     5000 // ...and again this often
#define WATCHDOG_STACKSIZE  (16 * 1024)

struct TraceSlot
{
	ThreadId id;
	const char * name;
	const char * volatile where;
	volatile uint32_t since; // NowMs() when it entered `where`
};

struct TraceEvent
{
	uint32_t when; // NowMs()
	int slot;
	const char * volatile where;
};

static TraceSlot slots[TRACE_MAX_THREADS];
static volatile int slotCount = 0; // slots[0..slotCount) are filled in
static TraceEvent ring[TRACE_RING_SIZE];
static volatile uint32_t ringPos = 0; // total events ever added
static void (*stateFn)(char *, size_t) = nullptr;
static Thread watchdogThread;
static Ticks epoch = 0;
static bool epochSet = false;

static Mutex & RegisterLock() { static Mutex m; return m; }

//! Milliseconds since the first thread registered. 32 bits so it can be read
//! and written atomically on the 32-bit CPUs.
static uint32_t NowMs()
{
	return epochSet ? SystemTime::diffMillisecs(epoch, SystemTime::now()) : 0;
}

static bool IsIdle(const char * where)
{
	return !where || strcmp(where, "idle") == 0;
}

static int FindSlotIndex()
{
	ThreadId me = ThreadId::current();
	int count = slotCount;

	for(int i = 0; i < count; i++)
		if(slots[i].id == me)
			return i;

	return -1;
}

static void RingAdd(int slot, const char * where, uint32_t now)
{
	uint32_t index = __atomic_fetch_add(&ringPos, 1, __ATOMIC_RELAXED) % TRACE_RING_SIZE;
	ring[index].when = now;
	ring[index].slot = slot;
	ring[index].where = where;
}

void TraceThread(const char * name)
{
	MutexLock guard(RegisterLock());

	if(!epochSet)
	{
		epoch = SystemTime::now();
		epochSet = true;
	}

	if(slotCount >= TRACE_MAX_THREADS)
		return;

	TraceSlot & slot = slots[slotCount];
	slot.id = ThreadId::current();
	slot.name = name;
	slot.where = "idle";
	slot.since = NowMs();
	slotCount = slotCount + 1;
}

void TraceAt(const char * where)
{
	int index = FindSlotIndex();
	if(index < 0)
		return;

	TraceSlot & slot = slots[index];
	uint32_t now = NowMs();
	const char * previous = slot.where;

	if(previous == where) // eg. a per-frame heartbeat: just proves the thread is alive
	{
		slot.since = now;
		return;
	}

	uint32_t ms = now - slot.since;

	slot.where = where;
	slot.since = now;
	RingAdd(index, where, now);

	if(ms >= TRACE_SLOW_PHASE_MS && !IsIdle(previous))
		LOG_WARN("[trace] [%s] '%s' took %u ms (now '%s')", slot.name, previous, (unsigned)ms, where);
}

void TraceLog(const char * fmt, ...)
{
	char message[192];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	int index = FindSlotIndex();
	LOG_INFO("[%s] %s", index >= 0 ? slots[index].name : "?", message);
}

void TraceSetStateFn(void (*fn)(char *, size_t))
{
	stateFn = fn;
}

static void DumpEverything(uint32_t now)
{
	int count = slotCount;

	for(int i = 0; i < count; i++)
		LOG_ERROR("[trace]   [%s] '%s' for %u ms", slots[i].name, slots[i].where ? slots[i].where : "?", (unsigned)(now - slots[i].since));

	if(stateFn)
	{
		char state[192] = { 0 };
		stateFn(state, sizeof(state));
		LOG_ERROR("[trace]   state: %s", state);
	}
}

static void DumpHistory(uint32_t now)
{
	uint32_t total = ringPos;
	uint32_t events = total < TRACE_DUMP_EVENTS ? total : TRACE_DUMP_EVENTS;

	LOG_ERROR("[trace]   last %u phase changes, oldest first:", (unsigned)events);

	for(uint32_t n = events; n > 0; n--)
	{
		const TraceEvent & event = ring[(total - n) % TRACE_RING_SIZE];
		if(!event.where || event.slot < 0 || event.slot >= slotCount)
			continue;

		LOG_ERROR("[trace]     -%5u ms [%s] %s", (unsigned)(now - event.when), slots[event.slot].name, event.where);
	}
}

static void * watchdog(void *)
{
	uint32_t nextReport[TRACE_MAX_THREADS];
	uint32_t reportedSince[TRACE_MAX_THREADS];
	bool historyDumped = false;

	for(int i = 0; i < TRACE_MAX_THREADS; i++)
	{
		nextReport[i] = TRACE_STALL_MS;
		reportedSince[i] = 0;
	}

	while(!watchdogThread.stopRequested())
	{
		usleep(250000);

		uint32_t now = NowMs();
		int count = slotCount;
		bool anyStalled = false;

		for(int i = 0; i < count; i++)
		{
			uint32_t since = slots[i].since;
			const char * where = slots[i].where;

			if(since != reportedSince[i]) // moved on since the last report
			{
				reportedSince[i] = since;
				nextReport[i] = TRACE_STALL_MS;
			}

			if(IsIdle(where))
				continue;

			uint32_t ms = now - since;
			if(ms >= nextReport[i])
			{
				nextReport[i] = ms + TRACE_REPEAT_MS;
				anyStalled = true;
				LOG_ERROR("[trace] STALL [%s] in '%s' for %u ms", slots[i].name, where, (unsigned)ms);
			}
		}

		if(anyStalled)
		{
			DumpEverything(now);

			if(!historyDumped) // once - the history is what led up to the first stall
			{
				historyDumped = true;
				DumpHistory(now);
			}
		}
	}
	return nullptr;
}

void TraceStartWatchdog()
{
	watchdogThread.start(watchdog, nullptr, WATCHDOG_STACKSIZE, ThreadPriority::Normal);
}

#endif // LOGGING_ENABLED
