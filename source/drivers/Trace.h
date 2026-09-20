/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Trace.h
 *
 * Stall diagnostics that don't disturb timing.
 *
 * Every thread records - in plain RAM, with no I/O and no locks - the name of
 * the phase it is in right now, plus a short ring of the most recent phase
 * changes across all threads. Nothing is written anywhere while things are
 * healthy. A watchdog thread reports any thread that has sat in one phase for
 * too long, together with the recent history and every other thread's phase;
 * and a phase that finished but was slow is reported when it ends
 * ("[main] 'LoadFile:fopen' took 6700 ms").
 *
 * TRACE_LOG() is the sparing exception: a handful of milestone lines that do
 * go to the log, tagged with the calling thread.
 *
 * All of it compiles to nothing unless LOGGING_ENABLED is set, so it costs
 * release builds nothing - no thread, no memory.
 *
 * Only pass string literals to TRACE_AT()/TRACE_THREAD(): the pointer itself
 * is what gets stored. Threads that are just waiting for work mark themselves
 * "idle" so they are never reported as stalled.
 *
 * Reports go through LOG_*(), which serializes on the Logger's mutex - and a
 * file backend holds that mutex while it writes to the SD card. If the stall
 * being chased is the SD card itself, a report may not get out; use the UDP
 * backend for those runs.
 ***************************************************************************/
#pragma once

#include "Logger.h"

#if LOGGING_ENABLED

#include <stddef.h>

//! Names the calling thread. Call once, first thing in the thread.
void TraceThread(const char * name);
//! The calling thread is now in phase `where` ("idle" = waiting for work).
void TraceAt(const char * where);
//! Logs one line tagged with the calling thread's name.
void TraceLog(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
//! Registers a function that describes application state (queues, flags) for
//! stall reports. It is called from the watchdog thread with no locks held, so
//! it may only read plain variables - never take a lock or do I/O.
void TraceSetStateFn(void (*fn)(char * out, size_t size));
//! Starts the stall detector thread.
void TraceStartWatchdog();

#define TRACE_THREAD(name)      TraceThread(name)
#define TRACE_AT(where)         TraceAt(where)
#define TRACE_LOG(...)          TraceLog(__VA_ARGS__)
#define TRACE_STATE_FN(fn)      TraceSetStateFn(fn)
#define TRACE_WATCHDOG()        TraceStartWatchdog()

#else

#define TRACE_THREAD(name)      ((void)0)
#define TRACE_AT(where)         ((void)0)
#define TRACE_LOG(...)          ((void)0)
#define TRACE_STATE_FN(fn)      ((void)0)
#define TRACE_WATCHDOG()        ((void)0)

#endif
