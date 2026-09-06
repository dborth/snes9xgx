/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * GameCubePlatform.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogc/lwp_threads.h>
#include <stdio.h>

#include "GameCubePlatform.h"
#include "OgcDebugOutput.h"

extern "C" {
extern void __exception_setreload(int t);
}

/****************************************************************************
 * ipl_set_config
 * lowlevel Qoob Modchip disable
 ***************************************************************************/
static void ipl_set_config(unsigned char c)
{
	volatile unsigned long* exi = (volatile unsigned long*)0xCC006800;
	unsigned long val,addr;
	addr=0xc0000000;
	val = c << 24;
	exi[0] = ((((exi[0]) & 0x405) | 256) | 48);	//select IPL
	//write addr of IPL
	exi[0 * 5 + 4] = addr;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);
	//write the ipl we want to send
	exi[0 * 5 + 4] = val;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);

	exi[0] &= 0x405;	//deselect IPL
}

/****************************************************************************
 * init/shutdown
 ***************************************************************************/
void GameCubePlatform::init(int width, int height)
{
	ipl_set_config(6); // disable Qoob modchip
	__exception_setreload(8);

	this->threadDriver = new OgcThreadDriver();
	this->threadDriver->init();

	this->videoDriver = new OgcVideoDriver();
	this->videoDriver->init(width, height);

	this->audioDriver = new GameCubeAudioDriver();
	this->audioDriver->init();

	this->inputDriver = new OgcInputDriver();
	this->inputDriver->init();

	this->fileSystemDriver = new GameCubeFileSystemDriver();
	this->fileSystemDriver->init();
}

void GameCubePlatform::shutdown()
{
	if (fileSystemDriver) {
		fileSystemDriver->shutdown();
		delete fileSystemDriver;
		fileSystemDriver = nullptr;
	}

	if (inputDriver) {
		inputDriver->shutdown();
		delete inputDriver;
		inputDriver = nullptr;
	}

	if (audioDriver) {
		audioDriver->shutdown();
		delete audioDriver;
		audioDriver = nullptr;
	}

	if (videoDriver) {
		videoDriver->shutdown();
		delete videoDriver;
		videoDriver = nullptr;
	}

	if (threadDriver) {
		threadDriver->shutdown();
		delete threadDriver;
		threadDriver = nullptr;
	}
}

/****************************************************************************
 * Console/memory info
 ***************************************************************************/
const char* GameCubePlatform::getConsoleDetails() {
	static char description[64];
	snprintf(description, sizeof(description), "GameCube (486 MHz)"); // 162 MHz bus * 3x multiplier
	return description;
}

const char* GameCubePlatform::getMemoryFreeInfo() {
	static char memoryFreeInfo[50];

	uint32_t mem1_bytes = SYS_GetArena1Size();
	float mem1_mb = (float)mem1_bytes / (1024.0f * 1024.0f);

	snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "MEM1 free: %.2fMB", mem1_mb);

	return memoryFreeInfo;
}

/****************************************************************************
 * Exit
 ***************************************************************************/
#define PSOSDLOADID 0x7c6000a6
static int *psoid = (int *) 0x80001800;
static void (*PSOReload) () = (void (*)()) 0x80001800;

void GameCubePlatform::requestExit(int exitAction, bool /*autoloadedGame*/)
{
	this->shutdown();

	if(exitAction == EXITACTION_GC_REBOOT) // Reboot
	{
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, FALSE);
	}
	else // Exit to Loader
	{
		if (psoid[0] == PSOSDLOADID)
		{
			SYS_ResetSystem(SYS_SHUTDOWN, 0, FALSE);
			__lwp_thread_stopmultitasking(PSOReload);
		}
		else
		{
			exit(0);
		}
	}
}
