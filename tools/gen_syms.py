"""Generate the N64Recomp symbol file for Rush 2049 (USA): syms/rush2049.us.syms.toml.

Function boundaries come from spimdisasm. libultra/libc functions in the boot segment are named
by matching n64sym's signature database (ref/n64sym, MIT): a function matches a signature when
the CRC32 of its first 8 bytes and of its whole body agree once relocated fields are masked out.
Names from syms/names.toml (our own registry of worked-out functions) are applied on top.

Usage: .venv/Scripts/python -I tools/gen_syms.py   (after tools/extract.py)
"""
import csv
import struct
import subprocess
import sys
import tempfile
import tomllib
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECOMP_ROM = ROOT / "roms" / "rush2049.us.recomp.z64"
SIG_PATH = ROOT / "ref" / "n64sym" / "src" / "builtin_signatures.sig"
NAMES_PATH = ROOT / "syms" / "names.toml"
OUT_PATH = ROOT / "syms" / "rush2049.us.syms.toml"

# name, rom (fake ROM for the inflated segments, see extract.py), vram, size of .text
SECTIONS = [
    ("boot", 0x00001000, 0x80000400, 0x80027DD0 - 0x80000400),  # rspboot text starts at 0x80027DD0
    # One CPU function is linked after the microcode text: a cache 0x0D (create dirty exclusive) loop.
    ("boot_tail", 0x0002D1E0, 0x8002C5E0, 0x8002C5FC - 0x8002C5E0),
    ("main", 0x01000000, 0x80086A50, 0x8010FD80 - 0x80086A50),  # = the osInvalICache range at 0x80002300
    ("ovl1", 0x010A0000, 0x8038A400, 0x803AF980 - 0x8038A400),
    ("ovl2", 0x010D0000, 0x8038A400, 0x80393B40 - 0x8038A400),
]

# libultra functions the signature scan can't name (hand-written assembly, mostly exceptasm.s).
# N64Recomp drops or reimplements libultra functions by name, so these must carry their real names.
MANUAL_NAMES = {
    0x8000C980: "__osExceptionPreamble",  # lui/addiu/jr $k0 into __osException
    0x8000C990: "__osException",
    0x8000CEDC: "send_mesg",
    # PI/SI access-queue helpers have identical masked code, so the scan can't tell them apart.
    0x8000DBC0: "__osPiCreateAccessQueue",
    0x8000DC10: "__osPiGetAccess",
    0x8000DC54: "__osPiRelAccess",
    0x8000E560: "__osSiCreateAccessQueue",
    0x8000E5B0: "__osSiGetAccess",
    0x8000E5F4: "__osSiRelAccess",
    0x8000C11C: "__osTimerInterrupt",
    # Identified by hand from calls, hardware registers and position in the libultra link order.
    0x80006BB0: "viMgrMain",               # __osViSwapContext, osGetCount, __osTimerInterrupt loop
    0x800077D0: "osViGetNextFramebuffer",  # follows osViGetCurrentFramebuffer, reads __osViNext
    0x80007840: "_VirtualToPhysicalTask",  # osSpTaskLoad helper: bcopy + 7x osVirtualToPhysical
    0x80008128: "__createSpeedParam",      # PI_BSD_DOM1/2 registers 0xA4600014-0xA4600030
    0x80008630: "osPiStartDma",            # (mb, pri, direction, devAddr, vAddr, nbytes, mq), types 0xB/0xC
    0x80009450: "osContInit",
    0x80009854: "osContGetReadData",
    0x800098E0: "__osPackReadData",
    0x80009E18: "__osPfsCheckRamArea",
    0x8000A088: "__osMakeMotorData",       # calls __osContAddressCrc
    0x8000ADF0: "__osPfsGetNextPage",      # calls __osPfsRWInode
    0x8000BF00: "osAiSetFrequency",        # AI_DACRATE/AI_BITRATE, osViClock
    0x8000C660: "__osGetActiveQueue",
    0x8000E390: "osPiGetCmdQueue",
    0x8000F124: "__osGetId",               # pack id check/repair, the real one
    0x8000D9C0: "__ll_rem",
    0x800081D0: "osPiReadIo",              # calls __osPiRawReadIo; the signature scan said osPiWriteIo
    0x8000CF90: "handle_CpU",
    0x8002C5E0: "dcache_create_dirty_8002C5E0",  # (start, end): cache 0x0D per 16-byte line; no-op on PC
}

# Function starts spimdisasm misses (it treats __osExceptionPreamble's jr $k0 as part of one function).
EXTRA_FUNCTION_STARTS = {0x8000C990, 0x8000CF90}

