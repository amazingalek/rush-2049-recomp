// Music replacement. The game's music is MusyX sequences played through low-rate samples (most instruments play
// back at a quarter to half of the 22050 Hz mix rate), so it sounds muffled however well it is mixed. This plays
// the player's own recordings of the songs instead, mixed in at the output device's rate.
//
// The game controls music by sending commands to its music thread (func_800992AC) through the queue at 0x801427A8.
// A hook where that thread switches on the command type sees each one (rush2049_music_command):
//   type 0   play song +4 (0-11). The thread ignores it while a song is playing.
//   type 1   stop the song.
//   type 10  volume: target (float 0-1) at +4, fade time in seconds (float) at +8, applies to music when the
//            byte at +C is set and to sound effects when the byte at +D is.
// When a replacement file exists for a song, its play command becomes a type the thread doesn't handle, so the
// game's own sequence never starts and the file plays instead. Stop and volume commands drive the file the same
// way they would drive the sequence, so the in-game MUSIC VOLUME setting, fade-outs and track selection all apply.
//
// Files go in a "music" folder next to the executable or in the app data folder. A file belongs to a song when its
// name starts with the song's two-digit number or its name from the game's MUSIC TRACK setting ("00.ogg",
// "bassy.flac", "03 - Trancey.mp3"). OGG Vorbis, FLAC, MP3 and WAV are read. A file loops from the LOOPSTART tag
// (OGG comments, in samples) to LOOPEND or LOOPSTART + LOOPLENGTH when it has them, otherwise the whole file loops.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include "SDL.h"
#include <Windows.h>
#else
#include "SDL2/SDL.h"
#endif

#include "recomp.h"
#include "recompui/config.h"
#include "util/file.h"
#include "rush2049_game.h"
#include "rush2049_hooks.h"

#define STB_VORBIS_HEADER_ONLY
#include "../lib/decoders/stb_vorbis.c"
#define DR_FLAC_IMPLEMENTATION
#include "../lib/decoders/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../lib/decoders/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../lib/decoders/dr_wav.h"

namespace {

constexpr uint32_t current_song_addr = 0x80110240;  // the music thread's playing song, -1 for none
constexpr uint8_t command_ignored = 0xFF;           // a type the music thread doesn't handle

const char* const song_names[] = {
    "bassy", "garage", "night", "trancey", "seventies", "credits",
    "title", "retro", "stunted", "flier", "battle1", "battle2",
};
constexpr int song_count = sizeof(song_names) / sizeof(song_names[0]);

// One decoded stream, read as interleaved float frames.
class Decoder {
public:
    virtual ~Decoder() = default;
    virtual size_t read(float* out, size_t frames) = 0;
    virtual bool seek(uint64_t frame) = 0;
    uint32_t channels = 0;
    uint32_t rate = 0;
    uint64_t loop_start = 0;
    uint64_t loop_end = 0;  // 0: the end of the file
};

class VorbisDecoder : public Decoder {
public:
    stb_vorbis* v = nullptr;
    std::vector<uint8_t> data;
    ~VorbisDecoder() override {
        if (v != nullptr) {
            stb_vorbis_close(v);
        }
    }
    size_t read(float* out, size_t frames) override {
        return (size_t)stb_vorbis_get_samples_float_interleaved(v, (int)channels, out, (int)(frames * channels));
    }
    bool seek(uint64_t frame) override {
        return stb_vorbis_seek(v, (unsigned int)frame) != 0;
    }
};

class FlacDecoder : public Decoder {
public:
    drflac* f = nullptr;
    std::vector<uint8_t> data;
    ~FlacDecoder() override {
        drflac_close(f);
    }
    size_t read(float* out, size_t frames) override {
        return (size_t)drflac_read_pcm_frames_f32(f, frames, out);
    }
    bool seek(uint64_t frame) override {
        return drflac_seek_to_pcm_frame(f, frame) != 0;
    }
};

class Mp3Decoder : public Decoder {
public:
    drmp3 m{};
    std::vector<uint8_t> data;
    ~Mp3Decoder() override {
        drmp3_uninit(&m);
    }
    size_t read(float* out, size_t frames) override {
        return (size_t)drmp3_read_pcm_frames_f32(&m, frames, out);
    }
    bool seek(uint64_t frame) override {
        return drmp3_seek_to_pcm_frame(&m, frame) != 0;
    }
};

class WavDecoder : public Decoder {
public:
    drwav w{};
    std::vector<uint8_t> data;
    ~WavDecoder() override {
        drwav_uninit(&w);
    }
    size_t read(float* out, size_t frames) override {
        return (size_t)drwav_read_pcm_frames_f32(&w, frames, out);
    }
    bool seek(uint64_t frame) override {
        return drwav_seek_to_pcm_frame(&w, frame) != 0;
    }
};

std::string lower(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

bool read_file(const std::filesystem::path& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    out.resize((size_t)f.tellg());
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), (std::streamsize)out.size());
    return (bool)f;
}

