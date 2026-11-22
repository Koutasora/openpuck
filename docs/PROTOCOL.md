# OpenPuck Controller Protocol

This document describes the complete controller-to-puck protocol implemented by `OpenPuck.ino`.

## 1. Scope

OpenPuck emulates the Valve Steam Controller 2 "puck" dongle. There are two separate protocols:

1. USB host-facing protocol between the puck and the computer.
2. 2.4 GHz radio protocol between the puck and the controller.

Both are required for a fully compatible implementation.

## 2. Bond record

A bond slot is 24 bytes:

```text
offset  size  meaning
0x00    4     proteus_uuid / token, little-endian
0x04    4     ibex_uuid, little-endian
0x08    16    controller serial, ASCII, not null-terminated if fully filled
```

The puck stores four independent bond slots. In Steam mode each slot has its own USB HID interface; Xbox mode selects only one RF controller for one static USB gamepad (§9.2).

An empty slot is all zeroes.

## 3. USB protocol

### 3.1 Steam-mode USB layout

Steam mode enumerates as Valve `28DE:1142` and exposes four HID interfaces with the same report descriptor as the real puck. Each interface owns one bond slot. The firmware uses the classic Steam Controller dongle PID because SDL's Steam HIDAPI driver reliably treats `0x1142` as the four-slot wireless receiver; newer puck PIDs may work in Steam but are not consistently surfaced as Steam Controllers to SDL apps. SDL also expects the Steam slot HIDs on USB interface numbers 1..4, so WebUSB is registered before the slot HIDs in puck mode to reserve interface 0.

Relevant report IDs:

- Input: `0x40`, `0x41`, `0x42`, `0x43`, `0x44`, `0x45`, `0x79`, `0x7B`
- Output: `0x80` to `0x89`
- Feature: `0x01`, `0x02`

### 3.2 Feature commands

Feature payload framing is:

```text
[cmd][len][payload...]
```


Implemented commands:

