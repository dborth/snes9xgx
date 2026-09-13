/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUsbGecko.h
 *
 * Direct EXI reads/writes to a USB Gecko adapter, shared by GameCube and
 * Wii (both expose the same EXI bus / memory card slots). Channel is
 * config-driven (LogConfig::geckoChannel) rather than hardcoded, so a
 * Gecko in slot A vs slot B is a config change, not a rebuild.
 *
 * Detection happens once, in init() - if no Gecko answers the identify
 * command on the configured channel, this backend stays inert (write()
 * becomes a no-op) rather than re-probing the bus on every write(), which
 * would add EXI bus traffic and latency to every single log call for a
 * cable that was never attached.
 ***************************************************************************/
#pragma once

#include "../Logger.h"

class OgcLoggerUsbGecko : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "USBGecko"; }

	private:
		//! \return true if a USB Gecko answered the identify command on `channel`.
		bool detect(int channel);
		//! Sends one byte, retrying the ready-check a bounded number of
		//! times. \return true if the byte was accepted.
		bool sendByte(int channel, uint8_t byte);

		int  channel = 1;
		bool attached = false;
};
