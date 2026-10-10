// Functions the executable gives mods (docs/games/rush2049.md, "Mods"). A mod imports these from "*"
// (RECOMP_IMPORT("*", ...)), next to the patches library's exports. recomp_vprintf is a native the patches library
// calls (patches/syms.ld), not an export.

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <string>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/helpers.hpp"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

#include "rush2049_game.h"

namespace recompui {
    void register_ui_exports();
}

namespace {

template <typename T>
void append_formatted(std::string& out, const std::string& spec, T value) {
    int len = snprintf(nullptr, 0, spec.c_str(), value);
    if (len <= 0) {
        return;
    }
    size_t start = out.size();
    out.resize(start + len + 1);
    snprintf(out.data() + start, len + 1, spec.c_str(), value);
    out.resize(start + len);
}

} // namespace

// int recomp_vprintf(const char* fmt, va_list args), for patches/print.c's recomp_printf. Formats one conversion
// at a time with the host's snprintf, taking each argument from the MIPS o32 va_list in RDRAM: 4-byte slots, and
// 8-byte values (double, long long) aligned to 8.
extern "C" void recomp_vprintf(uint8_t* rdram, recomp_context* ctx) {
    gpr fmt = ctx->r4;
    gpr ap = ctx->r5;
    auto next_word = [&]() -> uint32_t {
        uint32_t value = (uint32_t)MEM_W(0, ap);
        ap += 4;
        return value;
    };
    auto next_dword = [&]() -> uint64_t {
        ap = (ap + 7) & ~(gpr)7;
        uint64_t hi = (uint32_t)MEM_W(0, ap);
        uint64_t lo = (uint32_t)MEM_W(4, ap);
        ap += 8;
        return (hi << 32) | lo;
    };
    auto at = [&](size_t i) -> char { return (char)MEM_B(i, fmt); };

    std::string out;
    size_t i = 0;
    char c;
    while ((c = at(i++)) != '\0') {
        if (c != '%') {
            out += c;
            continue;
        }
        std::string spec = "%";
        while ((c = at(i)) != '\0' && strchr("-+ #0", c)) {
            spec += c;
            i++;
        }
        if (at(i) == '*') {
            spec += std::to_string((int32_t)next_word());
            i++;
        }
        while ((c = at(i)) >= '0' && c <= '9') {
            spec += c;
            i++;
        }
        if (at(i) == '.') {
            spec += '.';
            i++;
            if (at(i) == '*') {
                spec += std::to_string((int32_t)next_word());
                i++;
            }
            while ((c = at(i)) >= '0' && c <= '9') {
                spec += c;
                i++;
            }
        }
        // Length: on o32 only ll and j are 64-bit. h and hh go to the host as they are; the rest are 32-bit.
        std::string length;
        bool wide = false;
        while ((c = at(i)) != '\0' && strchr("hljztL", c)) {
            bool ll = c == 'l' && at(i + 1) == 'l';
            if (c == 'h') {
                length += c;
            }
            wide |= ll || c == 'j';
            i += ll ? 2 : 1;
        }
        c = at(i);
        if (c == '\0') {
            break;
        }
        i++;
        switch (c) {
            case 'd': case 'i':
                if (wide) {
                    append_formatted(out, spec + "ll" + c, (long long)next_dword());
                } else {
                    append_formatted(out, spec + length + c, (int)next_word());
                }
                break;
            case 'u': case 'o': case 'x': case 'X':
                if (wide) {
                    append_formatted(out, spec + "ll" + c, (unsigned long long)next_dword());
                } else {
                    append_formatted(out, spec + length + c, (unsigned int)next_word());
                }
                break;
            case 'c':
                append_formatted(out, spec + c, (int)next_word());
                break;
            case 'p':
                append_formatted(out, std::string("0x%08X"), (unsigned int)next_word());
                break;
            case 's': {
                gpr str = (int32_t)next_word();
                std::string s;
                if (str == 0) {
                    s = "(null)";
                } else {
                    for (size_t j = 0; (c = (char)MEM_B(j, str)) != '\0'; j++) {
                        s += c;
                    }
                }
                append_formatted(out, spec + 's', s.c_str());
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
                append_formatted(out, spec + c, std::bit_cast<double>(next_dword()));
                break;
            case 'n':
                next_word();
                break;
            case '%':
                out += '%';
                break;
            default:
                out += spec;
                out += c;
                break;
        }
    }

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
    _return(ctx, (int)out.size());
}

// void recomp_get_window_resolution(u32* width, u32* height)
extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx) {
    int width, height;
    recompui::get_window_size(width, height);
    MEM_W(0, _arg<0, PTR(u32)>(rdram, ctx)) = (u32)width;
    MEM_W(0, _arg<1, PTR(u32)>(rdram, ctx)) = (u32)height;
}

// float recomp_get_target_aspect_ratio(float original): the aspect ratio the game is shown at, from RT64's Aspect
// Ratio option; `original` (4:3 for this game) when that is Original.
extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);
    switch (ultramodern::renderer::get_graphics_config().ar_option) {
        case ultramodern::renderer::AspectRatio::Expand:
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return(ctx, original);
            return;
    }
}

// u32 rush2049_get_game_frame_rate(void): frames a second the game draws at (the Game Frame Rate setting).
extern "C" void rush2049_get_game_frame_rate(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, (u32)rush2049::game_frame_rate());
}

#define REGISTER_FUNC(name) recomp::overlays::register_base_export(#name, name)

void rush2049::register_mod_exports() {
    REGISTER_FUNC(recomp_get_window_resolution);
    REGISTER_FUNC(recomp_get_target_aspect_ratio);
    REGISTER_FUNC(rush2049_get_game_frame_rate);
    // recompui's mod UI API (menus, config pages), as BanjoRecomp and Zelda64Recomp register it.
    recompui::register_ui_exports();
}
