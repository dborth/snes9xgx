/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerUdp.h
 *
 * Non-blocking UDP log streaming over BSD sockets (nsysnet, bundled with
 * wut). Wii U's network stack is already up whenever the OS has network
 * configured, so unlike the Wii backend there is no explicit net_init()
 * equivalent to call here.
 ***************************************************************************/
#pragma once

#include "../Logger.h"
#include <netinet/in.h>

class WutLoggerUdp : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "UDP"; }

	private:
		int sock = -1;
		struct sockaddr_in serverAddr {};
};
