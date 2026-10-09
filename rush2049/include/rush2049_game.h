#ifndef RUSH2049_GAME_H
#define RUSH2049_GAME_H

#include <string>

namespace rush2049 {
    inline const std::u8string program_id = u8"Rush2049Recompiled";
    inline const std::string program_name = "San Francisco Rush 2049: Recompiled";

    void register_overlays();
    void init_config();
}

#endif
