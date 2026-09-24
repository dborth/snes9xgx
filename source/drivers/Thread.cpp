/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Thread.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include <unistd.h>

#include "Platform.h"
#include "Time.h"
#include "Logger.h"

//!Guards the JoinAll() registry (registryHead / each Thread's registryNext).
static Mutex & RegistryLock() { static Mutex m; return m; }
static Thread * registryHead = nullptr;

//!Guards parkFlag transitions and the parked/joining flags; checkpoint()
//!sleeps on gateCond. One shared cond (broadcast) - the set is tiny.
static Mutex & GateLock() { static Mutex m; return m; }
static Cond & GateCond() { static Cond c; return c; }
volatile bool Thread::parkFlag = false;

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

	// construct the function-local statics here, not on first use from
	// inside some thread
	GateLock();
	GateCond();

	// Reset before the thread exists - once it's running it may already be
	// in checkpoint()/stopRequested().
	stopFlag = false;
	joining = false;
	parked = false;
	finished = false;
	userEntry = entry;
	userArg = arg;
	wakeFn = wake;

	// handle is passed by address so the driver can publish it before the
	// new thread starts running - entry() may call back into this Thread
	// (e.g. to suspend itself) as its first action. See ThreadDriver::createThread.
	if(!platform->getThread()->createThread(Trampoline, this, stackSize, priority, &handle))
	{
		handle = nullptr;
		return false;
	}

	MutexLock guard(RegistryLock());
	registryNext = registryHead;
	registryHead = this;

	return true;
}

void * Thread::Trampoline(void * self)
{
	Thread * t = static_cast<Thread *>(self);
	void * result = t->userEntry(t->userArg);
	t->finished = true; // an exited thread can't be running anything - counts as parked
	return result;
}

//!Removes this Thread from the JoinAll() registry. Only called for a
//!Thread that has a handle (ie. is registered), so a never-started or
//!already-joined Thread - eg. a static one destroyed at exit(), after the
//!registry lock itself is gone - never touches the lock.
void Thread::registryUnlink()
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
	if(!handle)
		return;

	registryUnlink();

	// release the thread if it's parked - it must be able to run to its
	// exit for the join to complete
	{
		MutexLock gate(GateLock());
		joining = true;
		GateCond().signal();
	}

	if(platform && platform->getThread())
		platform->getThread()->joinThread(handle);
	handle = nullptr;
}

void Thread::wakeThread()
{
	if(wakeFn)
		wakeFn();
	if(wakeArgFn)
		wakeArgFn(wakeArgArg);
}

void Thread::requestStop()
{
	stopFlag = true;
	{
		MutexLock gate(GateLock());
		GateCond().signal(); // release it from checkpoint() if parked
	}
	wakeThread();
}

void Thread::checkpoint()
{
	if(!parkFlag)
		return; // fast path - one volatile read per call

	MutexLock gate(GateLock());
	if(!parkFlag || stopFlag || joining)
		return;

	parked = true;
	while(parkFlag && !stopFlag && !joining)
		GateCond().wait(GateLock());
	parked = false;
}

bool Thread::ParkAll(uint32_t timeoutMs)
{
	{
		MutexLock gate(GateLock());
		parkFlag = true;
	}

	Ticks start = SystemTime::now();
	while(true)
	{
		Thread * pending[16];
		int pendingCount = 0;
		{
			MutexLock guard(RegistryLock());
			for(Thread * t = registryHead; t && pendingCount < 16; t = t->registryNext)
			{
				if(!t->parked && !t->finished && !t->stopFlag && !t->joining)
					pending[pendingCount++] = t;
			}
		}

		if(pendingCount == 0)
			return true;

		if(SystemTime::diffMillisecs(start, SystemTime::now()) >= timeoutMs)
		{
			for(int i = 0; i < pendingCount; i++)
				LOG_WARN("ParkAll: thread '%s' did not reach a checkpoint", pending[i]->name ? pending[i]->name : "?");
			return false;
		}

		// Wake outside the registry lock, like JoinAll(): a wake callback
		// takes its subsystem's mutex. Repeated each pass, so a thread that
		// wasn't in its wait yet on the first pass still gets the signal.
		for(int i = 0; i < pendingCount; i++)
			pending[i]->wakeThread();

		usleep(1000);
	}
}

void Thread::UnparkAll()
{
	MutexLock gate(GateLock());
	if(!parkFlag)
		return;
	parkFlag = false;
	GateCond().signal(); // broadcast on every backend (see Cond)
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

	registryUnlink();

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
	if(!handle || !platform || !platform->getThread())
		return false;

	return platform->getThread()->isThreadSuspended(handle);
}

ThreadId ThreadId::current()
{
	if(platform && platform->getThread())
		return ThreadId(platform->getThread()->getCurrentThreadId());

	return ThreadId();
}
