/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../FileSystemDriver.h"

class OgcFileSystemDriver : public FileSystemDriver
{
	public:
		void init() override;
		void shutdown() override;
};
