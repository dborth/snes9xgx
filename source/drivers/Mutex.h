/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Mutex.h
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#pragma once

//!A simple mutual-exclusion lock. Delegates to ThreadDriver for the actual
//!platform primitive.
class Mutex
{
	public:
		Mutex();
		~Mutex();

		Mutex(const Mutex &) = delete;
		Mutex & operator=(const Mutex &) = delete;

		void lock();
		void unlock();

	protected:
		friend class Cond; //!< Cond::wait() needs the raw handle to pass to ThreadDriver::waitCond()
		void * handle = nullptr; //!< Backend-assigned mutex handle
};

//!RAII lock guard - locks on construction, unlocks on destruction. Use this
//!instead of calling Mutex::lock()/unlock() directly wherever possible, so
//!an early return or exception can't leave the mutex held.
class MutexLock
{
	public:
		explicit MutexLock(Mutex & m) : mutex(m) { mutex.lock(); }
		~MutexLock() { mutex.unlock(); }

		MutexLock(const MutexLock &) = delete;
		MutexLock & operator=(const MutexLock &) = delete;

	protected:
		Mutex & mutex;
};
