// Bring-up probes: counters fed by us.toml hooks, printed every few seconds with --probes or --stacks.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>

#include "recomp.h"
#include "rush2049_hooks.h"
#include "debug_probe.h"
#include "audio_limiter.h"
#include "musyx_hle.h"

namespace {

std::atomic<uint8_t*> probe_rdram{ nullptr };
// Scheduler messages 0x29A..0x29E (retrace, RSP done, RDP done, pre-NMI, audio task queued).
std::atomic<uint32_t> sched_counts[5]{};

// The music thread's ticks: how many ran, how many were skipped, and the most sound commands in flight as one
// started (since the previous print).
std::atomic<uint32_t> music_ticks{ 0 };
std::atomic<uint32_t> music_ticks_skipped{ 0 };
std::atomic<uint32_t> music_commands_peak{ 0 };

// The game's scheduler struct (libultra sched.c layout plus Rush 2049's fields).
constexpr uint32_t sched_addr = 0x8002E8E8;

} // namespace

std::atomic<uint32_t> rush2049::probe_input_reads{ 0 };

extern "C" void rush2049_trace(const char* tag, uint32_t value) {
    static const auto start = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    fprintf(stderr, "%8lld %s 0x%08X\n", ms, tag, value);
}

extern "C" void rush2049_probe_sched_msg(uint8_t* rdram, uint32_t msg) {
    probe_rdram.store(rdram);
    if (msg >= 0x29A && msg <= 0x29E) {
        sched_counts[msg - 0x29A]++;
    }
}

namespace {
std::atomic<uint32_t> audio_buffers{ 0 };
std::atomic<uint64_t> audio_samples{ 0 };
std::atomic<int> audio_peak{ 0 };
std::atomic<uint32_t> audio_frequency{ 0 };
std::atomic<uint32_t> queue_buffers{ 0 };
std::atomic<uint32_t> queue_draining{ 0 };
std::atomic<uint32_t> queue_empty{ 0 };
std::atomic<uint32_t> queue_interval_buffers{ 0 };
std::atomic<uint64_t> queue_interval_us{ 0 };
std::atomic<uint32_t> queue_max_us{ 0 };

std::mutex dump_mutex;
FILE* dump_file = nullptr;
uint32_t dump_bytes = 0;
FILE* output_dump_file = nullptr;
uint32_t output_dump_bytes = 0;

// 44-byte WAV header for stereo, 16-bit PCM or 32-bit float; rewritten with the final sizes after each buffer.
void write_wav_header(FILE* f, uint32_t rate, uint32_t data_bytes, bool is_float = false) {
    auto u32 = [f](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [f](uint16_t v) { fwrite(&v, 2, 1, f); };
    uint16_t bits = is_float ? 32 : 16;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); u32(36 + data_bytes);
    fwrite("WAVEfmt ", 1, 8, f); u32(16); u16(is_float ? 3 : 1); u16(2); u32(rate); u32(rate * bits / 4);
    u16(bits / 4); u16(bits);
    fwrite("data", 1, 4, f); u32(data_bytes);
    fseek(f, 0, SEEK_END);
}
} // namespace

void rush2049::start_audio_dump(const char* path) {
    std::lock_guard lock{ dump_mutex };
    dump_file = fopen(path, "wb");
    if (dump_file != nullptr) {
        write_wav_header(dump_file, 0, 0, true);
    }
}

void rush2049::probe_audio_frequency(uint32_t freq) {
    if (audio_frequency.exchange(freq) != freq) {
        fprintf(stderr, "[audio] frequency %u Hz\n", freq);
    }
}

void rush2049::start_output_dump(const char* path) {
    std::lock_guard lock{ dump_mutex };
    output_dump_file = fopen(path, "wb");
    if (output_dump_file != nullptr) {
        write_wav_header(output_dump_file, 0, 0);
    }
}

void rush2049::probe_output(const float* samples, size_t frames, uint32_t rate) {
    std::lock_guard lock{ dump_mutex };
    if (output_dump_file == nullptr) {
        return;
    }
    for (size_t i = 0; i < frames * 2; i++) {
        float v = std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f;
        int16_t s = (int16_t)std::lround(v);
        fwrite(&s, sizeof(s), 1, output_dump_file);
    }
    output_dump_bytes += (uint32_t)(frames * 4);
    write_wav_header(output_dump_file, rate, output_dump_bytes);
    fflush(output_dump_file);
}

void rush2049::probe_audio(const float* samples, size_t frames) {
    {
        std::lock_guard lock{ dump_mutex };
        if (dump_file != nullptr) {
            fwrite(samples, sizeof(float) * 2, frames, dump_file);
            dump_bytes += (uint32_t)(frames * 8);
            write_wav_header(dump_file, audio_frequency.load(), dump_bytes, true);
            fflush(dump_file);
        }
    }
    audio_buffers++;
    audio_samples += frames * 2;
    int peak = audio_peak.load();
    for (size_t i = 0; i < frames * 2; i++) {
        int v = (int)std::lround(std::abs(samples[i]) * 32768.0f);
        if (v > peak) {
            peak = v;
        }
    }
    audio_peak.store(peak);
}

