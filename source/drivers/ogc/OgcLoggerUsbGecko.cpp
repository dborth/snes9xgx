/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerUsbGecko.cpp
 ***************************************************************************/
#include "OgcLoggerUsbGecko.h"
#include <ogc/exi.h>

// USB Gecko EXI protocol. Every transaction is a 32-bit EXI immediate 
// transfer: selecting device 0 on the target channel, writing a command 
// word, then reading a status/response word back.
namespace
{
	constexpr uint32_t GECKO_CMD_IDENTIFY = 0x90000000;
	constexpr uint32_t GECKO_IDENTIFY_REPLY_MASK = 0xFFFF0000;
	constexpr uint32_t GECKO_IDENTIFY_REPLY_VALUE = 0x04700000;

	// Send command: top byte 0xB0, next byte is the data byte to send.
	// The low half of the status word read back afterward has the
	// "transmit buffer has room" bit (0x04) set in its top byte when the
	// byte was accepted; if it's clear, the Gecko's buffer is full and
	// the caller should retry.
	constexpr uint32_t GECKO_CMD_SEND = 0xB0000000;
	constexpr uint32_t GECKO_SEND_READY_BIT = 0x04000000;

	constexpr int GECKO_MAX_RETRIES_PER_BYTE = 32;
}

bool OgcLoggerUsbGecko::detect(int chan)
{
	if (!EXI_Lock(chan, EXI_DEVICE_0, nullptr))
		return false;

	bool selected = EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED8MHZ);
	if (!selected)
	{
		EXI_Unlock(chan);
		return false;
	}

	uint32_t cmd = GECKO_CMD_IDENTIFY;
	uint32_t reply = 0;
	bool ok = EXI_Imm(chan, &cmd, 4, EXI_WRITE, nullptr) != 0;
	ok = ok && EXI_Sync(chan) != 0;
	ok = ok && EXI_Imm(chan, &reply, 4, EXI_READ, nullptr) != 0;
	ok = ok && EXI_Sync(chan) != 0;

	EXI_Deselect(chan);
	EXI_Unlock(chan);

	return ok && (reply & GECKO_IDENTIFY_REPLY_MASK) == GECKO_IDENTIFY_REPLY_VALUE;
}

bool OgcLoggerUsbGecko::sendByte(int chan, uint8_t byte)
{
	for (int attempt = 0; attempt < GECKO_MAX_RETRIES_PER_BYTE; attempt++)
	{
		if (!EXI_Lock(chan, EXI_DEVICE_0, nullptr))
			return false;

		if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED8MHZ))
		{
			EXI_Unlock(chan);
			return false;
		}

		uint32_t cmd = GECKO_CMD_SEND | (static_cast<uint32_t>(byte) << 16);
		uint32_t status = 0;
		bool ok = EXI_Imm(chan, &cmd, 4, EXI_WRITE, nullptr) != 0;
		ok = ok && EXI_Sync(chan) != 0;
		ok = ok && EXI_Imm(chan, &status, 4, EXI_READ, nullptr) != 0;
		ok = ok && EXI_Sync(chan) != 0;

		EXI_Deselect(chan);
		EXI_Unlock(chan);

		if (ok && (status & GECKO_SEND_READY_BIT) != 0)
			return true;

		// Buffer was full or the transfer failed - the adapter is a
		// fixed-rate USB-serial bridge underneath, so a short bounded
		// retry is the correct behavior rather than blocking; giving up
		// after GECKO_MAX_RETRIES_PER_BYTE keeps a detached/stalled
		// cable from ever hanging a log call.
	}

	return false;
}

bool OgcLoggerUsbGecko::init(const LogConfig & config)
{
	channel = config.geckoChannel;
	attached = detect(channel);
	// Always "succeeds": an unattached Gecko is not an error condition,
	// it just means write() below becomes a cheap no-op.
	return true;
}

void OgcLoggerUsbGecko::shutdown()
{
	attached = false;
}

void OgcLoggerUsbGecko::write(LogLevel /*level*/, const char * line, size_t len)
{
	if (!attached)
		return;

	for (size_t i = 0; i < len; i++)
	{
		if (!sendByte(channel, static_cast<uint8_t>(line[i])))
		{
			// Lost the adapter mid-stream (unplugged, host-side capture
			// tool closed) - stop treating it as attached so subsequent
			// write() calls short-circuit instead of retrying byte by
			// byte against a cable that's gone.
			attached = false;
			return;
		}
	}
}
