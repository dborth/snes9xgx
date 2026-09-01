/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Cond.h
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#pragma once
#include "Mutex.h"

//!A condition variable. Delegates to ThreadDriver for the actual platform
//!primitive. wait() must be called with lock already held; signal() wakes
//!every waiter (see ThreadDriver::signalCond - there is no single-waiter
//!wake across backends).
class Cond
{
	public:
		Cond();
		~Cond();

		Cond(const Cond &) = delete;
		Cond & operator=(const Cond &) = delete;

		//!Unlocks lock and blocks until signal() is called, then
		//!reacquires lock before returning. lock must be locked by the
		//!calling thread on entry. Since signal() wakes every waiter,
		//!callers must re-check their wake condition in a loop.
		void wait(Mutex & lock);
		//!Wakes every thread currently blocked in wait() on this
		//!condition variable.
		void signal();

	protected:
		void * handle = nullptr; //!< Backend-assigned condition variable handle
};
