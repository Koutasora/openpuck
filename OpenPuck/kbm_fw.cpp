#include "kbm_fw.h"
#include "config.h"
#include "controllers.h"
#include "host_detect.h"
#include "usb_mount.h"
#include "usb_tx.h"
#include "wake_hid.h"
#include "bonds.h"
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <Adafruit_TinyUSB.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;

#define HOST_FILE "/host.bin"
#define HOST_MAGIC 0xB7
// The keyboard has to be wanted (or not wanted) this long before the USB device re-enumerates: absorbs the burst
// of probes a host sends while it settles, and keeps a flapping result from flapping the descriptor.
#define KBD_SETTLE_MS 1500u

// Non-boot keyboard: the host needs no BIOS protocol, and the boot variant is the one that failed to enumerate
// on Windows next to the wake mouse.
static const uint8_t KBD_DESC[] = { TUD_HID_REPORT_DESC_KEYBOARD() };

// pool object serving as keyboard, nullptr = not in the descriptor
static Adafruit_USBD_HID *s_kbd;
// setting and detected host as stored in host.bin
static uint8_t s_saved[2] = { 0xFF, 0xFF };
static uint32_t s_diffSinceMs;
static bool s_diff;

// A controller that stops reporting (link lost, powered off) must not leave a key held down: the poll delivers
// ~250 reports/s per linked controller, so this much silence means it is gone.
#define SLOT_STALE_MS 300u

static BindsKbm s_slot[NSLOT];
static uint32_t s_slotMs[NSLOT];
static hid_keyboard_report_t s_sentKbd;
static uint8_t s_sentButtons;
static int8_t s_wheelPending;

static void save()
{
	const uint8_t set = hostSetting(), det = hostDetectedKind();
	uint8_t b[4] = { HOST_MAGIC, set, det, (uint8_t)(set ^ det ^ 0xA5) };
	InternalFS.remove(HOST_FILE);
	File f(InternalFS);
	if (f.open(HOST_FILE, FILE_O_WRITE)) {
		f.write(b, sizeof b);
		f.close();
	}
	s_saved[0] = set;
	s_saved[1] = det;
}

void kbmFwInit()
{
	uint8_t b[4] = { 0 };
	File f(InternalFS);
	if (f.open(HOST_FILE, FILE_O_READ)) {
		if (f.read(b, sizeof b) != (int)sizeof b)
			b[0] = 0;
		f.close();
	}
	const bool ok = b[0] == HOST_MAGIC &&
			b[3] == (uint8_t)(b[1] ^ b[2] ^ 0xA5) &&
			b[1] <= HOSTSET_MAX;
	hostDetectInit(ok ? b[1] : HOSTSET_AUTO, ok ? b[2] : HOST_UNKNOWN);
	if (ok) {
		s_saved[0] = b[1];
		s_saved[1] = b[2];
	}
}

// the keyboard interface could not be added to the descriptor
static bool s_failedKbd;

// What this boot's USB configuration should contain, with k pads mounted.
static bool wantKeyboard(uint8_t k)
{
	return !s_failedKbd && modeHasKbm(g_usbMode) && wakeHidPresent() &&
	       hostKind() == HOST_PC && g_active && g_active->kbdCarrier(k);
}

// decision of the enumeration in progress (host state can move under us on the usbd task)
static bool s_enumWant;

bool kbmFwEnumBegin(uint8_t k)
{
	if (s_kbd)
		usbTxClear(s_kbd);
	s_kbd = nullptr;
	s_enumWant = wantKeyboard(k);
	if (g_active && modeHasKbm(g_usbMode))
		for (uint8_t j = 0; j < NSLOT; j++)
			g_active->padRole(j);
	return s_enumWant;
}

void kbmFwEnumEnd(uint8_t k)
{
	s_diff = false;
	if (!s_enumWant)
		return;
	Adafruit_USBD_HID *h = g_active->kbdCarrier(k);
	usbTxClear(h);
	h->enableOutEndpoint(false);
	h->setReportCallback(NULL, NULL);
	h->setReportDescriptor(KBD_DESC, sizeof KBD_DESC);
	h->setBootProtocol(HID_ITF_PROTOCOL_NONE);
	h->setPollInterval(10);
	if (USBDevice.addInterface(*h))
		s_kbd = h;
	else
		// no loop of re-enumerations: keep it off until the setting or host changes
		s_failedKbd = true;
	memset(&s_sentKbd, 0, sizeof s_sentKbd);
}

