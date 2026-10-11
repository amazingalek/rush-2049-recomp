#ifndef __RUSH2049_AUDIO_RESAMPLER_H__
#define __RUSH2049_AUDIO_RESAMPLER_H__

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rush2049 {
    // Stereo float sample-rate conversion with a 64-tap Kaiser-windowed sinc that keeps its input between calls, so
    // buffers join without a seam and the output keeps the exact rate. SDL 2 has neither: SDL_ConvertAudio pads every
    // buffer with silence, and SDL_AudioStream drops the fraction of a frame each put leaves over.
    class Resampler {
    public:
        void reset(uint32_t in_rate, uint32_t out_rate);
        // Appends the output for `frames` interleaved stereo frames to `out`. It lags the input by 32 frames.
        void process(const float* in, size_t frames, std::vector<float>& out);
        // Plays the input faster by `ppm` parts per million (slower if negative): fewer output frames per input frame,
        // and the pitch up by as much. Kept across reset.
        void set_speed(int32_t ppm);

    private:
        static constexpr uint64_t ppm_scale = 1000000;
        uint32_t in_rate = 0;
        uint32_t out_rate = 0;
        int32_t speed_ppm = 0;
        uint64_t step = 0;          // input advanced per output, in 1/(out_rate * ppm_scale) frames
        std::vector<float> kernel;  // (phases + 1) rows of taps
        std::vector<float> input;   // interleaved; frame 0 is the oldest the next output reads
        uint64_t phase = 0;         // where the next output sits past input frame half - 1, in 1/(out_rate * ppm_scale) frames
    };
}

#endif
