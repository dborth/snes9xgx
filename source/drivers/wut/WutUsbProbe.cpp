/****************************************************************************
 * libgui - drivers/wut
 * Daryl Borth 2026
 * WutUsbProbe.cpp
 *
 * Intentionally does NOT touch Mocha_usbX_disc_interface
 ***************************************************************************/
#include <nsysuhs/uhs.h>
#include <nsysuhs/uhs_usbspec.h>
#include <malloc.h>
#include <string.h>

#include "WutUsbProbe.h"

#define MAX_PROFILES 4

void ScanUsbHardwareSignature(UsbHardwareSignature & out)
{
	out.count = 0;

	void * cfgBuffer = memalign(0x40, UHS_CONFIG_BUFFER_SIZE);
	if(!cfgBuffer)
		return;

	UhsConfig config;
	memset(&config, 0, sizeof(config));
	config.controller_num = 0;
	config.buffer = cfgBuffer;
	config.buffer_size = UHS_CONFIG_BUFFER_SIZE;

	UhsHandle handle;
	memset(&handle, 0, sizeof(handle));

	UHSStatus openStatus = UhsClientOpen(&handle, &config);
	if(openStatus == UHS_STATUS_OK)
	{
		UhsInterfaceFilter filter;
		memset(&filter, 0, sizeof(filter));
		filter.match_params = MATCH_ANY;

		UhsInterfaceProfile * profiles = (UhsInterfaceProfile *)memalign(0x40, sizeof(UhsInterfaceProfile) * MAX_PROFILES);
		if(profiles)
		{
			memset(profiles, 0, sizeof(UhsInterfaceProfile) * MAX_PROFILES);

			// UhsQueryInterfaces()'s return value is the count of interfaces
			// written into profiles. Use it directly as the loop bound.
			int foundCount = (int)UhsQueryInterfaces(&handle, &filter, profiles, MAX_PROFILES);

			for(int i = 0; i < foundCount; i++)
			{
				bool isStorage = (profiles[i].if_desc.bInterfaceClass == USBCLASS_STORAGE);

				if(isStorage && out.count < UsbHardwareSignature::kMaxInterfaces)
				{
					UsbHardwareInterfaceInfo & entry = out.interfaces[out.count++];
					entry.ifHandle = profiles[i].if_handle;
					entry.vid = profiles[i].dev_desc.idVendor;
					entry.pid = profiles[i].dev_desc.idProduct;
				}
			}
			free(profiles);
		}
		UhsClientClose(&handle);
	}
	free(cfgBuffer);
}

bool UsbHardwareSignatureChanged(const UsbHardwareSignature & a, const UsbHardwareSignature & b)
{
	if(a.count != b.count)
		return true;

	for(int i = 0; i < a.count; i++)
	{
		bool foundInB = false;
		for(int j = 0; j < b.count; j++)
		{
			if(a.interfaces[i].ifHandle == b.interfaces[j].ifHandle)
			{
				foundInB = true;
				break;
			}
		}
		if(!foundInB)
			return true;
	}

	return false;
}
