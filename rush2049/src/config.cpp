// Config tabs: the stock tabs, plus the controller accessory, Controller Pak size and rewind options on the General
// tab, the game frame rate, hi-res font and split-screen aspect ratio on the Graphics tab, and MusyX mixing and the
// music replacement options on the Sound tab.

#include <algorithm>
#include <atomic>
#include <filesystem>

#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "musyx_hle.h"
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

    enum class PakSize {
        Original,
        Large,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> pak_size_options = {
        {PakSize::Original, "Original", "Original (32 KiB)"},
        {PakSize::Large, "Large", "Large (1.9 MB)"},
    };
    const std::string pak_size_option = "pak_size";

    enum class Rewind {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> rewind_options = {
        {Rewind::Off, "Off", "Off"},
        {Rewind::On, "On", "On"},
    };
    const std::string rewind_option = "rewind";

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

    enum class HiresTitle {
        Original,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> hires_title_options = {
        {HiresTitle::Original, "Original", "Original"},
        {HiresTitle::On, "On", "On"},
    };
    const std::string hires_title_option = "hires_title";

    enum class SplitScreenRatio {
        Expand,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> split_screen_ratio_options = {
        {SplitScreenRatio::Expand, "Expand", "Expand"},
        {SplitScreenRatio::Original, "Original", "Original"},
    };
    const std::string split_screen_ratio_option = "split_screen_ratio";
    // Read on the game thread with every display list.
    std::atomic<SplitScreenRatio> split_screen_ratio_setting{ SplitScreenRatio::Expand };

    enum class MenuRatio {
        Expand,
        Original,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> menu_ratio_options = {
        {MenuRatio::Expand, "Expand", "Expand"},
        {MenuRatio::Original, "Original", "Original"},
    };
    const std::string menu_ratio_option = "menu_ratio";
    // Read on the game thread with every display list.
    std::atomic<MenuRatio> menu_ratio_setting{ MenuRatio::Expand };

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

    enum class MusyxMix {
        Original,
        HighQuality,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> musyx_mix_options = {
        {MusyxMix::Original, "Original", "Original"},
        {MusyxMix::HighQuality, "HighQuality", "High Quality"},
    };
    const std::string musyx_mix_option = "musyx_mix";
    const std::string music_replacement_volume_option = "music_replacement_volume";
}

uint32_t rush2049::game_frame_rate() {
    return game_fps.load(std::memory_order_relaxed);
}

bool rush2049::split_screen_expand() {
    return split_screen_ratio_setting.load(std::memory_order_relaxed) == SplitScreenRatio::Expand;
}

bool rush2049::menu_expand() {
    return menu_ratio_setting.load(std::memory_order_relaxed) == MenuRatio::Expand;
}

bool rush2049::rumble_pak_selected() {
    auto value = recompui::config::get_general_config().get_option_value(accessory_option);
    return static_cast<Accessory>(std::get<uint32_t>(value)) == Accessory::RumblePak;
}

bool rush2049::large_controller_pak() {
    auto value = recompui::config::get_general_config().get_option_value(pak_size_option);
    return static_cast<PakSize>(std::get<uint32_t>(value)) == PakSize::Large;
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

// Gamepad defaults for driving on a modern pad (Xbox, PlayStation, Switch Pro; any pad SDL knows as a game
// controller). The stock defaults are an N64 layout: gas and brake on the A and X face buttons, the triggers on
// R (shift up) and Z, and shift down on RB. Here the triggers are gas and brake, the bumpers shift, view moves
// to Y and Z to B. The face buttons keep gas and brake for menus, and the right stick stays the C buttons.
// Only defaults: saved bindings and the game's own CONTROLS screen still apply.
static void set_controller_defaults() {
    using recompinput::GameInput;
    using recompinput::InputField;
    recompinput::set_default_mapping_for_controller(GameInput::A, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true),
        InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_SOUTH) });
    recompinput::set_default_mapping_for_controller(GameInput::B, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true),
        InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_WEST) });
    recompinput::set_default_mapping_for_controller(GameInput::R,
        { InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) });
    recompinput::set_default_mapping_for_controller(GameInput::C_DOWN, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true),
        InputField::controller_digital(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) });
    recompinput::set_default_mapping_for_controller(GameInput::L,
        { InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_NORTH) });
    recompinput::set_default_mapping_for_controller(GameInput::C_LEFT,
        { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false) });
    recompinput::set_default_mapping_for_controller(GameInput::Z,
        { InputField::controller_digital(recompinput::SDL_CONTROLLER_BUTTON_EAST) });
    recompinput::set_default_mapping_for_controller(GameInput::C_RIGHT,
        { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true) });
}

