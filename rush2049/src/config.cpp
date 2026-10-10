// Config tabs: the stock tabs, plus the controller accessory option on the General tab.

#include <filesystem>

#include "rush2049_game.h"
#include "librecomp/config.hpp"
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

void rush2049::init_config() {
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
