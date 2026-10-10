#pragma once

#include <string>

namespace rush2049 {
    // Looks for the game's ROM in the places people usually keep one (next to the game, roms/, Downloads, Desktop,
    // Documents) and stores the first that matches, as the launcher's Load ROM does. Returns whether one was stored.
    bool find_rom(const std::u8string& game_id, const std::string& internal_name);
}