void rush2049::init_config() {
    set_keyboard_defaults();
    set_controller_defaults();

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
    general.add_enum_option(
        pak_size_option,
        "Controller Pak Size",
        "How much each Controller Pak holds. A player profile takes 9 of a pak's pages, and a pak holds 16 files "
        "whatever its size."
        "<br/>"
        "<b>Original:</b> Nintendo's 32 KiB pak, 123 pages, about 13 profiles. "
        "<b>Large:</b> 62 banks, 7748 pages, the most libultra can address, so it never fills up. "
        "Emulators that only take 32 KiB paks can't read it."
        "<br/>"
        "Saves are moved to the new size the next time the game starts. A pak whose saves don't fit in 32 KiB "
        "stays large.",
        pak_size_options,
        PakSize::Large
    );
    general.add_enum_option(
        rewind_option,
        "Rewind",
        "Hold <recomp-color primary>Rewind</recomp-color> (Controls tab) in a single-player race and the race runs "
        "backwards: every car, the clock, the laps and the ghost. Let go to drive on from there. Goes back up to about "
        "a minute, and uses up to 128 MB of memory for it.",
        rewind_options,
        Rewind::On
    );
    // Called on load too.
    general.add_option_change_callback(rewind_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_rewind(static_cast<Rewind>(std::get<uint32_t>(cur_value)) == Rewind::On);
        });

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

    graphics.add_enum_option(
        hires_title_option,
        "Hi-res Title Screen",
        "Draws the title screen from a 4x upscale of its picture (made with Real-ESRGAN) instead of the 240p original. "
        "The game rebuilds the upscale from its own picture and a patch that ships with it."
        "<br/>"
        "Texture packs you enable take priority.",
        hires_title_options,
        HiresTitle::On
    );
    graphics.add_option_change_callback(hires_title_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_hires_title(static_cast<HiresTitle>(std::get<uint32_t>(cur_value)) == HiresTitle::On);
        });

    graphics.add_enum_option(
        split_screen_ratio_option,
        "Split-Screen Aspect Ratio",
        "How the 2- and 4-player views fit a wide window. The original game splits a 4:3 screen, so at 16:9 the "
        "views sit inside a 4:3 area with bars at the sides."
        "<br/>"
        "<b>Expand:</b> each view widens to fill its share of the window, and its HUD moves with it. Needs "
        "<recomp-color primary>Aspect Ratio</recomp-color> at Expand and "
        "<recomp-color primary>HUD Placement</recomp-color> at something other than Original.",
        split_screen_ratio_options,
        SplitScreenRatio::Expand
    );
    graphics.add_option_change_callback(split_screen_ratio_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            split_screen_ratio_setting = static_cast<SplitScreenRatio>(std::get<uint32_t>(cur_value));
        });

    graphics.add_enum_option(
        menu_ratio_option,
        "Menu Aspect Ratio",
        "How the menus fit a wide window. The original game draws them on a 4:3 picture, so at 16:9 there are bars "
        "at the sides."
        "<br/>"
        "<b>Expand:</b> the menu stays where it is and the background picture is mirrored out to the window's edges "
        "(up to about 21:9). Needs <recomp-color primary>Aspect Ratio</recomp-color> at Expand.",
        menu_ratio_options,
        MenuRatio::Expand
    );
    graphics.add_option_change_callback(menu_ratio_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            menu_ratio_setting = static_cast<MenuRatio>(std::get<uint32_t>(cur_value));
        });

    recompui::config::create_controls_tab();

    recomp::config::Config& sound = recompui::config::create_sound_tab();
    sound.add_enum_option(
        musyx_mix_option,
        "MusyX Mixing",
        "<b>High Quality</b> mixes the game's music and sound effects at 44.1 kHz with a sharper interpolator, so "
        "sounds played above their recorded pitch keep their treble and loud passages don't clip. "
        "<b>Original</b> plays the game's own 22 kHz mix.",
        musyx_mix_options,
        MusyxMix::HighQuality
    );
    // Called on load too.
    sound.add_option_change_callback(musyx_mix_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            rush2049::set_musyx_hires(static_cast<MusyxMix>(std::get<uint32_t>(cur_value)) == MusyxMix::HighQuality);
        });
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