std::unique_ptr<Decoder> open_decoder(const std::filesystem::path& path) {
    std::string ext = lower(path.extension().string());
    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        return nullptr;
    }
    if (ext == ".ogg") {
        auto d = std::make_unique<VorbisDecoder>();
        d->data = std::move(data);
        int error = 0;
        d->v = stb_vorbis_open_memory(d->data.data(), (int)d->data.size(), &error, nullptr);
        if (d->v == nullptr) {
            return nullptr;
        }
        stb_vorbis_info info = stb_vorbis_get_info(d->v);
        d->channels = (uint32_t)info.channels;
        d->rate = info.sample_rate;
        stb_vorbis_comment comments = stb_vorbis_get_comment(d->v);
        uint64_t loop_length = 0;
        for (int i = 0; i < comments.comment_list_length; i++) {
            std::string c = comments.comment_list[i];
            size_t eq = c.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            std::string key = lower(c.substr(0, eq));
            uint64_t value = strtoull(c.c_str() + eq + 1, nullptr, 10);
            if (key == "loopstart") {
                d->loop_start = value;
            } else if (key == "loopend") {
                d->loop_end = value;
            } else if (key == "looplength") {
                loop_length = value;
            }
        }
        if (d->loop_end == 0 && loop_length != 0) {
            d->loop_end = d->loop_start + loop_length;
        }
        return d;
    }
    if (ext == ".flac") {
        auto d = std::make_unique<FlacDecoder>();
        d->data = std::move(data);
        d->f = drflac_open_memory(d->data.data(), d->data.size(), nullptr);
        if (d->f == nullptr) {
            return nullptr;
        }
        d->channels = d->f->channels;
        d->rate = d->f->sampleRate;
        return d;
    }
    if (ext == ".mp3") {
        auto d = std::make_unique<Mp3Decoder>();
        d->data = std::move(data);
        if (!drmp3_init_memory(&d->m, d->data.data(), d->data.size(), nullptr)) {
            return nullptr;
        }
        d->channels = d->m.channels;
        d->rate = d->m.sampleRate;
        return d;
    }
    if (ext == ".wav") {
        auto d = std::make_unique<WavDecoder>();
        d->data = std::move(data);
        if (!drwav_init_memory(&d->w, d->data.data(), d->data.size(), nullptr)) {
            return nullptr;
        }
        d->channels = d->w.channels;
        d->rate = d->w.sampleRate;
        return d;
    }
    return nullptr;
}

std::vector<std::filesystem::path> music_folders() {
    std::vector<std::filesystem::path> folders;
#ifdef _WIN32
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (n != 0 && n < MAX_PATH) {
        folders.push_back(std::filesystem::path(exe).parent_path() / "music");
    }
#endif
    std::filesystem::path app = recompui::file::get_app_folder_path();
    if (!app.empty()) {
        folders.push_back(app / "music");
    }
    return folders;
}

// The file for a song, or an empty path.
std::filesystem::path find_song_file(int song) {
    char number[8];
    snprintf(number, sizeof(number), "%02d", song);
    for (const auto& folder : music_folders()) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::string ext = lower(entry.path().extension().string());
            if (ext != ".ogg" && ext != ".flac" && ext != ".mp3" && ext != ".wav") {
                continue;
            }
            std::string stem = lower(entry.path().stem().string());
            bool by_number = stem.compare(0, 2, number) == 0 && (stem.size() == 2 || !std::isdigit((unsigned char)stem[2]));
            size_t name_len = strlen(song_names[song]);
            bool by_name = stem.compare(0, name_len, song_names[song]) == 0 &&
                           (stem.size() == name_len || !std::isalnum((unsigned char)stem[name_len]));
            if (by_number || by_name) {
                return entry.path();
            }
        }
    }
    return {};
}

std::mutex music_mutex;
bool replacement_enabled = true;
float replacement_gain = 1.0f;

// The playing file. Owned under music_mutex.
std::unique_ptr<Decoder> decoder;
int playing_song = -1;
uint64_t decoded_frames = 0;  // position in the file
SDL_AudioStream* resampler = nullptr;
uint32_t resampler_rate = 0;
bool decoder_ended = false;

// The game's music volume: current, target, and change per second while fading.
float volume = 1.0f;
float volume_target = 1.0f;
float volume_rate = 0.0f;

void stop_locked() {
    decoder.reset();
    playing_song = -1;
    if (resampler != nullptr) {
        SDL_FreeAudioStream(resampler);
        resampler = nullptr;
    }
}

