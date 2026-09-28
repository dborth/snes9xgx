/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUsbGecko.cpp
 ***************************************************************************/
#include "OgcLoggerUsbGecko.h"
#include <ogc/usbgecko.h>

namespace
{
	constexpr uint32_t PROBE_INTERVAL_MS = 1000;
	constexpr int GECKO_RETRY_SPINS_PER_BYTE = 64;
}

bool OgcLoggerUsbGecko::probe()
{
	attached = usb_isgeckoalive(channel) != 0;
	lastProbe = SystemTime::now();
	probed = true;
	return attached;
}

bool OgcLoggerUsbGecko::init(const LogConfig & config)
{
	channel = config.geckoChannel;
	probed = false;
	probe();
	return true;
}

void OgcLoggerUsbGecko::shutdown()
{
	attached = false;
	probed = false;
}

void OgcLoggerUsbGecko::write(LogLevel /*level*/, const char * line, size_t len)
{
	if (len == 0)
		return;

	if (!attached)
	{
		// Detached: re-probe at a bounded rate, drop the line otherwise.
		if (probed && SystemTime::diffMillisecs(lastProbe, SystemTime::now()) < PROBE_INTERVAL_MS)
			return;
		if (!probe())
			return;
	}

	int retries = static_cast<int>(len) * GECKO_RETRY_SPINS_PER_BYTE;
	int sent = usb_sendbuffer_safe_ex(channel, line, static_cast<int>(len), retries);

	if (sent < static_cast<int>(len))
	{
		// Unplugged, or the host stopped draining the Gecko's buffer.
		// Drop the rest of this line and go back to the detached state;
		attached = false;
		lastProbe = SystemTime::now();
		probed = true;
	}
}
