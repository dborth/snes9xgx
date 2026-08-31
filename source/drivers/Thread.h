/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Thread.h
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#pragma once

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
		//!\return true on success, false if a thread is already running or
		//!the backend failed to create one
		bool start(ThreadEntry entry, void * arg = nullptr, uint32_t stackSize = 8192, ThreadPriority priority = ThreadPriority::Normal);
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

	protected:
		void * handle; //!< Backend-assigned thread handle
};
