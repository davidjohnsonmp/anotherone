#pragma once
#include "Common.h"

namespace hiccup
{
/** The glitch pattern is a pure function of (seed, step, amount). Raising a knob only ever adds
    events, it never reshuffles the ones already there, and a loop always repeats identically.
    The GUI calls the same functions to draw the pattern on the LCD. */
namespace pattern
{
    enum class Rhythm : uint8_t { none, repeat2, repeat3, repeat4, reverse, jumpBack, tapeStop, drop, gate };
    enum class Pitch  : uint8_t { none, jump, slideUp, slideDown, flutter };

    struct Event
    {
        Rhythm rhythm = Rhythm::none;
        Pitch pitch = Pitch::none;
        float semitones = 0.0f;   // for jump + slides
        int jumpSteps = 1;
    };

    inline float scaleInterval (int scale, float u) noexcept
    {
        static constexpr float octave[] = { -24, -12, -12, 12, 12, 24 };
        static constexpr float fifth[]  = { -12, -7, -5, 5, 7, 12, 19, -19 };
        static constexpr float wild[]   = { -13, -11, -8, -6, -3, -1, 1, 2, 3, 6, 8, 11, 14, 17 };
        switch (scale)
        {
            case 0:  return octave[juce::jlimit (0, 5, (int) (u * 6.0f))];
            case 1:  return fifth[juce::jlimit (0, 7, (int) (u * 8.0f))];
            default: return wild[juce::jlimit (0, 13, (int) (u * 14.0f))];
        }
    }

    inline Event eventAt (uint32_t seed, int step, float rhythmAmount, float pitchAmount, int scale) noexcept
    {
        Event e;
        const auto st = (uint32_t) step;
        const float gateR = rhythmAmount * rhythmAmount * 0.25f + rhythmAmount * 0.55f;
        const float gateP = pitchAmount * pitchAmount * 0.25f + pitchAmount * 0.5f;

        if (hashUnit (seed ^ 0x11111111u, st) < gateR)
        {
            const float t = hashUnit (seed ^ 0x22222222u, st);
            if      (t < 0.26f) e.rhythm = Rhythm::repeat2;
            else if (t < 0.40f) e.rhythm = Rhythm::repeat3;
            else if (t < 0.54f) e.rhythm = Rhythm::repeat4;
            else if (t < 0.66f) e.rhythm = Rhythm::reverse;
            else if (t < 0.80f) e.rhythm = Rhythm::jumpBack;
            else if (t < 0.87f) e.rhythm = Rhythm::tapeStop;
            else if (t < 0.94f) e.rhythm = Rhythm::drop;
            else                e.rhythm = Rhythm::gate;
            static constexpr int jumps[] = { 1, 2, 3, 4, 6, 8 };
            e.jumpSteps = jumps[juce::jlimit (0, 5, (int) (hashUnit (seed ^ 0x33333333u, st) * 6.0f))];
        }
        if (hashUnit (seed ^ 0x44444444u, st) < gateP)
        {
            const float t = hashUnit (seed ^ 0x55555555u, st);
            if      (t < 0.58f) e.pitch = Pitch::jump;
            else if (t < 0.73f) e.pitch = Pitch::slideUp;
            else if (t < 0.88f) e.pitch = Pitch::slideDown;
            else                e.pitch = Pitch::flutter;
            e.semitones = scaleInterval (scale, hashUnit (seed ^ 0x66666666u, st));
        }
        return e;
    }

    inline int stepsInPattern (double gridBeats, double beatsPerBar) noexcept
    {
        return juce::jmax (1, (int) std::llround (2.0 * beatsPerBar / gridBeats));
    }
} // namespace pattern
} // namespace hiccup
