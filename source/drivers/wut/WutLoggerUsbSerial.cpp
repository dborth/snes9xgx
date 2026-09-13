/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerUsbSerial.cpp
 ***************************************************************************/
#include "WutLoggerUsbSerial.h"

#if WUT_USBSERIAL_AVAILABLE

#include <usbserial/usbserial.h>

// The module's Init() doesn't take a baud rate/port selector - it claims
// whichever supported USB-to-TTL adapter is plugged in and runs it at a
// fixed rate the module itself defines. LogConfig::serialBaudRate is kept
// for backends against modules/adapters that do accept one; it's simply
// unused here.
bool WutLoggerUsbSerial::init(const LogConfig & /*config*/)
{
	ready = USBSerial_Init();
	return ready;
}

void WutLoggerUsbSerial::shutdown()
{
	if (ready)
	{
		USBSerial_Deinit();
		ready = false;
	}
}

void WutLoggerUsbSerial::write(LogLevel /*level*/, const char * line, size_t len)
{
	if (!ready)
		return;

	// USBSerial_Write() is documented as non-blocking - a full transfer
	// ring simply drops the write rather than stalling the caller.
	USBSerial_Write(line, static_cast<uint32_t>(len));
}

#else // !WUT_USBSERIAL_AVAILABLE

bool WutLoggerUsbSerial::init(const LogConfig & /*config*/) { return false; }
void WutLoggerUsbSerial::shutdown() { }
void WutLoggerUsbSerial::write(LogLevel /*level*/, const char * /*line*/, size_t /*len*/) { }

#endif