// Decodes up to `frames` frames into the resampler, looping at the loop end.
void feed_resampler(size_t frames) {
    std::vector<float> buffer(frames * decoder->channels);
    size_t got = 0;
    int empty_reads = 0;
    while (got < frames) {
        size_t want = frames - got;
        if (decoder->loop_end != 0 && decoded_frames + want > decoder->loop_end) {
            want = (size_t)(decoder->loop_end - std::min(decoder->loop_end, decoded_frames));
        }
        size_t n = want == 0 ? 0 : decoder->read(buffer.data() + got * decoder->channels, want);
        got += n;
        decoded_frames += n;
        if (n < want || want == 0) {
            if (!decoder->seek(decoder->loop_start)) {
                decoder_ended = true;
                break;
            }
            decoded_frames = decoder->loop_start;
            // Nothing decodes from the loop start either.
            if (n == 0 && ++empty_reads > 2) {
                decoder_ended = true;
                break;
            }
        }
        if (n != 0) {
            empty_reads = 0;
        }
    }
    SDL_AudioStreamPut(resampler, buffer.data(), (int)(got * decoder->channels * sizeof(float)));
}

} // namespace

void rush2049::set_music_replacement(bool enabled) {
    std::lock_guard lock{ music_mutex };
    replacement_enabled = enabled;
}

void rush2049::set_music_replacement_volume(float gain) {
    std::lock_guard lock{ music_mutex };
    replacement_gain = gain;
}

void rush2049::music_mix(float* out, size_t frames, uint32_t channels, uint32_t rate, float main_volume) {
    std::lock_guard lock{ music_mutex };
    if (decoder == nullptr || decoder_ended || channels < 2) {
        return;
    }
    if (resampler == nullptr || resampler_rate != rate) {
        if (resampler != nullptr) {
            SDL_FreeAudioStream(resampler);
        }
        resampler = SDL_NewAudioStream(AUDIO_F32, (Uint8)decoder->channels, (int)decoder->rate, AUDIO_F32, 2, (int)rate);
        resampler_rate = rate;
        if (resampler == nullptr) {
            decoder_ended = true;
            return;
        }
    }
    size_t needed_bytes = frames * 2 * sizeof(float);
    while ((size_t)SDL_AudioStreamAvailable(resampler) < needed_bytes && !decoder_ended) {
        feed_resampler(std::max<size_t>(1024, (size_t)((uint64_t)frames * decoder->rate / rate) + 64));
    }
    std::vector<float> music(frames * 2, 0.0f);
    SDL_AudioStreamGet(resampler, music.data(), (int)needed_bytes);

    float step = volume_rate / (float)rate;
    for (size_t i = 0; i < frames; i++) {
        if (volume != volume_target) {
            volume += step;
            if ((step > 0 && volume > volume_target) || (step < 0 && volume < volume_target) || step == 0) {
                volume = volume_target;
            }
        }
        // Squared, as the 0-10 setting is heard roughly as a loudness scale.
        float gain = volume * volume * replacement_gain * main_volume;
        out[i * channels + 0] += music[i * 2 + 0] * gain;
        out[i * channels + 1] += music[i * 2 + 1] * gain;
    }
}

extern "C" void rush2049_music_command(uint8_t* rdram, uint32_t command) {
    gpr cmd = (gpr)(int32_t)command;
    uint8_t type = MEM_BU(2, cmd);
    if (type == 0) {
        int song = (int)MEM_W(4, cmd);
        bool enabled;
        bool streaming;
        {
            std::lock_guard lock{ music_mutex };
            enabled = replacement_enabled;
            streaming = decoder != nullptr;
        }
        if (streaming) {
            // A song is playing: the music thread ignores play commands until it stops.
            MEM_B(2, cmd) = (int8_t)command_ignored;
            return;
        }
        if (!enabled || song < 0 || song >= song_count || (int32_t)MEM_W(0, (gpr)(int32_t)current_song_addr) >= 0) {
            return;
        }
        std::filesystem::path path = find_song_file(song);
        if (path.empty()) {
            return;
        }
        std::unique_ptr<Decoder> d = open_decoder(path);
        if (d == nullptr || d->channels == 0 || d->rate == 0) {
            fprintf(stderr, "[music] can't read %s\n", path.string().c_str());
            return;
        }
        fprintf(stderr, "[music] song %d (%s): %s, %u Hz, %u channels\n", song, song_names[song],
                path.string().c_str(), d->rate, d->channels);
        std::lock_guard lock{ music_mutex };
        stop_locked();
        decoder = std::move(d);
        playing_song = song;
        decoded_frames = 0;
        decoder_ended = false;
        MEM_B(2, cmd) = (int8_t)command_ignored;
    } else if (type == 1) {
        std::lock_guard lock{ music_mutex };
        stop_locked();
    } else if (type == 10 && MEM_BU(0xC, cmd) != 0) {
        uint32_t w4 = (uint32_t)MEM_W(4, cmd);
        uint32_t w8 = (uint32_t)MEM_W(8, cmd);
        float target, fade;
        memcpy(&target, &w4, 4);
        memcpy(&fade, &w8, 4);
        target = std::clamp(target, 0.0f, 1.0f);
        std::lock_guard lock{ music_mutex };
        volume_target = target;
        if (fade > 0.0f) {
            volume_rate = (volume_target - volume) / fade;
        } else {
            volume = volume_target;
            volume_rate = 0.0f;
        }
    }
}
