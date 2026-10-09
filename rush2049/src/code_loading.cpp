// Registers recompiled code with the runtime when the game inflates it into RDRAM.
//
// Rush 2049 keeps its main code and both overlays as raw deflate streams in ROM and inflates them itself with
// func_80006814(rom, dst, flag). tools/rush2049/extract.py appends the inflated segments to the recomp ROM at
// fake addresses, which is where N64Recomp's section table places them. The hooks in us.toml call
// rush2049_inflate_begin at the function's entry (to capture the source ROM address, which isn't saved anywhere
// else) and rush2049_inflate_end once the output is written, and this maps the original ROM address to the
// extracted section.
//
// Both overlays inflate to 0x8038A400, so whichever is loaded there is unloaded first.

#include <cstdio>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "rush2049_hooks.h"

namespace {

struct CodeSegment {
    uint32_t original_rom; // deflate stream in the original ROM
    uint32_t recomp_rom;   // inflated copy in the recomp ROM (tools/rush2049/extract.py)
    uint32_t size;         // inflated text size covered by the symbol file
    const char* name;
};

constexpr CodeSegment code_segments[] = {
    { 0x00B0CB10, 0x01000000, 0x8010FD80 - 0x80086A50, "main" },
    { 0x00B5C534, 0x010A0000, 0x803AF980 - 0x8038A400, "ovl1" },
    { 0x00B6FEC4, 0x010D0000, 0x80393B40 - 0x8038A400, "ovl2" },
};

constexpr int32_t overlay_vram = (int32_t)0x8038A400;
constexpr uint32_t overlay_region_size = 0x803AF980 - 0x8038A400; // the larger overlay

// Game threads are host threads under ultramodern, so the source address is per thread.
thread_local uint32_t inflate_src_rom = 0;

} // namespace

extern "C" void rush2049_inflate_begin(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    // The game passes cart addresses as plain ROM offsets.
    inflate_src_rom = (uint32_t)ctx->r4 & 0x0FFFFFFF;
}

extern "C" void rush2049_inflate_end(uint8_t* rdram, recomp_context* ctx) {
    // dst is still in the caller's argument slot (sw $a1, 0x4C($sp) at entry).
    int32_t dst = MEM_W(0x4C, ctx->r29);

    for (const CodeSegment& seg : code_segments) {
        if (seg.original_rom != inflate_src_rom) {
            continue;
        }
        if (dst == overlay_vram) {
            unload_overlays(overlay_vram, overlay_region_size);
        }
        fprintf(stderr, "[rush2049] loading %s code: rom 0x%08X -> 0x%08X\n", seg.name, seg.original_rom, (uint32_t)dst);
        load_overlays(seg.recomp_rom, dst, seg.size);
        break;
    }
    inflate_src_rom = 0;
}
