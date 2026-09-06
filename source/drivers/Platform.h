#pragma once

#include "AudioDriver.h"
#include "VideoDriver.h"
#include "EmulatorVideoDriver.h"
#include "InputDriver.h"
#include "FileSystemDriver.h"
#include "ThreadDriver.h"

class AudioDriver;
class VideoDriver;
class InputDriver;
class FileSystemDriver;
class ThreadDriver;

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

class Platform
{
	public:
		virtual ~Platform() = default;

		virtual void init(int width, int height) = 0;
		virtual void shutdown() = 0;

		virtual AudioDriver* getAudio() = 0;
		virtual VideoDriver* getVideo() = 0;
		virtual InputDriver* getInput() = 0;
		virtual FileSystemDriver* getFileSystem() = 0;
		virtual ThreadDriver* getThread() = 0;

		//!Current hardware/OS-level system event, if any. A single query
		//!rather than independent shutdown/reset flags.
		virtual SystemEvent getSystemEvent() = 0;

		//!Human-readable console/CPU description and free-memory summary,
		//!for an in-app diagnostics screen.
		virtual const char* getConsoleDetails() = 0;
		virtual const char* getMemoryFreeInfo() = 0;

		//!Hands control back to the OS/loader. exitAction and autoloadedGame
		//!are interpreted per-platform. Never returns.
		virtual void requestExit(int exitAction, bool autoloadedGame) = 0;
};

//! The globally accessible platform instance
extern Platform* platform;
