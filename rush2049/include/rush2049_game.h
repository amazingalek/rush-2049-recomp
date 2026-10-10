#ifndef RUSH2049_GAME_H
#define RUSH2049_GAME_H

#include <string>

namespace rush2049 {
    inline const std::u8string program_id = u8"Rush2049Recompiled";
    inline const std::string program_name = "San Francisco Rush 2049: Recompiled";

    void register_overlays();
    void init_config();
    // The accessory in every controller: a Controller Pak (saves) or a Rumble Pak (rumble, no saves).
    bool rumble_pak_selected();
    // --dump-dl: write the first display list the game submits after the given number of seconds (src/dl_dump.cpp).
    void start_dl_dump(const char* path, double after_seconds);
}

#endif
