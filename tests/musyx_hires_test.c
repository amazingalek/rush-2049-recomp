/* The high-quality MusyX side mix's voice interpolator, run by CI: a voice above 2x is low-passed at the output's
 * Nyquist frequency before it is resampled, so what used to fold back down is gone, and its passband is untouched.
 * Includes musyx.c for its static functions. */

#include "musyx.c"

#include <stdio.h>

static const double pi = 3.14159265358979323846;
static int failures = 0;

void HleVerboseMessage(void* user_defined, const char *message, ...) { (void)user_defined; (void)message; }
void HleWarnMessage(void* user_defined, const char *message, ...) { (void)user_defined; (void)message; }
void musyx_hires_store(const uint32_t* native, const float* hires) { (void)native; (void)hires; }
void rsp_break(struct hle_t* hle, unsigned int setbits) { (void)hle; (void)setbits; }

static void check(bool ok, const char *what, double value)
{
    printf("%s %s (%.2f)\n", ok ? "ok  " : "FAIL", what, value);
    if (!ok)
        failures++;
}

enum { LOOP = 480 }; /* the test voice loops over this many samples of its main segment */

/* One subframe of a full-scale-ish sine, k cycles per LOOP samples, played at pitch_step; returns its bus. */
static const float *render(uint32_t pitch_step, int k)
{
    static hires_voice_t voice;
    const int32_t env[4] = { 32768 << 16, 32768 << 16, 0, 0 }, env_step[4] = { 0, 0, 0, 0 };
    int i;

    memset(&hires_state, 0, sizeof(hires_state));
    memset(&voice, 0, sizeof(voice));
    voice.main_count = SAMPLE_BUFFER_SIZE;
    for (i = 0; i < HIRES_EXTRA + SAMPLE_BUFFER_SIZE + HIRES_EXTRA; ++i)
        voice.main[i] = (int16_t)lrint(16384.0 * sin(2.0 * pi * k * (i - HIRES_EXTRA) / LOOP + 0.3));

    mix_voice_hires(&hires_state, &voice, 0, LOOP, 0, 0x4321, pitch_step, env, env_step);
    return hires_state.left[(pitch_step > 0x10000) ? 1 : 0];
}

/* the level of f (cycles per output) in y, in dB of the test sine, through a Blackman-Harris window */
static double level_db(const float *y, double f)
{
    double re = 0.0, im = 0.0, wsum = 0.0;
    int i;

    for (i = 0; i < HIRES_SIZE; ++i) {
        double a = 2.0 * pi * i / (HIRES_SIZE - 1);
        double w = 0.35875 - 0.48829 * cos(a) + 0.14128 * cos(2 * a) - 0.01168 * cos(3 * a);

        re += w * y[i] * cos(2.0 * pi * f * i);
        im -= w * y[i] * sin(2.0 * pi * f * i);
        wsum += w;
    }

    return 20.0 * log10(sqrt(re * re + im * im) / wsum * 2.0 / 16384.0 + 1e-12);
}

/* plays k cycles per LOOP at pitch (x the sample rate) and checks the level where it lands, folded or not */
static void tone(double pitch, int k, double lo, double hi)
{
    const uint32_t pitch_step = (uint32_t)(pitch * 0x10000) & ~15u;
    const double f = (double)k / LOOP * (pitch_step / 65536.0) / 2.0; /* cycles per output */
    const double folded = fabs(f - floor(f + 0.5));
    const double db = level_db(render(pitch_step, k), folded);
    char what[96];

    snprintf(what, sizeof(what), "%.1fx: %.0f Hz %s %.0f Hz, dB", pitch, f * 44100.0,
             (f > 0.5) ? "folds to" : "plays at", folded * 44100.0);
    check(db >= lo && db <= hi, what, db);
}

int main(void)
{
    int16_t x[2 * 9];
    uint32_t frac;
    double worst = 0.0;
    int i;

    init_hires_kernel();

    /* the stretched kernel starts where the fixed one ends: stretched by a hair, it gives the same output */
    for (i = 0; i < 2 * 9; ++i)
        x[i] = (int16_t)lrint(12000.0 * sin(0.9 * i) + 3000.0 * cos(2.3 * i));
    for (frac = 0; frac < 0x10000; frac += 0x0bcd) {
        double d = fabs(hires_interpolate_wide(x, 9, frac, 0x10001) - hires_interpolate(x + 1, frac));

        worst = (d > worst) ? d : worst;
    }
    check(worst < 2.0, "stretch 1 matches the 16 taps, max difference in 32768", worst);

    /* at and below 2x nothing is stretched */
    tone(2.0, 106, -0.1, 0.1);
    tone(2.0, 216, -1.5, -0.9);

    /* above 2x the passband is what it is at 2x, flat to 17.6 kHz */
    tone(2.5, 144, -0.1, 0.1);
    tone(3.0, 120, -0.1, 0.1);
    tone(4.0, 90, -0.1, 0.1);

    /* and what lands past 26.5 kHz, which folds below 17.6 kHz, is gone (it was 0 dB to -3.4 dB) */
    tone(2.5, 235, -200.0, -45.0);
    tone(3.0, 200, -200.0, -45.0);
    tone(3.0, 216, -200.0, -45.0);
    tone(4.0, 144, -200.0, -45.0);
    tone(4.0, 192, -200.0, -45.0);
    tone(8.0, 144, -200.0, -45.0);

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
