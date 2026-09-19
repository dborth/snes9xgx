/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcSmbDriver.h
 *
 * Wii / GameCube SmbDriver. libsmb2 is a plain client library with no
 * devoptab of its own: it hands back a struct smb2_context* and POSIX-shaped
 * calls, so this class registers a "smb:/" newlib devoptab on top of it
 * (see OgcSmbDriver.cpp).
 *
 * Bringing the network interface itself up is a precondition for any SMB
 * connection but isn't this class's job
 ***************************************************************************/
#pragma once
#include "../SmbDriver.h"

struct smb2_context;

//!Wraps libsmb2 behind a "smb:/" devoptab. Both GameCube (via BBA) and Wii
//! bring the network up, then share the identical libsmb2 connect/devoptab
class OgcSmbDriver : public SmbDriver
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
		//! in OgcSmbDriver.cpp. Only one OgcSmbDriver/mount exists at a time.
		static smb2_context * getContext() { return ctx; }

		bool isNetworkUp() const override;

		//! Ensures the network is up via WiiNetwork/GameCubeNetwork::ensureUp().
		//! Blocking. Returns false (with getLastError() set) if it couldn't be
		//! brought up.
		bool ensureNetworkUp() override;

	private:
		static smb2_context * ctx;
		SmbShareInfo current = {};
		bool devoptabAdded = false;
};
