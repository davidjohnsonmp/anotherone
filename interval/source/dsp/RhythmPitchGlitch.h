#pragma once
#include "GlitchPattern.h"

namespace hiccup
{
/** Rhythm glitch + pitch glitch. Steps along a 2-bar seeded pattern on the chosen grid, and on each
    event reads the recent past differently: ratchets, reversals, jump-backs, tape stops, drops and
    gates for rhythm; jumps, slides and flutter for pitch. No event = live signal, no latency. */
class RhythmPitchGlitch
{
public:
    struct Params
    {
        float rhythm = 0.0f, pitch = 0.0f;
        double gridBeats = 0.25;
        int scale = 0;
        uint32_t seed = 1;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        ring.prepare ((int) (sr * 12.0));
        xfadeLen = juce::jmax (16, (int) (sr * 0.003));
        reset();
    }
    void reset()
    {
        ring.clear();
        lastStep = std::numeric_limits<int64_t>::min();
        a = {}; b = {};
        a.live = true;
        currentStep.store (0);
    }

    /** Writes into `L/R` in place. Always records, so engaging later has history to read. */
    void process (float* L, float* R, int n, const Clock& clock, const Params& p, bool active) noexcept
    {
        patternLength.store (pattern::stepsInPattern (p.gridBeats, clock.beatsPerBar), std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            ring.write (L[i], R[i]);
            if (! active) continue;

            const double ppq = clock.ppqAt (i);
            const auto step = (int64_t) std::floor (ppq / p.gridBeats + 1.0e-7);
            if (step != lastStep)
            {
                beginStep (step, clock, p);
                lastStep = step;
            }
            advanceStep (clock);

            float outL = 0.0f, outR = 0.0f;
            renderHead (a, outL, outR, true);
            if (b.alive) renderHead (b, outL, outR, false);

            // gate / drop shaping across the step, slew limited so no edge can click
            const float slew = 1.0f / (float) (0.002 * sampleRate);
            shapedGain += juce::jlimit (-slew, slew, shapeGain() - shapedGain);
            L[i] = outL * shapedGain;
            R[i] = outR * shapedGain;
        }
    }

    std::atomic<int> currentStep { 0 }, patternLength { 32 };

private:
    struct Head
    {
        bool alive = true, live = true;
        double pos = 0.0, rate = 1.0;
        int fade = 0;
    };

    void jumpTo (double pos, double rate, bool live) noexcept
    {
        b = a;
        b.fade = xfadeLen;
        a.alive = true; a.live = live; a.pos = pos; a.rate = rate; a.fade = xfadeLen;
    }

    void beginStep (int64_t step, const Clock& clock, const Params& p) noexcept
    {
        const int len = pattern::stepsInPattern (p.gridBeats, clock.beatsPerBar);
        const int inPattern = (int) (((step % len) + len) % len);
        currentStep.store (inPattern, std::memory_order_relaxed);

        ev = pattern::eventAt (p.seed, inPattern, p.rhythm, p.pitch, p.scale);
        stepLen = p.gridBeats / clock.beatsPerSample;
        stepAge = 0.0;
        stepStart = (double) ring.position() - 1.0;
        gateMode = 0; subRetrigger = 0; subCount = 0;
        rate0 = rate1 = 1.0;

        if (ev.pitch == pattern::Pitch::jump)
            rate0 = rate1 = semisToRatio (ev.semitones);
        else if (ev.pitch == pattern::Pitch::slideUp)
        { rate0 = 1.0; rate1 = semisToRatio (std::abs (ev.semitones)); }
        else if (ev.pitch == pattern::Pitch::slideDown)
        { rate0 = 1.0; rate1 = semisToRatio (-std::abs (ev.semitones)); }
        else if (ev.pitch == pattern::Pitch::flutter)
        { rate0 = rate1 = 1.0; flutterHigh = semisToRatio (ev.semitones); subCount = 4; }

        using R = pattern::Rhythm;
        switch (ev.rhythm)
        {
            case R::repeat2: subCount = 2; break;
            case R::repeat3: subCount = 3; break;
            case R::repeat4: subCount = 4; break;
            case R::tapeStop: rate1 = 0.02; rate0 = juce::jmin (rate0, 1.0); break;
            case R::drop: gateMode = 1; break;
            case R::gate: gateMode = 2; break;
            default: break;
        }

        const bool isLive = ev.rhythm == R::none && ev.pitch == pattern::Pitch::none;
        if (isLive || ev.rhythm == R::drop || ev.rhythm == R::gate)
        {
            if (! (rate0 != 1.0 || rate1 != 1.0))
            {
                if (! a.live) jumpTo (0.0, 1.0, true);
                return;
            }
        }
        startRead (0.0);
    }

