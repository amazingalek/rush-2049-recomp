#ifndef RUSH2049_MUSYX_HLE_H
#define RUSH2049_MUSYX_HLE_H

#include <cstdint>
#include <vector>

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

    // The high-quality mix: the HLE also renders every subframe at twice the game's rate, with a better
    // interpolator, and the audio output plays that instead. On by default; `--musyx native` turns it off.
    void set_musyx_hires(bool enabled);
    bool musyx_hires_enabled();
    // Rewrites one AI buffer (s16 stereo, as the game queues it) at twice the rate into `out`.
    void musyx_hires_buffer(const int16_t* samples, size_t sample_count, std::vector<int16_t>& out);
    // Frames played from the high-quality mix, and frames it had no match for (upsampled from the game's).
    void musyx_hires_counts(uint64_t& matched, uint64_t& fallback);
}

#endif
