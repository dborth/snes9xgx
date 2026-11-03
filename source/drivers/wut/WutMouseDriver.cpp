/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutMouseDriver.cpp
 *
 * WUT has no mouse API, so this sits on the generic HID library. Mice are
 * switched to the HID boot protocol, whose report layout is fixed, so no
 * report descriptor needs parsing:
 *     byte 0  buttons (bit0 left, bit1 right, bit2 middle)
 *     byte 1  X movement (int8, relative)
 *     byte 2  Y movement (int8, relative)
 *     byte 3+ wheel and extras, ignored
 *
 * Each mouse has one read outstanding at a time, re-armed from its own
 * completion callback. The callbacks only append to a small queue; poll()
 * drains it on the main thread.
 ***************************************************************************/
#include <malloc.h>
#include <string.h>

#include "WutMouseDriver.h"
#include "../Mutex.h"

// USB HID interface subclass/protocol (HID spec 1.11, section 4.2/4.3)
static const uint8_t kSubClassBoot = 1;
static const uint8_t kProtocolMouse = 2;
// HIDSetProtocol() argument: 0 = boot protocol, 1 = report protocol
static const uint8_t kProtocolBoot = 0;

// HIDAttachCallback return value. The OS offers each attached device to
// every HID client in turn: 1 = this client uses the device, 0 = it does not
// and the device is left to the other clients (eg. the OS keyboard library,
// which would otherwise never see a keyboard). Values as used by RetroArch's
// Wii U HID driver (DEVICE_USED / DEVICE_UNUSED); the WUT headers do not
// document them.
static const int32_t kAttachClaimed = 1;
static const int32_t kAttachUnclaimed = 0;

static WutMouseDriver * instance = nullptr;

static int32_t AttachTrampoline(HIDClient *, HIDDevice * device, HIDAttachEvent attach)
{
	if (!instance || !device)
		return kAttachUnclaimed;
	return instance->onAttach(device, attach);
}

static void ProtocolTrampoline(uint32_t handle, int32_t error, uint8_t *, uint32_t, void * context)
{
	if (instance)
		instance->onProtocolSet((int)(intptr_t)context, handle, error);
}

static void ReadTrampoline(uint32_t handle, int32_t error, uint8_t *, uint32_t bytes, void * context)
{
	if (instance)
		instance->onReport((int)(intptr_t)context, handle, error, bytes);
}

void WutMouseDriver::init()
{
	if (initialized)
		return;

	// The attach callback can fire from inside HIDAddClient() (for mice that
	// are already plugged in), so everything it touches is set up first.
	lock = new Mutex();
	instance = this;
	head = count = 0;
	lastButtons = 0;
	for (int i = 0; i < kMaxMice; i++)
		slots[i] = Slot();
	memset(&client, 0, sizeof(client));

	// HIDSetup() may already have been done by the OS keyboard library; only
	// tear down in shutdown() what we set up ourselves
	hidSetupOk = (HIDSetup() >= 0);
	initialized = (HIDAddClient(&client, AttachTrampoline) >= 0);

	if (!initialized)
	{
		if (hidSetupOk)
			HIDTeardown();
		hidSetupOk = false;
		instance = nullptr;
		delete lock;
		lock = nullptr;
	}
}

void WutMouseDriver::shutdown()
{
	if (!initialized)
		return;

	// Stop callbacks reaching us before tearing down what they use
	instance = nullptr;
	HIDDelClient(&client);
	if (hidSetupOk)
		HIDTeardown();

	for (int i = 0; i < kMaxMice; i++)
	{
		free(slots[i].buffer);
		slots[i] = Slot();
	}

	delete lock;
	lock = nullptr;
	hidSetupOk = false;
	initialized = false;
}

uint8_t WutMouseDriver::combinedButtons() const
{
	uint8_t b = 0;
	for (int i = 0; i < kMaxMice; i++)
		if (slots[i].active)
			b |= slots[i].buttons;
	return b;
}

void WutMouseDriver::push(int dx, int dy)
{
	const uint8_t buttons = combinedButtons();

	if (dx == 0 && dy == 0 && buttons == lastButtons)
		return;

	// Merge into the newest event when only motion changed (a fast mouse
	// reports far more often than the main loop polls), or when the queue
	// is full. Adopting the latest button state means a full queue can drop
	// a click but never leave a button stuck down.
	if (count > 0)
	{
		MouseEvent & tail = queue[(head + count - 1) % kQueueSize];
		if (tail.buttons == buttons || count >= kQueueSize)
		{
			tail.dx += dx;
			tail.dy += dy;
			tail.buttons = buttons;
			lastButtons = buttons;
			return;
		}
	}

	MouseEvent & e = queue[(head + count) % kQueueSize];
	e.dx = dx;
	e.dy = dy;
	e.buttons = buttons;
	count++;
	lastButtons = buttons;
}

