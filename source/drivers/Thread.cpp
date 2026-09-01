/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Thread.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include "Platform.h"

Thread::~Thread()
{
	join();
}

bool Thread::start(ThreadEntry entry, void * arg, uint32_t stackSize, ThreadPriority priority)
{
	if(handle)
		return false;

	if(!platform || !platform->getThread())
		return false;

	// handle is passed by address so the driver can publish it before the
	// new thread starts running - entry() may call back into this Thread
	// (e.g. to suspend itself) as its first action. See ThreadDriver::createThread.
	if(!platform->getThread()->createThread(entry, arg, stackSize, priority, &handle))
		handle = nullptr;

	return handle != nullptr;
}

void Thread::join()
{
	if(!handle)
		return;

	if(platform && platform->getThread())
		platform->getThread()->joinThread(handle);
	handle = nullptr;
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
