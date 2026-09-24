/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiNetwork.cpp
 ***************************************************************************/
#include "WiiNetwork.h"

#include <unistd.h>
#include <errno.h>
#include <network.h>

#include "../../ThreadDriver.h"
#include "../../Time.h"

#define NETWORK_THREAD_STACKSIZE (32 * 1024)
#define NETWORK_BRINGUP_TIMEOUT_SECS 10 // don't block forever if the network never comes up

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
		NetSync().idleCond.signal(); // wake anything blocked in ensureUp()
		while(networkIdle && !networkThread.stopRequested())
			NetSync().workCond.wait(NetSync().mutex);
	}
	NetSync().mutex.unlock();
	return nullptr;
}

bool WiiNetwork::isUp()
{
	return net_gethostip() > 0;
}

bool WiiNetwork::ensureUp()
{
	// The network can go stale (cable pulled, AP dropped, IOS reload) even
	// after a prior successful bring-up, so re-check liveness every call
	// rather than trusting a cached "we succeeded once" flag. This is a
	// cheap call - only fall through to a full (re-)init if it fails.
	if(net_gethostip() > 0)
		return true;

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

	Ticks start = SystemTime::now();
	while(SystemTime::diffSecs(start, SystemTime::now()) <= NETWORK_BRINGUP_TIMEOUT_SECS)
	{
		bool idle, up;
		{
			MutexLock guard(NetSync().mutex);
			idle = networkIdle;
			up   = networkUp;
		}

		if(idle)
			return up && net_gethostip() > 0;

		usleep(50 * 1000);
	}
	return false;
}

void WiiNetwork::shutdown()
{
	networkThread.requestStop();
	networkThread.join();
}
