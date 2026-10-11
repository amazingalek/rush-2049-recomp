#include <algorithm>
#include <cmath>

#include "audio_limiter.h"

namespace {

constexpr double lookahead_seconds = 0.003;
constexpr double release_seconds = 0.06;

} // namespace

rush2049::Limiter rush2049::output_limiter{ 0.9f, 1.0f };

// The threshold plus a tanh curve that approaches the ceiling.
float rush2049::Limiter::knee(float x) const {
    return threshold + (ceiling - threshold) * std::tanh((x - threshold) / (ceiling - threshold));
}

void rush2049::Limiter::reset(uint32_t rate) {
    uint32_t lookahead = std::max(1u, (uint32_t)std::lround(rate * lookahead_seconds));
    window = lookahead + 1;
    release = (float)(1.0 - std::exp(-1.0 / (release_seconds * rate)));
    delay.assign(lookahead * 2, 0.0f);
    needed.assign(window, 1.0f);
    ramp.assign(window, 1.0f);
    delay_pos = 0;
    pos = 0;
    since_needed = window;
    ramp_below = 0;
    ramp_sum = window;
    held = 1.0f;
    limiting = false;
    frames_total = 0;
    frames_limited = 0;
    engagements = 0;
    min_gain_ppm = 1000000;
}

// The gain applied to the frame leaving the delay line is the mean of the last `window` held gains, each of which is
// the lowest gain any of the `window` frames before it needed. Every one of those windows contains the leaving frame,
// so the mean is never above the gain it needs: the gain ramps down linearly across the look-ahead to meet the peak.
void rush2049::Limiter::process(float* samples, size_t frames) {
    uint64_t limited = 0;
    uint64_t engaged = 0;
    float lowest = 1.0f;
    for (size_t i = 0; i < frames; i++) {
        float* frame = samples + 2 * i;
        float peak = std::max(std::abs(frame[0]), std::abs(frame[1]));
        float need = peak > threshold ? knee(peak) / peak : 1.0f;
        needed[pos] = need;
        since_needed = need < 1.0f ? 0 : std::min(since_needed + 1, window);

        float hold = 1.0f;
        if (since_needed < window) {
            hold = *std::min_element(needed.begin(), needed.end());
        }
        // Recover toward just past 1 so the gain gets back to exactly 1 instead of creeping up on it.
        held = std::min(hold, held + (1.001f - held) * release);

        float old = ramp[pos];
        ramp_below += (held < 1.0f) - (old < 1.0f);
        ramp_sum += (double)held - old;
        ramp[pos] = held;
        pos = (pos + 1) % window;
        float gain = 1.0f;
        if (ramp_below == 0) {
            ramp_sum = window;
        }
        else {
            gain = (float)(ramp_sum / window);
        }

        float* delayed = delay.data() + delay_pos;
        float left = delayed[0];
        float right = delayed[1];
        delayed[0] = frame[0];
        delayed[1] = frame[1];
        delay_pos = (delay_pos + 2) % delay.size();
        frame[0] = left * gain;
        frame[1] = right * gain;

        if (gain < 1.0f) {
            limited++;
            engaged += !limiting;
            lowest = std::min(lowest, gain);
        }
        limiting = gain < 1.0f;
    }
    frames_total += frames;
    frames_limited += limited;
    engagements += engaged;
    uint32_t lowest_ppm = (uint32_t)std::lround(lowest * 1e6);
    if (lowest_ppm < min_gain_ppm) {
        min_gain_ppm = lowest_ppm;
    }
}
