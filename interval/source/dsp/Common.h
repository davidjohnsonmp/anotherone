#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace hiccup
{
/** Musical clock. Follows the host when it is playing and free-runs otherwise, so every effect
    stays on a grid even with the transport stopped. */
struct Clock
{
    double bpm = 120.0, ppq = 0.0, beatsPerSample = 120.0 / 60.0 / 44100.0;
    double beatsPerBar = 4.0;
    bool playing = false;
    double ppqAt (int sampleOffset) const noexcept { return ppq + beatsPerSample * sampleOffset; }
};

/** Small, fast, deterministic PRNG. Used so glitch patterns repeat exactly for a given seed. */
struct FastRandom
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) noexcept { s = v * 2654435761u + 0x9E3779B9u; if (s == 0) s = 1; }
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() noexcept { return (float) next() * (1.0f / 4294967296.0f); }
    float bipolar() noexcept { return unit() * 2.0f - 1.0f; }
};

inline uint32_t hash2 (uint32_t a, uint32_t b) noexcept
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0xBF58476Du;
    h ^= h >> 15; h *= 0x94D049BBu; h ^= h >> 13; h *= 0x1B873593u; h ^= h >> 16;
    return h;
}
inline float hashUnit (uint32_t a, uint32_t b) noexcept { return (float) hash2 (a, b) * (1.0f / 4294967296.0f); }

inline float semisToRatio (float s) noexcept { return std::pow (2.0f, s / 12.0f); }

/** Raised-cosine fade, 0→1. Used everywhere an edge must not click. */
inline float smoothFade (float x) noexcept
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * x);
}

/** Stereo circular buffer addressed by an absolute, ever-increasing sample index. */
class Ring
{
public:
    void prepare (int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        size = n; mask = n - 1;
        data[0].assign ((size_t) n, 0.0f);
        data[1].assign ((size_t) n, 0.0f);
        writeIndex = 0;
    }
    void clear()
    {
        std::fill (data[0].begin(), data[0].end(), 0.0f);
        std::fill (data[1].begin(), data[1].end(), 0.0f);
        writeIndex = 0;
    }
    void write (float l, float r) noexcept
    {
        data[0][(size_t) (writeIndex & mask)] = l;
        data[1][(size_t) (writeIndex & mask)] = r;
        ++writeIndex;
    }
    /** 4-point, 3rd-order Hermite read at a fractional absolute position. */
    void read (double pos, float& l, float& r) const noexcept
    {
        const auto ip = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) ip);
        l = hermite (data[0].data(), ip, f);
        r = hermite (data[1].data(), ip, f);
    }
    int64_t position() const noexcept { return writeIndex; }
    int capacity() const noexcept { return size; }

private:
    float hermite (const float* d, int64_t i, float f) const noexcept
    {
        const float xm = d[(size_t) ((i - 1) & mask)], x0 = d[(size_t) (i & mask)],
                    x1 = d[(size_t) ((i + 1) & mask)], x2 = d[(size_t) ((i + 2) & mask)];
        const float c = 0.5f * (x1 - xm);
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + 0.5f * (x2 - x0);
        const float b = w + a;
        return (((a * f) - b) * f + c) * f + x0;
    }
    std::array<std::vector<float>, 2> data;
    int size = 0, mask = 0;
    int64_t writeIndex = 0;
};
} // namespace hiccup
