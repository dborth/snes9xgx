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

//! Platform execution state - what the app may currently do.
enum class Status
{
	//! Have the foreground: drawing, prompting and saving are all fine.
	Running,
	//! Wii U: the OS has taken (or is taking) the foreground away, eg. for
	//! the HOME menu. Nothing may be drawn or prompted. May go back to Running.
	Paused,
	//! Shutting down, with the foreground still ours (the app asked to exit,
	//! or a power button was pressed). Saving is still fine, but nothing is
	//! drawn or prompted any more.
	Exiting,
	//! Wii U: the OS closed the app while it was in the background (eg. Close
	//! Software from the HOME menu). Nothing may be drawn, prompted or saved -
	//! only clean up and exit.
	Closed
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

struct PlatformConfig
{
	int canvasWidth;
	int canvasHeight;
	float assetScaleX = 1.0f;
	float assetScaleY = 1.0f;
};

//!Composition root for a platform. Owns the five concrete drivers below
//!and is the only place app code needs an `#ifdef` to pick a platform -
//!everything else goes through the abstract driver interfaces.
class Platform
{
	public:
		virtual ~Platform() = default;

		//!Constructs and initializes all five drivers for this platform.
		//!\param config GUI canvas size and asset scale for this platform
		virtual void init(const PlatformConfig& config) = 0;
		//!The PlatformConfig this platform was init()'d with.
		const PlatformConfig& getConfig() const { return config; }
		//!Tears down the platform (via shutdown()) and then performs
		//!whatever platform-appropriate action actually ends the app -
		//!return to loader/menu, power off, or just exit(), depending on
		//!how getSystemEvent() last reported and how the platform was
		//!reached. It does not return.
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

		//! Current platform lifecycle state.
		virtual Status getStatus() const = 0;
		//!Handler for setSaveHandler().
		typedef void (*SaveHandler)();
		//!Wii U only (other platforms never call it): the handler is called,
		//!on the main thread from inside getSystemEvent(), when the OS is about
		//!to take the foreground away - the HOME menu, the power button, or
		//!closing the app. It is the last chance to write to storage, so it
		//!should only save. The status is no longer Running while it runs.
		virtual void setSaveHandler(SaveHandler handler) { (void)handler; }
		//! Transitions platform state to move to Exiting.
		virtual void triggerExit() = 0;
		//!True once triggerExit() has been called, or the platform's own
		//!getSystemEvent() independently reports ShutdownRequested (eg. a
		//!hardware power button). Not every platform folds Status::Exiting
		//!into its getSystemEvent() report - GameCube has no hardware
		//!event source and always reports None, relying entirely on
		//!triggerExit() - so callers wanting to leave promptly on either
		//!signal should check this rather than either alone.
		bool shouldExit() { return isExiting() || getSystemEvent() == SystemEvent::ShutdownRequested; }
		//!True once the platform is shutting down (Exiting or Closed). Only
		//!reads the status, so any thread may call it.
		bool isExiting() const { return getStatus() == Status::Exiting || getStatus() == Status::Closed; }

	protected:
		//!Set by init() in every concrete Platform - store the passed-in
		//!config as the very first line of the override.
		PlatformConfig config{};

		//!Shuts down and releases all five drivers. Any background
		//!Thread that might still call into a driver must be
		//!stopped and joined (eg. via Thread::JoinAll()) before calling
		//!this, since the drivers it deletes may be in active use.
		virtual void shutdown() = 0;
};

//! The globally accessible platform instance
extern Platform* platform;
