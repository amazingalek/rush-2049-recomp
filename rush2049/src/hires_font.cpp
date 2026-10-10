// Hi-res font. Every font in the game (menus, race HUD, messages) is a 4-bit intensity atlas, 96 texels wide and
// about 8-13 texels per glyph, drawn as texture rectangles, so at window resolution the text is blocky. RT64 can
// replace a texture with a bigger one from a texture pack, keyed by a hash of what the texture loads into TMEM.
// The fonts come from the user's ROM, so no pack can ship in this repo. Instead the game makes its own: before
// each graphics task starts, the display list is walked with the same TMEM loads RT64 does, and every intensity
// texture a texture rectangle draws is hashed the way RT64 hashes it. A hash not seen before is upscaled 4x and
// written to hires_font/ in the app folder, which is registered with RT64 as a built-in pack below any texture
// packs the user enables. Only the first frame that draws a new font shows the original; after that the pack
// already has it, across restarts too.
//
// The atlases are anti-aliased (rasterised from a vector font), so the upscale is a smooth bicubic followed by a
// moderate contrast curve around half the atlas's brightest value: edges come out clean without closing the
// one-texel gaps inside glyphs, which a hard threshold or a pixel-art scaler does.

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "recomp.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"
#include "recompui/renderer.h"
#include "util/file.h"

#include "xxHash/xxh3.h"
#include "shared/rt64_f3d_defines.h"
#include "common/rt64_load_types.h"
#include "common/rt64_tmem_hasher.h"

#define F3DEX_GBI_2
#include "../lib/rt64/include/rt64_extended_gbi.h"

namespace {

// Bump when the upscale changes, so textures made by an older version are thrown away.
const char* const pack_version = "v1";
constexpr int scale = 4;
constexpr float contrast = 2.5f;

constexpr uint32_t tmem_bytes = 4096;
constexpr uint32_t fmt_rgba = 0, fmt_i = 4;
constexpr uint32_t siz_4b = 0, siz_8b = 1, siz_32b = 3;

// On by default: the option's change callback doesn't run on a first launch, before there is a saved config.
std::atomic<bool> enabled{ true };
std::filesystem::path pack_dir;
// Every hash already handled: in the pack, or drawn but not wanted (empty).
std::unordered_set<uint64_t> seen;
std::vector<uint64_t> in_pack;

struct Rdp {
    uint8_t* rdram;
    uint32_t segments[16]{};
    struct {
        uint32_t address = 0, fmt = 0, siz = 0, width = 0;
    } timg;
    RT64::LoadTile tiles[8]{};
    uint8_t tmem[tmem_bytes]{};
    // Which TMEM bytes hold a known texture: written by a load block. Other loads (tile, TLUT) aren't emulated,
    // so they mark what they may have overwritten as unknown, and rectangles drawing from there are skipped.
    bool tmem_known[tmem_bytes]{};
    std::vector<uint64_t> found;

    uint32_t phys(uint32_t addr) const {
        return (segments[(addr >> 24) & 0xF] + (addr & 0x00FFFFFF)) & 0x007FFFFF;
    }
    uint32_t rd32(uint32_t p) const { return *reinterpret_cast<uint32_t*>(rdram + (p & 0x007FFFFC)); }
    uint8_t rd8(uint32_t p) const { return rdram[(p & 0x007FFFFF) ^ 3]; }

    // RT64's RDP::loadBlockOperation / loadToTMEMCommon for everything but 32-bit RGBA.
    void load_block(uint32_t t, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t dxt) {
        RT64::LoadTile& tile = tiles[t];
        tile.uls = uls;
        tile.ult = ult;
        tile.lrs = lrs;
        tile.lrt = dxt;
        if (uls > lrs || lrs >= 0x800) {
            return;
        }
        if (tile.siz == siz_32b && tile.fmt == fmt_rgba) {
            std::fill(std::begin(tmem_known), std::end(tmem_known), false);
            return;
        }
        uint32_t bytes_per_row = timg.width << timg.siz >> 1;
        uint32_t src = timg.address + (uls << timg.siz >> 1) + bytes_per_row * ult;
        uint32_t words = ((lrs - uls) >> (4 - tile.siz)) + 1;
        uint32_t dst = (tile.tmem << 3) & (tmem_bytes - 1);
        uint32_t stride = tile.line << 3;
        uint32_t xor_mask = 0, dxt_counter = 0;
        for (uint32_t w = 0; w < words && w < tmem_bytes / 8; w++) {
            for (uint32_t i = 0; i < 8; i++) {
                uint32_t a = ((dst + i) ^ xor_mask) & (tmem_bytes - 1);
                tmem[a] = rd8(src + i);
                tmem_known[a] = true;
            }
            dxt_counter += dxt;
            while (dxt_counter >= 0x800) {
                dst = (dst + stride) & (tmem_bytes - 1);
                dxt_counter -= 0x800;
                xor_mask ^= 4;
            }
            src += 8;
            dst = (dst + 8) & (tmem_bytes - 1);
        }
    }

