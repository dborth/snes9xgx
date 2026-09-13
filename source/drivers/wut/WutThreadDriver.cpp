/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutThreadDriver.cpp
 *
 * Wraps wut/coreinit OSThread and OSMutex
 ***************************************************************************/
#include <coreinit/thread.h>
#include <coreinit/mutex.h>
#include <coreinit/time.h>
#include <coreinit/condition.h>
#include <malloc.h>

#include "WutThreadDriver.h"

namespace
{
	struct WutThreadHandle
	{
		OSThread * thread = nullptr;
		void * stack = nullptr;
		ThreadEntry entry = nullptr;
		void * arg = nullptr;
	};

	// OSThreadEntryPointFn is int(*)(int argc, const char ** argv). There is
	// no generic void* userdata slot in OSCreateThread - by convention (as
	// used throughout wut homebrew) the raw argv pointer itself, not *argv,
	// is repurposed to carry a single opaque payload pointer.
	int WutThreadTrampoline(int argc, const char ** argv)
	{
		(void)argc;
		WutThreadHandle * handle = reinterpret_cast<WutThreadHandle *>(argv);
		handle->entry(handle->arg);
		return 0;
	}

	int MapWutPriority(ThreadPriority priority)
	{
		switch (priority)
		{
			case ThreadPriority::Idle:         return 31;
			case ThreadPriority::Low:          return 24;
			case ThreadPriority::Normal:       return 16;
			case ThreadPriority::High:         return 10;
			case ThreadPriority::TimeCritical: return 3;
			default:                           return 16;
		}
	}
}

void WutThreadDriver::init()
{
}

void WutThreadDriver::shutdown()
{
}

bool WutThreadDriver::createThread(ThreadEntry entry, void * arg, uint32_t stackSize, ThreadPriority priority, void ** outHandle)
{
	WutThreadHandle * handle = new WutThreadHandle();
	handle->entry = entry;
	handle->arg = arg;

	// OSThread control blocks must be allocated by the caller (8-byte
	// aligned per wut's WUT_CHECK_SIZE/OSThread layout); the stack likewise
	// needs a real aligned allocation since OSCreateThread takes the raw
	// top-of-stack address, not a size hint.
	handle->thread = static_cast<OSThread *>(memalign(8, sizeof(OSThread)));
	if(!handle->thread)
	{
		delete handle;
		return false;
	}

	uint32_t alignedStackSize = (stackSize + 31) & ~31u;
	handle->stack = memalign(32, alignedStackSize);
	if(!handle->stack)
	{
		free(handle->thread);
		delete handle;
		return false;
	}

	// Publish the handle to the caller's storage BEFORE resuming the new
	// thread, same rationale as OgcThreadDriver::createThread - entry() may
	// reference *outHandle itself as its first action.
	*outHandle = handle;
	uint8_t * stackTop = static_cast<uint8_t *>(handle->stack) + alignedStackSize;
	int nativePriority = MapWutPriority(priority);

	BOOL ok = OSCreateThread(handle->thread, WutThreadTrampoline, 0, reinterpret_cast<char *>(handle),
		stackTop, alignedStackSize, nativePriority, OS_THREAD_ATTRIB_AFFINITY_ANY);

	if(!ok)
	{
		*outHandle = nullptr;
		free(handle->stack);
		free(handle->thread);
		delete handle;
		return false;
	}

	// OSCreateThread starts threads with a suspend count of 1 - they don't
	// run until explicitly resumed.
	OSResumeThread(handle->thread);

	return true;
}

void WutThreadDriver::joinThread(void * thread)
{
	if(!thread)
		return;

	WutThreadHandle * handle = static_cast<WutThreadHandle *>(thread);
	OSJoinThread(handle->thread, nullptr);
	free(handle->stack);
	free(handle->thread);
	delete handle;
}

void WutThreadDriver::cancelThread(void * thread)
{
	if(!thread)
		return;

	// OSCancelThread only requests cancellation - it takes effect the next
	// time the target thread hits a cancellation point (implicitly tested
	// in mutex/spinlock operations), so there's no way to know from here
	// when the thread is actually done touching its stack/control block.
	// Best effort, request the cancellation and intentionally leak the
	// thread/stack allocation rather than risk freeing memory the thread
	// may still be running on. Prefer signalling the thread to exit
	// cooperatively and calling joinThread() instead, which does clean up
	// properly.
	WutThreadHandle * handle = static_cast<WutThreadHandle *>(thread);
	OSCancelThread(handle->thread);
	delete handle;
}

void WutThreadDriver::suspendThread(void * thread)
{
	if(!thread)
		return;

	OSSuspendThread(static_cast<WutThreadHandle *>(thread)->thread);
}

void WutThreadDriver::resumeThread(void * thread)
{
	if(!thread)
		return;

	OSResumeThread(static_cast<WutThreadHandle *>(thread)->thread);
}

bool WutThreadDriver::isThreadSuspended(void * thread)
{
	if(!thread)
		return false;

	return OSIsThreadSuspended(static_cast<WutThreadHandle *>(thread)->thread) != FALSE;
}

void * WutThreadDriver::createMutex()
{
	OSMutex * mutex = new OSMutex();
	OSInitMutex(mutex);
	return mutex;
}

void WutThreadDriver::destroyMutex(void * mutex)
{
	if(!mutex)
		return;

	delete static_cast<OSMutex *>(mutex);
}

void WutThreadDriver::lockMutex(void * mutex)
{
	if(!mutex)
		return;

	OSLockMutex(static_cast<OSMutex *>(mutex));
}

void WutThreadDriver::unlockMutex(void * mutex)
{
	if(!mutex)
		return;

	OSUnlockMutex(static_cast<OSMutex *>(mutex));
}

void * WutThreadDriver::createCond()
{
	OSCondition * cond = new OSCondition();
	OSInitCond(cond);
	return cond;
}

void WutThreadDriver::destroyCond(void * cond)
{
	if(!cond)
		return;

	delete static_cast<OSCondition *>(cond);
}

void WutThreadDriver::waitCond(void * cond, void * mutex)
{
	if(!cond || !mutex)
		return;

	// OSWaitCond unlocks mutex, sleeps, and reacquires it before
	// returning - same contract as ThreadDriver::waitCond.
	OSWaitCond(static_cast<OSCondition *>(cond), static_cast<OSMutex *>(mutex));
}

void WutThreadDriver::signalCond(void * cond)
{
	if(!cond)
		return;

	// OSSignalCond already wakes every waiter (it's notify_all, not
	// notify_one) - matches ThreadDriver::signalCond's contract exactly.
	OSSignalCond(static_cast<OSCondition *>(cond));
}


void WutThreadDriver::sleepMilliseconds(uint32_t ms)
{
	OSSleepTicks(OSMillisecondsToTicks(ms));
}

uintptr_t WutThreadDriver::getCurrentThreadId()
{
	// OSGetCurrentThread() returns the OSThread* of the calling thread -
	// including the app's original/main thread, which (unlike threads
	// created via createThread() above) was never itself heap-allocated
	// by this driver. The pointer alone is a stable, comparable identity
	// for as long as that thread exists, which is all ThreadId needs.
	return reinterpret_cast<uintptr_t>(OSGetCurrentThread());
}
