/* mload.c (for PPC) (c) 2009, Hermes 

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <gccore.h>
#include "unistd.h"
#include "ehcmodule_elf.h"

#define MLOAD_GET_IOS_BASE	    0x4D4C4401
#define MLOAD_GET_MLOAD_VERSION 0x4D4C4402
#define MLOAD_RUN_THREAD        0x4D4C4482
#define MLOAD_MEMSET			0x4D4C4491

#define getbe32(x) ((adr[x]<<24) | (adr[x+1]<<16) | (adr[x+2]<<8) | (adr[x+3]))

typedef struct
{
	u32 ident0;
	u32 ident1;
	u32 ident2;
	u32 ident3;
	u32 machinetype;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shtrndx;
} elfheader;

typedef struct
{
	u32 type;
	u32 offset;
	u32 vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
} elfphentry;

typedef struct
{
	void *start;
	int prio;
	void *stack;
	int size_stack;
} data_elf;

static const char mload_fs[] ATTRIBUTE_ALIGN(32) = "/dev/mload";

static s32 mload_fd = -1;
static s32 hid = -1;

int mloadVersion = -1;
int iosBase = -1;

// to close the device (remember call it when rebooting the IOS!)
int mload_close()
{
	int ret;

	if (hid >= 0)
	{
		iosDestroyHeap(hid);
		hid = -1;
	}

	if (mload_fd < 0)
		return -1;

	ret = IOS_Close(mload_fd);
	mload_fd = -1;
	return ret;
}

// to init/test if the device is running
int mload_init()
{
	int n;

	if (hid < 0)
		hid = iosCreateHeap(0x800);

	if (hid < 0)
	{
		if (mload_fd >= 0)
			IOS_Close(mload_fd);

		mload_fd = -1;
		return hid;
	}

	if (mload_fd >= 0)
		return 0;

	for (n = 0; n < 20; n++) // try 5 seconds
	{
		mload_fd = IOS_Open(mload_fs, 0);

		if (mload_fd >= 0)
			break;

		usleep(250 * 1000);
	}

	if (mload_fd < 0)
		return mload_close();

	mloadVersion = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_MLOAD_VERSION, ":"); 
	iosBase = IOS_IoctlvFormat(hid, mload_fd, MLOAD_GET_IOS_BASE, ":");

	if(mloadVersion < 0x52) // unsupported IOS202
		return mload_close();

	return mload_fd;
}

// fix starlet address to read/write (uses SEEK_SET, etc as mode)
static int mload_seek(int offset, int mode)
{
	if (mload_init() < 0)
		return -1;

	return IOS_Seek(mload_fd, offset, mode);
}

// write bytes from starlet (it update the offset)
static int mload_write(const void * buf, u32 size)
{
	if (mload_init() < 0)
		return -1;

	return IOS_Write(mload_fd, buf, size);
}

// fill a block (similar to memset)
static int mload_memset(void *starlet_addr, int set, int len)
{
	if (mload_init() < 0)
		return -1;

	return IOS_IoctlvFormat(hid, mload_fd, MLOAD_MEMSET, "iii:", starlet_addr, set, len);
}

// load a module from the PPC
// the module must be a elf made with stripios
static int mload_elf(void *my_elf, data_elf *data_elf)
{
	int n, m;
	int p;
	u8 *adr;
	u32 elf = (u32) my_elf;

	if (elf & 3)
		return -1; // aligned to 4 please!

	elfheader *head = (void *) elf;
	elfphentry *entries;

	if (head->ident0 != 0x7F454C46)
		return -1;
	if (head->ident1 != 0x01020161)
		return -1;
	if (head->ident2 != 0x01000000)
		return -1;

	p = head->phoff;

	data_elf->start = (void *) head->entry;

	for (n = 0; n < head->phnum; n++)
	{
		entries = (void *) (elf + p);
		p += sizeof(elfphentry);

		if (entries->type == 4)
		{
			adr = (void *) (elf + entries->offset);

			if (getbe32(0) != 0)
				return -2; // bad info (sure)

			for (m = 4; m < entries->memsz; m += 8)
			{
				switch (getbe32(m))
				{
					case 0x9:
						data_elf->start = (void *) getbe32(m+4);
						break;
					case 0x7D:
						data_elf->prio = getbe32(m+4);
						break;
					case 0x7E:
						data_elf->size_stack = getbe32(m+4);
						break;
					case 0x7F:
						data_elf->stack = (void *) (getbe32(m+4));
						break;
				}
			}
		}
		else if (entries->type == 1 && entries->memsz != 0 && entries->vaddr != 0)
		{
			if (mload_memset((void *) entries->vaddr, 0, entries->memsz) < 0)
				return -1;
			if (mload_seek(entries->vaddr, SEEK_SET) < 0)
				return -1;
			if (mload_write((void *) (elf + entries->offset), entries->filesz) < 0)
				return -1;
		}
	}
	return 0;
}

// run one thread (you can use to load modules or binary files)
static int mload_run_thread(void *starlet_addr, void *starlet_top_stack, int stack_size, int priority)
{
	if (mload_init() < 0)
		return -1;

	return IOS_IoctlvFormat(hid, mload_fd, MLOAD_RUN_THREAD, "iiii:", starlet_addr, starlet_top_stack, stack_size, priority);
}

bool load_ehci_module()
{
	data_elf elf;
	memset(&elf, 0, sizeof(data_elf));

	if(mload_elf((void *) ehcmodule_elf, &elf) != 0)
		return false;

	if(mload_run_thread(elf.start, elf.stack, elf.size_stack, elf.prio) < 0)
		return false;
	
	usleep(5000);
	return true;
}
