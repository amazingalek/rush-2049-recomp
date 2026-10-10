// Config tabs: the stock tabs, plus the controller accessory option on the General tab.

#include <filesystem>

#include "rush2049_game.h"
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
}

bool rush2049::rumble_pak_selected() {
    auto value = recompui::config::get_general_config().get_option_value(accessory_option);
    return static_cast<Accessory>(std::get<uint32_t>(value)) == Accessory::RumblePak;
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

    recompui::config::create_graphics_tab();

    recompui::config::create_controls_tab();

    recompui::config::create_sound_tab();

    recompui::config::create_mods_tab();

    recompui::config::finalize();
}
