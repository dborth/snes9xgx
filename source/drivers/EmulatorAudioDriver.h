/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorAudioDriver.h
 ***************************************************************************/
#pragma once

class EmulatorAudioDriver
{
	public:
		virtual ~EmulatorAudioDriver() = default;

		virtual void init() = 0;

		//! Clears buffered/queued audio state and the dynamic-rate controller.
		//! Called when loading a new game and when returning from the menu.
		virtual void resetAudio() = 0;
};
