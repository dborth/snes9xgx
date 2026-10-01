/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * SmbDriver.h
 *
 * Network-share driver. Exactly one instance is owned by each platform's
 * FileSystemDriver and returned from FileSystemDriver::getSmb().
 *
 * On a successful connect(), a devoptab-style mount path is available via
 * getMountPath() and the rest of the app reads through it with ordinary
 * POSIX calls.
 ***************************************************************************/
#pragma once

//! Credentials/target for a single SMB share. All fields are plain
//! null-terminated strings. A share with no credentials (guest access)
//! leaves user/password empty.
struct SmbShareInfo
{
	char host[64];      //!< server IP or hostname
	char share[64];     //!< share name, eg. "games" (no leading/trailing slashes)
	char user[32];      //!< "" for guest/anonymous
	char password[32];
};

//! Result of a single connect() attempt. Deliberately mirrors MountResult's
//! shape (see FileSystemDriver.h) but SMB gets its own enum since the
//! failure modes are different (network down vs. auth/share rejected) and
//! callers may want to tell those apart in the UI.
enum class SmbConnectResult
{
	Success,
	InvalidSettings,     //!< host or share is empty - nothing to try
	NetworkUnavailable,  //!< no usable network connection at all
	ConnectFailed,       //!< network's up but the server/share/credentials didn't work
	TimedOut,            //!< the server never answered within the connect deadline
	Cancelled            //!< the cancel check asked to stop - not an error, don't prompt
};

//! Polled by a blocking ensureNetworkUp()/connect() (from its own thread, about
//! ten times a second); return true to abort. It runs on the thread doing the
//! connecting, so it must be cheap and must only read plain flags.
typedef bool (*SmbCancelCheck)(void);

class SmbDriver
{
	public:
		virtual ~SmbDriver() = default;

		//! One-time setup. Does not connect - just prepares whatever the
		//! platform needs before a connect() can succeed.
		virtual void init() = 0;
		virtual void shutdown() = 0;

		//! True if the network is already up, i.e. connect() won't first have to
		//! bring it up. Cheap and non-blocking - safe to poll.
		virtual bool isNetworkUp() const = 0;

		//! Brings the network up if it isn't already. Bounded - gives up after a
		//! fixed time - and abortable via setCancelCheck().
		//! \return false (with getLastError() set) if it couldn't be brought up.
		virtual bool ensureNetworkUp() = 0;

		//! Installs (or, with nullptr, removes) the check that lets a caller
		//! abort a blocking ensureNetworkUp()/connect(). Stateless on purpose:
		//! the caller owns the flag, so a stale request can never abort a
		//! later attempt. connect() then returns Cancelled.
		virtual void setCancelCheck(SmbCancelCheck) {}

		//! True if setCancelCheck() is honored, ie. a blocking connect can
		//! really be aborted - callers use it to decide whether to offer
		//! the user a Cancel button.
		virtual bool supportsCancel() const { return false; }

		//! Attempts to connect and mount in one call. No-ops (returns Success)
		//! if already connected to the same host+share; reconnects if info
		//! describes a different target. Always returns within a bounded time
		//! (TimedOut) and can be aborted through setCancelCheck().
		virtual SmbConnectResult connect(const SmbShareInfo & info) = 0;

		//! Unmounts and drops the connection. Safe to call whether or not
		//! currently connected.
		virtual void disconnect() = 0;

		virtual bool isConnected() const = 0;

		//! devoptab-style mount path (eg. "smb:/"), or "" if not currently
		//! connected.
		virtual const char * getMountPath() const = 0;

		//! Short, user-displayable reason for a non-Success SmbConnectResult.
		//! Never returns nullptr.
		virtual const char * connectResultMessage(SmbConnectResult result) const = 0;

		//! Raw, implementation-specific detail behind the *last* failure.
		//! Not something callers should rely on being non-empty.
		virtual const char * getLastError() const { return ""; }
};