- `0x83`: `GET_ATTRIBUTES_VALUES`
- `0xAE`: `GET_STRING_ATTRIBUTE`
- `0xB4`: `DONGLE_GET_WIRELESS_STATE` (connection state for the interface's slot)
- `0xAD`: `ENABLE_PAIRING` (pairing mode enable/disable)
- `0xA2`: `TRITON_A2_OBSERVED_PAIRING_RECORD` (write or clear the interface's 24-byte bond slot)
- `0xA3`: `TRITON_A3_BOND_EVENT_OR_STATUS` (read the interface's 24-byte bond slot)

For a full list of commands the controller supports, check steam_commands.h

#### `0xB4` response

```text
[0xB4][0x01][state]
```

- `0x02`: controller connected on this slot
- `0x01`: not connected

#### `0xA2` write

`len` must be at least `24`. A zero-filled 24-byte payload clears the slot. Any non-zero valid payload writes the bond verbatim.

#### `0xA3` read

Returns:

```text
[0xA3][0x18][24-byte bond]
```

### 3.3 Host-to-controller relay

When Steam writes feature report `0x01`, OpenPuck forwards it over RF to the controller as an `E3` frame
containing a sub-TLV. There are two on-air forms:

```text
haptic   [E3][1+len][0x05][report_id][data...]             (type 05, no inner-len)
normal  [E3][2+len][0x01][report_id][innerlen][data...]    (type 01, KEEPS inner-len)
```


```text
haptic on    E3 04 05 82 01 01 F7              (report 0x82, legacy)
brightness   E3 05 01 87 03 2D <val> 00        (report 0x87 reg 0x2D, LANDING)
power-off    E3 06 01 9F 04 6F 66 66 21        (report 0x9F "off!", LANDING)
```

The relay carries the command's declared length, up to 60 bytes — the most one RF frame fits. Relays are staged
in a small ring (not a single buffer): the USB SET callbacks run in ISR context and Steam sends
settings/calibration as back-to-back bursts, so a single pending slot both drops reports and can be torn
mid-flush. One queued relay is emitted per poll cycle.

### 3.4 Connection presentation (input reports `0x79` / `0x7B`)

- `0x79` (1 byte): connection state, `0x02` = connected, `0x01` = disconnected. **Edge-triggered**, like the
  real puck. OpenPuck additionally re-sends it every 750 ms after a connect edge *until Steam reacts* (its
  first OUTPUT/settings write after the edge), covering a missed edge. It must not be re-sent unconditionally:
  each `0x79=02` can re-trigger Steam's connect handling (connect chime) and loop the haptic before Steam
  starts consuming `0x45`.
- `0x7B` (12 bytes): periodic status, sent every 2 s while connected. Byte 8 is the controller→puck signal
  strength as **signed dBm** (live-captured example `0xDD` = −35 dBm). OpenPuck fills it with the smoothed
  radio RSSI sampled on each CRC-good controller reply (`RSSISAMPLE`, started by an `ADDRESS→RSSISTART`
  short during the poll RX window). The other bytes are replayed from the live capture
  (`F7 01 89 00 00 00 03 00 __ 00 3A 02`); their semantics are unconfirmed.

## 4. Radio physical layer

OpenPuck uses Nordic raw radio mode configured to match the puck firmware:

- PHY: `Ble_2Mbit`
- Dynamic-length packet format
- `PCNF0 = 0x00030008`
- `PCNF1 = 0x01040040`
- 5-byte address (`BALEN=4`)
- Big-endian
- Whitening disabled
- CRC16, address included
- Polynomial `0x11021`
- Init `0xFFFF`

CRC is standard CRC-16-CCITT over address plus payload, MSB-first.

### 4.1 Address transform

Stored address bytes are converted to on-air address bytes by bit-reversing each byte.

For the nRF `BASE0` register, the bit-reversed base bytes are then packed big-endian:

```text
BASE0 = bitrev8(base[0]) << 24 |
        bitrev8(base[1]) << 16 |
        bitrev8(base[2]) <<  8 |
        bitrev8(base[3])

PREFIX0 = bitrev8(prefix)
```

## 5. Discovery / reconnect

Discovery uses a fixed address and fixed channel:

- Base bytes: ASCII `"ibex"` = `69 62 65 78`
- Prefix: `0x10`
- Channel: `2`

The puck transmits a host frame on that address until the controller connects.

## 6. Host frame (`E1`)

OpenPuck's discovery host frame is a dynamic-length ESB payload:

```text
RAM buffer:
[0]  = 0x12                     ; payload length
[1]  = S1 byte                  ; PID in bits 1..2, no-ack bit clear
[2]  = 0xE1                     ; host-frame opcode
[3..6]   = proteus_uuid         ; 4 bytes, little-endian
[7..10]  = ibex_uuid            ; 4 bytes, little-endian
[11]     = session channel
[12..14] = 0x00 0x00 0x00
[15..18] = session base         ; 4 bytes, stored form
[19]     = session prefix
```

Semantics:

- `proteus_uuid` is the first 4 bytes of the bond record.
- `ibex_uuid` is the next 4 bytes of the bond record.
- The host chooses the connected-session channel and address and advertises them here.
- The session base/prefix are PER-BOND (one address per controller, derived from the bond UUID + the
  puck's FICR DEVICEID so two pucks that bond the same controller still isolate). The real puck
  supports up to four simultaneous controllers on the same channel; OpenPuck mirrors that.

## 7. Connected session

After discovery, the puck becomes the poll master.

### 7.1 Controller wake / version frame (`E7`)

OpenPuck sends:

```text
E7 00 00
```

This is the normal "host awake" form.

### 7.2 Input poll (`E3`)

OpenPuck polls for report `0x45` using:

```text
E3 [len] 0x01 0x45 [param]
```

- `0x01`: GET subtype
- `0x45`: controller input report
- `param`: implementation tunable, usually `0x00` or `0x2D`

The controller replies in the ACK window.

### 7.3 Controller replies

Observed reply opcodes:

- `0xF1`: full TLV container carrying input data
- `0xF2`: disconnect / shutdown
- `0xF3`: status

#### `0xF1` container

The TLV scan starts at byte 6. Each record is:

```text
[len][tag][value...]
```

Known tags:

- `0x02`: 4-byte control/status field
- `0x04`: bulk data blob
- `0x06`: embedded report wrapper

For controller input, the embedded report is `0x45`.

## 8. Report `0x45` layout

Report `0x45` is 46 bytes:

```text
offset  size  meaning
0x00    1     report id = 0x45
0x01    1     sequence
0x02    4     buttons, little-endian u32
0x06    2     left trigger, u16
0x08    2     right trigger, u16
0x0A    2     left stick X, s16
0x0C    2     left stick Y, s16
0x0E    2     right stick X, s16
0x10    2     right stick Y, s16
0x12    2     left pad X, s16
0x14    2     left pad Y, s16
0x16    2     left pad press, s16
0x18    2     right pad X, s16
0x1A    2     right pad Y, s16
0x1C    2     right pad press, s16
0x1E    4     IMU timestamp, u32
0x22    2     accel X, s16
0x24    2     accel Y, s16
0x26    2     accel Z, s16
0x28    2     gyro X, s16
0x2A    2     gyro Y, s16
0x2C    2     gyro Z, s16
```

### 8.1 Button bitfield

```text
0x00000001 A
0x00000002 B
0x00000004 X
0x00000008 Y
0x00000010 QAM
0x00000020 R3
0x00000040 View
0x00000080 R4
0x00000100 R5
0x00000200 RB
0x00000400 D-pad down
0x00000800 D-pad right
0x00001000 D-pad left
0x00002000 D-pad up
0x00004000 Menu
0x00008000 L3
0x00010000 Steam
0x00020000 L4
0x00040000 L5
0x00080000 LB
0x00100000 right stick touch
0x00200000 right pad touch
0x00400000 right pad click
0x00800000 right trigger click
0x01000000 left stick touch
0x02000000 left pad touch
0x04000000 left pad click
0x08000000 left trigger click
0x10000000 right grip touch
0x20000000 left grip touch
```

## 9. USB presentation modes

The RF side stays the same across modes. Only USB enumeration changes.

USB personalities include Steam (`0`), Xbox 360 (`1`), Hori (`2`), Switch Pro (`4`),
and the other modes listed in [../README.md](../README.md). Xbox 360 uses a static
wired-controller layout: `clearConfiguration` removes the auto-added interfaces,
and `bcdUSB` remains `0x0200` (no USB-2.1 BOS). It has no CDC, WebUSB, mouse, or wake
HID. Mode switches detach and reboot to rebuild the USB descriptors; Xbox RF
joins/departures do not change its USB layout or trigger re-enumeration.

### 9.1 Steam mode

- VID:PID `28DE:1142`
- Four puck HID interfaces (CDC + WebUSB also present)
- `0x45` reports are forwarded to the connected slot's HID interface
- **Seamless lizard**: when Steam is driving the device, `0x45` is forwarded; when it isn't
  (Steam closed, 7 s watchdog) the same `0x45` is translated into mouse (`0x40`) + keyboard (`0x41`)
  reports on the **same** puck interface, so the device is a driverless desktop keyboard+mouse with no
  mode switch. This is purely USB-side; the RF poll and relay are unchanged. (There is no standalone
  lizard mode.)
- **"Steam is driving" signal**: *any* Steam OUTPUT/settings report (`0x80`–`0x89`, e.g. the `0x87`
  lizard-off heartbeat, LED, or a `0x82` haptic) refreshes the watchdog — not just `0x87`. This makes
  the puck leave lizard for gamepad on Steam's **first** contact, even if that first packet is a haptic
  that arrives before the heartbeat.
- **Haptics are gated on this decision**: haptic reports are **never** relayed to the controller while
  the puck is presenting lizard. Relaying haptics while Steam isn't reading `0x45` back made Steam loop
  the same command, leaving the controller buzzing; suppressing them keeps the lizard state clean. The
  same gate applies during the post-resume input mute (`POST_RESUME_MUTE_MS`): Steam can't read `0x45`
  back in that window either, so a wake-time haptic would loop identically.
- **Which OUTPUT reports are relayed**: the haptic/actuator reports `0x80`–`0x86` are forwarded to the
  **connected slot only** (the slot gate is what stops a haptic aimed at another of the four exposed
  slots from buzzing the single controller). The 63-byte settings/config reports `0x87`/`0x88`/`0x89`
  are not haptics and are not pushed on this path (`0x87` lizard-off reaches the controller via the
  feature `0x01` passthrough). Steam owns `IMU_MODE` in native puck mode. Switch Pro mode instead writes
  raw accelerometer + gyroscope (`0x18`) when the RF link comes up because the controller retains Steam's
  IMU-off state across pucks. Restricting haptics to `0x82` alone silently dropped the ping/grip/test
  haptics, which use other report IDs.

#### ⚠️ Two different `0x8x` id spaces — never share a rule between them

Steam drives the actuators through the Triton **OUTPUT report** space, which is a *different* id space
from the feature-`0x01` **command** space even though the numbers overlap. Ground truth: SDL
`src/joystick/hidapi/steam/controller_structs.h` (`ValveTritonOutReportMessageIDs`) and
`controller_constants.h`. The payload sizes match this firmware's report descriptor exactly:

| id | OUTPUT report (haptics/actuators) | payload | feature `0x01` command |
|----|-----------------------------------|---------|------------------------|
| `0x80` | `HAPTIC_RUMBLE` `[type][intensity u16][{speed u16,gain}L][{…}R]` | 9 | `SET_DIGITAL_MAPPINGS` |
| `0x81` | `HAPTIC_PULSE` `[side][on_us u16][off_us u16][repeat u16]` | 7 | `CLEAR_DIGITAL_MAPPINGS` |
| `0x82` | `HAPTIC_COMMAND` `[side][command][gain_db]` | 3 | `GET_DIGITAL_MAPPINGS` |
| `0x83` | `HAPTIC_LFO_TONE` `[side][gain_db][freq u16][dur u16][lfo_freq u16][lfo_depth]` | 9 | `GET_ATTRIBUTES_VALUES` |
| `0x84` | `HAPTIC_LOG_SWEEP` `[side][gain_db][dur u16][start u16][end u16]` | 8 | `GET_ATTRIBUTE_LABEL` |
| `0x85` | `HAPTIC_SCRIPT` `[side][script_id][gain_db]` | 3 | `SET_DEFAULT_DIGITAL_MAPPINGS` |
| `0x86` | (unnamed) | 3 | `FACTORY_RESET` |
| `0x87`+ | 63-byte settings/config | 63 | `SET_SETTINGS_VALUES` … |


### 9.2 Xbox mode

- `MODE_XBOX` (`1`): one static wired Xbox 360 pad, Microsoft VID:PID `045E:028E`,
  `bcdDevice=0x0114`, device class/subclass/protocol `FF/FF/FF`.
- Four vendor interfaces: control (`0`, `FF/5D/01`), audio (`1`, `FF/5D/03`,
  no accessory), plugin (`2`, `FF/5D/02`, no accessory), security (`3`,
  `FF/FD/13`). Only gamepad endpoints `0x81` IN and `0x02` OUT are opened;
  the advertised accessory endpoints are not serviced. Configuration length
  is 153 bytes (9-byte header + 144-byte body).
- The first fresh report from a live RF bond selects the controller. Selection
  stays fixed through brief gaps, until more than 1200 ms without an RF reply
  (or bond removal). RF injects synthetic neutral input after 300 ms of silence;
  that releases held inputs but cannot acquire an unselected bond. A replacement
  controller does not require USB re-enumeration.
- `0x45` is converted into a 20-byte input report, including Guide. Valid rumble
  OUT packets are relayed only to the selected RF controller. Headset/chatpad
  functions are **not emulated**. LED commands are parsed, but the physical
  player ring is not equivalent to a genuine Xbox 360 pad.
- Retail XSM3 authentication runs in software using libxsm3 on the nRF52840:
  no donor controller, USB host adapter, or additional hardware. This is wired
  USB authentication, not Xbox wireless RF pairing.
- Vendor requests `0x81` (identification), `0x82` (init), `0x86` (status),
  `0x83` (response), and `0x87` (verify) are handled through EP0. Init/verify
  require exactly 34/22 received bytes; invalid lengths, checksums, or MACs fail
  closed without a ready/stale response. USB RESET invalidates pending work so
  a worker finishing afterward cannot publish its old result. USB and XSM3
  identification use a stable, hardware-derived 12-character serial.
- A dedicated idle-priority crypto task has a 2048-word (8 KiB) stack, separate
  from the small USB-task stack. The main loop lends it 1 ms between RF polls
  only while authentication work is pending/running, never inside an RX window.
- The string callback wrapper supplies the full **178-byte** security string
  descriptor at index 4 (88 UTF-16 code units plus header), avoiding the core's
  truncation. Vendor-routing and EP0 received-length wrappers are also required;
  see [BUILD_AND_DEPLOY.md](BUILD_AND_DEPLOY.md).

Configure in Steam mode using fixed **back-4 + A**, then return with **back-4 + X**
(default, configurable). Xbox has no mouse, WebUSB, CDC, or wake HID. See
[XBOX360.md](XBOX360.md) for licensing and physical tests. **Stock-console support
is experimental, not verified compatibility:** the default Adafruit nRF52 1.7.0
build passed, but the firmware has not been flashed or console-tested. Independent
synthetic native fixtures are not evidence of acceptance by a real console.

### 9.3 Switch mode

- VID:PID `0F0D:0092` (HORI Pokkén Tournament Pro Pad), clean device (no CDC/WebUSB)
- Single HID interface with the canonical HORIPAD descriptor (interrupt IN + OUT endpoints), accepted by
  a real Switch console with no handshake; an 8-byte report is streamed at ~250 Hz

### 9.4 Original Xbox mode

- VID:PID `045E:0289` (Microsoft Controller S). Composite: the XID interface plus the wake mouse and
  the WebUSB panel, so the config panel stays reachable on a PC
- Proprietary XID interface (class `0x58`, subclass `0x42`, protocol `0x00`) with interrupt IN +
  OUT endpoints; 20-byte input reports, rumble applied from the OUT endpoint
- The console refuses the controller until three `0xC1` vendor control requests are answered:
  `bRequest 0x06` / `wValue 0x4200` (XID descriptor), and `bRequest 0x01` with `wValue 0x0100` /
  `0x0200` (input and output capabilities)
- Also answers the XID report requests the protocol carries on EP0, in parallel with the interrupt
  endpoints: `GET_REPORT` (`0xA1 0x01`, `wValue 0x0100`) and `SET_REPORT` (`0x21 0x09`, `wValue 0x0200`)

## 10. WebUSB control channel

The WebUSB vendor interface is absent in Xbox 360 and clean PlayStation modes.
For Xbox configuration or updates, return to Steam mode with **back-4 + A**, use
the panel on a PC, then **back-4 + X** (default assignment) to return to Xbox.

Messages:

- Host to device:
  - `0x01`: get status blob
  - `0x02 <field> <value>`: set one field. Notable fields: `22` host-rumble strength as **percent/2**
    (10–500%, revived in blob version 21), `39` host-rumble style (`RUMBLE_STYLE_*` in `haptics.h`:
    0 normal, 1 mono, 2 heavy, 3 light, 4 swapped, 5 punchy, 6 soft), `38` Switch Pro gyro mapping
  - `0x03 <mode>`: switch mode and reboot
  - `0x07`: re-init haptics (clear a stuck buzz)
  - `0x08`: send controller power-off
  - `0x16`: test rumble — buzz every linked controller with the configured style/strength for 500 ms,
    auto-stopped by the firmware. **Requires status-blob version ≥ 21**; older firmware drops it silently
    (the parser only accepts `0x01`–`0x15` and `0x20`–`0x25`).
  - `0x09`: export all bond slots (reply: `0xA7` frame) — see §10.1
  - `0x0A 0x45 0x52 0x53`: factory erase (`"ERS"` magic), then reboot
  - `0x0B` / `0x0C`: reboot into serial DFU / UF2 bootloader
  - `0x0D <slot> <used> <24-byte rec>`: write one bond slot into RAM — see §10.1
  - `0x0E <mode>`: commit imported bonds (+ apply mode, `0xFF` = unchanged), then reboot
  - `0x11`: get the lizard binding map (device replies `0xAA`). **Requires status-blob version ≥ 16** —
    older firmware drops the unknown op silently and never replies, so a host that issues a blocking read
    for the `0xAA` frame would hang the shared endpoint. The panel gates all `0x11`–`0x15` sends on the
    version byte (`0xA5` payload byte 0) reaching 16 first.
  - `0x13 <count>`: begin a lizard-map edit — set the binding count
  - `0x12 <idx> <16-byte binding>`: set one lizard binding (see below)
  - `0x14`: commit the edited lizard map to flash (device echoes `0xAA`)
  - `0x15`: reset the lizard map to built-in defaults (device echoes `0xAA`)
  - `0x26 <idx>`: read chunk `idx` of the back-paddle binds blob (reply `0xB2`). **Requires status-blob version ≥ 23**
    (older firmware drops `0x26`–`0x2A` silently). See §10.2.
  - `0x27 <slot>`: live state of `slot` (`0xFF` = current slot), reply `0xB0`; the panel may poll it at ~30 Hz
  - `0x28 <idx> <len> <data>`: stage binds blob chunk (`len` 1..96; chunk 0 restarts staging, chunks must be in order)
  - `0x29`: validate + apply + save the staged blob (`binds.bin`)
  - `0x2A <profile>`: set active profile 0..2 (saved, confirmation buzz of profile+1 pulses)
  - `0x2B <setting>`: manual host setting, `0` auto, `1` always PC, `2` always console (saved in `host.bin`),
    acked with `0xB3`. **Requires status-blob version ≥ 24.** See §10.3.
  - `0x2C`: diagnostics, reply `0xB4` (§10.4). Sent only while the panel is connected.
  - `0x2D <idx>`: read chunk `idx` of the sounds blob (reply `0xB5`). **Requires status-blob version ≥ 25** (older
    firmware drops `0x2D`–`0x31` silently). See §10.5. For `idx` out of range (or a full TX buffer) there is no `0xB5`
    reply; the host retries.
  - `0x2E <idx> <len> <data>`: stage sounds blob chunk (`len` 1..96; chunk 0 restarts staging, chunks in order)
  - `0x2F`: validate + apply + save the staged blob (`sounds.bin`)
  - `0x30 <n>`: play stored melody `n` (0..7) now
  - `0x31 <count> <count x 4 bytes>`: play the given melody now, not stored
  - `0x20`–`0x24`: staged firmware update (begin/data/end/reboot/abort), acked with `0xAB` frames
  - `0x25 0x57 0x49 0x50 0x45`: **full board wipe** (`"WIPE"` magic, debug panel only). Erases the app
    region + LittleFS (settings + bonds) + bootloader-settings page and reboots app-less, so the board mounts
    as the UF2 bootloader drive on **every** boot until firmware is flashed again. Unlike `0x0A` (factory
    reset, which keeps the firmware), this leaves no trace of OpenPuck. Irreversible without re-flashing.
- Device to host:
  - `0xA5 <len> <payload>`: status blob
  - `0xA7 <len> <payload>`: bond export (§10.1)
  - `0xA8 ...`: flight-recorder stream
  - `0xA9 ...`: live wedge report
  - `0xAA <count> <count×16-byte bindings>`: lizard binding map
  - `0xAB 5 <status> <nextOff u32 LE>`: firmware-update ack
  - `0xB0 25 <payload>`: live state (§10.2)
  - `0xB2 <2+n> <idx> <total> <n bytes>`: binds blob chunk (§10.2)
  - `0xB4 119 <payload>`: diagnostics (§10.4)
  - `0xB5 <2+n> <idx> <total> <n bytes>`: sounds blob chunk (§10.5)
  - `0xB6 3 <op> <ok> 0`: ack for `0x2E` / `0x2F` / `0x30` / `0x31` (`ok` 1 = done; for `0x30` / `0x31` 0 also means no
    controller was linked and ready, so nothing played)
  - `0xB3 3 <op> <ok> <profile>`: ack for `0x28` / `0x29` / `0x2A` / `0x2B` (`ok` 1 = done); for `0x2B` the last byte
    is the host byte of §10.3 instead of the profile

Lizard binding wire format (16 bytes), matching `LizardBinding` in `lizard_map.h`:

```text
[0]     outType   (0 none, 1 keyboard chord, 2 mouse btn, 3 mouse axis, 4 scroll, 5 consumer)
[1..7]  outData[0..6]  (type-specific payload)
[8..11] trigMask  u32 LE  (button bits: any-of)
[12..15] holdMask u32 LE  (button bits: all-of guard)
```

> Lizard-map commands and the `0xAA` reply were renumbered from `0x0D–0x11`/`0xA7` on the merges with
> `main`, which had independently claimed `0x0D–0x10`, `0xA7` (bond export) and `0xA8` (flight
> recorder).

Status blob payload:

```text
[0]  version
[1]  usb mode
[2]  legacy xbox mouse sensitivity divisor (no mouse in MODE_XBOX)
[3]  legacy xbox mouse friction (no mouse in MODE_XBOX)
[4]  steam pad smoothing
[5]  A/B + X/Y swap
[6]  back mapping 0
[7]  back mapping 1
[8]  back mapping 2
[9]  back mapping 3
[10] connected slot or 0xFF
[11] link up flag
[12] delivered reports/s low
[13] delivered reports/s high
[14] poll interval / 100
[15] new reports/s low
[16] new reports/s high
[17] E7 mode byte
[18] relay opcode
[19] relay subtype
[20] forward-new-only flag
[21] QoS auto-hop flag
[22] persist-mode flag
```

Later fields are appended (the version byte says how far the payload goes). From version 20 the tail carries
the per-emulated-type trackpad-to-stick mapping at payload bytes 187..194 — two bytes per type
(`{left pad, right pad}`), each `0` off / `1` left stick / `2` right stick. Set with
`0x02 <80 + type*2 + pad> <value>`. Fields 40..75 are the per-type config block, so the mapping starts at 80.
A mapped pad **blends** with its stick: while the pad is touched each axis reports whichever of the two
sources is deflected further from center (signed); an untouched pad contributes nothing and the physical
stick passes straight through. A mapped pad also stops reporting as a touchpad contact / mouse.


### 10.1 Backup / clone (bond export & import)

A puck's whole portable identity is its four 24-byte bond records (`[proteus_uuid 4][ibex_uuid 4][serial 16]`,
§2). The per-bond on-air *session address* is **not** stored or transferred: each puck re-derives it from the
bond UUID mixed with its own FICR `DEVICEID`, and the bonded controller relearns it from the `E1` host beacon on
every reconnect. So copying the bond records onto a second puck is sufficient for any controller paired to the
first to connect to the second **with no re-pairing** — the two pucks just derive different (non-colliding)
session addresses for the same controller.

This makes a puck cloneable. The WebUSB panel exposes it as **Export to file** / **Import from file**.

**Export — `0x09` → `0xA7` frame:**

```text
[0]    format version (1)
[1]    used mask (bit s = slot s bonded)
[2..]  slot 0..3 records, 24 bytes each (96 bytes)        (len = 2 + 4*24 = 98)
```

**Import — `0x0D` (per slot) then `0x0E` (commit):**

```text
0x0D  <slot 0..3>  <used 0|1>  <24-byte record>      writes one slot into RAM (used=0 clears it)
0x0E  <mode>                                         persist all slots, apply mode (0xFF=unchanged), reboot
```

Bonds are written one slot at a time so no command exceeds a single USB-FS packet, and each `0x0D`/`0x02` is
acked with a status blob so the host's read-after-write keeps the OUT pipe flowing. The panel sends all four
`0x0D` slot writes plus every config field via `0x02`, then a single `0x0E` to persist and reboot — so the
reboot regenerates the session addresses from the new UUIDs and the restored puck reproduces both the pairings
and all settings. Other settings (mouse, chords, per-type button maps, Switch Pro gyro mapping) ride in the
backup file as plain `0x02` field values, not in the bond commands.


### 10.2 Back-paddle binds (blob version 2, status blob 24)

Status blob byte `p[197]` = active profile (0..2). Old fields `5..8` and `40+et*9+k` (k < 4) now set a plain
button action in the active profile; `g_type[].back` / blob `p[8..11]` always show the button code of the active
profile (0 for a combo or macro paddle).

Binds blob (`binds.h`, 1079 bytes, read/written in 96-byte chunks, 12 chunks): `[0xB1][version][payload length u16 LE]`,
payload, `u16 LE` sum of the payload bytes. Payload: `active`, then actions `[profile 3][type 4: xbox, switch, ds4, ds5][paddle 4: L4 R4 L5 R5]`
each `{kind, arg, mask u32 LE}`, then 8 macros `{flags (0), count 0..16, 16 x {mask u32 LE, ms u16 LE}}`.
Macros are shared by all profiles; a macro fires on the paddle press and plays once to the end. A bad blob is
rejected whole (`0x29` acks `ok = 0`). Stored in `binds.bin`; `cfg.bin` is unchanged.

Action kinds:

| kind | name | `arg` | `mask` |
|---|---|---|---|
| 0 | button | button code 0..20 | 0 |
| 1 | combo | 0 | `TB_*` bits pressed together |
| 2 | macro | macro 0..7 | 0 |
| 3 | key (version 2) | HID keyboard usage `0x04..0xE7` | modifier byte, bits 8..31 must be 0 |
| 4 | mouse (version 2) | mouse code 1..9 (below) | 0 |

Modifier byte of a key action = the HID keyboard modifier byte: `0x01` left Ctrl, `0x02` left Shift, `0x04` left Alt,
`0x08` left Win, `0x10` right Ctrl, `0x20` right Shift, `0x40` right Alt, `0x80` right Win. The key (and its
modifiers) is held while the paddle is held. `arg` is a usage from the HID "Keyboard/Keypad" page (`0x04` = A,
`0x1E` = 1, `0x28` = Enter, `0x2C` = Space, `0x3A` = F1, `0x4F` = Right arrow, `0xE1` = Left Shift as a key, ...);
`0x00..0x03` (rollover/error codes) are rejected.

Mouse codes of a mouse action:

| code | action |
|---|---|
| 1 | left button (held with the paddle) |
| 2 | right button |
| 3 | middle button |
| 4 | back button (button 4) |
| 5 | forward button (button 5) |
| 6 | wheel up, one notch per press |
| 7 | wheel down, one notch per press |
| 8 | wheel up, one notch per press, repeated every 100 ms while held |
| 9 | wheel down, repeated every 100 ms while held |

Version 2 has exactly the layout of version 1 (same length, same checksum rule); it only allows kinds 3 and 4. A
version 1 blob is still accepted (and rejected whole if it contains kind 3 or 4); the firmware always writes and
sends version 2. A page that edits the blob must keep the version byte it read, or write 2 when it uses kinds 3/4.
Key and mouse actions only act on a PC host in the modes listed in §10.3; in every other mode the paddle simply
does nothing. Macro steps stay gamepad-only (no key steps).

Live state `0xB0` payload (25 bytes since status blob version 24; a parser that reads the length byte ignores
what it does not know): `[0..3]` raw buttons u32 LE (before binds), `[4..7]` buttons after binds
(what the emulation modes see), `[8]` LT, `[9]` RT (0..255), `[10..17]` left X, left Y, right X, right Y (s16 LE),
`[18]` active profile, `[19]` slot, `[20]` emulated type (0xFF = Steam/Lizard), `[21]` link up (1/0),
`[22]` battery % (0 = no reading), `[23]` charge state (`EChargeState`: 2 charging, 4 charge complete, anything else discharging),
`[24]` host byte (§10.3).

Profile switch on the pad: all four paddles + RB = next, + LB = previous (cyclic, emulated modes only), N short buzzes = profile N.


### 10.3 Keyboard and mouse on a PC host

Goal: on a PC the puck shows the gamepad **and** a keyboard **and** a mouse as separate devices (Windows Device
Manager: the pad, "HID Keyboard Device", "HID-compliant mouse"), and a back paddle can be a key or a mouse
action (§10.2). On a console nothing changes: it must see the descriptor it always saw.

Modes with keyboard / mouse: `MODE_PS5` (DualSense with panel) and `MODE_HIDGYRO` (DS4 + gyro). Everything else
is untouched: the clean PlayStation modes, PS3, PS5 console via auth adapter, Switch, Xbox 360, Original Xbox
(console personalities, or deliberately single-interface), DInput / SInput (no paddle actions at all) and
Steam / Lizard (Steam does its own keyboard and mouse).

**Xbox 360 mode (`MODE_XBOX`) was examined for a PC keyboard and deliberately left out.** Findings:

- The personality is static and exact: `xi_open` accepts only the four XInput / XSM3 interfaces byte for byte, the
  descriptor builder reserves IN/OUT endpoints 1..6, and `usbIdentity()` sets the *device* class to `0xFF/0xFF/0xFF`
  like a real pad. Only IN endpoint 7 (`0x87`) is left, so at most **one** extra HID interface fits: keyboard and
  mouse would have to share it (two report IDs, which Windows still shows as two devices).
- With device class `0xFF` Windows binds the XInput driver to the whole device and does not load the composite
  parent driver (`usbccgp`), so an added HID interface would most likely never be enumerated. Making it appear
  means changing the device class (0 or EF/02/01 with IAD), i.e. a different descriptor than a real Xbox 360 pad,
  which the console's XSM3 check and the PC XInput driver binding both depend on. This cannot be verified without
  a Windows PC and an Xbox 360 console, and the requirement is that the console is not affected.
- The mode has no WebUSB / BOS, so Windows' MS OS 2.0 request never arrives; `GET_DESCRIPTOR(string 0xEE)` does
  arrive once per machine and is then cached by Windows, so it cannot drive auto detection. Console side the
  only signal would be the XSM3 requests (`0x81 0x82 0x83 0x84 0x86 0x87`). Linux (`xpad`) and macOS give nothing.
- The Xbox mode has no config channel (no WebUSB, no CDC), so the manual setting could only be made in another
  mode (`HOST P` in the debug console) before switching.

Alternative for push-to-talk and similar on a PC: use `MODE_PS5` or `MODE_HIDGYRO` (keyboard / mouse work there),
or a Steam / Lizard binding. If Xbox 360 + keyboard is still wanted, the experiment would be an opt-in, manual-only
setting (never auto, never remembered) that switches the device class to composite and adds one combined HID
interface on endpoint `0x87`, tested first on a PC with the real XInput driver and only then offered.

- **Mouse** = the existing wake-mouse interface (boot mouse, 5 buttons + wheel), present in both modes already, so
  the descriptor does not change. Mouse actions are sent on it only while the host is a PC.
- **Keyboard** = one more HID interface (non-boot keyboard descriptor, 6 keys + modifiers), placed after the pad
  interfaces and before WebUSB. HID instance numbers follow descriptor order, so the keyboard takes the pool object
  right after the mounted pads and that object gets its pad role back on the next re-enumeration. It is present
  only while the host is a PC and fewer than 4 pads are mounted. The USB serial carries a `k` suffix then, so hosts
  do not reuse a cached configuration. With 4 pads mounted there is no keyboard: the pool has
  4 objects, so `kbdCarrier(k)` is null for `k` = 4 and no free object is left to carry it.
- The puck starts **without** the keyboard unless the host was a PC last time. When the host turns out to be a PC
  (or the setting forces it), the device re-enumerates once (detach / attach, no reboot, RF links stay up) after the
  result has held for 1.5 s; when it turns out to be a console it re-enumerates back without it.

Detection (`host_detect.cpp`; nothing here is certain, so every result carries its reason):

| signal | result | reason code |
|---|---|---|
| host fetches the MS OS 2.0 descriptor (vendor request 2, `wIndex` 7) | PC (Windows) | 1 |
| host fetches the WebUSB landing page (vendor request 1) or connects the panel (class request `0x22`, `wValue` ≠ 0) | PC (a browser, any OS) | 2 |
| pad identity feature reports read (DualSense `0x05 0x09 0x20`, DS4 `0x02 0x12 0x81 0xA3`) and 5 s later no auth report | PC (Linux / macOS / SDL), weak | 3 |
| feature report `0xF0..0xF3` GET or SET on the pad interface (Sony auth) | console (PS4 / PS5) | 4 |
| setting forced by hand | as set | 5 |
| nothing new yet, result of the previous session (stored in `host.bin`) | as stored | 6 |
| nothing seen, nothing stored | unknown (= no keyboard, no mouse actions) | 0 |

A strong signal (1, 2, 4) beats a weak one; between two strong ones the latest wins. The Switch modes
raise the Windows / browser signals too (shown, not acted on); a Switch console gives no signal and stays
"unknown". The Xbox modes report nothing at all. Where it can be wrong: a PC that never asks for the MS OS descriptor again (Windows caches it) and
never probes the pad shows "unknown" until a probe or the panel opens (the remembered result covers the next
boots); a console that does not run the Sony auth in these modes looks like "PC" after 5 s of probes. The manual
setting fixes both.

Manual setting (`0` auto, `1` always PC, `2` always console), saved with the last auto result in `host.bin`
(`[0xB7][setting][detected][setting ^ detected ^ 0xA5]`, 4 bytes; a bad file = auto). Set it with WebUSB `0x2B`
or the serial console `HOST A|P|C` (`HOST` alone prints the state; the console exists on the debug-CDC boot, then
restart into the mode: the setting is saved).

Host byte (live state `0xB0` payload `[24]`, and the last byte of the `0xB3` ack for `0x2B`):

```text
bits 0-1  detected / effective host: 0 unknown, 1 PC, 2 console
bits 2-3  manual setting: 0 auto, 1 PC, 2 console
bits 4-7  reason code (table above)
```

Serial console (CDC): `BA <p> <et> <i> K<usage hex>[:<mods hex>]` (e.g. `BA 1 3 0 K06:03` = Ctrl+Shift+C),
`BA <p> <et> <i> Mo<name>` with `L R M` buttons, `B` back, `F` forward, `U` / `D` wheel per press, `U+` / `D+`
wheel repeating while held; `BL` lists them as `K06:03` / `MoU+`; `HOST [A|P|C]`.

### 10.4 Diagnostics frame `0xB4`

Request `0x2C` (no argument). Reply: `0xB4`, length byte `119`, then 119 payload bytes. All counters are `u32` little
endian, counted since boot. Use it to find where a back-paddle action is lost: raw buttons seen, binds output,
`allowed`, queued reports, USB endpoint results.

| offset | size | field |
|---|---|---|
| 0 | 1 | layout version (1) |
| 1 | 1 | `g_usbMode` |
| 2 | 1 | `g_etype` (0xFF none) |
| 3 | 1 | active profile |
| 4 | 1 | host kind (0 unknown, 1 PC, 2 console) |
| 5 | 1 | host reason (§10.3) |
| 6 | 1 | host setting |
| 7 | 1 | flags: bit0 keyboard in descriptor, bit1 keyboard add failed, bit2 wake mouse present, bit3 wake mouse ready, bit4 USB mounted, bit5 suspended |
| 8 | 1 | mounted pads (`g_usbMountCount`) |
| 9 | 1 | pad pool size (`maxSlots`) |
| 10 | 4 | `bindsFwFilter` calls |
| 14 | 4 | last raw buttons (any slot) |
| 18 | 4 | last buttons after binds |
| 22 | 4 | calls with a non-empty key / mouse state |
| 26 | 1 | last non-empty state: keyboard modifiers |
| 27 | 6 | its key usages |
| 33 | 1 | its mouse buttons |
| 34 | 1 | its wheel (s8) |
| 35 | 4 | `sendState` passes with `allowed` true |
| 39 | 4 | passes with `allowed` false |
| 43 | 4 | mouse reports accepted (`wakeHidMouse` true) |
| 47 | 4 | mouse reports refused (not ready) |
| 51 | 4 | keyboard reports queued |
| 55 | 1 | last mouse buttons sent |
| 56 | 1 | last keyboard modifiers sent |
| 57 | 6 | last keyboard usages sent |
| 63 | 20 | wake mouse USB channel: `enqueued, sent, sendFail, dropped, notReady` (5 x u32) |
| 83 | 20 | keyboard channel, same fields, counted since the keyboard took the interface |
| 103 | 4 | every raw button bit ever seen (OR) |
| 107 | 4 | every button bit ever seen after binds (OR) |
| 111 | 4 | re-enumerations requested by the keyboard logic |
| 115 | 4 | `host.bin` flash writes |

`dropped` = oldest report discarded because the ring was full, `notReady` = drain passes that found a report waiting
and the endpoint not ready, `sendFail` = `sendReport` refused on a ready endpoint. Button bit numbers are the `TB_*`
bits of §8.1 (paddles: R4 `0x80`, R5 `0x100`, L4 `0x20000`, L5 `0x40000`).

The `0xB0` live state with slot `0xFF` now picks a linked controller that has input in hand (else the first linked
one) instead of the slot the poll happened to be on.

### 10.5 Melodies (sounds blob version 1, status blob 25)

The puck can play short melodies through the controller's actuator: each note is a `HAPTIC_LFO_TONE` OUTPUT report
`0x83` (§ "Two different `0x8x` id spaces") relayed like rumble, once per side (0 and 1), with `gain_db` = the volume
setting and no LFO (the report layout comes from SDL; its audibility and the meaning of `side` were not checked on hardware yet). The sequencer runs in `loop()` without blocking; every rest and the end of a melody send the zero
`0x80` rumble report, so the actuator cannot stay latched if a tone ignores its duration.

Sounds blob (`sounds.h`, 1048 bytes, read/written in 96-byte chunks, 11 chunks): `[0xB8][version 1][payload length u16 LE]`,
payload, `u16 LE` sum of the payload bytes. Payload (1042 bytes):

| offset | size | field |
|---|---|---|
| 0 | 1 | confirmations: `0` off, `1` vibration, `2` sound, `3` both |
| 1 | 1 | volume = `gain_db`, signed, `-60..0` (`0` loudest), default `-6` |
| 2 | 8 | melody index (0..7) per event, `0xFF` = no melody |
| 10 | 8 x 129 | melodies: `{count 0..32, 32 x {freq u16 LE, ms u16 LE}}`; notes past `count` are ignored |

Events (index into the event table): `0` profile 1, `1` profile 2, `2` profile 3, `3` mode change to a PlayStation mode
(PS5, PS5 game, PS5 auth, HID gyro/DS4, DS4 game, PS3), `4` mode change to Xbox (Xbox, Xbox OG), `5` mode change to Switch
(HORI, Pro), `6` mode change to any other mode (Steam, Lizard, DirectInput, SInput), `7` a controller connected.
Notes: `freq` 0 = rest, otherwise `1..4000` Hz; `ms` is `30..5000`. A bad blob is rejected whole (`0x2F` acks `ok = 0`) and a
bad or missing `sounds.bin` means the defaults (own short motifs, melody `i` for event `i`, confirmations = vibration).

The 30 ms minimum protects the relay queue: a note is two `0x83` entries per controller and the relay sends about one
entry per poll cycle (~4 ms), so shorter notes would flood the 32-entry queue and push out other frames. The sequencer
also holds the next note while a linked slot has more than 4 entries queued (the note is late, not dropped; a queue
that does not drain for 1 s ends the melody). With confirmation "both", the melody of a profile switch starts after the
last vibration pulse. Melodies do not follow the per-type rumble setting: sound is its own setting (confirmations).

The confirmations setting decides how a profile switch is confirmed (vibration = N pulses as before, sound = the
profile's melody). The melodies of the other events play only when it includes sound. A mode change plays the melody
of the new mode's family on the first connection after the reboot (a flag file lives across the reboot); a connection
plays the connect melody once per 20 s per controller. A melody starts only for controllers that are linked. The
preview commands `0x30` / `0x31` play whatever the setting is.

`0x31` payload is the melody as in the blob without padding: `[count][count x {freq u16 LE, ms u16 LE}]`, `count` 0..32.

Console: `SNL` list, `SNP <n>` play melody n, `SNE <e>` play the melody of event e, `SNM <0..3>` confirmations,
`SNG <-60..0>` volume.

### 10.6 Back4 chords set to None (status blob 26)

The back4 + B/X/Y chords (fields `17..19`, blob `p[25..27]`) and back4 + D-pad chords (fields `34..37`, blob `p[182..185]`) take a mode number or `0xFE` = None: the combination does nothing. From status blob version 26; older firmware rejects `0xFE`. back4 + A (always Steam) and back4 + LB/RB (profiles) are not configurable.

## 11. Timing notes

- Default poll interval: `4000` microseconds (250 Hz, matches SC2 report rate)
- Default RX window: `1200` microseconds
- Discovery beacon continues on channel 2 even after a connected session exists
- Connected session can be moved to a cleaner channel such as 18 or 52

## 12. Reimplementation checklist

To build a compatible puck from scratch:

1. Expose four independent bond slots over USB.
2. Implement `0x83`, `0xAE`, `0xB4`, `0xAD`, `0xA2`, `0xA3`.
3. Persist 24-byte bond records exactly.
4. Configure the radio exactly as described above.
5. Beacon `E1` host frames on `"ibex"` / prefix `0x10` / channel 2.
6. Poll with `E7` then `E3 GET 0x45`.
7. Parse `F1` and unpack report `0x45`.
8. Relay host feature writes to the controller as `E3` SET sub-TLVs.
9. Re-enumerate USB cleanly when switching modes; Xbox uses the fixed four-vendor-interface layout in §9.2, not a gamepad/mouse composite or a dynamic multi-pad layout.

If those pieces match, the controller-to-puck protocol is fully reimplemented.
