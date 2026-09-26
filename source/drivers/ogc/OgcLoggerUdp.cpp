/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUdp.cpp
 ***************************************************************************/
#include "OgcLoggerUdp.h"

#ifdef HW_RVL

#include <cstring>

#include "../Platform.h"

OgcLoggerUdp * OgcLoggerUdp::self = nullptr;

bool OgcLoggerUdp::init(const LogConfig & config)
{
	shutdown();
	pendingConfig = config;

	if (WiiNetwork::isUp())
		return activateSocket();

	WiiNetwork::notifyWhenUp(&OgcLoggerUdp::OnNetworkUp);
	return false;
}

bool OgcLoggerUdp::activateSocket()
{
	sock = net_socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return false;

	if (pendingConfig.nonBlocking)
	{
		u32 nonBlocking = 1;
		net_ioctl(sock, FIONBIO, &nonBlocking);
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(pendingConfig.targetPort);

	if (inet_aton(pendingConfig.targetIp, &serverAddr.sin_addr) == 0)
	{
		// Malformed targetIp - fail activation loudly rather than silently
		// sending to a zeroed/broadcast address.
		net_close(sock);
		sock = -1;
		return false;
	}

	return true;
}

void OgcLoggerUdp::OnNetworkUp()
{
	// Only ever reached from WiiNetwork's background thread
	if (!self || !self->activateSocket())
		return;

	if (platform && platform->getLogger())
		platform->getLogger()->activateDeferred(LOGGER_UDP);
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
