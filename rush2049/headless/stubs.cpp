// What the game's sources need from the config, the menus and the input layer, which the headless driver doesn't
// have: every option at its default, nobody holding Rewind, and the hi-res font off.

#include <filesystem>

#include "recomp.h"
#include "rush2049_game.h"
#include "util/file.h"
#include "recompinput/profiles.h"

bool rush2049::split_screen_expand() { return true; }
bool rush2049::menu_expand() { return true; }
bool rush2049::rumble_pak_selected() { return false; }
bool rush2049::large_controller_pak() { return true; }
extern "C" void rush2049_hires_font_scan(uint8_t*, uint32_t) {}

std::filesystem::path recompui::file::get_app_folder_path() { return "headless-config"; }
bool recompinput::profiles::get_game_input_held(int, recompinput::GameInput) { return false; }
