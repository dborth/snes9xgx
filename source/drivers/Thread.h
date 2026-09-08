/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Thread.h
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#pragma once

#include <stdint.h>

#include "Mutex.h"
#include "Cond.h"

enum class ThreadPriority
{
	Idle,
	Low,
	Normal,
	High,
	TimeCritical
};

//!A platform thread. Owns at most one running/joinable backend thread at a
//!time - start() on an already-running Thread fails rather than leaking
//!the previous one.
//!
//!Every Thread that successfully start()s registers itself in a process-
//!wide list (removed again on join()) so that JoinAll() can be used as a
//!single, generic app-exit safety net: it stops and joins every thread
//!still outstanding, regardless of which subsystem started it or whether
//!that subsystem remembered to tear it down itself. This does not replace
//!a subsystem's own halt/resume pausing during normal operation - it's
//!the final guarantee that nothing is left running (and touching platform/
//!driver state) once shutdown begins in earnest.
class Thread
{
	public:
		Thread() : handle(nullptr) {}
		//!Joins the thread if it's still running, same as calling join().
		~Thread();

		Thread(const Thread &) = delete;
		Thread & operator=(const Thread &) = delete;

		//!Starts entry(arg) running on a new thread.
		//!\param entry Thread entry point
		//!\param arg Argument passed to entry
		//!\param stackSize Stack size in bytes for the new thread
		//!\param priority Backend-defined thread priority
		//!\param wake Optional callback JoinAll()/requestStop() invokes to
		//!break entry() out of whatever wait it may be parked in once a
		//!stop has been requested - eg. signalling the cond it sleeps on.
		//!Not needed if entry() only ever polls stopRequested() between
		//!short, bounded waits.
		//!\return true on success, false if a thread is already running or
		//!the backend failed to create one
		bool start(ThreadEntry entry, void * arg = nullptr, uint32_t stackSize = 8192, ThreadPriority priority = ThreadPriority::Normal, void (*wake)(void) = nullptr);
		//!Blocks until the thread exits.
		void join();
		//!Requests the thread stop at its next safe point without
		//!blocking. This is a best-effort operation and may leak backend
		//!resources (see ThreadDriver::cancelThread) - prefer signalling
		//!the thread to exit and calling join() where the entry function
		//!can cooperate.
		void cancel();
		//!Suspends thread execution. The thread itself keeps running once
		//!resumed - suspend is not a substitute for stopping it.
		void suspend();
		//!Resumes a suspended thread.
		void resume();
		//!\return true if the thread is currently suspended
		bool isSuspended() const;
		//!\return true if start() has an outstanding thread that hasn't
		//!been joined or cancelled yet
		bool isRunning() const { return handle != nullptr; }

		//!Sets the flag entry() should check (via stopRequested()) to know
		//!it should return, and invokes the wake callback passed to
		//!start(), if any. Does not block - call join() afterward to wait
		//!for entry() to actually return. Safe to call from any thread.
		void requestStop();
		//!\return true once requestStop() has been called for this Thread.
		//!entry() should check this in its loop condition(s) instead of a
		//!private bool, so a generic caller (JoinAll()) can stop threads
		//!it didn't start and knows nothing else about.
		bool stopRequested() const { return stopFlag; }

		//!Requests every currently-registered, still-running Thread stop
		//!(per its own wake callback, if any) and joins each in turn.
		//!Intended to be called once, late in app shutdown - after this
		//!returns, no background thread started via Thread::start() can
		//!still be touching platform/driver state.
		static void JoinAll();

	protected:
		void * handle = nullptr; //!< Backend-assigned thread handle
		volatile bool stopFlag = false; //!< Set by requestStop(); polled by entry() via stopRequested()
		void (*wakeFn)(void) = nullptr; //!< Optional callback to break entry() out of a wait; set by start()
		Thread * registryNext = nullptr; //!< Intrusive next-pointer for the JoinAll() registry
};

//!A lightweight, comparable identifier for a thread - including the app's
//!own main/original thread, which was never itself started via Thread and
//!so has no Thread object of its own to name it. Use this when code just
//!needs to answer "is this the same thread that did X earlier?" (eg. "is
//!this the main/GUI thread?"), not to own or join a thread.
class ThreadId
{
	public:
		ThreadId() : id(0) {}

		//!\return an identifier for the calling thread.
		static ThreadId current();

		bool operator==(const ThreadId & other) const { return id == other.id; }
		bool operator!=(const ThreadId & other) const { return !(*this == other); }

	protected:
		explicit ThreadId(uintptr_t v) : id(v) {}
		uintptr_t id;
};

//!Bundles the mutex and pair of condition variables used by the common
//!producer/consumer handshake between a background thread and its caller:
//!one side sets a flag (protected by mutex) and signals workCond to wake
//!the other; the other clears the flag and signals idleCond once it's
//!done/idle. Both conds share the single mutex that protects whatever
//!flag(s) the caller defines. Not every user needs both directions - eg.
//!a request that's only ever polled, never woken, can leave workCond
//!unused.
struct ThreadSync
{
	Mutex mutex;
	Cond  workCond; //!< signalled to wake a waiter when new work/state is available
	Cond  idleCond; //!< signalled to wake a waiter once the other side is idle/done
};