    // RT64's sample size (State::updateDrawCall...) and hash for a rectangle drawn with tile `t`.
    void texture_rect(uint32_t t) {
        const RT64::LoadTile& tile = tiles[t];
        if (tile.fmt != fmt_i || (tile.siz != siz_4b && tile.siz != siz_8b) || tile.line == 0) {
            return;
        }
        bool clamp_s = tile.masks == 0 || (tile.cms & 2);
        bool clamp_t = tile.maskt == 0 || (tile.cmt & 2);
        int tile_w = clamp_s ? std::max((tile.lrs - tile.uls + 4) / 4, 1) : 0xFFFF;
        int tile_h = clamp_t ? std::max((tile.lrt - tile.ult + 4) / 4, 1) : 0xFFFF;
        int mask_w = tile.masks > 0 ? 1 << tile.masks : 0xFFFF;
        int mask_h = tile.maskt > 0 ? 1 << tile.maskt : 0xFFFF;
        uint16_t width = (uint16_t)std::min(tile_w, mask_w);
        uint16_t height = (uint16_t)std::min(tile_h, mask_h);
        if (width > 1024 || height > 1024 || RT64::TMEMHasher::requiresRawTMEM(tile, width, height, 0)) {
            return;
        }
        uint32_t row_bytes = tile.line << 3;
        uint32_t start = tile.tmem << 3;
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < (uint32_t)((width << tile.siz) + 1) / 2; x++) {
                if (!tmem_known[(start + y * row_bytes + x) & (tmem_bytes - 1)]) {
                    return;
                }
            }
        }
        uint64_t hash = RT64::TMEMHasher::hash(tmem, tile, width, height, 0, RT64::TMEMHasher::CurrentHashVersion);
        if (seen.insert(hash).second) {
            make_texture(hash, tile, width, height);
        }
    }

    void make_texture(uint64_t hash, const RT64::LoadTile& tile, uint32_t width, uint32_t height);

    void walk(uint32_t addr, int depth) {
        if (depth > 12) {
            return;
        }
        uint32_t p = phys(addr);
        for (int n = 0; n < 50000; n++, p += 8) {
            uint32_t w0 = rd32(p), w1 = rd32(p + 4);
            switch (w0 >> 24) {
            case 0xDF:  // G_ENDDL
                return;
            case 0xDE:  // G_DL
                if (w0 & 0x00010000) {  // branch
                    p = phys(w1) - 8;
                } else {
                    walk(w1, depth + 1);
                }
                break;
            case 0xDB:  // G_MOVEWORD
                if (((w0 >> 16) & 0xFF) == 0x06) {
                    segments[((w0 & 0xFFFF) / 4) & 0xF] = w1 & 0x00FFFFFF;
                }
                break;
            case 0xFD:  // G_SETTIMG
                timg.fmt = (w0 >> 21) & 7;
                timg.siz = (w0 >> 19) & 3;
                timg.width = (w0 & 0xFFF) + 1;
                timg.address = phys(w1);
                break;
            case 0xF5: {  // G_SETTILE
                RT64::LoadTile& tile = tiles[(w1 >> 24) & 7];
                tile.fmt = (w0 >> 21) & 7;
                tile.siz = (w0 >> 19) & 3;
                tile.line = (w0 >> 9) & 0x1FF;
                tile.tmem = w0 & 0x1FF;
                tile.palette = (w1 >> 20) & 0xF;
                tile.cmt = (w1 >> 18) & 3;
                tile.maskt = (w1 >> 14) & 0xF;
                tile.shiftt = (w1 >> 10) & 0xF;
                tile.cms = (w1 >> 8) & 3;
                tile.masks = (w1 >> 4) & 0xF;
                tile.shifts = w1 & 0xF;
                break;
            }
            case 0xF2: {  // G_SETTILESIZE
                RT64::LoadTile& tile = tiles[(w1 >> 24) & 7];
                tile.uls = (w0 >> 12) & 0xFFF;
                tile.ult = w0 & 0xFFF;
                tile.lrs = (w1 >> 12) & 0xFFF;
                tile.lrt = w1 & 0xFFF;
                break;
            }
            case 0xF3:  // G_LOADBLOCK
                load_block((w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF);
                break;
            case 0xF4:  // G_LOADTILE
            case 0xF0:  // G_LOADTLUT
                std::fill(std::begin(tmem_known), std::end(tmem_known), false);
                break;
            case 0xE4:  // G_TEXRECT
            case 0xE5:  // G_TEXRECTFLIP
                texture_rect((w1 >> 24) & 7);
                p += 16;
                break;
            case RT64_EXTENDED_OPCODE:
                if ((w0 & 0xFFFFFF) == G_EX_TEXRECT_V1) {  // what the widescreen HUD turns race rectangles into
                    texture_rect(w1 & 7);
                    p += 16;
                }
                break;
            }
        }
    }
};

