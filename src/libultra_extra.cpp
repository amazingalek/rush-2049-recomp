// libultra functions N64Recomp treats as reimplemented that this runtime version doesn't provide.

#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"

// s32 osPiReadIo(u32 devAddr, u32 *data). The game's only call (boot main, past the end of the ROM) is patched
// out in us.toml; this exists because the function table still references it.
extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint32_t physical_addr = (uint32_t)ctx->r4 & 0x1FFFFFFF;
    gpr dram_addr = ctx->r5;

    if (physical_addr >= recomp::rom_base) {
        recomp::do_rom_pio(rdram, dram_addr, physical_addr);
    }
    ctx->r2 = 0;
}
