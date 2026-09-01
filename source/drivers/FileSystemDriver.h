/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * FileSystemDriver.h
 ***************************************************************************/
#pragma once

class FileSystemDriver
{
	public:
		virtual ~FileSystemDriver() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;
};
