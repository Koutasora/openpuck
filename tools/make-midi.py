"""Original short motifs for the puck sounds (SMF format 0, 1 tick = 1 ms).

Usage: python tools/make-midi.py docs/midi
Import the files in the configurator: Mapping > Sounds > Import MIDI file.
Limits of the puck: 32 positions, 30-5000 ms each, notes above ~130 Hz are audible.
"""
import os
import struct
import sys

NAMES = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6,
         "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def midi_num(n):
    name, octave = n[:-1], int(n[-1])
    return 12 * (octave + 1) + NAMES[name]


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def vlq(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.insert(0, 0x80 | (v & 0x7F))
        v >>= 7
    return bytes(out)


def build(title, seq):
    ev = bytearray()
    t = title.encode("ascii")
    ev += vlq(0) + b"\xff\x03" + vlq(len(t)) + t
    # 500000 us per quarter with 500 ticks per quarter: one tick is one millisecond
    ev += vlq(0) + b"\xff\x51\x03" + (500000).to_bytes(3, "big")
    ev += vlq(0) + b"\xc0\x50"  # square lead, only for previews in other players
    wait = 0
    for note, ms in seq:
        if note is None:
            wait += ms
            continue
        m = midi_num(note)
        ev += vlq(wait) + bytes([0x90, m, 100])
        ev += vlq(ms) + bytes([0x80, m, 0])
        wait = 0
    ev += vlq(wait) + b"\xff\x2f\x00"
    hdr = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 500)
    return hdr + b"MTrk" + struct.pack(">I", len(ev)) + bytes(ev)


R = None
MOTIFS = {
    # airy, slow, wide intervals with a lydian lift resolving onto a held third
    "puck-playstation": ("Puck PlayStation", [
        ("G4", 180), ("D5", 180), ("A5", 180), ("C#6", 260),
        ("B5", 700), (R, 80), ("D6", 900)]),
    # driving low pulse, quick run upward, octave landing
    "puck-xbox": ("Puck Xbox", [
        ("E4", 70), (R, 40), ("E4", 70), (R, 40), ("B4", 80), (R, 40),
        ("E5", 90), ("G5", 90), ("A5", 90), ("B5", 300), (R, 60),
        ("E6", 600)]),
    # bright, bouncy staccato with a small grace step before the top
    "puck-switch": ("Puck Switch", [
        ("C6", 60), (R, 50), ("G5", 60), (R, 50), ("C6", 60), (R, 50),
        ("D6", 40), ("E6", 60), (R, 80), ("G6", 260)]),
    # neutral two-step rise for the remaining PC modes (Steam, Lizard, DirectInput, SInput)
    "puck-other": ("Puck other modes", [
        ("A4", 120), (R, 40), ("E5", 120), (R, 40), ("A5", 400)]),
    # short "hello" when a controller links: falling then rising third
    "puck-connect": ("Puck controller connected", [
        ("E5", 90), ("C5", 90), (R, 50), ("G5", 300)]),
}

if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for fname, (title, seq) in MOTIFS.items():
        data = build(title, seq)
        with open(os.path.join(out, fname + ".mid"), "wb") as f:
            f.write(data)
        notes = [n for n, _ in seq if n]
        lo = min(hz(midi_num(n)) for n in notes)
        hi = max(hz(midi_num(n)) for n in notes)
        total = sum(ms for _, ms in seq)
        short = min(ms for _, ms in seq)
        print(f"{fname}.mid: {len(seq)} positions, {total} ms, "
              f"{lo:.0f}-{hi:.0f} Hz, shortest {short} ms, {len(data)} B")
