/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeNetwork.cpp
 ***************************************************************************/
#include "GameCubeNetwork.h"

#include <network.h>

bool GameCubeNetwork::isUp()
{
	return net_gethostip() > 0;
}

bool GameCubeNetwork::ensureUp()
{
	if(net_gethostip() > 0)
		return true;

	char ip[16];
	return if_config(ip, NULL, NULL, true) >= 0 && net_gethostip() > 0;
}
