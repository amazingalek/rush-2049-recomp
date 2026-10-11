#include <algorithm>
#include <cmath>
#include <numeric>

#include "audio_resampler.h"

namespace {

constexpr int half = 32;            // taps on each side of the output
constexpr int taps = 2 * half;
constexpr int phases = 256;         // kernel rows; the output's position between two rows is interpolated
constexpr double beta = 9.0;        // Kaiser window: about -90 dB in the stop band
constexpr double transition = 0.09; // of the input rate, the stop band starting at the lower Nyquist frequency
constexpr double pi = 3.14159265358979323846;

double bessel_i0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 32; k++) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

} // namespace

void rush2049::Resampler::reset(uint32_t new_in_rate, uint32_t new_out_rate) {
    uint32_t g = std::gcd(new_in_rate, new_out_rate);
    in_rate = new_in_rate / g;
    out_rate = new_out_rate / g;
    phase = 0;
    step = (uint64_t)in_rate * (ppm_scale + speed_ppm);
    input.assign((half - 1) * 2, 0.0f);

    // Cutoff in cycles per input frame, centred in the transition band below the lower of the two Nyquist frequencies.
    double cutoff = std::min(1.0, (double)out_rate / in_rate) * 0.5 - transition / 2.0;
    kernel.assign((phases + 1) * taps, 0.0f);
    for (int p = 0; p <= phases; p++) {
        double frac = (double)p / phases;
        double sum = 0.0;
        for (int k = 0; k < taps; k++) {
            double d = (k - (half - 1)) - frac;  // tap k reads input frame half - 1 + k - (half - 1) past the output's
            double x = d / half;
            double w = std::abs(x) >= 1.0 ? 0.0 : bessel_i0(beta * std::sqrt(1.0 - x * x)) / bessel_i0(beta);
            double s = d == 0.0 ? 1.0 : std::sin(2.0 * pi * cutoff * d) / (pi * d);
            kernel[p * taps + k] = (float)(s * w);
            sum += s * w;
        }
        for (int k = 0; k < taps; k++) {
            kernel[p * taps + k] = (float)(kernel[p * taps + k] / sum);
        }
    }
}

void rush2049::Resampler::set_speed(int32_t ppm) {
    speed_ppm = ppm;
    step = (uint64_t)in_rate * (ppm_scale + speed_ppm);
}

void rush2049::Resampler::process(const float* in, size_t frames, std::vector<float>& out) {
    const uint64_t period = (uint64_t)out_rate * ppm_scale;
    input.insert(input.end(), in, in + frames * 2);
    size_t available = input.size() / 2;
    size_t start = 0;  // the first input frame the next output reads
    while (start + taps <= available) {
        float pos = (float)((double)phase * phases / period);
        int row = std::min((int)pos, phases - 1);
        float t = pos - row;
        const float* a = &kernel[row * taps];
        const float* b = a + taps;
        const float* x = &input[start * 2];
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < taps; k++) {
            float w = a[k] + (b[k] - a[k]) * t;
            l += w * x[2 * k];
            r += w * x[2 * k + 1];
        }
        out.push_back(l);
        out.push_back(r);
        phase += step;
        start += phase / period;
        phase %= period;
    }
    input.erase(input.begin(), input.begin() + start * 2);
}
