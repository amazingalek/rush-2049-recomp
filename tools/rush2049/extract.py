"""Prepare a Rush 2049 (USA) ROM for N64Recomp.

Rush 2049 keeps everything but its boot segment as raw deflate streams that the game inflates
itself at run time (func_80006814(rom, dest, flag)). N64Recomp reads instructions straight from
the ROM, so this writes a copy with each compressed code segment appended uncompressed at a
fake ROM address (SEGMENTS below). N64Recomp reads code from that copy; the runtime loads the
original ROM.

Memory map:
  segment    ROM (original)       recomp ROM   VRAM                     notes
  boot       0x1000 (plain)       0x1000       0x80000400-0x80027DD0    libultra 2.0J, MusyX CPU side
  ucode      in boot              -            0x80027DD0-0x8002C5E0    rspboot, F3DEX2, MusyX text: not CPU code
  boot_tail  in boot              0x2D1E0      0x8002C5E0 (0x1C)        a `cache 0x0D` loop, stubbed
  main       0xB0CB10 (deflate)   0x01000000   0x80086A50-0x8010FD80    data to 0x801249F0, bss to 0x8017A640
  ovl1       0xB5C534 (deflate)   0x010A0000   0x8038A400-0x803AF980
  ovl2       0xB6FEC4 (deflate)   0x010D0000   0x8038A400-0x80393B40    shares VRAM with ovl1
- The entry clears bss at 0x8002E8E0 (size 0x58170), sets sp = 0x80086A50 and jumps to boot_main
  (0x800020F0). The main thread inflates main at 0x80002334; func_800026C0 inflates the rest.
- The overlays' ROM addresses are a table at 0x8002B020; func_800A11E4 loads them, both to 0x8038A400.
- libultra links the RSP microcode text into .text after the CPU code. Decoded as MIPS it looks like
  code (jal 0x84000xxx, dense COP2), and a jal 0x80001088 in it once split func_80000FE8 in two, so
  the boot segment's text ends at 0x80027DD0, the first IMEM-range j/jal (rspboot).

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
