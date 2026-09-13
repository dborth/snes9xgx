/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcSmbDriver.h
 *
 * GameCube + Wii SmbDriver (shared, like the rest of drivers/ogc/) built on
 * libogc's bundled tinysmb (<smb.h>). smbInit() registers its own "smb:/"
 * devoptab internally on success, so once connected the rest of the app
 * reads through it exactly like sd:/ or usb:/ - no other code needs to
 * speak SMB (or the underlying network API) at all.
 *
 * This class also owns bringing the network interface itself up, since
 * that's a precondition for any SMB connection and nothing else in the
 * app needs it independently. connect() re-validates the network is
 * actually still up on every call rather than trusting a cached flag
 * Bring-up is:
 *   - HW_RVL: asynchronous (net_init_async) and run on a dedicated
 *     background Thread, since it can take several seconds and the menu
 *     needs to keep responding to input/animating while it waits.
 *   - HW_DOL: synchronous (if_config) - there's no async variant on
 *     GameCube's more limited BBA stack.
 ***************************************************************************/
#pragma once
#include "../SmbDriver.h"

class OgcSmbDriver : public SmbDriver
{
	public:
		void init() override;
		void shutdown() override;

		//! Ensures the network is up (bringing it up, or re-validating and
		//! reviving it if it's gone stale, as needed) and then connects to
		//! the given share. A single attempt - callers own their own
		//! retry/prompt policy, same as FileSystemDriver::mountStorageDevice().
		SmbConnectResult connect(const SmbShareInfo & info) override;
		void disconnect() override;
		bool isConnected() const override { return connected; }

		const char * getMountPath() const override { return connected ? "smb:/" : ""; }
		const char * connectResultMessage(SmbConnectResult result) const override;

	private:
		//! Brings the network interface up if it isn't already, or
		//! confirms it's still alive if it is. \return true once the
		//! console has a usable IP.
		bool ensureNetworkUp();

		bool connected = false;
		SmbShareInfo currentInfo{}; //!< what we're connected to, so a repeat connect() with identical info is a no-op
};
