// Headless driver: runs the game with no renderer, no window, no menus and no input, and prints how fast its CPU side
// runs. It is the first step towards a web build (README, CLAUDE.md): it builds with Emscripten as well as natively,
// so the game threads, the runtime and the MusyX audio can be timed in node and in a browser, and compared with the
// same build on Windows.
//
//   Rush2049Headless <rom.z64> [--seconds n] [--fps n] [--mix original|hq] [--config dir]
//
// The renderer takes each display list and returns at once, as if the RDP were infinitely fast, and counts them and
// the retraces. The audio sink drains at the rate the game set, the way a sound card would, so the audio thread is
// paced as it is in the game. Nobody presses anything, so the game runs its title screen and attract races, which
// draw 30 frames a second. --fps is the Game Frame Rate option (30 is Original, 60 the default). --mix is the MusyX
// Mixing option (High Quality is the default).
//
// Every 5 seconds it prints retraces, display lists and audio buffers a second, and the CPU time the whole process
// used in that time as a share of one core (not in a browser, which has no way to read it). At the end, the same over
// the whole run but its first interval (the boot), and the CPU time per display list: what a frame costs the CPU side,
// against the 16.7 ms a frame has at 60 fps.

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <sys/resource.h>
#endif

#include "recomp.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/renderer_context.hpp"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "rush2049_game.h"
#include "crash_handler.h"
#include "musyx_hle.h"
#ifdef HEADLESS_RT64
#include "rt64_web_renderer.h"
#endif

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();

namespace {
    std::atomic<uint64_t> retraces{0};
    std::atomic<uint64_t> display_lists{0};
    std::atomic<uint64_t> audio_buffers{0};

    // Counts what the runtime sends the renderer. With RT64 in the build (HEADLESS_RT64), it hands everything on to RT64,
    // which draws on the page's canvas; without, it returns at once, as if the RDP were infinitely fast.
    class CountingRenderer : public ultramodern::renderer::RendererContext {
    public:
        CountingRenderer(std::unique_ptr<ultramodern::renderer::RendererContext> drawer) : drawer(std::move(drawer)) {
            setup_result = this->drawer ? this->drawer->get_setup_result() : ultramodern::renderer::SetupResult::Success;
            chosen_api = ultramodern::renderer::GraphicsApi::Auto;
        }
        bool valid() override { return !drawer || drawer->valid(); }
        bool update_config(const ultramodern::renderer::GraphicsConfig& old_config, const ultramodern::renderer::GraphicsConfig& new_config) override {
            return !drawer || drawer->update_config(old_config, new_config);
        }
        void enable_instant_present() override { if (drawer) drawer->enable_instant_present(); }
        void send_dl(const OSTask* task) override {
            display_lists++;
            if (drawer) drawer->send_dl(task);
        }
        void send_dummy_workload(uint32_t fb_address) override { if (drawer) drawer->send_dummy_workload(fb_address); }
        void update_screen() override {
            retraces++;
            if (drawer) drawer->update_screen();
        }
        void shutdown() override { if (drawer) drawer->shutdown(); }
        uint32_t get_display_framerate() const override { return drawer ? drawer->get_display_framerate() : 60; }
        float get_resolution_scale() const override { return drawer ? drawer->get_resolution_scale() : 1.0f; }
    private:
        std::unique_ptr<ultramodern::renderer::RendererContext> drawer;
    };

    // Frames queued and not yet "played", in the game's own sample rate.
    std::mutex audio_mutex;
    double audio_queued = 0.0;
    uint32_t audio_rate = 22050;
    auto audio_clock = std::chrono::steady_clock::now();

    void drain_audio() {
        auto now = std::chrono::steady_clock::now();
        audio_queued = std::max(0.0, audio_queued - std::chrono::duration<double>(now - audio_clock).count() * audio_rate);
        audio_clock = now;
    }

