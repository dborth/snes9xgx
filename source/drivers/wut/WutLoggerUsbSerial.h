/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerUsbSerial.h
 *
 * USB-to-TTL / USB-serial output for Wii U. Stock wut has no first-party
 * USB-serial API - this backend targets the community "usbserial" homebrew
 * module (loaded as a .rpl at runtime, eg. via Aroma's module system),
 * which exposes Init()/Deinit()/Write() over a small C header.
 *
 * That header is treated as an OPTIONAL dependency, not a hard build
 * requirement: if <usbserial/usbserial.h> isn't present in the toolchain,
 * this file still compiles - init() simply reports failure and write() is
 * a no-op, exactly like a Gecko backend with nothing plugged in. This
 * keeps "no hardcoded options" honest at the build-dependency level too:
 * a project that doesn't ship the module doesn't need an #ifdef at every
 * call site, and one that does gets the real backend for free.
 *
 * To enable: add the usbserial-wiiu package to your (wut) toolchain and
 * rebuild - no source changes needed here.
 ***************************************************************************/
#pragma once

#include "../Logger.h"

#if __has_include(<usbserial/usbserial.h>)
	#define WUT_USBSERIAL_AVAILABLE 1
#else
	#define WUT_USBSERIAL_AVAILABLE 0
#endif

class WutLoggerUsbSerial : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "USBSerial"; }

	private:
		bool ready = false;
};
