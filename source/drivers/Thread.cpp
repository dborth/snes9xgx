/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Thread.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include "Platform.h"
#include "Time.h"

//!Guards the JoinAll() registry (registryHead / each Thread's registryNext).
static Mutex & RegistryLock() { static Mutex m; return m; }
static Thread * registryHead = nullptr;

Thread::~Thread()
{
	join();
}

bool Thread::start(ThreadEntry entry, void * arg, uint32_t stackSize, ThreadPriority priority, void (*wake)(void))
{
	if(handle)
		return false;

	if(!platform || !platform->getThread())
		return false;

	// Before the thread exists: it can run (and read stopRequested()) as soon as
	// createThread() returns, and the flag is still set from the last requestStop()
	stopFlag = false;
	wakeFn = wake;

	// handle is passed by address so the driver can publish it before the
	// new thread starts running - entry() may call back into this Thread
	// (e.g. to suspend itself) as its first action. See ThreadDriver::createThread.
	if(!platform->getThread()->createThread(entry, arg, stackSize, priority, &handle))
	{
		handle = nullptr;
		wakeFn = nullptr;
		return false;
	}

	MutexLock guard(RegistryLock());
	registryNext = registryHead;
	registryHead = this;

	return true;
}

void Thread::unregister()
{
	MutexLock guard(RegistryLock());
	Thread ** link = &registryHead;
	while(*link)
	{
		if(*link == this)
		{
			*link = registryNext;
			registryNext = nullptr;
			break;
		}
		link = &(*link)->registryNext;
	}
}

void Thread::join()
{
	unregister();

	if(!handle)
		return;

	if(platform && platform->getThread())
		platform->getThread()->joinThread(handle);
	handle = nullptr;
}

bool Thread::join(uint32_t timeoutMs)
{
	if(handle && platform && platform->getThread())
	{
		if(!platform->getThread()->joinThreadTimeout(handle, timeoutMs))
			return false;
	}

	unregister();
	handle = nullptr;
	return true;
}

void Thread::abandon()
{
	unregister();
	handle = nullptr; // deliberately leaked: the thread may still be running on it
}

void Thread::requestStop()
{
	stopFlag = true;
	if(wakeFn)
		wakeFn();
}

void Thread::JoinAll(uint32_t timeoutMs)
{
	const Ticks begin = SystemTime::now();

	// Ask every thread to stop first, so they wind down during the one wait rather
	// than one after another. Outside the lock: a wake callback may lock another mutex.
	Thread * pending[32];
	int count = 0;
	{
		MutexLock guard(RegistryLock());
		for(Thread * t = registryHead; t && count < 32; t = t->registryNext)
			pending[count++] = t;
	}
	for(int i = 0; i < count; i++)
		pending[i]->requestStop();

	while(true)
	{
		Thread * t;
		{
			MutexLock guard(RegistryLock());
			t = registryHead;
		}

		if(!t)
			break;

		t->requestStop(); // a no-op for those asked above

		const uint32_t elapsed = SystemTime::diffMillisecs(begin, SystemTime::now());
		const uint32_t remaining = elapsed < timeoutMs ? timeoutMs - elapsed : 0;

		if(!t->join(remaining))
			t->abandon(); // unregisters t, so the loop moves on
	}
}

void Thread::cancel()
{
	if(!handle)
		return;

	if(platform && platform->getThread())
		platform->getThread()->cancelThread(handle);
	handle = nullptr;
}

void Thread::suspend()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->suspendThread(handle);
}

void Thread::resume()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->resumeThread(handle);
}

bool Thread::isSuspended() const
{
	if(!handle && platform && platform->getThread())
		return false;

	return platform->getThread()->isThreadSuspended(handle);
}

ThreadId ThreadId::current()
{
	if(platform && platform->getThread())
		return ThreadId(platform->getThread()->getCurrentThreadId());

	return ThreadId();
}
