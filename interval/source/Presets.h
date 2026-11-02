#pragma once
#include "Params.h"
#include <vector>

namespace hiccup
{
struct PresetValue { const juce::String& id; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

/** Values are in real units: percent, ms, or the choice index. */
inline const std::vector<Preset>& scenes()
{
    using namespace ids;
    static const std::vector<Preset> list = {
        { "first hiccup", { {stutterOn,1},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,100},{glitchRhythm,22},{glitchPitch,10},{glitchGrains,0},{glitchSize,80},{glitchSpray,30},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,7},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "machine gun",  { {stutterOn,1},{stutterMix,100},{stutterInterval,7},{stutterRelease,60},{stutterRepeats,4},{stutterReverse,0},
                            {glitchOn,0},{glitchMix,100},{glitchRhythm,0},{glitchPitch,0},{glitchGrains,0},{glitchSize,80},{glitchSpray,30},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,3},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "tape dream",   { {stutterOn,0},{stutterMix,100},{stutterInterval,3},{stutterRelease,600},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,90},{glitchRhythm,34},{glitchPitch,28},{glitchGrains,32},{glitchSize,190},{glitchSpray,48},{glitchGrid,0},{glitchScale,1},{glitchMode,0},{glitchSeed,41},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "crystal cloud",{ {stutterOn,0},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,80},{glitchRhythm,0},{glitchPitch,48},{glitchGrains,82},{glitchSize,240},{glitchSpray,62},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,12},
                            {pitchOn,1},{pitchMix,70},{pitchTranspose,12},{pitchChord,6},{pitchBlend,34} } },
        { "broken drums", { {stutterOn,1},{stutterMix,70},{stutterInterval,4},{stutterRelease,140},{stutterRepeats,3},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,100},{glitchRhythm,68},{glitchPitch,14},{glitchGrains,0},{glitchSize,60},{glitchSpray,20},{glitchGrid,1},{glitchScale,1},{glitchMode,0},{glitchSeed,88},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "chipmunk riot",{ {stutterOn,0},{stutterMix,100},{stutterInterval,7},{stutterRelease,90},{stutterRepeats,4},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,100},{glitchRhythm,38},{glitchPitch,82},{glitchGrains,12},{glitchSize,40},{glitchSpray,18},{glitchGrid,2},{glitchScale,2},{glitchMode,0},{glitchSeed,256},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "choir stack",  { {stutterOn,0},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,100},{glitchRhythm,0},{glitchPitch,0},{glitchGrains,16},{glitchSize,160},{glitchSpray,30},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,7},
                            {pitchOn,1},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,68} } },
        { "backwards sub",{ {stutterOn,1},{stutterMix,100},{stutterInterval,1},{stutterRelease,1400},{stutterRepeats,1},{stutterReverse,1},
                            {glitchOn,0},{glitchMix,100},{glitchRhythm,0},{glitchPitch,0},{glitchGrains,0},{glitchSize,80},{glitchSpray,30},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,7},
                            {pitchOn,1},{pitchMix,100},{pitchTranspose,-12},{pitchChord,1},{pitchBlend,25} } },
        { "frozen lake",  { {stutterOn,0},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,1},{glitchMix,100},{glitchRhythm,0},{glitchPitch,20},{glitchGrains,100},{glitchSize,320},{glitchSpray,78},{glitchGrid,1},{glitchScale,1},{glitchMode,2},{glitchSeed,7},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
        { "all clean",    { {stutterOn,0},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0},
                            {glitchOn,0},{glitchMix,100},{glitchRhythm,0},{glitchPitch,0},{glitchGrains,0},{glitchSize,80},{glitchSpray,30},{glitchGrid,1},{glitchScale,0},{glitchMode,0},{glitchSeed,7},
                            {pitchOn,0},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,0} } },
    };
    return list;
}

inline const std::vector<Preset>& stutterPresets()
{
    using namespace ids;
    static const std::vector<Preset> list = {
        { "16ths",         { {stutterOn,1},{stutterMix,100},{stutterInterval,5},{stutterRelease,220},{stutterRepeats,2},{stutterReverse,0} } },
        { "gated 8ths",    { {stutterOn,1},{stutterMix,100},{stutterInterval,3},{stutterRelease,110},{stutterRepeats,2},{stutterReverse,0} } },
        { "machine gun",   { {stutterOn,1},{stutterMix,100},{stutterInterval,7},{stutterRelease,50},{stutterRepeats,4},{stutterReverse,0} } },
        { "triplet hop",   { {stutterOn,1},{stutterMix,85},{stutterInterval,4},{stutterRelease,180},{stutterRepeats,1},{stutterReverse,0} } },
        { "reverse swell", { {stutterOn,1},{stutterMix,100},{stutterInterval,1},{stutterRelease,1990},{stutterRepeats,0},{stutterReverse,1} } },
        { "hold it",       { {stutterOn,1},{stutterMix,100},{stutterInterval,5},{stutterRelease,1990},{stutterRepeats,7},{stutterReverse,0} } },
    };
    return list;
}

inline const std::vector<Preset>& pitchPresets()
{
    using namespace ids;
    static const std::vector<Preset> list = {
        { "up an octave",   { {pitchOn,1},{pitchMix,100},{pitchTranspose,12},{pitchChord,0},{pitchBlend,0} } },
        { "down an octave", { {pitchOn,1},{pitchMix,100},{pitchTranspose,-12},{pitchChord,1},{pitchBlend,0} } },
        { "fifth stack",    { {pitchOn,1},{pitchMix,100},{pitchTranspose,0},{pitchChord,7},{pitchBlend,55} } },
        { "major choir",    { {pitchOn,1},{pitchMix,100},{pitchTranspose,0},{pitchChord,8},{pitchBlend,65} } },
        { "minor choir",    { {pitchOn,1},{pitchMix,100},{pitchTranspose,0},{pitchChord,9},{pitchBlend,65} } },
        { "sub and air",    { {pitchOn,1},{pitchMix,100},{pitchTranspose,0},{pitchChord,6},{pitchBlend,50} } },
    };
    return list;
}
} // namespace hiccup
