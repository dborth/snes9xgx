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
	handle = platform->getThread()->createMutex();
}

Mutex::~Mutex()
{
	if(handle)
		platform->getThread()->destroyMutex(handle);
}

void Mutex::lock()
{
	if(handle)
		platform->getThread()->lockMutex(handle);
}

void Mutex::unlock()
{
	if(handle)
		platform->getThread()->unlockMutex(handle);
}
