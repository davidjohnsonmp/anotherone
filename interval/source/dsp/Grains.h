#pragma once
#include "GlitchPattern.h"

namespace hiccup
{
/** Granular cloud fed by the glitch chain. Grain length, density, scatter and pitch are all
    randomised around the knob values. CLOUD plays forward, REVERSE plays backward, FREEZE stops
    recording so the cloud hangs on the last moment of audio. */
class Grains
{
public:
    enum Mode { cloud = 0, reversed = 1, freeze = 2 };
    struct Params
    {
        float amount = 0.0f, sizeMs = 80.0f, spray = 0.3f, pitch = 0.0f;
        int scale = 0, mode = cloud;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        ring.prepare ((int) (sr * 6.0));
        for (int i = 0; i < windowSize + 1; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) windowSize);
        reset();
    }
    void reset()
    {
        ring.clear();
        for (auto& g : grains) g.alive = false;
        countdown = 0.0;
        spawned.store (0);
    }

    /** Replaces L/R with live*cos + cloud*sin, equal power in the amount knob. */
    void process (float* L, float* R, int n, const Params& p) noexcept
    {
        const bool frozen = p.mode == freeze && p.amount > 0.001f;
        const float density = p.amount <= 0.001f ? 0.0f : 5.0f + 60.0f * p.amount * p.amount;
        const float sizeSamples = p.sizeMs * 0.001f * (float) sampleRate;
        const float overlap = density * p.sizeMs * 0.001f;
        const float norm = 1.0f / std::sqrt (juce::jmax (1.0f, overlap * 0.5f));

        for (int i = 0; i < n; ++i)
        {
            if (! frozen) ring.write (L[i], R[i]);

            if (density > 0.0f)
            {
                countdown -= 1.0;
                if (countdown <= 0.0)
                {
                    spawn (p, sizeSamples);
                    const float jitter = 0.45f + rng.unit() * 1.1f;
                    countdown = sampleRate / density * jitter;
                }
            }

            float cl = 0.0f, cr = 0.0f;
            for (auto& g : grains)
            {
                if (! g.alive) continue;
                float x, y;
                ring.read (g.pos, x, y);
                const float phase = (float) (g.age / g.length) * (float) windowSize;
                const int wi = juce::jlimit (0, windowSize - 1, (int) phase);
                const float frac = phase - (float) wi;
                const float w = window[(size_t) wi] + (window[(size_t) wi + 1] - window[(size_t) wi]) * frac;
                const float s = (x * g.gl + y * g.gr);   // grains are panned by weighting the source channels
                cl += (x * g.gl * g.widthKeep + s * (1.0f - g.widthKeep) * 0.5f) * w * g.gain;
                cr += (y * g.gr * g.widthKeep + s * (1.0f - g.widthKeep) * 0.5f) * w * g.gain;
                g.pos += g.rate;
                g.age += 1.0;
                if (g.age >= g.length) g.alive = false;
            }

            ampTarget = p.amount;
            amp += (ampTarget - amp) * ampCoef();
            const float dry = std::cos (amp * juce::MathConstants<float>::halfPi);
            const float wet = std::sin (amp * juce::MathConstants<float>::halfPi);
            L[i] = L[i] * dry + cl * norm * wet;
            R[i] = R[i] * dry + cr * norm * wet;
        }
    }

    std::atomic<uint32_t> spawned { 0 };

private:
    static constexpr int maxGrains = 64, windowSize = 1024;
    struct Grain
    {
        bool alive = false;
        double pos = 0.0, rate = 1.0, age = 0.0, length = 1.0;
        float gl = 1.0f, gr = 1.0f, gain = 1.0f, widthKeep = 1.0f;
    };

    float ampCoef() const noexcept { return 1.0f - std::exp (-1.0f / (float) (0.02 * sampleRate)); }

    void spawn (const Params& p, float sizeSamples) noexcept
    {
        Grain* free = nullptr;
        for (auto& g : grains) if (! g.alive) { free = &g; break; }
        if (free == nullptr) return;

        auto& g = *free;
        const float lengthScale = 0.72f + rng.unit() * 0.56f;
        g.length = juce::jmax (64.0, (double) (sizeSamples * lengthScale));

        float semis = 0.0f;
        if (rng.unit() < p.pitch * 0.85f)
            semis = pattern::scaleInterval (p.scale, rng.unit());
        const double baseRate = (double) semisToRatio (semis + rng.bipolar() * 0.06f);
        g.rate = p.mode == reversed ? -baseRate : baseRate;

        const double travel = g.length * baseRate;
        const double sprayBack = (double) (p.spray * p.spray) * 2.2 * sampleRate * (double) rng.unit();
        const double newest = (double) ring.position() - 8.0;
        double start = g.rate > 0.0 ? newest - travel - sprayBack
                                    : newest - sprayBack;
        const double oldest = (double) ring.position() - (double) ring.capacity() + 8192.0;
        start = juce::jmax (start, oldest + (g.rate < 0.0 ? travel : 0.0));
        g.pos = start;
        g.age = 0.0;

        const float pan = rng.bipolar() * (0.25f + 0.7f * p.spray);
        const float th = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        g.gl = std::cos (th) * 1.414f;
        g.gr = std::sin (th) * 1.414f;
        g.gain = 0.72f + rng.unit() * 0.28f;
        g.widthKeep = 0.6f;
        g.alive = true;
        spawned.fetch_add (1, std::memory_order_relaxed);
    }

    double sampleRate = 44100.0, countdown = 0.0;
    Ring ring;
    std::array<Grain, maxGrains> grains;
    std::array<float, windowSize + 1> window {};
    FastRandom rng;
    float amp = 0.0f, ampTarget = 0.0f;
};
} // namespace hiccup
