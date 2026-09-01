/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Mutex.cpp
 *
 * Generic - Everything platform-specific lives behind platform->getThread().
 ***************************************************************************/
#include "Platform.h"

Mutex::Mutex()
{
	if(platform && platform->getThread())
		handle = platform->getThread()->createMutex();
}

Mutex::~Mutex()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->destroyMutex(handle);
}

void Mutex::lock()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->lockMutex(handle);
}

void Mutex::unlock()
{
	if(handle && platform && platform->getThread())
		platform->getThread()->unlockMutex(handle);
}
