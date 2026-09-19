/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeNetwork.h
 *
 * GameCube network interface bring-up
 *
 * ensureUp() re-validates the network is actually still up on every call
 * rather than trusting a cached flag. Bring-up is synchronous (if_config)
 ***************************************************************************/
#pragma once

class GameCubeNetwork
{
	public:
		//! Brings the network interface up if it isn't already, or
		//! confirms it's still alive if it is. Blocks for however long
		//! if_config takes until the console has a usable IP or bring-up is
		//! given up on.
		//! \return true once the console has a usable IP.
		static bool ensureUp();

		//! Non-blocking: true if the console currently has a usable IP.
		static bool isUp();

		//! No-op on GameCube
		static void shutdown() {}
};
