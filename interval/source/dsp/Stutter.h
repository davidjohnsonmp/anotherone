#pragma once
#include "Common.h"

namespace hiccup
{
/** Tempo-locked stutter.
    Every `interval` the effect fires a hit. A cycle of `repeats` hits starts with the live signal
    being recorded; the remaining hits replay that slice. Each hit is shaped by the release envelope
    (release length), so short releases chop and long releases sustain. Never adds latency. */
class Stutter
{
public:
    struct Params
    {
        bool engaged = true, reverse = false;
        float mix = 1.0f;
        double intervalBeats = 0.25;
        int repeats = 4;                // 0 = hold the first slice for as long as the pedal is on
        float releaseMs = 300.0f;       // >= holdThresholdMs means no decay at all
    };
    static constexpr float holdThresholdMs = 1990.0f;

    void prepare (double sr)
    {
        sampleRate = sr;
        const int maxSamples = (int) std::ceil (sr * 8.0) + 8192;
        slice[0].assign ((size_t) maxSamples, 0.0f);
        slice[1].assign ((size_t) maxSamples, 0.0f);
        crossfadeLen = juce::jmax (16, (int) (sr * 0.0035));
        reset();
    }
    void reset()
    {
        lastIndex = std::numeric_limits<int64_t>::min();
        lastInterval = -1.0;
        hasSlice = false; capturePos = 0; captureTarget = 0; sliceLen = 0;
        active = {}; fading = {};
        wet = 0.0f;
        hitCounter.store (0);
    }

    void process (float* L, float* R, int n, const Clock& clock, const Params& p) noexcept
    {
        const float wetTarget = p.engaged ? p.mix : 0.0f;
        const float wetSlew = 1.0f / (float) (0.006 * sampleRate);
        if (! p.engaged && wet < 1.0e-4f)
        {
            wet = 0.0f;
            lastIndex = std::numeric_limits<int64_t>::min();
            hasSlice = false;
            return;
        }
        const double releaseSamples = (double) p.releaseMs * 0.001 * sampleRate;
        const bool noDecay = p.releaseMs >= holdThresholdMs;

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];

            const auto index = (int64_t) std::floor (clock.ppqAt (i) / p.intervalBeats + 1.0e-7);
            if (index != lastIndex)
            {
                onHit (index, clock, p);
                lastIndex = index;
                lastInterval = p.intervalBeats;
            }

            // keep recording the current slice (plus a little extra so repeats can crossfade out)
            if (capturePos < captureTarget)
            {
                slice[0][(size_t) capturePos] = inL;
                slice[1][(size_t) capturePos] = inR;
                ++capturePos;
            }

            float wetL = 0.0f, wetR = 0.0f;
            renderHead (active, inL, inR, wetL, wetR, releaseSamples, noDecay, true);
            if (fading.alive)
                renderHead (fading, inL, inR, wetL, wetR, releaseSamples, noDecay, false);

            wet += juce::jlimit (-wetSlew, wetSlew, wetTarget - wet);
            L[i] = inL * (1.0f - wet) + wetL * wet;
            R[i] = inR * (1.0f - wet) + wetR * wet;
        }
    }

    /** Raises every time a hit fires. Read by the GUI to animate. */
    std::atomic<uint32_t> hitCounter { 0 };

private:
    struct Head
    {
        bool alive = false, live = true, reversed = false;
        int64_t pos = 0, age = 0;
        int fadeIn = 0, fadeOut = 0;     // remaining samples of each fade
    };

    void onHit (int64_t index, const Clock& clock, const Params& p) noexcept
    {
        const bool continuous = (index == lastIndex + 1) && std::abs (lastInterval - p.intervalBeats) < 1.0e-9;
        const auto wanted = (int64_t) std::llround (p.intervalBeats / clock.beatsPerSample);
        const int maxLen = (int) slice[0].size() - crossfadeLen - 2;

        bool capture;
        if (p.repeats <= 0)
            capture = ! hasSlice || ! continuous;              // hold: freeze the first slice
        else
            capture = ! hasSlice || ! continuous || (index % p.repeats) == 0;

        fading = active;
        fading.fadeIn = 0;
        fading.fadeOut = crossfadeLen;

        Head next;
        next.alive = true;
        next.fadeIn = crossfadeLen;
        next.age = 0;

        if (capture)
        {
            sliceLen = (int) juce::jlimit<int64_t> (32, maxLen, wanted);
            capturePos = 0;
            captureTarget = sliceLen + crossfadeLen;
            hasSlice = true;
            next.live = true;     // first pass is the live input, which is being recorded right now
        }
        else
        {
            next.live = false;
            // only reverse once the slice is fully recorded (matters right after the pedal is engaged)
            next.reversed = p.reverse && capturePos >= sliceLen;
            next.pos = next.reversed ? (int64_t) sliceLen - 1 : 0;
        }
        active = next;
        hitCounter.fetch_add (1, std::memory_order_relaxed);
    }

    inline void renderHead (Head& h, float inL, float inR, float& outL, float& outR,
                            double releaseSamples, bool noDecay, bool isActive) noexcept
    {
        if (! h.alive) return;
        float a, b;
        if (h.live) { a = inL; b = inR; }
        else
        {
            int64_t idx = h.pos;
            if (idx < 0) idx = -idx;                                   // reversed head mirrors at the slice start
            if (idx >= (int64_t) slice[0].size()) idx = (int64_t) slice[0].size() - 1;
            a = slice[0][(size_t) idx];
            b = slice[1][(size_t) idx];
            h.pos += h.reversed ? -1 : 1;
        }
        float g = 1.0f;
        if (! noDecay)
        {
            const double t = (double) h.age / juce::jmax (1.0, releaseSamples);
            g = t >= 1.0 ? 0.0f : 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) t);
        }
        if (h.fadeIn > 0)
        {
            g *= 1.0f - (float) h.fadeIn / (float) crossfadeLen;
            --h.fadeIn;
        }
        if (! isActive)
        {
            g *= (float) h.fadeOut / (float) crossfadeLen;
            if (--h.fadeOut <= 0) h.alive = false;
        }
        outL += a * g;
        outR += b * g;
        ++h.age;
    }

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> slice;
    int sliceLen = 0, capturePos = 0, captureTarget = 0, crossfadeLen = 128;
    int64_t lastIndex = std::numeric_limits<int64_t>::min();
    double lastInterval = -1.0;
    bool hasSlice = false;
    Head active, fading;
    float wet = 0.0f;
};
} // namespace hiccup
