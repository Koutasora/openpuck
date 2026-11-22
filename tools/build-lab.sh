#!/bin/sh
# Windows (Git Bash) build without make/python3: same flags as `make build`, then .hex -> .uf2.
# Needs arduino-cli on PATH (or in %LOCALAPPDATA%/Programs/arduino-cli) with adafruit:nrf52 installed.
set -e
cd "$(dirname "$0")/.."
PATH="$LOCALAPPDATA/Programs/arduino-cli:$PATH"
OUT=build/openpuck
sh gen_version.sh
arduino-cli compile -b adafruit:nrf52:feather52840 --clean \
	--build-path build/cache/openpuck --output-dir "$OUT" \
	--build-property "build.extra_flags=-DNRF52840_XXAA {build.flags.usb} -DCFG_TUD_HID=6 -DCFG_TUD_TASK_QUEUE_SZ=512 -DCFG_TUD_VENDOR_TX_BUFSIZE=256 -DOPK_RF_JOURNAL_BASE=0x86000 $EXTRA_FLAGS" \
	--build-property "compiler.c.elf.extra_flags=-Wl,--wrap=tud_vendor_control_xfer_cb -Wl,--wrap=tud_descriptor_string_cb -Wl,--wrap=usbd_control_xfer_cb -Wl,--section-start=.rf_journal_guard=0x86000" \
	OpenPuck
DATA_DIR="$(arduino-cli config get directories.data)"
UF2CONV="$(find "$DATA_DIR/packages/adafruit/hardware/nrf52" -name uf2conv.py | sort | tail -n1)"
python "$UF2CONV" -c -f 0xADA52840 -o "$OUT/OpenPuck.uf2" "$OUT/OpenPuck.ino.hex"
