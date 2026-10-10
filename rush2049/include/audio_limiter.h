#ifndef __RUSH2049_AUDIO_LIMITER_H__
#define __RUSH2049_AUDIO_LIMITER_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rush2049 {
    // Stereo-linked look-ahead peak limiter (1.0 = device full scale). Everything below the threshold passes untouched;
    // a peak above it is bent softly toward the ceiling, which it never reaches. The gain eases down over the 3 ms
    // before a peak and recovers over ~60 ms.
    class Limiter {
    public:
        Limiter(float threshold, float ceiling) : threshold(threshold), ceiling(ceiling) {}
        void reset(uint32_t rate);
        // Limits `frames` interleaved stereo frames in place. The output lags the input by the 3 ms look-ahead.
        void process(float* samples, size_t frames);

        // Since the last reset, for --probes: frames processed, frames with the gain below 1, the number of times the
        // gain left 1, and the lowest gain in millionths.
        std::atomic<uint64_t> frames_total{ 0 };
        std::atomic<uint64_t> frames_limited{ 0 };
        std::atomic<uint64_t> engagements{ 0 };
        std::atomic<uint32_t> min_gain_ppm{ 1000000 };

    private:
        // The level a peak of `x` (above the threshold) leaves at.
        float knee(float x) const;

        const float threshold;
        const float ceiling;
        uint32_t window = 0;       // look-ahead frames + 1
        float release = 0.0f;      // per-frame step of the gain's recovery toward 1
        std::vector<float> delay;  // the last window - 1 input frames, interleaved
        std::vector<float> needed; // the gain each of the last window frames needs
        std::vector<float> ramp;   // the last window held gains; their mean is the gain applied
        size_t delay_pos = 0;
        size_t pos = 0;
        uint32_t since_needed = 0; // frames since the last one that needed a gain below 1
        uint32_t ramp_below = 0;   // entries of ramp below 1
        double ramp_sum = 0.0;
        float held = 1.0f;
        bool limiting = false;
    };

    // The two in main.cpp's queue_samples; --probes prints their counters. output_limiter keeps the game's own mix
    // (0.9 to 1.0) from clipping: the high-quality MusyX mix's loudest hits reach twice full scale. mix_limiter (0.95 to
    // 0.99) runs on the sum once the replacement music is added, so a loud hit over a loud bar of music doesn't clip
    // either; set higher, it leaves the music alone the rest of the time.
    extern Limiter output_limiter;
    extern Limiter mix_limiter;
}

#endif
