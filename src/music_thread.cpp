// The game's music thread (func_800992AC): the game sends it music and sound commands through the queue at
// 0x801427A8, and each retrace it runs a tick that updates the playing sounds.
//
// The music thread's tick on each retrace (func_80098FB8) posts a sound command to the thread's own queue for each
// sound it updates. Commands are 24-byte entries from a pool of 128 at 0x80142DD8: func_80091B00 hands one out (NULL
// when none is free, which its callers don't check) and the thread frees it once it has run it. On hardware the
// thread (priority 10, above the game's 4) runs as each retrace arrives, so a tick starts with the previous tick's
// commands done. On a busy PC it can fall behind, retraces pile up in its queue, and each tick it works through
// queues its commands behind the rest, until the pool runs dry while a race loads and a caller writes through
// NULL. So a tick returns at once while another retrace is queued behind it, and the newest one does the update.

#include <cstdint>

#include "recomp.h"
#include "rush2049_hooks.h"
#include "debug_probe.h"

extern "C" int rush2049_music_tick_behind(uint8_t* rdram) {
    const gpr pool = (gpr)(int32_t)0x80142DD8;
    uint32_t in_flight = 0;
    for (int i = 0; i < 128; i++) {
        in_flight += MEM_B(i * 0x18 + 3, pool) != 0;
    }
    rush2049::probe_music_tick(in_flight);
    // OSMesgQueue: +8 valid count, +C first, +10 size, +14 buffer. A message is a retrace when its first halfword
    // is 1, as the thread itself tests.
    const gpr queue = (gpr)(int32_t)0x801427A8;
    int32_t count = MEM_W(0x8, queue);
    int32_t first = MEM_W(0xC, queue);
    int32_t size = MEM_W(0x10, queue);
    gpr buffer = (gpr)(int32_t)MEM_W(0x14, queue);
    for (int32_t i = 0; i < count; i++) {
        gpr msg = (gpr)(int32_t)MEM_W(4 * ((first + i) % size), buffer);
        if (MEM_H(0, msg) == 1) {
            rush2049::probe_music_tick_skipped();
            return 1;
        }
    }
    return 0;
}
