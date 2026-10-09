"""Prepare a Rush 2049 (USA) ROM for N64Recomp.

Rush 2049 keeps everything but its boot segment as raw deflate streams that the game inflates
itself at run time (func_80006814(rom, dest, flag)). N64Recomp reads instructions straight from
the ROM, so this writes a copy with each compressed code segment appended uncompressed at a
fake ROM address (SEGMENTS below). See docs/games/rush2049.md#memory-map.

Usage: py -3 -I tools/rush2049/extract.py <rom (.z64/.n64/.v64)> [out_dir]   (default out_dir: roms/)
Writes <out_dir>/rush2049.us.z64 (big-endian copy) and <out_dir>/rush2049.us.recomp.z64.
"""
import hashlib
import sys
import zlib
from pathlib import Path

EXPECTED_SHA1 = "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"  # big-endian (.z64) image

# name, real ROM offset of the raw deflate stream, fake ROM address, vram, inflated size
SEGMENTS = [
    ("main", 0xB0CB10, 0x01000000, 0x80086A50, 0x9DFA0),
    ("ovl1", 0xB5C534, 0x010A0000, 0x8038A400, 0x2F650),
    ("ovl2", 0xB6FEC4, 0x010D0000, 0x8038A400, 0xAB70),
]
RECOMP_ROM_SIZE = 0x010E0000


def to_big_endian(data: bytes) -> bytes:
    magic = data[:4]
    if magic == b"\x80\x37\x12\x40":
        return data
    if magic == b"\x37\x80\x40\x12":  # .v64: byte-swapped 16-bit words
        out = bytearray(data)
        out[0::2], out[1::2] = data[1::2], data[0::2]
        return bytes(out)
    if magic == b"\x40\x12\x37\x80":  # .n64: little-endian 32-bit words
        out = bytearray(len(data))
        for i in range(4):
            out[i::4] = data[3 - i::4]
        return bytes(out)
    raise SystemExit(f"Unrecognized ROM byte order: {magic.hex()}")


def inflate_at(rom: bytes, offset: int) -> bytes:
    z = zlib.decompressobj(-15)
    out = z.decompress(rom[offset:])
    if not z.eof:
        raise SystemExit(f"Deflate stream at 0x{offset:X} did not terminate")
    return out


def main() -> None:
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    out_dir = Path(sys.argv[2] if len(sys.argv) == 3 else Path(__file__).resolve().parents[2] / "roms")
    rom = to_big_endian(Path(sys.argv[1]).read_bytes())
    sha1 = hashlib.sha1(rom).hexdigest()
    if sha1 != EXPECTED_SHA1:
        raise SystemExit(f"Unexpected ROM (sha1 {sha1}); expected Rush 2049 USA {EXPECTED_SHA1}")
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "rush2049.us.z64").write_bytes(rom)

    recomp = bytearray(rom) + bytes(RECOMP_ROM_SIZE - len(rom))
    for name, src, dst, vram, size in SEGMENTS:
        seg = inflate_at(rom, src)
        if len(seg) != size:
            raise SystemExit(f"{name}: inflated to 0x{len(seg):X} bytes, expected 0x{size:X}")
        recomp[dst:dst + size] = seg
        print(f"{name}: rom 0x{src:06X} -> 0x{size:05X} bytes at fake rom 0x{dst:08X} (vram 0x{vram:08X})")
    (out_dir / "rush2049.us.recomp.z64").write_bytes(recomp)
    print(f"recomp rom sha1 {hashlib.sha1(recomp).hexdigest()}")


if __name__ == "__main__":
    main()
