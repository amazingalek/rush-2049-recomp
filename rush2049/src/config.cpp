// Config tabs: the stock tabs, plus the controller accessory option on the General tab and the game frame rate
// and hi-res font on the Graphics tab.

#include <atomic>
#include <filesystem>

#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "librecomp/config.hpp"
#include "recompinput/input_mapping.h"
#include "recompui/config.h"
#include "util/file.h"

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
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> game_frame_rate_options = {
        {GameFrameRate::Original, "Original", "30 FPS (Original)"},
        {GameFrameRate::Sixty, "60", "60 FPS"},
    };
    const std::string game_frame_rate_option = "game_frame_rate";
    // Read by the scheduler thread on every swap check, so it is cached rather than looked up in the config.
    std::atomic<bool> sixty_fps{ true };

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
}

bool rush2049::rumble_pak_selected() {
    auto value = recompui::config::get_general_config().get_option_value(accessory_option);
    return static_cast<Accessory>(std::get<uint32_t>(value)) == Accessory::RumblePak;
}

extern "C" void rush2049_frame_pace(recomp_context* ctx) {
    // $t3 = retraces since the last swap; the game swaps when it is at least 2.
    if (sixty_fps.load(std::memory_order_relaxed) && ctx->r11 == 1) {
        ctx->r11 = 2;
    }
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
        "Car physics and timers follow real time, so a race runs at the same speed at either rate."
        "<br/>"
        "Separate from <recomp-color primary>Framerate</recomp-color> above, which sets how often frames are presented.",
        game_frame_rate_options,
        GameFrameRate::Sixty
    );
    // Called on load too, and while the menu previews a change.
    graphics.add_option_change_callback(game_frame_rate_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            sixty_fps = static_cast<GameFrameRate>(std::get<uint32_t>(cur_value)) == GameFrameRate::Sixty;
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

    recompui::config::create_sound_tab();

    recompui::config::create_mods_tab();

    recompui::config::finalize();
}
