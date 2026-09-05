/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcThreadDriver.cpp
 *
 * Wraps libogc LWP threads/mutexes. lwp_t/mutex_t are opaque integer
 * handles on this platform, so each is boxed in a small heap struct to
 * satisfy the void* handle contract used by ThreadDriver.
 ***************************************************************************/
#include <ogcsys.h>
#include <unistd.h>
#include <ogc/cond.h>

#include "OgcThreadDriver.h"

namespace
{
	struct OgcThreadHandle
	{
		lwp_t thread = LWP_THREAD_NULL;
		ThreadEntry entry = nullptr;
		void * arg = nullptr;
	};

	void * OgcThreadTrampoline(void * arg)
	{
		OgcThreadHandle * handle = static_cast<OgcThreadHandle *>(arg);
		return handle->entry(handle->arg);
	}

	int MapOgcPriority(ThreadPriority priority)
	{
		switch (priority)
		{
			case ThreadPriority::Idle:         return 0;
			case ThreadPriority::Low:          return 32;
			case ThreadPriority::Normal:       return 64;
			case ThreadPriority::High:         return 80;
			case ThreadPriority::TimeCritical: return 110;
			default:                           return 64;
		}
	}
}

void OgcThreadDriver::init()
{
}

void OgcThreadDriver::shutdown()
{
}

bool OgcThreadDriver::createThread(ThreadEntry entry, void * arg, uint32_t stackSize, ThreadPriority priority, void ** outHandle)
{
	OgcThreadHandle * handle = new OgcThreadHandle();
	handle->entry = entry;
	handle->arg = arg;

	// Publish the handle to the caller's storage BEFORE spawning the LWP
	// thread. entry() may reference *outHandle itself (e.g. Thread::suspend()
	// called from within its own thread) as its very first action, and LWP
	// can begin running the new thread before LWP_CreateThread returns here -
	// so outHandle must already be valid by that point, not assigned by the
	// caller afterwards.
	*outHandle = handle;

	int nativePriority = MapOgcPriority(priority);
	int32_t res = LWP_CreateThread(&handle->thread, OgcThreadTrampoline, handle, nullptr, stackSize, nativePriority);
	if(res < 0)
	{
		*outHandle = nullptr;
		delete handle;
		return false;
	}

	return true;
}

void OgcThreadDriver::joinThread(void * thread)
{
	if(!thread)
		return;

	OgcThreadHandle * handle = static_cast<OgcThreadHandle *>(thread);
	LWP_JoinThread(handle->thread, nullptr);
	delete handle;
}

void OgcThreadDriver::cancelThread(void * thread)
{
	if(!thread)
		return;

	// libogc/LWP has no true thread-cancel primitive (there is no
	// LWP_ThreadCancel - even libogc's own pthread shim leaves
	// pthread_cancel unimplemented). Best effort: permanently suspend the
	// thread so it stops running. This does NOT reclaim its stack/context,
	// so callers should prefer signalling the thread to exit cooperatively
	// and calling joinThread() instead, which does clean up properly.
	OgcThreadHandle * handle = static_cast<OgcThreadHandle *>(thread);
	LWP_SuspendThread(handle->thread);
	delete handle;
}

void OgcThreadDriver::suspendThread(void * thread)
{
	if(!thread)
		return;

	LWP_SuspendThread(static_cast<OgcThreadHandle *>(thread)->thread);
}

void OgcThreadDriver::resumeThread(void * thread)
{
	if(!thread)
		return;

	LWP_ResumeThread(static_cast<OgcThreadHandle *>(thread)->thread);
}

bool OgcThreadDriver::isThreadSuspended(void * thread)
{
	if(!thread)
		return false;

	return LWP_ThreadIsSuspended(static_cast<OgcThreadHandle *>(thread)->thread) != 0;
}

void * OgcThreadDriver::createMutex()
{
	mutex_t * mutex = new mutex_t;
	if(LWP_MutexInit(mutex, false) < 0)
	{
		delete mutex;
		return nullptr;
	}

	return mutex;
}

void OgcThreadDriver::destroyMutex(void * mutex)
{
	if(!mutex)
		return;

	mutex_t * m = static_cast<mutex_t *>(mutex);
	LWP_MutexDestroy(*m);
	delete m;
}

void OgcThreadDriver::lockMutex(void * mutex)
{
	if(!mutex)
		return;

	LWP_MutexLock(*static_cast<mutex_t *>(mutex));
}

void OgcThreadDriver::unlockMutex(void * mutex)
{
	if(!mutex)
		return;

	LWP_MutexUnlock(*static_cast<mutex_t *>(mutex));
}

void * OgcThreadDriver::createCond()
{
	cond_t * cond = new cond_t;
	if(LWP_CondInit(cond) < 0)
	{
		delete cond;
		return nullptr;
	}

	return cond;
}

void OgcThreadDriver::destroyCond(void * cond)
{
	if(!cond)
		return;

	cond_t * c = static_cast<cond_t *>(cond);
	LWP_CondDestroy(*c);
	delete c;
}

void OgcThreadDriver::waitCond(void * cond, void * mutex)
{
	if(!cond || !mutex)
		return;

	LWP_CondWait(*static_cast<cond_t *>(cond), *static_cast<mutex_t *>(mutex));
}

void OgcThreadDriver::signalCond(void * cond)
{
	if(!cond)
		return;

	// Broadcast rather than LWP_CondSignal
	LWP_CondBroadcast(*static_cast<cond_t *>(cond));
}

void OgcThreadDriver::sleepMilliseconds(uint32_t ms)
{
	usleep(ms * 1000);
}

uintptr_t OgcThreadDriver::getCurrentThreadId()
{
	// lwp_t is an opaque libogc handle that already uniquely identifies
	// the calling thread (including the app's original/main thread,
	// which was never created via createThread() above) - just widen it.
	return (uintptr_t)LWP_GetSelf();
}
