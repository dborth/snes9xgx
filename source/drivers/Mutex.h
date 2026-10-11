/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Mutex.h
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#pragma once

//!A simple mutual-exclusion lock. Delegates to ThreadDriver for the actual
//!platform primitive.
//!\ingroup grp_threads
class Mutex
{
	public:
		Mutex();
		~Mutex();

		Mutex(const Mutex &) = delete;
		Mutex & operator=(const Mutex &) = delete;

		//!Blocks until the mutex is acquired. Not recursive: locking twice from one thread deadlocks.
		void lock();
		//!Releases the mutex. Must be called by the thread that locked it.
		void unlock();

	protected:
		friend class Cond; //!< Cond::wait() needs the raw handle to pass to ThreadDriver::waitCond()
		void * handle = nullptr; //!< Backend-assigned mutex handle
};

//!RAII lock guard - locks on construction, unlocks on destruction. Use this
//!instead of calling Mutex::lock()/unlock() directly wherever possible, so
//!an early return or exception can't leave the mutex held.
//!\ingroup grp_threads
class MutexLock
{
	public:
		//!Locks m until this guard goes out of scope.
		explicit MutexLock(Mutex & m) : mutex(m) { mutex.lock(); }
		~MutexLock() { mutex.unlock(); }

		MutexLock(const MutexLock &) = delete;
		MutexLock & operator=(const MutexLock &) = delete;

	protected:
		Mutex & mutex; //!< The mutex held by this guard
};