void rush2049::probe_audio_queue(uint32_t queued_us, bool draining) {
    queue_buffers++;
    if (draining) {
        queue_draining++;
    }
    if (queued_us == 0) {
        queue_empty++;
    }
    queue_interval_buffers++;
    queue_interval_us += queued_us;
    uint32_t max = queue_max_us.load();
    while (queued_us > max && !queue_max_us.compare_exchange_weak(max, queued_us)) {
    }
}

uint8_t* rush2049::probe_get_rdram() {
    return probe_rdram.load();
}

void rush2049::probe_music_tick(uint32_t commands_in_flight) {
    music_ticks++;
    if (commands_in_flight > music_commands_peak.load()) {
        music_commands_peak = commands_in_flight;
    }
}

void rush2049::probe_music_tick_skipped() {
    music_ticks_skipped++;
}

void rush2049::print_probes() {
    // Peak is since the previous print.
    fprintf(stderr, "  audio: %u buffers, %llu samples, peak %d\n", audio_buffers.load(),
            (unsigned long long)audio_samples.load(), audio_peak.exchange(0));
    // Mean and max are since the previous print.
    uint32_t interval_buffers = queue_interval_buffers.exchange(0);
    uint64_t interval_us = queue_interval_us.exchange(0);
    fprintf(stderr, "  audio queue: mean %.1f ms, max %.1f ms; of %u buffers, %u drained, %u found it empty\n",
            interval_buffers ? interval_us / 1000.0 / interval_buffers : 0.0, queue_max_us.exchange(0) / 1000.0,
            queue_buffers.load(), queue_draining.load(), queue_empty.load());
    uint64_t hires_matched, hires_fallback;
    musyx_hires_counts(hires_matched, hires_fallback);
    for (const auto& [name, limiter] : { std::pair<const char*, const rush2049::Limiter&>{ "limiter", rush2049::output_limiter },
                                         { "music limiter", rush2049::mix_limiter } }) {
        uint64_t limiter_frames = limiter.frames_total.load();
        uint64_t limited = limiter.frames_limited.load();
        fprintf(stderr, "  %s: engaged %llu times, %llu of %llu frames (%.3f%%), lowest gain %.2f dB\n", name,
                (unsigned long long)limiter.engagements.load(), (unsigned long long)limited,
                (unsigned long long)limiter_frames, limiter_frames ? 100.0 * limited / limiter_frames : 0.0,
                20.0 * std::log10(limiter.min_gain_ppm.load() / 1e6));
    }
    fprintf(stderr, "  musyx hi-res: %llu frames played, %llu without a match\n", (unsigned long long)hires_matched,
            (unsigned long long)hires_fallback);
    fprintf(stderr, "  sched msgs: retrace %u rsp %u rdp %u nmi %u audio %u\n", sched_counts[0].load(),
            sched_counts[1].load(), sched_counts[2].load(), sched_counts[3].load(), sched_counts[4].load());
    fprintf(stderr, "  input reads: %u\n", probe_input_reads.load());
    fprintf(stderr, "  music ticks: %u, %u skipped behind a queued retrace; most sound commands in flight: %u of 128\n",
            music_ticks.load(), music_ticks_skipped.load(), music_commands_peak.exchange(0));
    uint8_t* rdram = probe_rdram.load();
    if (rdram == nullptr) {
        return;
    }
    gpr sc = (gpr)(int32_t)sched_addr;
    fprintf(stderr, "  sc+260..284:");
    for (int off = 0x260; off <= 0x284; off += 4) {
        fprintf(stderr, " %08X", (uint32_t)MEM_W(off, sc));
    }
    fprintf(stderr, "\n  sc queues (valid/count): +40 %d/%d +78 %d/%d\n", (int)MEM_W(0x48, sc), (int)MEM_W(0x50, sc),
            (int)MEM_W(0x80, sc), (int)MEM_W(0x88, sc));
    // Controller Pak bookkeeping: enable byte, query status of port 1, per-port pak flags.
    gpr pak = (gpr)(int32_t)0x80144030;
    fprintf(stderr, "  pak: enable %d status %02X flags", (int)MEM_B(0, (gpr)(int32_t)0x8011EAE0),
            (unsigned)(uint8_t)MEM_B(2, (gpr)(int32_t)0x80149440));
    for (int off = 0; off < 0xC; off++) {
        fprintf(stderr, " %d", (int)MEM_B(off, pak));
    }
    fprintf(stderr, " +74 %d\n", (int)MEM_W(0x74, pak));
}
