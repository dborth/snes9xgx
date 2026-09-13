/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerUdp.cpp
 ***************************************************************************/
#include "WutLoggerUdp.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

bool WutLoggerUdp::init(const LogConfig & config)
{
	shutdown();

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
		return false;

	if (config.nonBlocking)
	{
		int flags = fcntl(sock, F_GETFL, 0);
		if (flags >= 0)
			fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(config.targetPort);

	if (inet_pton(AF_INET, config.targetIp, &serverAddr.sin_addr) != 1)
	{
		// Malformed targetIp - fail activation loudly rather than silently
		// sending to a zeroed 0.0.0.0 address.
		close(sock);
		sock = -1;
		return false;
	}

	return true;
}

void WutLoggerUdp::shutdown()
{
	if (sock >= 0)
	{
		close(sock);
		sock = -1;
	}
}

void WutLoggerUdp::write(LogLevel /*level*/, const char * line, size_t len)
{
	if (sock < 0)
		return;

	// Best-effort, non-blocking: a dropped packet or an offline listener
	// must never stall the calling thread or trip the OS watchdog, so
	// the return value is deliberately ignored rather than retried.
	sendto(sock, line, len, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
}