void WutMouseDriver::detachSlot(Slot & s)
{
	s.active = false;
	s.buttons = 0;
	push(0, 0); // releases anything this mouse was holding
}

int32_t WutMouseDriver::onAttach(HIDDevice * device, HIDAttachEvent attach)
{
	if (!lock)
		return kAttachUnclaimed;

	MutexLock guard(*lock);

	if (attach == HID_DEVICE_DETACH)
	{
		for (int i = 0; i < kMaxMice; i++)
		{
			if (slots[i].active && slots[i].handle == device->handle)
			{
				detachSlot(slots[i]);
				return kAttachClaimed;
			}
		}
		return kAttachUnclaimed;
	}

	// Mice only: leave keyboards and everything else to their own clients
	if (device->subClass != kSubClassBoot || device->protocol != kProtocolMouse)
		return kAttachUnclaimed;

	int idx = -1;
	for (int i = 0; i < kMaxMice; i++)
	{
		if (!slots[i].active)
		{
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return kAttachUnclaimed;

	Slot & s = slots[idx];
	if (!s.buffer)
		s.buffer = (uint8_t *)memalign(0x40, kBufferSize);
	if (!s.buffer)
		return kAttachUnclaimed;

	uint32_t size = device->maxPacketSizeRx;
	if (size < 4)
		size = 4;
	if (size > kBufferSize)
		size = kBufferSize;

	s.active = true;
	s.handle = device->handle;
	s.interfaceIndex = device->interfaceIndex;
	s.readSize = size;
	s.buttons = 0;

	if (HIDSetProtocol(s.handle, s.interfaceIndex, kProtocolBoot,
			ProtocolTrampoline, (void *)(intptr_t)idx) < 0)
	{
		s.active = false;
		return kAttachUnclaimed;
	}
	return kAttachClaimed;
}

void WutMouseDriver::onProtocolSet(int slotIdx, uint32_t handle, int32_t error)
{
	if (!lock || slotIdx < 0 || slotIdx >= kMaxMice)
		return;

	MutexLock guard(*lock);
	Slot & s = slots[slotIdx];

	if (!s.active || s.handle != handle)
		return; // detached, or the slot has been reused

	// Without the boot protocol the report layout is unknown, so don't read
	if (error < 0 ||
		HIDRead(s.handle, s.buffer, s.readSize, ReadTrampoline, (void *)(intptr_t)slotIdx) < 0)
	{
		s.active = false;
	}
}

void WutMouseDriver::onReport(int slotIdx, uint32_t handle, int32_t error, uint32_t bytes)
{
	if (!lock || slotIdx < 0 || slotIdx >= kMaxMice)
		return;

	MutexLock guard(*lock);
	Slot & s = slots[slotIdx];

	if (!s.active || s.handle != handle)
		return; // detached, or the slot has been reused

	if (error < 0)
	{
		// The mouse is gone (or the read failed): stop reading it
		detachSlot(s);
		return;
	}

	if (bytes >= 3)
	{
		uint8_t buttons = 0;
		if (s.buffer[0] & 0x1) buttons |= MOUSE_BTN_LEFT;
		if (s.buffer[0] & 0x2) buttons |= MOUSE_BTN_RIGHT;
		if (s.buffer[0] & 0x4) buttons |= MOUSE_BTN_MIDDLE;
		s.buttons = buttons;

		push((int8_t)s.buffer[1], (int8_t)s.buffer[2]);
	}

	// Keep one read outstanding
	if (HIDRead(s.handle, s.buffer, s.readSize, ReadTrampoline, (void *)(intptr_t)slotIdx) < 0)
		detachSlot(s);
}

bool WutMouseDriver::isConnected() const
{
	if (!initialized || !lock)
		return false;

	// Attach and detach callbacks change the slots from the HID library's context
	MutexLock guard(*lock);
	for (int i = 0; i < kMaxMice; i++)
		if (slots[i].active)
			return true;
	return false;
}

bool WutMouseDriver::poll(MouseEvent & out)
{
	if (!initialized || !lock)
		return false;

	MutexLock guard(*lock);
	if (count == 0)
		return false;

	out = queue[head];
	head = (head + 1) % kQueueSize;
	count--;
	return true;
}
