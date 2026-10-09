#ifndef RUSH2049_MUSYX_HLE_H
#define RUSH2049_MUSYX_HLE_H

#include "librecomp/rsp.hpp"

namespace rush2049 {
    enum class MusyxMode {
        Hle,     // mupen64plus-rsp-hle's MusyX v1 (default)
        Lle,     // the game's microcode, recompiled by RSPRecomp
        Compare, // run both on the same input for the first tasks, keep LLE's result, log the differences
    };

    void set_musyx_mode(MusyxMode mode);
    // The function to run for the game's audio (M_AUDTASK) tasks.
    RspUcodeFunc* get_musyx_ucode();
}

#endif
