/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Thread.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include "Platform.h"

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

	// handle is passed by address so the driver can publish it before the
	// new thread starts running - entry() may call back into this Thread
	// (e.g. to suspend itself) as its first action. See ThreadDriver::createThread.
	if(!platform->getThread()->createThread(entry, arg, stackSize, priority, &handle))
	{
		handle = nullptr;
		return false;
	}

	stopFlag = false;
	wakeFn = wake;

	MutexLock guard(RegistryLock());
	registryNext = registryHead;
	registryHead = this;

	return true;
}

void Thread::join()
{
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

	if(!handle)
		return;

	if(platform && platform->getThread())
		platform->getThread()->joinThread(handle);
	handle = nullptr;
}

void Thread::requestStop()
{
	stopFlag = true;
	if(wakeFn)
		wakeFn();
}

void Thread::JoinAll()
{
	while(true)
	{
		Thread * t;
		{
			MutexLock guard(RegistryLock());
			t = registryHead;
		}

		if(!t)
			break;

		// requestStop() then join() outside the lock - entry() may itself
		// touch other Threads (eg. a wake callback locking a different
		// mutex), and join() blocks until entry() actually returns.
		t->requestStop();
		t->join(); // removes t from the registry on return
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
