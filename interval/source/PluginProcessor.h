#pragma once
#include "Params.h"
#include "Presets.h"
#include "dsp/Common.h"
#include "dsp/Stutter.h"
#include "dsp/RhythmPitchGlitch.h"
#include "dsp/Grains.h"
#include "dsp/PitchStage.h"

class HiccupProcessor final : public juce::AudioProcessor
{
public:
    HiccupProcessor();
    ~HiccupProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Interval"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return (int) hiccup::scenes().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void applyPreset (const hiccup::Preset&);

    juce::AudioProcessorValueTreeState apvts;

    // read by the GUI
    std::atomic<double> uiBpm { 120.0 }, uiBeatsPerBar { 4.0 };
    std::atomic<bool> uiPlaying { false };
    std::atomic<float> uiOutputLevel { 0.0f };
    hiccup::Stutter stutter;
    hiccup::RhythmPitchGlitch rhythmPitch;
    hiccup::Grains grains;

private:
    void updateClock (int numSamples);
    float raw (std::atomic<float>* p) const noexcept { return p->load (std::memory_order_relaxed); }

    hiccup::PitchStage pitch;
    hiccup::Clock clock;
    double sampleRate_ = 44100.0, internalPpq = 0.0;
    juce::AudioBuffer<float> work, glitchDry;
    float glitchWet = 0.0f;
    int currentProgram = 0;

    std::atomic<float>* pStutterOn, *pStutterMix, *pStutterInterval, *pStutterRelease, *pStutterRepeats, *pStutterReverse,
                       *pGlitchOn, *pGlitchMix, *pGlitchRhythm, *pGlitchPitch, *pGlitchGrains, *pGlitchSize, *pGlitchSpray,
                       *pGlitchGrid, *pGlitchScale, *pGlitchMode, *pGlitchSeed,
                       *pPitchOn, *pPitchMix, *pPitchTranspose, *pPitchChord, *pPitchBlend;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HiccupProcessor)
};
