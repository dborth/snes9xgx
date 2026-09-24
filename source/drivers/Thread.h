/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
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

		//!Names the thread for diagnostics (ParkAll() reports by name any
		//!thread that fails to park). Must be a string literal / static.
		void setName(const char * n) { name = n; }

		//!Like the wake callback of start(), but with a user pointer, for
		//!threads owned by an object (eg. a cache) rather than a static.
		//!Invoked alongside the start() wake callback.
		void setWake(void (*fn)(void *), void * arg) { wakeArgArg = arg; wakeArgFn = fn; }

		//!Requests every registered thread park and waits until each has (or
		//!until timeoutMs).
		//!\return true if every thread is parked; false on timeout
		static bool ParkAll(uint32_t timeoutMs = 15000);
		//!Releases every parked thread. No-op if not parked.
		static void UnparkAll();
		//!\return true from the start of ParkAll() until UnparkAll(). Idle
		//!waits use this as an extra exit condition.
		static bool ParkRequested() { return parkFlag; }

		//!Safe-point for entry(): returns immediately unless a park is in
		//!effect, in which case blocks until UnparkAll(), requestStop() or
		//!join(), so stopping or joining a parked thread never deadlocks.
		void checkpoint();

	protected:
		void * handle = nullptr; //!< Backend-assigned thread handle
		volatile bool stopFlag = false; //!< Set by requestStop(); polled by entry() via stopRequested()
		volatile bool joining = false; //!< Set by join(); releases the thread from checkpoint()
		volatile bool parked = false; //!< True while blocked in checkpoint()
		volatile bool finished = false; //!< entry() has returned (thread may still need join())
		ThreadEntry userEntry = nullptr;
		void * userArg = nullptr;
		static void * Trampoline(void * self); //!< runs entry(), then sets finished
		const char * name = nullptr; //!< Diagnostic name, see setName()
		void (*wakeFn)(void) = nullptr; //!< Optional callback to break entry() out of a wait; set by start()
		void (*wakeArgFn)(void *) = nullptr; //!< see setWake()
		void * wakeArgArg = nullptr;
		static volatile bool parkFlag; //!< Process-wide: a park is in effect
		void wakeThread(); //!< Invokes both wake callbacks, if set
		void registryUnlink(); //!< Removes this Thread from the JoinAll() registry
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
