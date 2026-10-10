// rush2049::Resampler checks, run by CI: the output rate is exact at normal speed and moves by exactly the speed asked
// for, and changing the speed mid-tone leaves no step in the waveform.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "audio_resampler.h"

namespace {

constexpr double pi = 3.14159265358979323846;
int failures = 0;

void check(bool ok, const char* what, double value) {
    printf("%s %s (%g)\n", ok ? "ok  " : "FAIL", what, value);
    if (!ok) {
        failures++;
    }
}

// Feeds `buffers` 192-frame buffers (the game's) of a sine, changing the speed to `ppm_after` halfway.
std::vector<float> run(uint32_t in_rate, uint32_t out_rate, double freq, size_t buffers, int32_t ppm_before,
                       int32_t ppm_after) {
    rush2049::Resampler r;
    r.reset(in_rate, out_rate);
    r.set_speed(ppm_before);
    std::vector<float> out, buf;
    size_t total = buffers * 192;
    for (size_t n = 0; n < total; n += 192) {
        if (n >= total / 2) {
            r.set_speed(ppm_after);
        }
        buf.clear();
        for (size_t i = n; i < n + 192; i++) {
            float v = (float)(0.5 * std::sin(2.0 * pi * freq * i / in_rate));
            buf.push_back(v);
            buf.push_back(v);
        }
        r.process(buf.data(), 192, out);
    }
    return out;
}

// The largest sample-to-sample step in the left channel, past the filter's start-up.
double max_step(const std::vector<float>& out) {
    double m = 0.0;
    for (size_t i = 2 * 128; i + 2 < out.size(); i += 2) {
        m = std::max(m, (double)std::abs(out[i + 2] - out[i]));
    }
    return m;
}

} // namespace

int main() {
    // 44.1 kHz (the high-quality MusyX mix) to 48 kHz for 10 s: the input's length at the output rate, less the last
    // 32 input frames, which the filter is still waiting to see past.
    {
        std::vector<float> out = run(44100, 48000, 1000.0, 2300, 0, 0);
        double frames = out.size() / 2.0;
        double expected = (2300.0 * 192 - 32) * 48000 / 44100;
        check(std::abs(frames - expected) <= 2.0, "44.1 -> 48 kHz at normal speed keeps the exact rate", frames - expected);
    }
    // 0.5% faster: 0.5% fewer output frames per input frame.
    {
        std::vector<float> out = run(22050, 48000, 1000.0, 1150, 5000, 5000);
        double frames = out.size() / 2.0;
        double expected = (1150.0 * 192 - 32) * 48000 / 22050 / 1.005;
        check(std::abs(frames - expected) <= 2.0, "22.05 -> 48 kHz at +5000 ppm is 0.5% shorter", frames - expected);
    }
    // A 1 kHz tone at 48 kHz moves at most 2 * pi * 1000 / 48000 * 0.5 = 0.065 per sample; a click would jump past it.
    {
        double limit = 2.0 * pi * 1000.0 / 48000 * 0.5 * 1.05;
        double step = max_step(run(22050, 48000, 1000.0, 230, 0, 20000));
        check(step < limit, "speeding up 2% mid-tone leaves no step", step);
        step = max_step(run(22050, 48000, 1000.0, 230, 20000, 0));
        check(step < limit, "slowing back down mid-tone leaves no step", step);
    }
    return failures == 0 ? 0 : 1;
}