    /** Works out where to read so that a head running above unity speed can never overtake the writer. */
    void startRead (double subIndex) noexcept
    {
        using R = pattern::Rhythm;
        const double maxRate = juce::jmax (rate0, rate1, ev.pitch == pattern::Pitch::flutter ? (double) flutterHigh : 1.0);
        const double dur = subCount > 0 ? stepLen / subCount : stepLen;
        const double need = (juce::jmax (1.0, maxRate)) * dur + 64.0;
        double start, rate = rate0;

        switch (ev.rhythm)
        {
            case R::reverse:
                start = stepStart - 8.0;
                rate = -juce::jmax (0.25, rate0);
                break;
            case R::jumpBack:
                start = stepStart - stepLen * ev.jumpSteps - juce::jmax (0.0, maxRate - 1.0) * dur;
                break;
            case R::repeat2: case R::repeat3: case R::repeat4:
                start = stepStart - need;
                break;
            default:
                // pitch only: read just far enough back to keep clear of the writer
                start = maxRate > 1.0 ? stepStart - (maxRate - 1.0) * dur - 64.0 : stepStart;
                break;
        }
        if (ev.pitch == pattern::Pitch::flutter && ((int) subIndex % 2) == 1)
            rate = flutterHigh;

        const double oldest = (double) ring.position() - (double) ring.capacity() + 4096.0;
        jumpTo (juce::jmax (start, oldest), rate, false);
    }

    void advanceStep (const Clock&) noexcept
    {
        if (! a.live)
        {
            const double t = juce::jlimit (0.0, 1.0, stepAge / juce::jmax (1.0, stepLen));
            if (ev.rhythm == pattern::Rhythm::tapeStop)
                a.rate = rate0 + (rate1 - rate0) * std::pow (t, 0.7);
            else if (ev.pitch == pattern::Pitch::slideUp || ev.pitch == pattern::Pitch::slideDown)
            {
                const double r = rate0 * std::pow (rate1 / rate0, t);
                a.rate = ev.rhythm == pattern::Rhythm::reverse ? -r : r;
            }
            if (subCount > 0)
            {
                const auto sub = (int) std::floor (t * subCount);
                if (sub > subRetrigger && sub < subCount)
                {
                    subRetrigger = sub;
                    startRead ((double) sub);
                }
            }
        }
        stepAge += 1.0;
    }

    float shapeGain() noexcept
    {
        if (gateMode == 0) return 1.0f;
        const double t = stepAge / juce::jmax (1.0, stepLen);
        const auto fadeT = (float) (0.004 * sampleRate / juce::jmax (1.0, stepLen));
        if (gateMode == 1)   // drop: dip to silence for the middle of the step
        {
            const auto x = (float) t;
            return 1.0f - smoothFade (x / fadeT) * smoothFade ((1.0f - x) / fadeT);
        }
        const double pulses = 3.0;                       // gate: three short pulses
        const auto ph = (float) (t * pulses - std::floor (t * pulses));
        return smoothFade (ph / (fadeT * (float) pulses)) * smoothFade ((0.55f - ph) / (fadeT * (float) pulses));
    }

    inline void renderHead (Head& h, float& outL, float& outR, bool isActive) noexcept
    {
        float x, y;
        if (h.live)
            ring.read ((double) ring.position() - 1.0, x, y);
        else
        {
            ring.read (h.pos, x, y);
            h.pos += h.rate;
            const double newest = (double) ring.position() - 4.0;
            if (h.pos > newest) h.pos = newest;
        }
        float g = 1.0f;
        if (h.fade > 0)
        {
            const float t = (float) h.fade / (float) xfadeLen;
            g = isActive ? 1.0f - t : t;
            --h.fade;
            if (! isActive && h.fade <= 0) h.alive = false;
        }
        outL += x * g;
        outR += y * g;
    }

    double sampleRate = 44100.0;
    Ring ring;
    Head a, b;
    pattern::Event ev;
    int64_t lastStep = std::numeric_limits<int64_t>::min();
    double stepLen = 1.0, stepAge = 0.0, stepStart = 0.0, rate0 = 1.0, rate1 = 1.0;
    float flutterHigh = 2.0f, shapedGain = 1.0f;
    int subCount = 0, subRetrigger = 0, gateMode = 0, xfadeLen = 128;
};
} // namespace hiccup