    void queue_samples(int16_t* samples, size_t sample_count) {
        // The High Quality mix replaces the game's 22 kHz buffer with its own 44.1 kHz one here, as main.cpp does.
        if (rush2049::musyx_hires_enabled()) {
            static std::vector<float> hires;
            rush2049::musyx_hires_buffer(samples, sample_count, hires);
        }
        std::lock_guard lock{audio_mutex};
        drain_audio();
        audio_queued += sample_count / 2.0;
        audio_buffers++;
    }

    size_t get_frames_remaining() {
        std::lock_guard lock{audio_mutex};
        drain_audio();
        return (size_t)audio_queued;
    }

    void set_frequency(uint32_t freq) {
        std::lock_guard lock{audio_mutex};
        drain_audio();
        audio_rate = freq;
    }

    RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
        return task->t.type == M_AUDTASK ? rush2049::get_musyx_ucode() : nullptr;
    }

    bool get_input(int, uint16_t* buttons, float* x, float* y) {
        *buttons = 0;
        *x = 0.0f;
        *y = 0.0f;
        return true;
    }

    ultramodern::input::connected_device_info_t get_connected_device_info(int) {
        return { .connected_device = ultramodern::input::Device::Controller, .connected_pak = ultramodern::input::Pak::RumblePak };
    }

    // CPU time the process has used, in seconds, or a negative number where it can't be read.
    double process_cpu_seconds() {
#ifdef _WIN32
        FILETIME created, exited, kernel, user;
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        auto seconds = [](FILETIME t) { return (((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime) * 1e-7; };
        return seconds(kernel) + seconds(user);
#elif defined(__EMSCRIPTEN__)
        // node's process.cpuUsage counts every worker thread too. A browser has nothing like it.
        return MAIN_THREAD_EM_ASM_DOUBLE({
            if (typeof process === 'undefined' || !process.cpuUsage) return -1;
            var u = process.cpuUsage();
            return (u.user + u.system) / 1e6;
        });
#else
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1e-6;
#endif
    }

    void report(int seconds) {
        constexpr int interval = 5;
        auto last_time = std::chrono::steady_clock::now();
        double last_cpu = process_cpu_seconds();
        uint64_t last_retraces = 0, last_dls = 0, last_audio = 0;
        auto start_time = last_time;
        double start_cpu = last_cpu;
        uint64_t start_retraces = 0, start_dls = 0;
        for (int t = interval; t <= seconds; t += interval) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            auto now = std::chrono::steady_clock::now();
            double wall = std::chrono::duration<double>(now - last_time).count();
            double cpu = process_cpu_seconds();
            uint64_t f = retraces, d = display_lists, a = audio_buffers;
            char cpu_text[32] = "n/a";
            if (cpu >= 0.0 && last_cpu >= 0.0) {
                snprintf(cpu_text, sizeof(cpu_text), "%.0f%%", 100.0 * (cpu - last_cpu) / wall);
            }
            printf("[headless] %3ds  retraces/s %5.1f  display lists/s %5.1f  audio buffers/s %5.1f  cpu %s\n", t,
                   (f - last_retraces) / wall, (d - last_dls) / wall, (a - last_audio) / wall, cpu_text);
            fflush(stdout);
            last_time = now;
            last_cpu = cpu;
            last_retraces = f;
            last_dls = d;
            last_audio = a;
            if (t == interval) {
                start_time = now;
                start_cpu = cpu;
                start_retraces = f;
                start_dls = d;
            }
        }
        double wall = std::chrono::duration<double>(last_time - start_time).count();
        uint64_t dls = last_dls - start_dls;
        char cpu_text[64] = "cpu n/a";
        if (start_cpu >= 0.0 && dls != 0) {
            snprintf(cpu_text, sizeof(cpu_text), "cpu %.0f%%, %.2f ms per display list", 100.0 * (last_cpu - start_cpu) / wall,
                     1000.0 * (last_cpu - start_cpu) / dls);
        }
        printf("[headless] %.0fs after boot: retraces/s %.1f  display lists/s %.1f  %s\n", wall,
               (last_retraces - start_retraces) / wall, dls / wall, cpu_text);
        fflush(stdout);
        // Not std::exit: the game threads are still running, and destroying them at exit aborts. In a web worker
        // _Exit only ends the worker, so node is told to exit; a browser page just keeps running.
#ifdef __EMSCRIPTEN__
        MAIN_THREAD_EM_ASM({
            if (typeof process !== 'undefined') process.exit(0);
        });
#endif
        std::_Exit(EXIT_SUCCESS);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom.z64> [--seconds n] [--fps n] [--mix original|hq] [--config dir]\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::filesystem::path rom_path = argv[1];
    std::filesystem::path config_path = "headless-config";
    int seconds = 120;
    uint32_t fps = 60;
    bool hq_mix = true;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--seconds") == 0) {
            seconds = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--fps") == 0) {
            fps = (uint32_t)atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--mix") == 0) {
            hq_mix = strcmp(argv[i + 1], "original") != 0;
        } else if (strcmp(argv[i], "--config") == 0) {
            config_path = argv[i + 1];
        }
    }
