#pragma once
#include "Common.h"
#include <signalsmith-stretch.h>

namespace hiccup
{
/** Transpose + two-voice harmonizer, built on Signalsmith Stretch (spectral, polyphonic-safe).
    The stage has a constant latency that is reported to the host, and the dry path is delayed to
    match, so nothing ever jumps in time when the pedal or its mix change. */
class PitchStage
{
public:
    struct Params
    {
        bool engaged = false;
        float transpose = 0.0f, mix = 1.0f, blend = 0.0f;
        float voiceA = 7.0f, voiceB = 12.0f;
        bool voiceAOn = true, voiceBOn = true;
    };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::ignoreUnused (maxBlock);
        const int blockSamples = (int) (sr * 0.072), interval = (int) (sr * 0.018);
        for (auto& v : voices)
        {
            v.stretch.configure (2, blockSamples, interval, true);
            v.out[0].assign (chunkSize, 0.0f);
            v.out[1].assign (chunkSize, 0.0f);
        }
        latency = voices[0].stretch.inputLatency() + voices[0].stretch.outputLatency();
        delay[0].assign ((size_t) latency + chunkSize + 8, 0.0f);
        delay[1].assign ((size_t) latency + chunkSize + 8, 0.0f);
        tempL.assign (chunkSize, 0.0f);
        tempR.assign (chunkSize, 0.0f);
        reset();
    }
    void reset()
    {
        for (auto& v : voices) { v.stretch.reset(); v.primed = 0; v.running = false; v.gain = 0.0f; v.semis = -1000.0f; }
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        delayPos = 0;
        wet = 0.0f;
        shiftBlend = 0.0f;
    }
    int getLatencySamples() const noexcept { return latency; }

    void process (float* L, float* R, int n, const Params& p) noexcept
    {
        int done = 0;
        while (done < n)
        {
            const int c = juce::jmin (n - done, chunkSize);
            processChunk (L + done, R + done, c, p);
            done += c;
        }
    }

private:
    static constexpr int chunkSize = 128;
    struct Voice
    {
        signalsmith::stretch::SignalsmithStretch<float> stretch { 12345 };
        std::array<std::vector<float>, 2> out;
        int primed = 0;
        bool running = false;
        float gain = 0.0f, semis = -1000.0f;
    };

    void runVoice (Voice& v, bool wanted, float semisTarget, const float* const* in, int n) noexcept
    {
        if (! wanted && v.gain < 1.0e-4f)
        {
            if (v.running) { v.stretch.reset(); v.running = false; v.primed = 0; v.semis = -1000.0f; }
            v.gain = 0.0f;
            std::fill_n (v.out[0].data(), n, 0.0f);
            std::fill_n (v.out[1].data(), n, 0.0f);
            return;
        }
        v.running = true;
        // glide between settings instead of stepping
        if (v.semis < -500.0f) v.semis = semisTarget;
        const float coef = 1.0f - std::exp (-(float) n / (float) (0.05 * sampleRate));
        v.semis += (semisTarget - v.semis) * coef;
        v.stretch.setTransposeSemitones (v.semis);

        float* outs[2] = { v.out[0].data(), v.out[1].data() };
        v.stretch.process (in, n, outs, n);

        // a freshly started voice stays silent until its first real output has arrived
        const bool ready = v.primed >= latency;
        v.primed = juce::jmin (latency, v.primed + n);
        const float target = (wanted && ready) ? 1.0f : 0.0f;
        const float step = (float) n / (float) (0.02 * sampleRate);
        const float g0 = v.gain;
        v.gain += juce::jlimit (-step, step, target - v.gain);
        for (int i = 0; i < n; ++i)
        {
            const float g = g0 + (v.gain - g0) * (float) (i + 1) / (float) n;
            v.out[0][(size_t) i] *= g;
            v.out[1][(size_t) i] *= g;
        }
    }

    void processChunk (float* L, float* R, int n, const Params& p) noexcept
    {
        // constant-latency dry path
        const int dlen = (int) delay[0].size();
        for (int i = 0; i < n; ++i)
        {
            const int rd = (delayPos + dlen - latency) % dlen;
            delay[0][(size_t) delayPos] = L[i];
            delay[1][(size_t) delayPos] = R[i];
            tempL[(size_t) i] = delay[0][(size_t) rd];
            tempR[(size_t) i] = delay[1][(size_t) rd];
            delayPos = (delayPos + 1) % dlen;
        }

        const float wetTarget = p.engaged ? p.mix : 0.0f;
        const float* in[2] = { L, R };
        const bool on = p.engaged;
        const bool shifted = on && std::abs (p.transpose) > 0.001f;
        runVoice (voices[0], on, p.transpose, in, n);
        runVoice (voices[1], on && p.blend > 0.001f && p.voiceAOn, p.transpose + p.voiceA, in, n);
        runVoice (voices[2], on && p.blend > 0.001f && p.voiceBOn, p.transpose + p.voiceB, in, n);

        const float wetStep = (float) n / (float) (0.008 * sampleRate);
        const float shiftStep = (float) n / (float) (0.03 * sampleRate);
        const float w0 = wet, s0 = shiftBlend;
        wet += juce::jlimit (-wetStep, wetStep, wetTarget - wet);
        const float shiftTarget = (shifted && voices[0].gain > 0.99f) ? 1.0f : 0.0f;
        shiftBlend += juce::jlimit (-shiftStep, shiftStep, shiftTarget - shiftBlend);

        constexpr float pan = 0.42f;
        const float harm = p.blend * 0.8f;
        const float voiceCount = (p.voiceAOn ? 1.0f : 0.0f) + (p.voiceBOn ? 1.0f : 0.0f);
        const float harmNorm = voiceCount > 1.0f ? 0.8f : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float w = w0 + (wet - w0) * t;
            const float s = s0 + (shiftBlend - s0) * t;
            const float dl = tempL[(size_t) i], dr = tempR[(size_t) i];
            // at 0 semitones the "main voice" is the delayed dry itself, so it stays bit-clean
            const float ml = dl + (voices[0].out[0][(size_t) i] - dl) * smoothFade (s);
            const float mr = dr + (voices[0].out[1][(size_t) i] - dr) * smoothFade (s);
            const float al = voices[1].out[0][(size_t) i], ar = voices[1].out[1][(size_t) i];
            const float bl = voices[2].out[0][(size_t) i], br = voices[2].out[1][(size_t) i];
            const float hl = (al * (1.0f + pan) + bl * (1.0f - pan)) * harm * harmNorm * 0.7f;
            const float hr = (ar * (1.0f - pan) + br * (1.0f + pan)) * harm * harmNorm * 0.7f;
            const float wl = ml + hl, wr = mr + hr;
            L[i] = dl * (1.0f - w) + wl * w;
            R[i] = dr * (1.0f - w) + wr * w;
        }
    }

    double sampleRate = 44100.0;
    std::array<Voice, 3> voices;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> tempL, tempR;
    int delayPos = 0, latency = 0;
    float wet = 0.0f, shiftBlend = 0.0f;
};
} // namespace hiccup
