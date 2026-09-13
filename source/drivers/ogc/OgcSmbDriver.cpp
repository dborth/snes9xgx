/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcSmbDriver.cpp
 ***************************************************************************/
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <network.h>
#include <smb.h>

#include "OgcSmbDriver.h"

#if defined(HW_RVL)
#include "../ThreadDriver.h"
#include "../Time.h"
#endif

// libogc's tinysmb always registers under the fixed device name "smb"
static const char * const kSmbDeviceName = "smb";

#if defined(HW_RVL)

#define NETWORK_THREAD_STACKSIZE (32 * 1024)
#define NETWORK_BRINGUP_TIMEOUT_SECS 10 // don't block the menu forever if the network never comes up

static ThreadSync & NetSync() { static ThreadSync s; return s; }
static Thread networkThread;
static bool networkIdle    = false; // protected by NetSync().mutex - true once the current bring-up attempt has finished (success or exhausted retries)
static bool networkUp      = false; // protected by NetSync().mutex - result of that attempt
static bool netPrevInit    = false; // true once net_init_async has succeeded at least once; drives the teardown-before-retry dance below

static void WakeNetworkThread()
{
	MutexLock guard(NetSync().mutex);
	NetSync().workCond.signal();
}

/****************************************************************************
 * NetworkThreadEntry
 *
 * Brings the network up (net_init_async, with its own bounded internal
 * retry) and goes idle until StartNetworkAttempt() wakes it for another
 * try, or JoinAll()/requestStop() tears it down at app exit.
 ***************************************************************************/
static void * NetworkThreadEntry(void *)
{
	NetSync().mutex.lock();
	while(!networkThread.stopRequested())
	{
		NetSync().mutex.unlock();

		s32 res = -1;
		int retry = 5;

		while(retry > 0 && !networkThread.stopRequested())
		{
			if(netPrevInit)
			{
				net_deinit();
				for(int i = 0; i < 400 && !networkThread.stopRequested(); i++) // up to 10 seconds to let the old connection tear down
				{
					res = net_get_status();
					if(res != -EBUSY) // not still busy tearing down the old connection
					{
						usleep(2000);
						net_wc24cleanup();
						netPrevInit = false; // net_wc24cleanup only needs to run once per net_init_async success
						usleep(20000);
						break;
					}
					usleep(20000);
				}
			}

			usleep(2000);
			res = net_init_async(NULL, NULL);

			if(res != 0)
			{
				sleep(1);
				retry--;
				continue;
			}

			res = net_get_status();
			int wait = 400; // ~8 sec
			while(res == -EBUSY && wait > 0 && !networkThread.stopRequested())
			{
				usleep(20000);
				res = net_get_status();
				wait--;
			}

			if(res == 0)
				break;

			retry--;
			usleep(2000);
		}

		bool success = false;
		if(res == 0)
		{
			struct in_addr hostip;
			hostip.s_addr = net_gethostip();
			if(hostip.s_addr)
			{
				success = true;
				netPrevInit = true;
			}
		}

		NetSync().mutex.lock();
		networkUp = success;
		networkIdle = true;
		NetSync().idleCond.signal(); // wake anything blocked in ensureNetworkUp()
		while(networkIdle && !networkThread.stopRequested())
			NetSync().workCond.wait(NetSync().mutex);
	}
	NetSync().mutex.unlock();
	return nullptr;
}

//!Starts the bring-up thread if it isn't running yet, or wakes it for a
//!fresh attempt if it's currently idle.
static void StartNetworkAttempt()
{
	if(!networkThread.isRunning())
	{
		{
			MutexLock guard(NetSync().mutex);
			networkIdle = false;
		}
		networkThread.start(NetworkThreadEntry, nullptr, NETWORK_THREAD_STACKSIZE, ThreadPriority::Low, WakeNetworkThread);
	}
	else
	{
		MutexLock guard(NetSync().mutex);
		networkIdle = false;
		NetSync().workCond.signal();
	}
}

#endif // HW_RVL

void OgcSmbDriver::init()
{
	connected = false;
	currentInfo = SmbShareInfo{};
}

void OgcSmbDriver::shutdown()
{
	disconnect();

#if defined(HW_RVL)
	networkThread.requestStop();
	networkThread.join();
#endif
}

bool OgcSmbDriver::ensureNetworkUp()
{
#if defined(HW_RVL)
	// The network can go stale (cable pulled, AP dropped, IOS reload) even
	// after a prior successful bring-up, so re-check liveness every call
	// rather than trusting a cached "we succeeded once" flag. This is a
	// cheap call - only fall through to a full (re-)init if it fails.
	if(net_gethostip() > 0)
		return true;

	StartNetworkAttempt();

	Ticks start = SystemTime::now();
	for(;;)
	{
		bool idle, up;
		{
			MutexLock guard(NetSync().mutex);
			idle = networkIdle;
			up   = networkUp;
		}

		if(idle)
			return up && net_gethostip() > 0;

		if(SystemTime::diffSecs(start, SystemTime::now()) > NETWORK_BRINGUP_TIMEOUT_SECS)
			return false;

		usleep(50 * 1000);
	}
#elif defined(HW_DOL)
	if(net_gethostip() > 0)
		return true;

	char ip[16];
	return if_config(ip, NULL, NULL, true) >= 0 && net_gethostip() > 0;
#else
	return false;
#endif
}

SmbConnectResult OgcSmbDriver::connect(const SmbShareInfo & info)
{
	if(info.host[0] == '\0' || info.share[0] == '\0')
		return SmbConnectResult::InvalidSettings;

	if(connected)
	{
		bool sameTarget =
			strcmp(info.host, currentInfo.host) == 0 &&
			strcmp(info.share, currentInfo.share) == 0 &&
			strcmp(info.user, currentInfo.user) == 0 &&
			strcmp(info.password, currentInfo.password) == 0;

		if(sameTarget)
			return SmbConnectResult::Success;

		disconnect(); // target changed - drop the old connection before reconnecting
	}

	if(!ensureNetworkUp())
		return SmbConnectResult::NetworkUnavailable;

	if(!smbInit(info.user, info.password, info.share, info.host))
		return SmbConnectResult::ConnectFailed;

	connected = true;
	currentInfo = info;
	return SmbConnectResult::Success;
}

void OgcSmbDriver::disconnect()
{
	if(!connected)
		return;

	smbClose(kSmbDeviceName);
	connected = false;
	currentInfo = SmbShareInfo{};
}

const char * OgcSmbDriver::connectResultMessage(SmbConnectResult result) const
{
	switch(result)
	{
		case SmbConnectResult::Success:            return "Connected.";
		case SmbConnectResult::InvalidSettings:    return "Network share host/name is blank.";
		case SmbConnectResult::NetworkUnavailable: return "Unable to initialize network!";
		case SmbConnectResult::ConnectFailed:      return "Failed to connect to network share.";
		default:                                   return "Unknown network share error.";
	}
}
