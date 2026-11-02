#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace hiccup::ids
{
    inline const juce::String stutterOn { "stutter_on" }, stutterMix { "stutter_mix" }, stutterInterval { "stutter_interval" },
        stutterRelease { "stutter_release" }, stutterRepeats { "stutter_repeats" }, stutterReverse { "stutter_reverse" };
    inline const juce::String glitchOn { "glitch_on" }, glitchMix { "glitch_mix" }, glitchRhythm { "glitch_rhythm" }, glitchPitch { "glitch_pitch" },
        glitchGrains { "glitch_grains" }, glitchSize { "glitch_size" }, glitchSpray { "glitch_spray" }, glitchGrid { "glitch_grid" },
        glitchScale { "glitch_scale" }, glitchMode { "glitch_mode" }, glitchSeed { "glitch_seed" };
    inline const juce::String pitchOn { "pitch_on" }, pitchMix { "pitch_mix" }, pitchTranspose { "pitch_transpose" },
        pitchChord { "pitch_chord" }, pitchBlend { "pitch_blend" };
}

namespace hiccup
{
struct Division { const char* name; double beats; };
inline constexpr Division intervalDivisions[] = {
    { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
    { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 }, { "1/32T", 1.0 / 12.0 }, { "1/64", 0.0625 }
};
inline constexpr Division gridDivisions[] = { { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 } };
inline constexpr int repeatChoices[] = { 2, 3, 4, 6, 8, 12, 16, 0 };   // 0 = hold

struct Chord { const char* name; float a; float b; bool hasB; };
inline constexpr Chord chords[] = {
    { "octave up", 12, 0, false }, { "octave down", -12, 0, false }, { "fifth", 7, 0, false }, { "fourth", 5, 0, false },
    { "major 3rd", 4, 0, false }, { "minor 3rd", 3, 0, false }, { "octaves", -12, 12, true }, { "power", 7, 12, true },
    { "major", 4, 7, true }, { "minor", 3, 7, true }, { "sus2", 2, 7, true }, { "sus4", 5, 7, true },
    { "maj7", 4, 11, true }, { "min7", 3, 10, true }, { "dim", 3, 6, true }, { "aug", 4, 8, true }
};

inline juce::StringArray namesOf (const Division* d, int n)
{
    juce::StringArray s;
    for (int i = 0; i < n; ++i) s.add (d[i].name);
    return s;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using AP = AudioProcessorValueTreeState;
    AP::ParameterLayout l;
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (pct).withLabel ("%");
    auto pid = [] (const String& s) { return ParameterID { s, 1 }; };

    // stutter
    l.add (std::make_unique<AudioParameterBool> (pid (ids::stutterOn), "Stutter On", true));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::stutterMix), "Stutter Mix", NormalisableRange<float> (0, 100, 0.1f), 100.0f, percent));
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::stutterInterval), "Stutter Intervals", namesOf (intervalDivisions, 10), 5));
    NormalisableRange<float> rel (5.0f, 2000.0f, 0.1f); rel.setSkewForCentre (160.0f);
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::stutterRelease), "Stutter Release", rel, 220.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
        {
            if (v >= 1990.0f) return String ("no decay");
            return v >= 1000.0f ? String (v * 0.001f, 2) + " s" : String (roundToInt (v)) + " ms";
        })));
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::stutterRepeats), "Stutter Repeats", StringArray { "2", "3", "4", "6", "8", "12", "16", "hold" }, 2));
    l.add (std::make_unique<AudioParameterBool> (pid (ids::stutterReverse), "Stutter Reverse", false));

    // glitch + grains
    l.add (std::make_unique<AudioParameterBool> (pid (ids::glitchOn), "Glitch On", true));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchMix), "Glitch Mix", NormalisableRange<float> (0, 100, 0.1f), 100.0f, percent));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchRhythm), "Rhythm Glitch", NormalisableRange<float> (0, 100, 0.1f), 22.0f, percent));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchPitch), "Pitch Glitch", NormalisableRange<float> (0, 100, 0.1f), 10.0f, percent));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchGrains), "Grains", NormalisableRange<float> (0, 100, 0.1f), 0.0f, percent));
    NormalisableRange<float> sz (10.0f, 500.0f, 0.1f); sz.setSkewForCentre (80.0f);
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchSize), "Grain Size", sz, 80.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::glitchSpray), "Grain Spray", NormalisableRange<float> (0, 100, 0.1f), 30.0f, percent));
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::glitchGrid), "Glitch Grid", namesOf (gridDivisions, 3), 1));
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::glitchScale), "Glitch Scale", StringArray { "OCTAVE", "FIFTH", "WILD" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::glitchMode), "Grain Mode", StringArray { "CLOUD", "REVERSE", "FREEZE" }, 0));
    l.add (std::make_unique<AudioParameterInt> (pid (ids::glitchSeed), "Glitch Seed", 0, 9999, 7));

    // pitch
    l.add (std::make_unique<AudioParameterBool> (pid (ids::pitchOn), "Pitch On", false));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::pitchMix), "Pitch Mix", NormalisableRange<float> (0, 100, 0.1f), 100.0f, percent));
    l.add (std::make_unique<AudioParameterInt> (pid (ids::pitchTranspose), "Transpose", -24, 24, 0,
        AudioParameterIntAttributes().withLabel ("st").withStringFromValueFunction ([] (int v, int)
        { return (v > 0 ? "+" : "") + String (v) + " st"; })));
    StringArray chordNames;
    for (auto& c : chords) chordNames.add (c.name);
    l.add (std::make_unique<AudioParameterChoice> (pid (ids::pitchChord), "Harmony", chordNames, 8));
    l.add (std::make_unique<AudioParameterFloat> (pid (ids::pitchBlend), "Harmony Blend", NormalisableRange<float> (0, 100, 0.1f), 0.0f, percent));
    return l;
}
} // namespace hiccup
