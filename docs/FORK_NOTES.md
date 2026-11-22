# Notes for upstream: what this fork changes

This fork ([Koutasora/openpuck](https://github.com/Koutasora/openpuck)) is based on upstream commit `512a406`
("Add mode 13: real PS5 console via a P5General auth adapter (#342)"). Upstream `main` has not moved since, so
`git diff 512a406 main` in the fork shows everything below (about 60 files, +8k lines, most of it in
`docs/index.html`, new firmware modules and tests).

**You are welcome to take anything from it, in whole or in part, without asking and without credit.** The code is
under the project's AGPL-3.0; as the author of these changes I also allow you to reuse them in upstream under
whatever terms upstream uses. If a feature is useful but you would rather have it as a focused PR, say which one
and I will split it out.

## Firmware

| Area | What | Where |
|---|---|---|
| Back paddles | Each of L4 R4 L5 R5 can be a button, a combo, a macro (steps with delays), a keyboard key or a mouse action, per controller type (Xbox, Switch, DS4, DS5). | `binds.*`, `binds_fw.*`, PROTOCOL §10.2 |
| Profiles | 3 profiles, switched on the pad with back4 + RB / LB; confirmed by N vibration pulses or a melody. | `binds_fw.cpp` |
| Keyboard and mouse on PC | In `MODE_PS5` and `MODE_HIDGYRO` the puck adds a keyboard and a mouse interface, so a paddle can type or click; console modes keep their exact descriptors. PC/console detection (with a manual override). | `kbm_fw.*`, `host_detect.*`, `usb_tx.*`, `wake_hid.*`, PROTOCOL §10.3 |
| Melodies | 8 melody slots, each with 4 voices (left trackpad, right trackpad, left grip, right grip), up to 32 notes per voice, assigned to events: profile 1-3, mode change per family (PlayStation / Xbox / Switch / other), controller connected. Played through `HAPTIC_LFO_TONE` (0x83) via the existing relay, one sequencer per voice. | `sounds.*`, `sounds_fw.*`, PROTOCOL §10.5 |
| Back4 chords | A chord can be set to None (`0xFE`). | `config.*`, PROTOCOL §10.6 |
| Diagnostics | Request `0x2C` / frame `0xB4`: counters along the back-paddle path (raw buttons, binds output, queued reports, USB endpoint results). | PROTOCOL §10.4 |

New status-blob protocol numbers (each gate is documented in `webusb_config.cpp` and PROTOCOL.md):
23 binds and profiles, 24 manual host setting, 25 melody ops `0x2D..0x31`, 26 chord None, 27 sounds blob
version 2 (4 voices). Data lives in separate LittleFS files (`binds.bin`, `sounds.bin`); upstream firmware never
reads or deletes them, and `cfg.bin` keeps its layout.

## Haptic tone findings (tested on a new Steam Controller through an OpenPuck in Steam mode)

These may be useful even without the rest of the fork:

- `0x83 HAPTIC_LFO_TONE` `[side][gain_db i8][freq u16][dur u16][lfo_freq u16][lfo_depth]` is audible and felt.
- `side`: 0 = left trackpad, 1 = right trackpad, 2 = both trackpads, 3 = left grip, 4 = right grip.
- `dur` ends the tone by itself. An "off tone" (`gain` -128, `freq` 0, `dur` 0x8000) stops one side; a zero
  `0x80` rumble report stops it too.
- At -24 dB every note from MIDI 24 to 48 and from 84 to 108 was audible on a trackpad (side 0) and on a grip
  (side 3), so no octave shifting is needed for low notes.
- Only the puck interface of the slot with a live controller passes the tone on (the slot gate in the relay).
- Chrome opens the puck's vendor HID interface (`28DE:1304`, usage page `0xFF00`) over WebHID with Steam
  running, so a page can send 0x83 directly.

## Configurator (`docs/index.html`)

- Steam Controller drawings (Valve, CC BY-NC-SA 4.0, see `docs/img/LICENSE-valve.txt`), per-paddle assignment UI,
  profiles, macro editor and recorder, live view, PNG shortcut sheet.
- Sounds: piano roll editor with 4 voice lanes, MIDI import by drag and drop (tracks or channels to lanes, BPM
  from the file), BPM stretch and beat grid, "Crop to 32", live playback on the controller over WebHID
  (`0x83` per side), browser preview, save to the puck.
- One Shortcuts table (Mapping > Shortcuts): chord to mode, the melody of each chord's mode family, profile and
  connect melodies, sound or vibration confirmation, volume.
- A haptic test panel in Status > Debug (tone per side, stop variants, sweep).
- Backup / restore includes binds and sounds, converting between sounds blob versions.

## Tests and tooling

- Host tests with plain g++: `tests/binds` (binds logic, host detection) and `tests/sounds` (melody logic, blob
  v1/v2, 4-voice player).
- `tests/sounds-web`: Playwright test of the configurator's Sounds and Shortcuts parts with a fake WebHID device.
- `tools/build-lab.sh`: build on Windows without make (portable arduino-cli); `tools/make-midi.py`: the sample
  melodies in `docs/midi` (own motifs, no console or game sounds).

Firmware changes pass clang-format 18 (`make check` is clean) and add no trailing-comment lint findings.
