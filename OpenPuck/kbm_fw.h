// kbm_fw.h -- firmware side of the PC keyboard / mouse: host.bin, the keyboard USB interface, KEY / MOUSE reports.
//
// The mouse is the wake mouse every non-clean mode already has (wake_hid.h): the console sees the same
// descriptor it always saw. The keyboard is a new interface and only exists while the host is a PC (host_detect.h):
// the puck starts without it, and the loop re-enumerates once a PC is recognised (or remembered from the last
// session, or forced with the manual setting). Only modeHasKbm() modes take part; everywhere else KEY / MOUSE
// actions do nothing.
//
// host.bin holds the manual setting and the last auto-detected host; it is its own file so cfg.bin and binds.bin
// stay exactly as they were.
#pragma once
#include <stdint.h>
#include "binds.h"

// Load host.bin and start host detection. Call after loadCfg().
void kbmFwInit();
// Main loop: detection timers, persisting a new result, re-enumeration when the keyboard should appear / go.
void kbmFwTask();

// usbReenumerate() hooks, with k = mounted pad count. Begin: before the mode adds its pad interfaces (gives every
// pool object its pad role back); returns whether the keyboard will be part of this descriptor, so the caller can
// put that in the USB serial (hosts cache the configuration per serial). End: right after the pads, adds it.
bool kbmFwEnumBegin(uint8_t k);
void kbmFwEnumEnd(uint8_t k);

// binds_fw.cpp, once per input report: the KEY / MOUSE state of `slot`.
void kbmFwSubmit(uint8_t slot, const BindsKbm &k);

// Manual host setting (HOSTSET_*), saved. False when out of range.
bool kbmFwSetHostSetting(uint8_t setting);
// Console line / panel text for the detected host.
const char *kbmFwHostName();
const char *kbmFwHostWhyName();
// One byte for the live view: bits 0-1 host (HOST_*), bits 2-3 setting (HOSTSET_*), bits 4-7 why (HOSTWHY_*).
uint8_t kbmFwHostByte();
// The keyboard interface is part of the descriptor the host holds right now.
bool kbmFwKeyboardMounted();