# Hand-written asm islands: PC-relative tricks (bltzal $zero to read the PC, bal to local subroutines, jalr
# through tables) and coefficient tables inside the code. N64Recomp can't translate them, so only their
# entry points are kept as symbols (each spanning to the next entry) and the runtime reimplements them.
ASM_ISLANDS = [
    # Q15 fixed-point DSP on halfword buffers, called from func_80026xxx (audio-side, see src/gsm_decoder.cpp).
    (0x800277D0, 0x80027DD0, (0x800277D0, 0x800278C4, 0x80027B44)),
]

# Signature names the scan hands to unrelated tiny stubs; never trust them.
UNTRUSTED_NAMES = {"n_alSynRemovePlayer", "osProfileFlush", "n_alSeqpDelete", "alSynFreeFX", "ptstart", "__dummy", "osSyncPrintf", "__osGetId", "osScGetCmdQ"}


def load_sigs():
    sigs, cur = [], None
    for line in SIG_PATH.read_text().splitlines():
        p = line.split()
        if not p or p[0].startswith("#"):
            continue
        if p[0].startswith("."):
            cur["relocs"] += [(int(o, 16), p[0], p[1]) for o in p[2:]]
        else:
            cur = {"name": p[0], "size": int(p[1], 16), "crc8": int(p[2], 16), "crc": int(p[3], 16), "relocs": []}
            sigs.append(cur)
    return [s for s in sigs if s["size"] >= 12]


def masked(buf, off, sig):
    data = bytearray(buf[off:off + sig["size"]])
    if len(data) < sig["size"]:
        return None
    for o, kind, _ in sig["relocs"]:
        w = struct.unpack_from(">I", data, o)[0]
        w &= 0xFFFF0000 if kind in (".hi16", ".lo16") else 0xFC000000
        struct.pack_into(">I", data, o, w)
    return bytes(data)


def match_library(buf, vram, func_addrs, sigs):
    """{vram: name} for functions whose code matches a library signature (largest match wins)."""
    by_crc8 = {}
    for s in sigs:
        by_crc8.setdefault(s["crc8"], []).append(s)
    names = {}
    for addr in func_addrs:
        off = addr - vram
        hits = []
        for s in by_crc8.get(zlib.crc32(masked(buf, off, {"size": 8, "relocs": []}) or b""), []):
            d = masked(buf, off, s)
            if d is not None and zlib.crc32(d[:8]) == s["crc8"] and zlib.crc32(d) == s["crc"]:
                hits.append(s)
        if not hits:
            continue
        size = max(s["size"] for s in hits)
        best = [s for s in hits if s["size"] == size]
        if len({s["name"] for s in best}) > 1:
            # Same masked code under several names (e.g. __osPiRelAccess/__osSiRelAccess differ only in
            # the global they touch): leave it to MANUAL_NAMES rather than guess.
            print(f"  ambiguous at {addr:08X}: {sorted({s['name'] for s in best})}")
            continue
        best = best[0]
        names[addr] = best["name"]
        # jal targets inside a matched function are named by its relocations too
        for o, kind, target in best["relocs"]:
            if kind == ".targ26" and "_text_" not in target:
                w = struct.unpack_from(">I", buf, off + o)[0]
                names.setdefault(((w & 0x3FFFFFF) << 2) | 0x80000000, target)
    return names


def spimdisasm_functions(buf, vram, tmp):
    binpath = Path(tmp) / f"{vram:08X}.bin"
    binpath.write_bytes(buf)
    info = Path(tmp) / f"{vram:08X}.csv"
    subprocess.run([sys.executable, "-m", "spimdisasm", "singleFileDisasm", str(binpath), str(Path(tmp) / "asm"),
                    "--start", "0", "--end", hex(len(buf)), "--vram", hex(vram), "--function-info", str(info)],
                   check=True, capture_output=True)
    with info.open() as f:
        return sorted((int(r["address"], 16), int(r["length"], 16)) for r in csv.DictReader(f))


