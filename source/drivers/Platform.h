/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Platform.h
 *
 * Primary entry point
 ***************************************************************************/
#pragma once

#include "AudioDriver.h"
#include "VideoDriver.h"
#include "EmulatorVideoDriver.h"
#include "InputDriver.h"
#include "FileSystemDriver.h"
#include "ThreadDriver.h"
#include "Logger.h"

class AudioDriver;
class VideoDriver;
class InputDriver;
class FileSystemDriver;
class ThreadDriver;
class Logger;

//! Platform execution state.
enum class Status
{
	Running,
	Paused,
	Exiting
};

//!A hardware/OS-level system event a Platform can report. These are
//!mutually exclusive by construction.
enum class SystemEvent
{
	None,
	//!Power button pressed (console or, on Wii, a Wiimote) - or, on Wii U,
	//!the OS asking the app to exit. Stop running as soon as practical.
	ShutdownRequested,
	//!Reset button pressed (Wii only).
	//!Soft-reset the currently running game and keep going.
	ResetRequested,
};

//!Composition root for a platform. Owns the five concrete drivers below
//!and is the only place app code needs an `#ifdef` to pick a platform -
//!everything else goes through the abstract driver interfaces.
class Platform
{
	public:
		virtual ~Platform() = default;

		//!Constructs and initializes all five drivers for this platform.
		//!\param width Design canvas width in pixels
		//!\param height Design canvas height in pixels
		virtual void init(int width, int height) = 0;
		//!Shuts down and releases all five drivers. Does not itself end
		//!the process/return to a menu/power off - see requestExit(). Any
		//!background Thread that might still call into a driver must be
		//!stopped and joined (eg. via Thread::JoinAll()) before calling
		//!this, since the drivers it deletes may be in active use.
		virtual void shutdown() = 0;
		//!Tears down the platform (via shutdown()) and then performs
		//!whatever platform-appropriate action actually ends the app -
		//!return to loader/menu, power off, or just exit(), depending on
		//!how getSystemEvent() last reported and how the platform was
		//!reached. Callers should call this instead of shutdown() to
		//!leave the platform; it does not return.
		virtual void requestExit(int exitAction, bool autoloadedGame) = 0;

		virtual AudioDriver* getAudio() = 0;
		virtual VideoDriver* getVideo() = 0;
		virtual InputDriver* getInput() = 0;
		virtual FileSystemDriver* getFileSystem() = 0;
		virtual ThreadDriver* getThread() = 0;
		//!May return nullptr on a Platform that hasn't finished init()
		//!yet - LogPrintf()/LOG_*() already guard against this, but code
		//!calling platform->getLogger() directly should too.
		virtual Logger* getLogger() = 0;

		//!Current hardware/OS-level system event, if any. A single query
		//!rather than independent shutdown/reset flags.
		virtual SystemEvent getSystemEvent() = 0;
		
		//!Human-readable console/CPU description and free-memory summary,
		//!for an in-app diagnostics screen.
		virtual const char* getConsoleDetails() = 0;
		virtual const char* getMemoryFreeInfo() = 0;

		//! Current platform lifecycle state (Running, Paused, Exiting).
		virtual Status getStatus() const = 0;
		//! Transitions platform state to move to Exiting.
		virtual void triggerExit() = 0;
};

//! The globally accessible platform instance
extern Platform* platform;
