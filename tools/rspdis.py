"""Disassemble an RSP microcode from the ROM and list what RSPRecomp needs to know.

usage: rspdis.py <rom> <text_rom_offset> <text_size> <data_rom_offset> <data_size> [--full]

Prints jr instructions whose register isn't $ra, DMA/overlay hints, and DMEM halfwords that look like IMEM
code addresses (jump-table candidates).
"""
import struct
import sys

import rabbitizer

IMEM_BASE = 0x04001080


def main():
    rom_path = sys.argv[1]
    text_off, text_size, data_off, data_size = (int(x, 16) for x in sys.argv[2:6])
    full = "--full" in sys.argv
    rom = open(rom_path, "rb").read()
    text = rom[text_off:text_off + text_size]
    data = rom[data_off:data_off + data_size]

    starts = set()
    for i in range(0, len(text), 4):
        word = struct.unpack_from(">I", text, i)[0]
        vram = IMEM_BASE + i
        instr = rabbitizer.Instruction(word, vram, rabbitizer.InstrCategory.RSP)
        dis = instr.disassemble()
        starts.add(vram & 0xFFF)
        if full:
            print(f"{vram:08X}: {word:08X}  {dis}")
        elif "jr" in dis.split()[0:1] and "$ra" not in dis:
            print(f"jr      {vram:08X}: {dis}")
        elif dis.startswith("jalr"):
            print(f"jalr    {vram:08X}: {dis}")
        elif dis.startswith("mtc0"):
            print(f"mtc0    {vram:08X}: {dis}")

    if not full:
        cands = []
        for i in range(0, len(data) - 1, 2):
            h = struct.unpack_from(">H", data, i)[0]
            if 0x1080 <= h < 0x1080 + text_size and h % 4 == 0:
                cands.append((i, h))
        print("dmem halfwords in IMEM range:")
        for i, h in cands:
            print(f"  dmem {i:03X}: {h:04X}")


if __name__ == "__main__":
    main()
