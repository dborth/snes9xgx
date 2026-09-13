/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUdp.cpp
 ***************************************************************************/
#include "OgcLoggerUdp.h"

#ifdef HW_RVL

#include <cstring>
#include <fcntl.h>

bool OgcLoggerUdp::init(const LogConfig & config)
{
	shutdown();

	// net_init() is safe to call even if some other subsystem already
	// brought the network stack up (eg. an SMB share) - it is reference
	// counted internally by libogc and returns immediately if so.
	if (net_init() < 0)
		return false;

	sock = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
		return false;

	if (config.nonBlocking)
	{
		int flags = net_fcntl(sock, F_GETFL, 0);
		if (flags >= 0)
			net_fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(config.targetPort);

	if (inet_aton(config.targetIp, &serverAddr.sin_addr) == 0)
	{
		// Malformed targetIp - fail activation loudly rather than silently
		// sending to a zeroed/broadcast address.
		net_close(sock);
		sock = -1;
		return false;
	}

	return true;
}

void OgcLoggerUdp::shutdown()
{
	if (sock >= 0)
	{
		net_close(sock);
		sock = -1;
	}
}

void OgcLoggerUdp::write(LogLevel, const char * line, size_t len)
{
	if (sock < 0)
		return;

	// Non-blocking send: a dropped packet or an offline listener returns
	// immediately (typically -EAGAIN) rather than stalling the caller -
	// deliberately not checked/retried, since UDP logging is inherently
	// best-effort and retrying here would reintroduce the exact stall
	// this backend exists to avoid.
	net_sendto(sock, line, len, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
}

#else // !HW_RVL - GameCube: no network hardware supported by this backend

bool OgcLoggerUdp::init(const LogConfig &) { return false; }
void OgcLoggerUdp::shutdown() { }
void OgcLoggerUdp::write(LogLevel, const char *, size_t) { }

#endif
