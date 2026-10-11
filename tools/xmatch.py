"""Match Rush 2049 functions against another game's named functions by address-masked code.

Usage: .venv/Scripts/python -I tools/xmatch.py <other.z64> <other.syms.toml> [section]

Reads the other game's symbol file (N64Recomp format, sections read straight from its ROM, so only
uncompressed sections such as boot work) and prints every unnamed (func_*) 2049 boot function whose code
equals a named function there once jal targets and address immediates are masked. Facts only: this
compares code, it copies nothing from the other project.
"""
import struct
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYMS = ROOT / "syms" / "rush2049.us.syms.toml"
ROM = ROOT / "roms" / "rush2049.us.recomp.z64"

ADDR_IMM_OPS = {0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x31, 0x35, 0x37, 0x39, 0x3D, 0x3F}


def masked_code(rom, sec_rom, sec_vram, vram, size):
    out = []
    for a in range(vram, vram + size, 4):
        w = struct.unpack_from(">I", rom, sec_rom + a - sec_vram)[0]
        op = w >> 26
        if op in (2, 3):
            w &= 0xFC000000
        elif op == 0x0F:
            w &= 0xFFFF0000
        elif op in ADDR_IMM_OPS and ((w >> 21) & 31) not in (0, 29):
            w &= 0xFFFF0000
        out.append(w)
    return tuple(out)


def functions(syms_path, rom, section):
    for sec in tomllib.loads(Path(syms_path).read_text())["section"]:
        if sec["name"] != section:
            continue
        for f in sec["functions"]:
            yield f["name"], f["vram"], masked_code(rom, sec["rom"], sec["vram"], f["vram"], f["size"])


def main():
    other_rom = Path(sys.argv[1]).read_bytes()
    section = sys.argv[3] if len(sys.argv) > 3 else "boot"
    known = {}
    for name, vram, code in functions(sys.argv[2], other_rom, section):
        if not name.startswith("func_") and len(code) >= 3:
            known.setdefault(code, set()).add(name)
    rom = ROM.read_bytes()
    for name, vram, code in functions(SYMS, rom, "boot"):
        if name.startswith("func_") and code in known:
            print(f"{vram:08X}  {name}  ->  {', '.join(sorted(known[code]))}")


if __name__ == "__main__":
    main()