void kbmFwSubmit(uint8_t slot, const BindsKbm &k)
{
	if (slot >= NSLOT)
		return;
	s_slot[slot] = k;
	s_slotMs[slot] = millis();
	if (k.wheel && s_wheelPending > -100 && s_wheelPending < 100)
		s_wheelPending = (int8_t)(s_wheelPending + k.wheel);
}

// All slots at once: modifiers and buttons OR, usages unique up to the 6 a keyboard report holds.
static void merge(hid_keyboard_report_t &kb, uint8_t &buttons)
{
	memset(&kb, 0, sizeof kb);
	buttons = 0;
	uint8_t n = 0;
	const uint32_t now = millis();
	for (uint8_t s = 0; s < NSLOT; s++) {
		if ((uint32_t)(now - s_slotMs[s]) > SLOT_STALE_MS)
			continue;
		kb.modifier |= s_slot[s].mods;
		buttons |= s_slot[s].buttons;
		for (uint8_t i = 0; i < BINDS_KEYS_MAX; i++) {
			const uint8_t u = s_slot[s].keys[i];
			bool have = !u;
			for (uint8_t j = 0; j < n; j++)
				have |= kb.keycode[j] == u;
			if (!have && n < 6)
				kb.keycode[n++] = u;
		}
	}
}

static void sendState()
{
	const bool allowed = modeHasKbm(g_usbMode) && hostKind() == HOST_PC;
	hid_keyboard_report_t kb;
	uint8_t buttons;
	if (allowed) {
		merge(kb, buttons);
	} else {
		// a host that stopped being a PC (manual switch) must not keep a key held down
		memset(&kb, 0, sizeof kb);
		buttons = 0;
		s_wheelPending = 0;
	}
	if (s_kbd && memcmp(&kb, &s_sentKbd, sizeof kb)) {
		usbTxHid(s_kbd, 0, &kb, sizeof kb);
		s_sentKbd = kb;
	}
	if (buttons != s_sentButtons || s_wheelPending) {
		if (wakeHidMouse(buttons, s_wheelPending)) {
			s_sentButtons = buttons;
			s_wheelPending = 0;
		}
	}
}

void kbmFwTask()
{
	const uint32_t now = millis();
	hostTick(now);
	if (hostDetectedFresh() &&
	    (s_saved[0] != hostSetting() || s_saved[1] != hostDetectedKind()))
		save();
	sendState();
	// keyboard in the descriptor <-> keyboard wanted
	const bool want = wantKeyboard(g_usbMountCount);
	if (want == (s_kbd != nullptr)) {
		s_diff = false;
		return;
	}
	if (!s_diff) {
		s_diff = true;
		s_diffSinceMs = now;
	} else if ((uint32_t)(now - s_diffSinceMs) >= KBD_SETTLE_MS) {
		s_diff = false;
		usbReenumerate(g_usbMountCount);
	}
}

bool kbmFwSetHostSetting(uint8_t setting)
{
	if (setting > HOSTSET_MAX)
		return false;
	hostSetSetting(setting);
	s_failedKbd = false;
	save();
	return true;
}

bool kbmFwKeyboardMounted()
{
	return s_kbd != nullptr;
}

const char *kbmFwHostName()
{
	switch (hostKind()) {
	case HOST_PC:
		return "PC";
	case HOST_CONSOLE:
		return "console";
	default:
		return "unknown";
	}
}

const char *kbmFwHostWhyName()
{
	switch (hostWhy()) {
	case HOSTWHY_MSOS2:
		return "Windows asked for the MS OS 2.0 descriptor";
	case HOSTWHY_WEBUSB:
		return "a browser opened the WebUSB panel";
	case HOSTWHY_PROBE:
		return "identity reports read, no console auth followed";
	case HOSTWHY_AUTH:
		return "PlayStation auth reports";
	case HOSTWHY_MANUAL:
		return "set by hand";
	case HOSTWHY_REMEMBERED:
		return "remembered from the last session";
	default:
		return "nothing seen yet";
	}
}

uint8_t kbmFwHostByte()
{
	return (uint8_t)((hostKind() & 3) | ((hostSetting() & 3) << 2) |
			 (hostWhy() << 4));
}
