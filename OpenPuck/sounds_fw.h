// sounds_fw.h -- firmware side of sounds.h: sounds.bin, playing melodies as HAPTIC_LFO_TONE relays, events.
//
// Tones go out like rumble: relayEnqueue() of OUTPUT report 0x83 to every linked controller, one per voice
// side. sounds.bin is its own InternalFS file (a bad or missing one means the default melodies).
#pragma once
#include <stdint.h>
#include "sounds.h"

// Load sounds.bin (or defaults) and the "mode just switched" flag. Call after loadCfg().
void soundsFwInit();
// Main loop: sends the next note / stop of the running melody and starts pending event melodies.
void soundsFwTask();

// Melody of an event on every ready controller, when the confirmation setting includes sound. False if nothing
// was started (setting, no melody assigned, no controller ready).
bool soundsFwPlayEvent(uint8_t event);
// Melody slot n (listen from the console / page), whatever the confirmation setting. False when n is out of range
// or empty.
bool soundsFwPlaySlot(uint8_t n);
// A melody that is not stored (WebUSB preview): [count][count x {freq u16 LE, ms u16 LE}], one voice played
// on both trackpads. False when invalid.
bool soundsFwPlayRaw(const uint8_t *buf, uint8_t len);

// A controller (re)connected (haptics.cpp): play the connect melody once it is ready.
void soundsFwOnConnect(int slot);
// A mode switch is about to reboot the puck: play that mode's melody when a controller is back.
void soundsFwNoteModeSwitch();

// Confirmation settings as stored.
bool soundsFwVibrate();

// Persist g_sounds to sounds.bin.
void soundsFwSave();
// Set confirmation mode / volume (console); false when out of range. Saved.
bool soundsFwSetConfirm(uint8_t confirm);
bool soundsFwSetGain(int gainDb);

// WebUSB blob transfer, same scheme as binds_fw.h.
#define SOUNDS_CHUNK 96
#define SOUNDS_CHUNKS ((SOUNDS_BLOB_BYTES + SOUNDS_CHUNK - 1) / SOUNDS_CHUNK)
uint8_t soundsFwReadChunk(uint8_t idx, uint8_t *out);
bool soundsFwStageChunk(uint8_t idx, const uint8_t *data, uint8_t len);
bool soundsFwStageCommit();
