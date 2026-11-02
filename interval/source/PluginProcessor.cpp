#include "PluginProcessor.h"
#include "gui/PluginEditor.h"

using namespace hiccup;

HiccupProcessor::HiccupProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "hiccup", createLayout())
{
    auto p = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pStutterOn = p (ids::stutterOn); pStutterMix = p (ids::stutterMix); pStutterInterval = p (ids::stutterInterval);
    pStutterRelease = p (ids::stutterRelease); pStutterRepeats = p (ids::stutterRepeats); pStutterReverse = p (ids::stutterReverse);
    pGlitchOn = p (ids::glitchOn); pGlitchMix = p (ids::glitchMix); pGlitchRhythm = p (ids::glitchRhythm); pGlitchPitch = p (ids::glitchPitch);
    pGlitchGrains = p (ids::glitchGrains); pGlitchSize = p (ids::glitchSize); pGlitchSpray = p (ids::glitchSpray);
    pGlitchGrid = p (ids::glitchGrid); pGlitchScale = p (ids::glitchScale); pGlitchMode = p (ids::glitchMode); pGlitchSeed = p (ids::glitchSeed);
    pPitchOn = p (ids::pitchOn); pPitchMix = p (ids::pitchMix); pPitchTranspose = p (ids::pitchTranspose);
    pPitchChord = p (ids::pitchChord); pPitchBlend = p (ids::pitchBlend);
}

bool HiccupProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

void HiccupProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    stutter.prepare (sampleRate);
    rhythmPitch.prepare (sampleRate);
    grains.prepare (sampleRate);
    pitch.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (pitch.getLatencySamples());
    work.setSize (2, samplesPerBlock, false, true, true);
    glitchDry.setSize (2, samplesPerBlock, false, true, true);
    glitchWet = 0.0f;
    internalPpq = 0.0;
}

void HiccupProcessor::reset()
{
    stutter.reset();
    rhythmPitch.reset();
    grains.reset();
    pitch.reset();
    glitchWet = 0.0f;
}

void HiccupProcessor::updateClock (int numSamples)
{
    double bpm = 120.0, ppq = 0.0, bar = 4.0;
    bool playing = false, hasPpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto t = pos->getTimeSignature()) if (t->denominator > 0) bar = t->numerator * 4.0 / t->denominator;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        }
    }
    clock.bpm = bpm;
    clock.beatsPerBar = bar;
    clock.playing = playing;
    clock.beatsPerSample = bpm / 60.0 / sampleRate_;
    // host position while it runs, otherwise a free-running clock so the pedals keep moving
    clock.ppq = (playing && hasPpq) ? ppq : internalPpq;
    internalPpq = clock.ppq + clock.beatsPerSample * numSamples;
    uiBpm.store (bpm, std::memory_order_relaxed);
    uiBeatsPerBar.store (bar, std::memory_order_relaxed);
    uiPlaying.store (playing, std::memory_order_relaxed);
}

void HiccupProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n == 0) return;
    if (work.getNumSamples() < n)    // hosts should not do this, but a plugin must survive it
    {
        work.setSize (2, n, false, false, true);
        glitchDry.setSize (2, n, false, false, true);
    }

    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, inCh > 1 ? 1 : 0, 0, n);
    auto* L = work.getWritePointer (0);
    auto* R = work.getWritePointer (1);

    updateClock (n);

    // ---- stutter ----
    Stutter::Params sp;
    sp.engaged = raw (pStutterOn) > 0.5f;
    sp.mix = raw (pStutterMix) * 0.01f;
    sp.intervalBeats = intervalDivisions[juce::jlimit (0, 9, (int) raw (pStutterInterval))].beats;
    sp.releaseMs = raw (pStutterRelease);
    sp.repeats = repeatChoices[juce::jlimit (0, 7, (int) raw (pStutterRepeats))];
    sp.reverse = raw (pStutterReverse) > 0.5f;
    stutter.process (L, R, n, clock, sp);

    // ---- glitch: rhythm + pitch events, then the grain cloud, then the pedal's own mix ----
    const bool glitchOn = raw (pGlitchOn) > 0.5f;
    const float glitchTarget = glitchOn ? raw (pGlitchMix) * 0.01f : 0.0f;
    const bool glitchActive = glitchOn || glitchWet > 1.0e-4f;
    if (glitchActive)
    {
        glitchDry.copyFrom (0, 0, L, n);
        glitchDry.copyFrom (1, 0, R, n);
    }
    RhythmPitchGlitch::Params gp;
    gp.rhythm = raw (pGlitchRhythm) * 0.01f;
    gp.pitch = raw (pGlitchPitch) * 0.01f;
    gp.gridBeats = gridDivisions[juce::jlimit (0, 2, (int) raw (pGlitchGrid))].beats;
    gp.scale = (int) raw (pGlitchScale);
    gp.seed = (uint32_t) raw (pGlitchSeed);
    rhythmPitch.process (L, R, n, clock, gp, glitchActive);

    Grains::Params gr;
    gr.amount = glitchActive ? raw (pGlitchGrains) * 0.01f : 0.0f;
    gr.sizeMs = raw (pGlitchSize);
    gr.spray = raw (pGlitchSpray) * 0.01f;
    gr.pitch = raw (pGlitchPitch) * 0.01f;
    gr.scale = gp.scale;
    gr.mode = (int) raw (pGlitchMode);
    grains.process (L, R, n, gr);

    if (glitchActive)
    {
        const float slew = 1.0f / (float) (0.006 * sampleRate_);
        const auto* dL = glitchDry.getReadPointer (0);
        const auto* dR = glitchDry.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            glitchWet += juce::jlimit (-slew, slew, glitchTarget - glitchWet);
            L[i] = dL[i] + (L[i] - dL[i]) * glitchWet;
            R[i] = dR[i] + (R[i] - dR[i]) * glitchWet;
        }
    }

    // ---- pitch ----
    PitchStage::Params pp;
    pp.engaged = raw (pPitchOn) > 0.5f;
    pp.mix = raw (pPitchMix) * 0.01f;
    pp.transpose = raw (pPitchTranspose);
    pp.blend = raw (pPitchBlend) * 0.01f;
    const auto& ch = chords[juce::jlimit (0, 15, (int) raw (pPitchChord))];
    pp.voiceA = ch.a; pp.voiceB = ch.b; pp.voiceAOn = true; pp.voiceBOn = ch.hasB;
    pitch.process (L, R, n, pp);

    // ---- out ----
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // safety net for stacked voices and hot grains; below 0.9 this is a straight wire
        for (auto* c : { L, R })
        {
            float x = c[i];
            if (! std::isfinite (x)) x = 0.0f;
            const float a = std::abs (x);
            if (a > 0.9f) x = std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), x);
            c[i] = x;
            peak = juce::jmax (peak, std::abs (x));
        }
    }
    uiOutputLevel.store (peak, std::memory_order_relaxed);

    if (outCh >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
        for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, n);
    }
    else
    {
        auto* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (L[i] + R[i]);
    }
}

void HiccupProcessor::applyPreset (const Preset& preset)
{
    for (const auto& v : preset.values)
        if (auto* param = apvts.getParameter (v.id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v.value));
            param->endChangeGesture();
        }
}

void HiccupProcessor::setCurrentProgram (int index)
{
    const auto& s = scenes();
    if (! juce::isPositiveAndBelow (index, (int) s.size())) return;
    currentProgram = index;
    applyPreset (s[(size_t) index]);
}

const juce::String HiccupProcessor::getProgramName (int index)
{
    const auto& s = scenes();
    return juce::isPositiveAndBelow (index, (int) s.size()) ? juce::String (s[(size_t) index].name) : juce::String();
}

void HiccupProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HiccupProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* HiccupProcessor::createEditor() { return createHiccupEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HiccupProcessor(); }
