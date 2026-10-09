"""Disassemble a vram range of Rush 2049 from the recomp ROM (after extract.py).

Usage: .venv/Scripts/python -I tools/rush2049/dis.py <start> <end> [section]
section picks between the two overlays at 0x8038A400 (ovl1 or ovl2; default ovl1).
"""
import struct
import sys
from pathlib import Path

import rabbitizer

ROOT = Path(__file__).resolve().parents[2]
SEGMENTS = {"boot": (0x80000400, 0x00001000, 0x2E4E0), "main": (0x80086A50, 0x01000000, 0x9DFA0),
            "ovl1": (0x8038A400, 0x010A0000, 0x2F650), "ovl2": (0x8038A400, 0x010D0000, 0xAB70)}


def main():
    start, end = int(sys.argv[1], 16), int(sys.argv[2], 16)
    ovl = sys.argv[3] if len(sys.argv) > 3 else "ovl1"
    rom = (ROOT / "roms" / "rush2049.us.recomp.z64").read_bytes()
    for name in ("boot", "main", ovl):
        vram, rom_off, size = SEGMENTS[name]
        if vram <= start < vram + size:
            break
    else:
        raise SystemExit("address not in any segment")
    for a in range(start, end, 4):
        w = struct.unpack_from(">I", rom, rom_off + a - vram)[0]
        print(f"{a:08X}: {w:08X}  {rabbitizer.Instruction(w, vram=a).disassemble()}")


if __name__ == "__main__":
    main()