#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    rush2049::install_crash_handler();

    rush2049::set_game_frame_rate(fps);
    rush2049::set_musyx_hires(hq_mix);

    // As main.cpp's supported_games.
    recomp::GameEntry game{
        .rom_hash = 0x86A0D6C1C9A3DEA1ULL,
        .internal_name = "Rush 2049",
        .display_name = "San Francisco Rush 2049",
        .game_id = u8"rush2049.n64.us",
        .mod_game_id = "rush2049",
        .save_type = recomp::SaveType::None,
        .is_enabled = false,
        .decompression_routine = rush2049::build_recomp_rom,
        .has_compressed_code = true,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
    };
    std::filesystem::create_directories(config_path);
    recomp::register_config_path(config_path);
    recomp::register_game(game);
    rush2049::register_overlays();

    std::u8string game_id = game.game_id;
    if (recomp::select_rom(rom_path, game_id) != recomp::RomValidationError::Good) {
        fprintf(stderr, "%s is not San Francisco Rush 2049 (USA)\n", rom_path.string().c_str());
        return EXIT_FAILURE;
    }
    // Started once the runtime is up, as the launcher does: the VI thread needs a retrace with the dummy VI mode the
    // runtime sets until the game starts.
    std::thread{[game_id, seconds]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        recomp::start_game(game_id, "");
        report(seconds);
    }}.detach();

    recomp::rsp::callbacks_t rsp_callbacks{ .get_rsp_microcode = get_rsp_microcode };
    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = [](uint8_t* rdram, ultramodern::renderer::WindowHandle, bool) -> std::unique_ptr<ultramodern::renderer::RendererContext> {
#ifdef HEADLESS_RT64
            return std::make_unique<CountingRenderer>(recompweb::create_rt64_renderer(rdram));
#else
            (void)rdram;
            return std::make_unique<CountingRenderer>(nullptr);
#endif
        },
    };
    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = nullptr,
        .create_window = [](ultramodern::gfx_callbacks_t::gfx_data_t) { return ultramodern::renderer::WindowHandle{}; },
        .update_gfx = nullptr,
    };
    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };
    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = [] {},
        .get_input = get_input,
        .set_rumble = [](int, bool) {},
        .get_connected_device_info = get_connected_device_info,
    };
    ultramodern::events::callbacks_t thread_callbacks{ .vi_callback = nullptr, .gfx_init_callback = nullptr };
    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = [](const char* message) { fprintf(stderr, "[headless] %s\n", message); },
    };

    recomp::start(
        {},
        {},
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        thread_callbacks,
        error_handling_callbacks,
        ultramodern::threads::callbacks_t{ .get_game_thread_name = rush2049::game_thread_name }
    );
    return EXIT_SUCCESS;
}
