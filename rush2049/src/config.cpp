// Config tabs: the stock tabs, plus the controller accessory option on the General tab and the game frame rate
// and hi-res font on the Graphics tab, and the music replacement options on the Sound tab.

#include <algorithm>
#include <atomic>
#include <filesystem>

#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "librecomp/config.hpp"
#include "recompinput/input_mapping.h"
#include "recompui/config.h"
#include "util/file.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    enum class Accessory {
        ControllerPak,
        RumblePak,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> accessory_options = {
        {Accessory::ControllerPak, "Controller Pak"},
        {Accessory::RumblePak, "Rumble Pak"},
    };
    const std::string accessory_option = "accessory";

    enum class GameFrameRate {
        Original,
        Sixty,
        MatchFramerate,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> game_frame_rate_options = {
        {GameFrameRate::Original, "Original", "30 FPS (Original)"},
        {GameFrameRate::Sixty, "60", "60 FPS"},
        {GameFrameRate::MatchFramerate, "MatchFramerate", "Match Framerate"},
    };
    const std::string game_frame_rate_option = "game_frame_rate";
    // Read by the scheduler thread on every retrace, so it is cached rather than looked up in the config.
    std::atomic<GameFrameRate> game_frame_rate_setting{ GameFrameRate::Sixty };
    // Frames a second the game draws, and the VI rate it was set up for (at least 60, the console's).
    std::atomic<uint32_t> game_fps{ 60 };
    uint32_t vi_rate = 60;

    // Match Framerate follows the Framerate option above it: Original is 60, Display the monitor's refresh rate,
    // Manual its value. Never below 60.
    uint32_t wanted_fps() {
        switch (game_frame_rate_setting.load(std::memory_order_relaxed)) {
            case GameFrameRate::Original:
                return 30;
            case GameFrameRate::Sixty:
                return 60;
            default:
                return std::max<uint32_t>(60, ultramodern::get_target_framerate(60));
        }
    }

    enum class HiresFont {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hires_font_options = {
        {HiresFont::Off, "Off", "Off"},
        {HiresFont::On, "On", "On"},
    };
    const std::string hires_font_option = "hires_font";

    enum class MusicReplacement {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> music_replacement_options = {
        {MusicReplacement::Off, "Off", "Off"},
        {MusicReplacement::On, "On", "On"},
    };
    const std::string music_replacement_option = "music_replacement";
    const std::string music_replacement_volume_option = "music_replacement_volume";
}

uint32_t rush2049::game_frame_rate() {
    return game_fps.load(std::memory_order_relaxed);
}

bool rush2049::rumble_pak_selected() {
    auto value = recompui::config::get_general_config().get_option_value(accessory_option);
    return static_cast<Accessory>(std::get<uint32_t>(value)) == Accessory::RumblePak;
}

extern "C" gpr rush2049_frame_pace(gpr retraces) {
    // Retraces since the last swap; the game swaps (and starts the next task with a swap pending) when it is at
    // least 2. From 60 fps up the VI runs at the frame rate, so one retrace is enough.
    if (game_fps.load(std::memory_order_relaxed) >= 60 && retraces == 1) {
        return 2;
    }
    return retraces;
}

// On every retrace, before the scheduler sends it on to its clients ($t8 = the first client, $t7 = the scheduler).
// Above 60 fps the VI runs at the frame rate. The main thread waits for a retrace before each frame, so it gets
// every one, but the others (controller, audio and music threads) still get 60 a second: the music sequencer counts
// them, so more would play it faster. The hook sends the retraces itself, the way the VI thread sends interrupts, and
// empties $t8 so the game's own loop sends none.
extern "C" void rush2049_retrace_clients(uint8_t* rdram, recomp_context* ctx) {
    constexpr uint32_t main_thread_queue = 0x8002ECF8;  // the client func_80002238 adds
    uint32_t fps = wanted_fps();
    if (fps != game_fps.load(std::memory_order_relaxed)) {
        game_fps = fps;
        vi_rate = std::max<uint32_t>(fps, 60);
        ultramodern::set_vi_rate(vi_rate);
        fprintf(stderr, "[frame rate] %u fps, VI %u Hz (display %u Hz)\n", fps, vi_rate,
                ultramodern::get_display_refresh_rate());
    }
    if (vi_rate <= 60) {
        return;
    }
    uint64_t n = (uint32_t)MEM_W(0x27C, ctx->r15);
    bool tick_60hz = n * 60 / vi_rate != (n - 1) * 60 / vi_rate;
    // Client: +0 next, +4 queue. The message is the scheduler's address, as the game's loop sends.
    for (gpr client = ctx->r24; client != 0; client = MEM_W(0x0, client)) {
        uint32_t queue = (uint32_t)MEM_W(0x4, client);
        if (tick_60hz || queue == main_thread_queue) {
            ultramodern::enqueue_external_message((int32_t)queue, (int32_t)ctx->r15, false, false);
        }
    }
    ctx->r24 = 0;
}

// Keyboard defaults for driving. The stock defaults put the C buttons on the arrow keys, where a PC player
// reaching for Up/Left would hit Abort (C-Up) and Reverse (C-Left). Here the arrows are a second stick (steering
// and menus, like WASD), gas and brake stay on Space and Left Shift, and the C buttons move to a T/F/G/H diamond.
// Only defaults: saved bindings and the game's own CONTROLS screen still apply.
static void set_keyboard_defaults() {
    using recompinput::GameInput;
    using recompinput::InputField;
    recompinput::set_default_mapping_for_keyboard(GameInput::X_AXIS_NEG,
        { InputField::keyboard(SDL_SCANCODE_A), InputField::keyboard(SDL_SCANCODE_LEFT) });
    recompinput::set_default_mapping_for_keyboard(GameInput::X_AXIS_POS,
        { InputField::keyboard(SDL_SCANCODE_D), InputField::keyboard(SDL_SCANCODE_RIGHT) });
    recompinput::set_default_mapping_for_keyboard(GameInput::Y_AXIS_POS,
        { InputField::keyboard(SDL_SCANCODE_W), InputField::keyboard(SDL_SCANCODE_UP) });
    recompinput::set_default_mapping_for_keyboard(GameInput::Y_AXIS_NEG,
        { InputField::keyboard(SDL_SCANCODE_S), InputField::keyboard(SDL_SCANCODE_DOWN) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_UP, { InputField::keyboard(SDL_SCANCODE_T) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_LEFT, { InputField::keyboard(SDL_SCANCODE_F) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_DOWN, { InputField::keyboard(SDL_SCANCODE_G) });
    recompinput::set_default_mapping_for_keyboard(GameInput::C_RIGHT, { InputField::keyboard(SDL_SCANCODE_H) });
}

void rush2049::init_config() {
    set_keyboard_defaults();

    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    general.add_enum_option(
        accessory_option,
        "Controller Accessory",
        "What is plugged into each controller. A controller has one accessory slot, so this is a choice between saving "
        "and rumble."
        "<br/>"
        "<b>Controller Pak:</b> saves players, records and ghosts. "
        "<b>Rumble Pak:</b> rumble, but the game can't save.",
        accessory_options,
        Accessory::ControllerPak
    );

    recomp::config::Config& graphics = recompui::config::create_graphics_tab();
    graphics.add_enum_option(
        game_frame_rate_option,
        "Game Frame Rate",
        "How often the game draws a new frame. The original draws on every other screen refresh, 30 times a second. "
        "Car physics and timers follow real time, so a race runs at the same speed at any rate."
        "<br/>"
        "<b>Match Framerate:</b> a new frame for every frame <recomp-color primary>Framerate</recomp-color> above "
        "presents, so Display draws at the monitor's refresh rate and Manual at its value (60 at least).",
        game_frame_rate_options,
        GameFrameRate::Sixty
    );
    // Called on load too, and while the menu previews a change.
    graphics.add_option_change_callback(game_frame_rate_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            game_frame_rate_setting = static_cast<GameFrameRate>(std::get<uint32_t>(cur_value));
        });

    rush2049::init_hires_font();
    graphics.add_enum_option(
        hires_font_option,
        "Hi-res Font",
        "Draws the menu and HUD text from smooth, 4x upscaled copies of the game's fonts instead of the blocky "
        "originals. The copies are made from your ROM as each font first appears, and kept in the app folder."
        "<br/>"
        "Texture packs you enable take priority.",
        hires_font_options,
        HiresFont::On
    );
    graphics.add_option_change_callback(hires_font_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_hires_font(static_cast<HiresFont>(std::get<uint32_t>(cur_value)) == HiresFont::On);
        });

    recompui::config::create_controls_tab();

    recomp::config::Config& sound = recompui::config::create_sound_tab();
    sound.add_enum_option(
        music_replacement_option,
        "Replacement Music",
        "Plays your own recordings of the game's songs instead of its music. Put them in a <b>music</b> folder next "
        "to the game, named by song number or name: <b>00.ogg</b>, <b>bassy.flac</b>, <b>03 - Trancey.mp3</b>. "
        "OGG, FLAC, MP3 and WAV are read. Songs with no file keep the game's own music."
        "<br/>"
        "Takes effect from the next song.",
        music_replacement_options,
        MusicReplacement::On
    );
    sound.add_number_option(
        music_replacement_volume_option,
        "Replacement Music Volume",
        "The volume of the replacement music, on top of the game's own MUSIC VOLUME setting.",
        0, 200, 1, 0, true, 100
    );
    // Called on load too.
    sound.add_option_change_callback(music_replacement_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_music_replacement(static_cast<MusicReplacement>(std::get<uint32_t>(cur_value)) == MusicReplacement::On);
        });
    sound.add_option_change_callback(music_replacement_volume_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_music_replacement_volume(static_cast<float>(std::get<double>(cur_value) / 100.0));
        });

    recompui::config::create_mods_tab();

    recompui::config::finalize();
}
