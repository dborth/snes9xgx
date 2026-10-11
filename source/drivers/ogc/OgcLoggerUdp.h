/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUdp.h
 *
 * Non-blocking UDP log streaming for Wii (HW_RVL)
 ***************************************************************************/
#pragma once

#include "../Logger.h"

#ifdef HW_RVL
#include <network.h>
#include "wii/WiiNetwork.h"
#endif

//!Non-blocking UDP log backend. Wii only - on GameCube init() reports failure and write() does nothing.
//!\ingroup grp_logging
class OgcLoggerUdp : public LoggingDriver
{
	public:
#ifdef HW_RVL
		OgcLoggerUdp() { self = this; }
		~OgcLoggerUdp() override { if (self == this) self = nullptr; }
#endif

		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "UDP"; }

	private:
#ifdef HW_RVL
		//! Opens the actual socket against pendingConfig
		bool activateSocket();

		//! WiiNetwork::notifyWhenUp() callback
		static void OnNetworkUp();

		static OgcLoggerUdp * self; // exactly one instance is ever registered (see WiiPlatform.cpp) - used so the static callback above can reach it

		s32 sock = -1;
		struct sockaddr_in serverAddr {};
		LogConfig pendingConfig {}; // captured in init(), used by activateSocket() whenever it actually runs
#endif
};
