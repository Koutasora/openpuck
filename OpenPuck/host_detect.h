// host_detect.h -- is the puck plugged into a PC or a console? Pure logic (no Arduino), driven by events the USB
// layer reports; tests/binds builds it with a host g++.
//
// Why it matters: only on a PC does the puck add a keyboard (and send mouse actions) next to the gamepad. A
// console must keep seeing exactly the gamepad it saw before, so the answer defaults to "unknown" = no keyboard.
//
// What a device can see of its host (nothing is certain; each source is tagged with HOSTWHY_*):
//   Windows  asks for the Microsoft OS 2.0 descriptor (vendor request from the BOS descriptor WebUSB carries).
//            Consoles never do.                                                    -> PC, strong
//   Chrome / Edge (any OS) fetch the WebUSB landing page or connect the panel.     -> PC, strong
//   PS4 / PS5 consoles run the Sony auth handshake: feature reports 0xF0..0xF3 on the pad interface. No PC
//            driver or game does.                                                  -> console, strong
//   Linux / macOS (hid-playstation, SDL, ...) read the identity feature reports (MAC, firmware, calibration) and
//            then go quiet. A console does that too, but follows with the auth reports within a second or so; so
//            "probed, and HOST_PROBE_PC_MS later still no auth" counts as a PC.   -> PC, weak
// A strong event always wins over a weak one; between two strong ones the latest wins (the cable moved).
#pragma once
#include <stdint.h>

#define HOST_UNKNOWN 0
#define HOST_PC 1
#define HOST_CONSOLE 2

// Manual setting (host.bin): what the user forces when auto guesses wrong.
#define HOSTSET_AUTO 0
#define HOSTSET_PC 1
#define HOSTSET_CONSOLE 2
#define HOSTSET_MAX 2

// Why the effective host is what it is.
#define HOSTWHY_NONE 0
#define HOSTWHY_MSOS2 1 // Windows asked for the MS OS 2.0 descriptor
#define HOSTWHY_WEBUSB 2 // a browser opened the WebUSB panel
#define HOSTWHY_PROBE 3 // identity reports read, no auth followed
#define HOSTWHY_AUTH 4 // PlayStation auth reports
#define HOSTWHY_MANUAL 5 // forced by the setting
#define HOSTWHY_REMEMBERED 6 // result of the previous session, no news yet

enum HostEvent {
	HE_MSOS2, // vendor request for the MS OS 2.0 descriptor
	HE_WEBUSB, // WebUSB landing page request or panel connect
	HE_PROBE, // feature report GET of a pad identity report
	HE_AUTH, // feature report 0xF0..0xF3 GET / SET on a pad interface
};

// Time after the first identity probe without any auth report that turns "unknown" into "PC".
#define HOST_PROBE_PC_MS 5000u

// setting = HOSTSET_*, remembered = HOST_* result stored by the previous session (HOST_UNKNOWN when none).
void hostDetectInit(uint8_t setting, uint8_t remembered);
// Safe from any context: only plain byte stores.
void hostEvent(enum HostEvent ev, uint32_t nowMs);
// Loop context, every iteration: resolves the probe timer.
void hostTick(uint32_t nowMs);

void hostSetSetting(uint8_t setting);
uint8_t hostSetting();
// Effective host (setting applied) and the reason for it.
uint8_t hostKind();
uint8_t hostWhy();
// Result of auto detection alone (HOST_UNKNOWN until something was seen), and whether it came from this session
// rather than from the remembered value; the caller stores it for the next boot.
uint8_t hostDetectedKind();
bool hostDetectedFresh();
