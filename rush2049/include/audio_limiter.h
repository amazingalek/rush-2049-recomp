#ifndef __RUSH2049_AUDIO_LIMITER_H__
#define __RUSH2049_AUDIO_LIMITER_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rush2049 {
    // Stereo-linked look-ahead peak limiter for the device output (1.0 = device full scale). Everything below 0.9 passes
    // untouched; a peak above it is bent softly toward 1.0, which it never reaches, so the high-quality MusyX mix's
    // loudest hits don't clip on the device. The gain eases down over the 3 ms before a peak and recovers over ~60 ms.
    class Limiter {
    public:
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

    // The one on the audio device's output (main.cpp's queue_samples); --probes prints its counters.
    extern Limiter output_limiter;
}

#endif