float cubic(float x) {
    // Catmull-Rom.
    x = std::fabs(x);
    if (x < 1.0f) {
        return 1.5f * x * x * x - 2.5f * x * x + 1.0f;
    }
    if (x < 2.0f) {
        return -0.5f * x * x * x + 2.5f * x * x - 4.0f * x + 2.0f;
    }
    return 0.0f;
}

// Uncompressed RGBA8 DDS with a full mip chain, so RT64 can filter the text when it is drawn smaller.
bool write_dds(const std::filesystem::path& path, std::vector<uint8_t> rgba, uint32_t width, uint32_t height) {
    uint32_t levels = 1;
    while ((width >> levels) > 0 || (height >> levels) > 0) {
        levels++;
    }
    uint32_t header[32]{};
    header[0] = 0x20534444;  // "DDS "
    header[1] = 124;
    header[2] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000;  // caps, height, width, pitch, pixel format, mip count
    header[3] = height;
    header[4] = width;
    header[5] = width * 4;
    header[7] = levels;
    header[19] = 32;
    header[20] = 0x1 | 0x40;  // alpha, RGB
    header[22] = 32;
    header[23] = 0x000000FF;
    header[24] = 0x0000FF00;
    header[25] = 0x00FF0000;
    header[26] = 0xFF000000;
    header[27] = 0x1000 | 0x400000 | 0x8;  // texture, mipmap, complex
    FILE* f = fopen(path.string().c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    fwrite(header, sizeof(header), 1, f);
    uint32_t w = width, h = height;
    for (uint32_t level = 0; level < levels; level++) {
        fwrite(rgba.data(), 1, (size_t)w * h * 4, f);
        uint32_t nw = std::max(w / 2, 1u), nh = std::max(h / 2, 1u);
        std::vector<uint8_t> next((size_t)nw * nh * 4);
        for (uint32_t y = 0; y < nh; y++) {
            for (uint32_t x = 0; x < nw; x++) {
                for (uint32_t c = 0; c < 4; c++) {
                    uint32_t x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
                    uint32_t y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
                    uint32_t sum = rgba[(y0 * w + x0) * 4 + c] + rgba[(y0 * w + x1) * 4 + c] +
                                   rgba[(y1 * w + x0) * 4 + c] + rgba[(y1 * w + x1) * 4 + c];
                    next[(y * nw + x) * 4 + c] = (uint8_t)((sum + 2) / 4);
                }
            }
        }
        rgba = std::move(next);
        w = nw;
        h = nh;
    }
    bool ok = ferror(f) == 0;
    fclose(f);
    return ok;
}

void write_database() {
    std::string json = "{\n  \"configuration\": { \"autoPath\": \"rt64\", \"configurationVersion\": 3, \"hashVersion\": " +
                       std::to_string(RT64::TMEMHasher::CurrentHashVersion) +
                       ", \"defaultOperation\": \"preload\", \"defaultShift\": \"none\" },\n  \"textures\": [";
    for (size_t i = 0; i < in_pack.size(); i++) {
        char entry[96];
        snprintf(entry, sizeof(entry), "%s\n    { \"hashes\": { \"rt64\": \"%016" PRIx64 "\" }, \"path\": \"\" }",
                 i == 0 ? "" : ",", in_pack[i]);
        json += entry;
    }
    json += "\n  ]\n}\n";
    std::filesystem::path tmp = pack_dir / "rt64.json.tmp";
    FILE* f = fopen(tmp.string().c_str(), "wb");
    if (f == nullptr) {
        return;
    }
    fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    std::error_code ec;
    std::filesystem::rename(tmp, pack_dir / "rt64.json", ec);
}

void Rdp::make_texture(uint64_t hash, const RT64::LoadTile& tile, uint32_t width, uint32_t height) {
    // The texels, read back the way the RDP samples them: odd rows have their 32-bit halves swapped in TMEM.
    std::vector<float> src((size_t)width * height);
    float peak = 0.0f;
    uint32_t row_bytes = tile.line << 3;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t a = (tile.tmem << 3) + y * row_bytes + (tile.siz == siz_4b ? x / 2 : x);
            if (y & 1) {
                a ^= 4;
            }
            uint8_t b = tmem[a & (tmem_bytes - 1)];
            float v = tile.siz == siz_4b ? ((x & 1) ? (b & 0xF) : (b >> 4)) / 15.0f : b / 255.0f;
            src[y * width + x] = v;
            peak = std::max(peak, v);
        }
    }
    if (peak == 0.0f) {
        return;
    }

    uint32_t out_w = width * scale, out_h = height * scale;
    std::vector<uint8_t> rgba((size_t)out_w * out_h * 4);
    auto texel = [&](int x, int y) {
        x = std::clamp(x, 0, (int)width - 1);
        y = std::clamp(y, 0, (int)height - 1);
        return src[y * width + x];
    };
    for (uint32_t oy = 0; oy < out_h; oy++) {
        float sy = (oy + 0.5f) / scale - 0.5f;
        int y0 = (int)std::floor(sy);
        for (uint32_t ox = 0; ox < out_w; ox++) {
            float sx = (ox + 0.5f) / scale - 0.5f;
            int x0 = (int)std::floor(sx);
            float v = 0.0f;
            for (int dy = -1; dy <= 2; dy++) {
                float wy = cubic(sy - (y0 + dy));
                for (int dx = -1; dx <= 2; dx++) {
                    v += wy * cubic(sx - (x0 + dx)) * texel(x0 + dx, y0 + dy);
                }
            }
            v = std::clamp((v / peak - 0.5f) * contrast + 0.5f, 0.0f, 1.0f) * peak;
            uint8_t i = (uint8_t)std::lround(v * 255.0f);
            uint8_t* o = &rgba[((size_t)oy * out_w + ox) * 4];
            o[0] = o[1] = o[2] = o[3] = i;
        }
    }

    char name[32];
    snprintf(name, sizeof(name), "%016" PRIx64 ".dds", hash);
    if (!write_dds(pack_dir / name, std::move(rgba), out_w, out_h)) {
        fprintf(stderr, "[hires font] can't write %s\n", (pack_dir / name).string().c_str());
        return;
    }
    in_pack.push_back(hash);
    found.push_back(hash);
}

