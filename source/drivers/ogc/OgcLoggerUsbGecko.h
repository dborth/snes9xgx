/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUsbGecko.h
 *
 * USB Gecko backend over EXI, shared by GameCube and Wii (both expose the
 * same EXI bus / memory card slots). Channel is config-driven
 * (LogConfig::geckoChannel) rather than hardcoded, so a Gecko in slot A vs
 * slot B is a config change, not a rebuild.
 *
 * This backend is a thin wrapper around libogc's own implementation
 *
 * Hot-plug: the adapter may be plugged in after startup, unplugged, or
 * stall temporarily (host-side capture tool not reading).
 ***************************************************************************/
#pragma once

#include "../Logger.h"
#include "../Time.h"

//!Log backend for a USB Gecko adapter over EXI, via usbgecko.c.
//!\ingroup grp_logging
class OgcLoggerUsbGecko : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "USBGecko"; }

	private:
		//! Probes the configured channel, updating `attached`, and records
		//! the attempt time for rate limiting. \return true if attached.
		bool probe();

		int    channel = 1;
		bool   attached = false;
		bool   probed = false;     //!< true once lastProbe holds a valid time
		Ticks  lastProbe = 0;      //!< time of the last probe attempt
};
