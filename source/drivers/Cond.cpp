/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Cond.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include "Platform.h"
#include "Cond.h"

Cond::Cond()
{
	if(platform && platform->getThread())
		handle = platform->getThread()->createCond();
}

Cond::~Cond()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->destroyCond(handle);
}

void Cond::wait(Mutex & lock)
{
	if(handle && lock.handle && platform && platform->getThread())
		platform->getThread()->waitCond(handle, lock.handle);
}

void Cond::signal()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->signalCond(handle);
}