def branch_targets(buf, vram, start, end):
    """Targets of PC-relative branches and j instructions in [start, end)."""
    out = set()
    for a in range(start, end, 4):
        w = struct.unpack_from(">I", buf, a - vram)[0]
        op = w >> 26
        if op in (4, 5, 6, 7, 20, 21, 22, 23) or (op == 1 and ((w >> 16) & 0x1F) in (0, 1, 2, 3)) or                 (op == 0x11 and ((w >> 21) & 0x1F) == 8):  # beq/bne/blez/bgtz(+likely), regimm, bc1x
            out.add(a + 4 + (((w & 0xFFFF) ^ 0x8000) - 0x8000) * 4)
        elif op == 2:  # j
            out.add(((a + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2))
    return out


def jal_targets(rom):
    """Every jal target in every code section (a real function start is called by something)."""
    out = set()
    for _, sec_rom, _, sec_size in SECTIONS:
        for o in range(sec_rom, sec_rom + sec_size, 4):
            w = struct.unpack_from(">I", rom, o)[0]
            if w >> 26 == 3:
                out.add(((w & 0x3FFFFFF) << 2) | 0x80000000)
    return out


def merge_split_functions(buf, vram, size, funcs, called):
    """spimdisasm sometimes starts a 'function' at a label in the middle of one (e.g. after a
    mid-function jr ra). When a function branches into the next one and nothing jal's that one,
    it is the same function: merge them, repeatedly."""
    starts = [a for a, _ in funcs]
    merged = True
    while merged:
        merged = False
        for i in range(len(starts) - 1):
            end = starts[i + 1] if i + 1 < len(starts) else vram + size
            for t in branch_targets(buf, vram, starts[i], end):
                if t >= end and t < vram + size:
                    # absorb every following start up to the one containing t, unless it is called
                    j = i + 1
                    while j < len(starts) and starts[j] <= t:
                        if starts[j] in called:
                            break
                        j += 1
                    else:
                        pass
                    absorb = [s for s in starts[i + 1:j] if s not in called]
                    if absorb and all(s not in called for s in starts[i + 1:j]):
                        starts = starts[:i + 1] + starts[j:]
                        merged = True
                        break
            if merged:
                break
    return [(a, 0) for a in starts]


def load_registry():
    """{(section, vram): name} from syms/names.toml."""
    if not NAMES_PATH.exists():
        return {}
    reg = tomllib.loads(NAMES_PATH.read_text())
    return {(f["section"], f["vram"]): f["name"] for f in reg.get("func", [])}


def main():
    rom = RECOMP_ROM.read_bytes()
    sigs = load_sigs()
    registry = load_registry()
    called = jal_targets(rom)
    out = ["# Generated by tools/gen_syms.py - do not edit by hand.", ""]
    used = set()
    with tempfile.TemporaryDirectory() as tmp:
        for sec, sec_rom, sec_vram, sec_size in SECTIONS:
            buf = rom[sec_rom:sec_rom + sec_size]
            funcs = spimdisasm_functions(buf, sec_vram, tmp)
            before = len(funcs)
            funcs = merge_split_functions(buf, sec_vram, sec_size, funcs, called)
            known = {a for a, _ in funcs}
            funcs = sorted(funcs + [(a, 0) for a in EXTRA_FUNCTION_STARTS
                                    if sec_vram <= a < sec_vram + sec_size and a not in known])
            for lo, hi, entries in ASM_ISLANDS:
                if sec_vram <= lo < sec_vram + sec_size:
                    funcs = sorted([f for f in funcs if not lo <= f[0] < hi] + [(a, 0) for a in entries])
            if len(funcs) != before:
                print(f"{sec}: merged {before - len(funcs)} mid-function splits")
            names = {}
            if sec in ("boot", "boot_tail"):
                names = {a: n for a, n in match_library(buf, sec_vram, [a for a, _ in funcs], sigs).items()
                         if n not in UNTRUSTED_NAMES}
                manual = set(MANUAL_NAMES.values())
                names = {a: n for a, n in names.items() if n not in manual}
                names.update(MANUAL_NAMES)
            out += ["[[section]]", f'name = "{sec}"', f"rom = 0x{sec_rom:08X}", f"vram = 0x{sec_vram:08X}",
                    f"size = 0x{sec_size:X}", "", "functions = ["]
            named = 0
            for i, (addr, length) in enumerate(funcs):
                end = funcs[i + 1][0] if i + 1 < len(funcs) else sec_vram + sec_size
                length = min(length, end - addr) if length else end - addr
                name = registry.get((sec, addr)) or names.get(addr)
                if name is None or name in used:
                    # Overlays share vram, so suffix their generic names with the section.
                    name = f"func_{addr:08X}" if sec in ("boot", "boot_tail", "main") else f"{sec}_func_{addr:08X}"
                else:
                    named += 1
                used.add(name)
                out.append(f'    {{ name = "{name}", vram = 0x{addr:08X}, size = 0x{length:X} }},')
            out += ["]", ""]
            print(f"{sec}: {len(funcs)} functions, {named} named")
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text("\n".join(out))
    print(f"Wrote {OUT_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
