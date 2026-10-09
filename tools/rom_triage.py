"""First-pass triage of an N64 ROM for recomp feasibility.

Prints the header, hashes, the microcode ID strings RT64 and RSPRecomp care about, and counts
of common compression magics. Reads the ROM only; writes nothing.

    py -3 -I tools/rom_triage.py <rom.z64> [more roms...]
"""
import hashlib
import re
import sys
from collections import Counter


def to_z64(data: bytes) -> bytes:
    magic = data[:4]
    if magic == b"\x80\x37\x12\x40":
        return data
    if magic == b"\x37\x80\x40\x12":  # .v64: byte-swapped 16-bit
        b = bytearray(data)
        b[0::2], b[1::2] = data[1::2], data[0::2]
        return bytes(b)
    if magic == b"\x40\x12\x37\x80":  # .n64: little-endian 32-bit
        b = bytearray(len(data))
        for i in range(4):
            b[i::4] = data[3 - i::4]
        return bytes(b)
    raise SystemExit(f"unknown ROM byte order: {magic.hex()}")


def triage(path: str) -> None:
    raw = open(path, "rb").read()
    rom = to_z64(raw)
    h = rom[:0x40]
    print(f"== {path}")
    print(f"size        {len(rom)} bytes ({len(rom) / (1 << 20):.0f} MiB)")
    print(f"sha1        {hashlib.sha1(raw).hexdigest()}  (file as stored)")
    print(f"sha1 z64    {hashlib.sha1(rom).hexdigest()}")
    print(f"name        {h[0x20:0x34].decode('ascii', 'replace').strip()}")
    print(f"game id     {h[0x3B:0x3F].decode('ascii', 'replace')}  rev {h[0x3F]}")
    print(f"crc1/crc2   {h[0x10:0x14].hex()} {h[0x14:0x18].hex()}")
    print(f"entrypoint  0x{int.from_bytes(h[8:12], 'big'):08X}")
    print(f"libultra    {h[0x0C:0x10].hex()}  (release field)")

    # Microcode ID strings: graphics ucodes carry "RSP Gfx ucode ..." / "RSP SW Version ..."
    ucode = Counter()
    for m in re.finditer(rb"RSP (?:Gfx|SW|Audio)[ -~]{4,80}", rom):
        ucode[m.group().decode("ascii")] += 1
    print("ucode strings:")
    for s, n in ucode.most_common() or [("(none found; possibly compressed or custom)", 0)]:
        print(f"  {n:3}x  {s}")

    # Compression magics (counts are hints only: 4-byte magics also occur by chance)
    magics = {"Yaz0": b"Yaz0", "Yay0": b"Yay0", "MIO0": b"MIO0", "gzip": b"\x1f\x8b\x08",
              "zlib(78 9c)": b"\x78\x9c", "zlib(78 da)": b"\x78\xda"}
    print("compression magics:")
    for name, mg in magics.items():
        print(f"  {name:12} {rom.count(mg)}")

    # Any other identifying text (copyright, toolchain, middleware)
    hits = Counter()
    for m in re.finditer(rb"[ -~]{6,100}", rom):
        s = m.group().decode("ascii")
        if re.search(r"(?i)copyright|\(c\)|libultra|MusyX|Factor ?5|Rare|Midway|Atari|ucode|"
                     r"F3D|S2D|L3D|Turbo3D|ZSort|n_aspMain|aspMain", s):
            hits[s.strip()] += 1
    print("notable strings:")
    for s, n in hits.most_common(25):
        print(f"  {n:3}x  {s}")
    print()


if __name__ == "__main__":
    for p in sys.argv[1:]:
        triage(p)
