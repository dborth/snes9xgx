/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutMouseDriver.h
 *
 * USB mouse via the OS HID library (nsyshid), using the HID boot protocol.
 * See drivers/MouseDriver.h for the contract.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <nsyshid/hid.h>
#include "../MouseDriver.h"

class Mutex;

class WutMouseDriver : public MouseDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool poll(MouseEvent & out) override;
		bool isConnected() const override;

		//!\name OS callbacks
		//!Called from the HID library's context, not the main thread.
		//!Only for use by the static trampolines in WutMouseDriver.cpp.
		//!@{
		int32_t onAttach(HIDDevice * device, HIDAttachEvent attach);
		void onProtocolSet(int slotIdx, uint32_t handle, int32_t error);
		void onReport(int slotIdx, uint32_t handle, int32_t error, uint32_t bytes);
		//!@}

	private:
		static const int kMaxMice = 4;
		static const int kQueueSize = 64;
		static const uint32_t kBufferSize = 64;

		struct Slot
		{
			bool active = false;     //!< a mouse is attached in this slot
			uint32_t handle = 0;     //!< HID handle of that mouse
			uint8_t interfaceIndex = 0;
			uint32_t readSize = 0;
			uint8_t * buffer = nullptr; //!< kept until shutdown(): a cancelled read may still point at it
			uint8_t buttons = 0;     //!< MouseButtons held on this mouse
		};

		//! Caller holds `lock`. Queues motion/button state, merging into the newest event where possible.
		void push(int dx, int dy);
		//! Caller holds `lock`. Buttons held across all attached mice.
		uint8_t combinedButtons() const;
		//! Caller holds `lock`. Marks a slot's mouse as gone and releases its buttons.
		void detachSlot(Slot & s);

		bool initialized = false;
		bool hidSetupOk = false;   //!< our HIDSetup() succeeded, so we owe a HIDTeardown()
		Mutex * lock = nullptr;
		HIDClient client;

		Slot slots[kMaxMice];

		MouseEvent queue[kQueueSize];
		int head = 0;
		int count = 0;
		uint8_t lastButtons = 0;   //!< buttons as of the newest queued event
};
