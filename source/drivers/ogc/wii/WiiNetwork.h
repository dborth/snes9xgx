/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiNetwork.h
 *
 * Wii network interface bring-up
 *
 * ensureUp() re-validates the network is actually still up on every call
 * rather than trusting a cached flag. Bring-up is asynchronous
 * via net_init_async and run on a dedicated background Thread
 ***************************************************************************/
#pragma once

class WiiNetwork
{
	public:
		//! Brings the network interface up if it isn't already, or
		//! confirms it's still alive if it is. Blocks ~10s until the
		//! console has a usable IP or bring-up is given up on.
		//! \param cancel optional; polled while waiting, return true to
		//! stop waiting (bring-up carries on in the background thread)
		//! \return true once the console has a usable IP.
		static bool ensureUp(bool (*cancel)(void) = nullptr);

		//! Non-blocking: true if the console currently has a usable IP.
		static bool isUp();

		//! Registers callback to run exactly once
		static void notifyWhenUp(void (*callback)());

		//! Stops and joins the background bring-up thread
		static void shutdown();
};
