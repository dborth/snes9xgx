/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutSmbDriver.h
 *
 * Wii U SmbDriver. libsmb2 is a plain client library with no devoptab of its
 * own: it hands back a struct smb2_context* and POSIX-shaped calls.
 * WutSmbDriver registers its own "smb:/" newlib devoptab on top of libsmb2.
 ***************************************************************************/
#pragma once
#include "../SmbDriver.h"

struct smb2_context;

class WutSmbDriver : public SmbDriver
{
	public:
		void init() override;
		void shutdown() override;

		SmbConnectResult connect(const SmbShareInfo & info) override;
		void disconnect() override;
		bool isConnected() const override { return ctx != nullptr; }

		const char * getMountPath() const override { return ctx ? "smb:/" : ""; }
		const char * connectResultMessage(SmbConnectResult result) const override;
		const char * getLastError() const override;

		//! The active context, used by the free-function devoptab callbacks
		//! in WutSmbDriver.cpp. Only one WutSmbDriver/mount exists at a time.
		static smb2_context * getContext() { return ctx; }

	private:
		//! Brings the Wii U network connection up if it isn't already, via
		//! nn::ac (ACConnect()). Blocking. Returns false (with getLastError()
		//! set) if AC wasn't initialized or the connect attempt failed.
		bool ensureNetworkUp();

		static smb2_context * ctx;
		SmbShareInfo current = {};
		bool devoptabAdded = false;
		bool acInitialized = false; //!< true once ACInitialize() has succeeded
		bool acConnected   = false; //!< true once we've brought the network up ourselves via ACConnect()
};
