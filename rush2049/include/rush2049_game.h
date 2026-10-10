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
    // The "Controller Pak Size" setting: 62 banks (about 1.9 MB) rather than the original 32 KiB.
    bool large_controller_pak();
    // Frames a second the game draws at the "Game Frame Rate" setting: 30, 60, or more with Match Framerate.
    uint32_t game_frame_rate();
    // Whether 2- and 4-player views widen with the window ("Split-Screen Aspect Ratio" at Expand).
    bool split_screen_expand();
    // --dump-dl: write the first display list the game submits after the given number of seconds (src/dl_dump.cpp).
    void start_dl_dump(const char* path, double after_seconds);
    // --dump-ram: write all of RDRAM (big-endian) to <prefix>N.bin at each of the comma-separated times in seconds.
    void start_ram_dump(const char* prefix, const char* times);

    // Hi-res font (src/hires_font.cpp): set up the generated texture pack in the app folder, then turn it on or off.
    void init_hires_font();
    void set_hires_font(bool on);

    // Music replacement (src/music.cpp): mixes the playing replacement file into `frames` frames of float audio
    // at `rate` Hz with `channels` channels.
    void music_mix(float* out, size_t frames, uint32_t channels, uint32_t rate, float main_volume);
    void set_music_replacement(bool enabled);
    void set_music_replacement_volume(float gain);
}

#endif
