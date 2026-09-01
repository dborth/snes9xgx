/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * ThreadDriver.h
 *
 * Platform threading backend Thread and Mutex delegate to. Exactly one
 * driver implements this and assigns the single global instance. Core and
 * application code should not implement against ThreadDriver directly -
 * use the Thread/Mutex wrappers in libgui/Thread.h instead.
 ***************************************************************************/
#pragma once

#include <stdint.h>

//! Entry point signature for a platform thread. Returns a value that is
//! discarded (kept simply for symmetry with libogc/LWP conventions).
typedef void* (*ThreadEntry)(void* arg);

#include "Thread.h"
#include "Mutex.h"

class ThreadDriver
{
	public:
		virtual ~ThreadDriver() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;

		//!Creates and starts a thread running entry(arg). Writes the new
		//!handle to *outHandle and returns true on success, or leaves
		//!*outHandle untouched and returns false on failure.
		//!
		//!*outHandle is written before the new thread is scheduled to run,
		//!not after this call returns to the caller - entry() is allowed
		//!to reference *outHandle itself (e.g. to suspend/query its own
		//!thread) without racing the assignment.
		virtual bool createThread(ThreadEntry entry, void * arg, uint32_t stackSize, ThreadPriority priority, void ** outHandle) = 0;
		//!Blocks until the thread exits, then releases the handle.
		virtual void joinThread(void * thread) = 0;
		//!Requests the thread stop running and releases the handle without
		//!blocking on it. This is a best-effort operation - not every
		//!backend can truly terminate a thread mid-flight (e.g. libogc/LWP
		//!has no cancel primitive, so that backend can only suspend it and
		//!leak its stack). Prefer having the thread watch its own exit
		//!condition and calling joinThread() to stop it cleanly wherever
		//!possible; reach for cancelThread() only as a last resort.
		virtual void cancelThread(void * thread) = 0;
		virtual void suspendThread(void * thread) = 0;
		virtual void resumeThread(void * thread) = 0;
		virtual bool isThreadSuspended(void * thread) = 0;

		//!Creates an unlocked mutex. Returns an opaque backend-defined
		//!handle, or nullptr on failure.
		virtual void * createMutex() = 0;
		virtual void destroyMutex(void * mutex) = 0;
		virtual void lockMutex(void * mutex) = 0;
		virtual void unlockMutex(void * mutex) = 0;

		//!Creates a condition variable. Returns an opaque backend-defined
		//!handle, or nullptr on failure.
		virtual void * createCond() = 0;
		virtual void destroyCond(void * cond) = 0;
		//!Atomically unlocks mutex and blocks the calling thread until
		//!signalCond() is called, then reacquires mutex before returning.
		//!mutex must be locked by the calling thread on entry.
		virtual void waitCond(void * cond, void * mutex) = 0;
		//!Wakes every thread currently blocked in waitCond() on this
		//!condition variable. This is a broadcast-only operation.
		virtual void signalCond(void * cond) = 0;

		//!Sleeps the calling thread for at least the given duration.
		virtual void sleepMilliseconds(uint32_t ms) = 0;
};