void register_pack() {
    recompui::renderer::set_builtin_texture_packs(enabled ? std::vector<std::filesystem::path>{ pack_dir }
                                                          : std::vector<std::filesystem::path>{});
}

} // namespace

void rush2049::init_hires_font() {
    std::filesystem::path root = recompui::file::get_app_folder_path() / "hires_font";
    pack_dir = root / pack_version;
    std::error_code ec;
    // Textures from an older upscale.
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (entry.path().filename() != pack_version) {
            std::filesystem::remove_all(entry.path(), ec);
        }
    }
    std::filesystem::create_directories(pack_dir, ec);
    for (const auto& entry : std::filesystem::directory_iterator(pack_dir, ec)) {
        if (entry.path().extension() == ".dds") {
            uint64_t hash = std::strtoull(entry.path().stem().string().c_str(), nullptr, 16);
            seen.insert(hash);
            in_pack.push_back(hash);
        }
    }
    write_database();
    register_pack();
}

void rush2049::set_hires_font(bool on) {
    enabled = on;
    register_pack();
}

extern "C" void rush2049_hires_font_scan(uint8_t* rdram, uint32_t task_addr) {
    if (!enabled.load(std::memory_order_relaxed) || pack_dir.empty()) {
        return;
    }
    gpr task = (gpr)(int32_t)task_addr;
    if (MEM_W(0x00, task) != 1) {  // M_GFXTASK
        return;
    }
    static Rdp rdp;
    rdp.rdram = rdram;
    rdp.found.clear();
    std::fill(std::begin(rdp.tmem_known), std::end(rdp.tmem_known), false);
    rdp.walk((uint32_t)MEM_W(0x30, task), 0);
    if (!rdp.found.empty()) {
        write_database();
        register_pack();
        fprintf(stderr, "[hires font] %zu new texture(s), %zu in the pack\n", rdp.found.size(), in_pack.size());
    }
}
