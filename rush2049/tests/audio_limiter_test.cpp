// rush2049::Limiter checks, run by CI: audio below the threshold passes untouched (only delayed by the look-ahead),
// peaks never pass the ceiling, and the gain gets back to exactly 1 after a peak.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "audio_limiter.h"

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr uint32_t rate = 48000;
constexpr size_t lookahead = 144; // 3 ms at 48 kHz
int failures = 0;

void check(bool ok, const char* what, double value) {
    printf("%s %s (%g)\n", ok ? "ok  " : "FAIL", what, value);
    if (!ok) {
        failures++;
    }
}

// Two seconds of a 0.5 sine at 440 Hz, with a 10 ms burst of `burst` x full scale at 500 ms when `burst` is not 0.
std::vector<float> signal(float burst) {
    std::vector<float> s;
    for (size_t i = 0; i < 2 * rate; i++) {
        double t = (double)i / rate;
        float v = (float)(0.5 * std::sin(2.0 * pi * 440.0 * t));
        if (burst != 0.0f && t >= 0.5 && t < 0.51) {
            v += (float)(burst * std::sin(2.0 * pi * 1000.0 * t));
        }
        s.push_back(v);
        s.push_back(-v);
    }
    return s;
}

// Runs `in` through `limiter` in the device's 256-frame buffers.
std::vector<float> run(rush2049::Limiter& limiter, std::vector<float> in) {
    limiter.reset(rate);
    for (size_t i = 0; i < in.size(); i += 512) {
        limiter.process(in.data() + i, std::min<size_t>(256, (in.size() - i) / 2));
    }
    return in;
}

double peak(const std::vector<float>& s) {
    double p = 0.0;
    for (float v : s) {
        p = std::max(p, (double)std::abs(v));
    }
    return p;
}

// The largest difference between `out` and `in` delayed by the look-ahead, from frame `from` on.
double error(const std::vector<float>& in, const std::vector<float>& out, size_t from) {
    double e = 0.0;
    for (size_t i = std::max(from, lookahead) * 2; i < out.size(); i++) {
        e = std::max(e, (double)std::abs(out[i] - in[i - lookahead * 2]));
    }
    return e;
}

} // namespace

int main() {
    for (rush2049::Limiter* limiter : { &rush2049::output_limiter, &rush2049::mix_limiter }) {
        const char* name = limiter == &rush2049::output_limiter ? "output_limiter" : "mix_limiter";
        float ceiling = limiter == &rush2049::output_limiter ? 1.0f : 0.99f;
        printf("%s\n", name);

        std::vector<float> quiet = signal(0.0f);
        std::vector<float> out = run(*limiter, quiet);
        check(error(quiet, out, 0) == 0.0, "a 0.5 sine passes untouched", error(quiet, out, 0));
        check(limiter->engagements == 0, "and never engages", (double)limiter->engagements);

        std::vector<float> loud = signal(1.5f);
        out = run(*limiter, loud);
        check(peak(loud) > 1.9, "the burst goes past full scale", peak(loud));
        check(peak(out) <= ceiling, "the output never passes the ceiling", peak(out));
        check(peak(out) > 0.95 * ceiling, "and still reaches close to it", peak(out));
        check(limiter->engagements >= 1, "it engages", (double)limiter->engagements);
        check(error(loud, out, rate) == 0.0, "the gain is back to exactly 1 half a second later", error(loud, out, rate));
    }
    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
