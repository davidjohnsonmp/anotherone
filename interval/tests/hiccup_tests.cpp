// Offline verification of every feature. Each test feeds a known signal and measures the result.
#include "../source/PluginProcessor.h"
#include "../source/gui/PluginEditor.h"
#include <atomic>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>

// ---- allocation tracker: nothing may allocate on the audio thread --------------------------------
static std::atomic<bool> trackAlloc { false };
static std::atomic<long> allocCount { 0 };
void* operator new (std::size_t n) { if (trackAlloc.load()) allocCount++; void* p = std::malloc (n); if (! p) throw std::bad_alloc(); return p; }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void* operator new[] (std::size_t n) { if (trackAlloc.load()) allocCount++; void* p = std::malloc (n); if (! p) throw std::bad_alloc(); return p; }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace hiccup;
struct TestHead : juce::AudioPlayHead
{
    double bpm = 120, ppq = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing);
        i.setTimeSignature (TimeSignature { 4, 4 }); return i;
    }
};
static int fails = 0, passes = 0;
#define CHECK(cond, ...) do { bool ok_ = (cond); printf ("  %s  ", ok_ ? "ok  " : "FAIL"); printf (__VA_ARGS__); printf ("\n"); if (ok_) passes++; else fails++; } while (0)

struct Rig
{
    HiccupProcessor p; TestHead head; double sr; int block;
    Rig (double sampleRate = 44100, int blockSize = 512) : sr (sampleRate), block (blockSize)
    {
        p.setPlayHead (&head);
        allOff();
        p.prepareToPlay (sr, block);
    }
    void set (const juce::String& id, float realValue)
    {
        auto* a = p.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (realValue));
    }
    void allOff() { p.setCurrentProgram (9); }
    std::vector<float> run (const std::vector<float>& inL, std::vector<float>* outR = nullptr, const std::vector<float>* inR = nullptr)
    {
        std::vector<float> o (inL.size()); if (outR) outR->resize (inL.size());
        juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
        size_t pos = 0;
        while (pos < inL.size())
        {
            int n = (int) std::min ((size_t) block, inL.size() - pos);
            juce::AudioBuffer<float> bb (b.getArrayOfWritePointers(), 2, n);
            for (int i = 0; i < n; ++i) { bb.setSample (0, i, inL[pos + i]); bb.setSample (1, i, inR ? (*inR)[pos + i] : inL[pos + i]); }
            p.processBlock (bb, m);
            for (int i = 0; i < n; ++i) { o[pos + i] = bb.getSample (0, i); if (outR) (*outR)[pos + i] = bb.getSample (1, i); }
            head.ppq += n * head.bpm / 60.0 / sr;
            pos += (size_t) n;
        }
        return o;
    }
};
static std::vector<float> sine (double f, double sr, int n, float a = .5f) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = a * std::sin (2 * M_PI * f * i / sr); return v; }
static std::vector<float> noise (int n, float a = .4f, unsigned s = 1) { std::mt19937 g (s); std::uniform_real_distribution<float> d (-a, a); std::vector<float> v ((size_t) n); for (auto& x : v) x = d (g); return v; }
static std::vector<float> chirp (double f0, double f1, double sr, int n, float a = .5f) { std::vector<float> v ((size_t) n); double ph = 0; for (int i = 0; i < n; ++i) { double f = f0 + (f1 - f0) * i / n; ph += 2 * M_PI * f / sr; v[(size_t) i] = a * std::sin (ph); } return v; }
static double rmsOf (const std::vector<float>& v, size_t a, size_t b) { double s = 0; b = std::min (b, v.size()); for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return std::sqrt (s / std::max<size_t> (1, b - a)); }
static double corr (const float* a, const float* b, size_t n, bool rev = false) { double ab = 0, aa = 0, bb = 0; for (size_t i = 0; i < n; ++i) { float y = rev ? b[n - 1 - i] : b[i]; ab += a[i] * y; aa += a[i] * a[i]; bb += y * y; } return ab / std::sqrt (aa * bb + 1e-20); }
// goertzel magnitude at frequency, windowed
static double tone (const std::vector<float>& v, size_t a, size_t b, double f, double sr) { b = std::min (b, v.size()); double w = 2 * M_PI * f / sr, re = 0, im = 0, wsum = 0; size_t n = b - a; for (size_t i = 0; i < n; ++i) { double h = .5 - .5 * std::cos (2 * M_PI * i / (n - 1)); re += h * v[a + i] * std::cos (w * i); im += h * v[a + i] * std::sin (w * i); wsum += h; } return 2 * std::sqrt (re * re + im * im) / wsum; }
static double peakFreq (const std::vector<float>& v, size_t a, size_t b, double sr, double lo, double hi) { double bf = lo, bm = -1; for (double f = lo; f < hi; f += 1.0) { double m = tone (v, a, b, f, sr); if (m > bm) { bm = m; bf = f; } } return bf; }
static float maxDelta (const std::vector<float>& v, size_t a = 0) { float m = 0; for (size_t i = a + 1; i < v.size(); ++i) m = std::max (m, std::abs (v[i] - v[i - 1])); return m; }
static bool allFinite (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x)) return false; return true; }

static void testBypass()
{
    puts ("\n[bypass] all pedals off must be a clean, delay-compensated wire");
    Rig r; r.set (ids::stutterOn, 0); r.set (ids::glitchOn, 0); r.set (ids::pitchOn, 0);
    int lat = r.p.getLatencySamples();
    auto in = noise (88200); auto out = r.run (in);
    double err = 0; for (size_t i = (size_t) lat; i < in.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
    CHECK (err < 1e-6, "null against input delayed by reported %d samples (%.1f ms): max error %.2e", lat, 1000.0 * lat / 44100, err);
}
static void testStutter()
{
    puts ("\n[stutter] intervals, repeats, release length, mix, reverse");
    const double sr = 48000; const int S = 6000;   // 1/16 at 120 bpm, a whole number of samples at 48 kHz
    { Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterInterval, 5); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 2000); r.set (ids::stutterMix, 100);
      auto in = chirp (200, 2000, sr, S * 16); auto out = r.run (in); size_t lat = (size_t) r.p.getLatencySamples();
      const float* cyc = out.data() + lat; size_t off = 300; size_t len = (size_t) S - 2 * off;
      double c1 = corr (cyc + S * 4 + off, cyc + S * 5 + off, len), c2 = corr (cyc + S * 4 + off, cyc + S * 6 + off, len), c3 = corr (cyc + S * 4 + off, cyc + S * 7 + off, len), cn = corr (cyc + S * 4 + off, cyc + S * 8 + off, len);
      CHECK (c1 > .99 && c2 > .99 && c3 > .99, "hits 2-4 of a 4-repeat cycle replay hit 1: r = %.4f %.4f %.4f", c1, c2, c3);
      CHECK (cn < .9, "next cycle captures fresh audio: r = %.3f", cn);
      CHECK (corr (cyc + S * 4 + off, in.data() + S * 4 + off, len) > .999, "first hit is the live signal, zero added delay"); }
    { // interval changes timing
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterInterval, 3); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 2000);
      auto in = chirp (200, 2000, sr, S * 32); auto out = r.run (in); const float* cyc = out.data() + r.p.getLatencySamples(); int S8 = S * 2; size_t off = 300;
      CHECK (corr (cyc + S8 * 4 + off, cyc + S8 * 5 + off, (size_t) S8 - 2 * off) > .99, "1/8 interval repeats every %d samples", S8); }
    { // release
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterInterval, 3); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 50);
      auto in = sine (440, sr, S * 32); auto out = r.run (in); size_t lat = (size_t) r.p.getLatencySamples(); int S8 = S * 2;
      double early = rmsOf (out, lat + S8 * 5 + 200, lat + S8 * 5 + 900), late = rmsOf (out, lat + S8 * 5 + 3000, lat + S8 * 6 - 300);
      CHECK (early > .15 && late < 1e-4, "50 ms release: rms %.3f right after the hit, %.6f once it has decayed", early, late);
      r.set (ids::stutterRelease, 400); auto o2 = r.run (in); double l2 = rmsOf (o2, lat + S8 * 5 + 3000, lat + S8 * 6 - 300);
      CHECK (l2 > .05, "400 ms release still rings through the same window: rms %.3f", l2); }
    { // mix
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterMix, 0); auto in = noise (48000); auto out = r.run (in); size_t lat = (size_t) r.p.getLatencySamples(); double err = 0; for (size_t i = 4000; i < in.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - lat]));
      CHECK (err < 1e-5, "mix 0 returns the dry signal: max error %.2e", err); }
    { // reverse
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterReverse, 1); r.set (ids::stutterInterval, 5); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 2000);
      auto in = chirp (200, 2000, sr, S * 16); auto out = r.run (in); const float* cyc = out.data() + r.p.getLatencySamples(); size_t off = 300;
      double c = corr (cyc + S * 4 + off, cyc + S * 5 + off, (size_t) S - 2 * off, true);
      CHECK (c > .97, "reverse: hit 2 is hit 1 played backwards: r = %.3f", c); }
    { // hold
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterRepeats, 7); r.set (ids::stutterRelease, 2000); r.set (ids::stutterInterval, 5);
      auto in = chirp (200, 2000, sr, S * 40); auto out = r.run (in); const float* cyc = out.data() + r.p.getLatencySamples(); size_t off = 300;
      CHECK (corr (cyc + S * 2 + off, cyc + S * 30 + off, (size_t) S - 2 * off) > .99, "hold repeats the same slice indefinitely"); }
    { // clicks
      Rig r (sr); r.set (ids::stutterOn, 1); r.set (ids::stutterInterval, 7); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 2000);
      auto in = sine (220, sr, 88200); auto out = r.run (in); float d = maxDelta (out, 4000), base = maxDelta (in);
      CHECK (d < base * 3.5f, "hit boundaries are declicked: max step %.4f vs %.4f for the input", d, base); }
}
static void testRhythmPitchGlitch()
{
    puts ("\n[rhythm glitch + pitch glitch] seeded, on the grid, audibly doing what the pattern says");
    const double sr = 48000; const int S = 6000;
    auto make = [&] (float rhythm, float pitch, int seed, int scale = 0)
    {
        Rig r (sr); r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, rhythm); r.set (ids::glitchPitch, pitch); r.set (ids::glitchSeed, (float) seed); r.set (ids::glitchGrid, 1); r.set (ids::glitchScale, (float) scale);
        auto in = noise (S * 96); return std::make_pair (in, r.run (in)); };
    auto [inA, a] = make (60, 0, 11); auto [inB, b] = make (60, 0, 11); auto [inC, c] = make (60, 0, 12);
    double same = 0; for (size_t i = 0; i < a.size(); ++i) same = std::max (same, (double) std::abs (a[i] - b[i]));
    CHECK (same < 1e-6, "same seed gives a sample-identical pattern (max diff %.1e)", same);
    double diff = 0; for (size_t i = 0; i < a.size(); ++i) diff += std::abs (a[i] - c[i]);
    CHECK (diff > 100, "a different seed gives a different pattern");
    Rig ref (sr); size_t lat = (size_t) ref.p.getLatencySamples();
    int ev = 0, evChanged = 0, none = 0, noneClean = 0;
    for (int s = 8; s < 32; ++s)
    {
        auto e = pattern::eventAt (11, s, .6f, 0, 0); const float* x = a.data() + lat + s * S + 400; const float* y = inA.data() + s * S + 400; size_t n = (size_t) S - 800;
        double cv = corr (x, y, n);
        if (e.rhythm == pattern::Rhythm::none) { none++; if (cv > .99) noneClean++; }
        else { ev++; if (cv < .9) evChanged++; }
    }
    CHECK (ev >= 6 && evChanged >= ev - 1, "steps with an event change the audio: %d of %d", evChanged, ev);
    CHECK (none >= 3 && noneClean == none, "steps with no event pass the live signal: %d of %d", noneClean, none);
    // knob density
    int low = 0, high = 0; for (int s = 0; s < 64; ++s) { low += pattern::eventAt (5, s, .15f, 0, 0).rhythm != pattern::Rhythm::none; high += pattern::eventAt (5, s, .85f, 0, 0).rhythm != pattern::Rhythm::none; }
    CHECK (high > low * 2, "rhythm knob sets event density: %d events at 15%%, %d at 85%% over 64 steps", low, high);
    bool monotonic = true; for (int s = 0; s < 200; ++s) if (pattern::eventAt (5, s, .15f, 0, 0).rhythm != pattern::Rhythm::none && pattern::eventAt (5, s, .85f, 0, 0).rhythm == pattern::Rhythm::none) monotonic = false;
    CHECK (monotonic, "raising the knob adds events without reshuffling existing ones");
    // pitch jump on a sine: find steps with a jump and no rhythm event, measure the frequency
    Rig r (sr); r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 70); r.set (ids::glitchSeed, 21); r.set (ids::glitchGrid, 0); r.set (ids::glitchScale, 0);
    int S8 = S * 2; auto in = sine (440, sr, S8 * 40); auto o = r.run (in);
    int tested = 0, correct = 0, liveOk = 0, liveCnt = 0;
    for (int s = 4; s < 32; ++s)
    {
        auto e = pattern::eventAt (21, s % 16, 0, .7f, 0); size_t x0 = lat + (size_t) s * S8 + 1200, x1 = lat + (size_t) (s + 1) * S8 - 1200;
        if (e.pitch == pattern::Pitch::jump) { double want = 440 * std::pow (2.0, e.semitones / 12.0); double got = peakFreq (o, x0, x1, sr, 80, 4000); tested++; if (std::abs (got - want) / want < .02) correct++; }
        else if (e.pitch == pattern::Pitch::none) { liveCnt++; if (std::abs (peakFreq (o, x0, x1, sr, 80, 4000) - 440) < 5) liveOk++; }
    }
    CHECK (tested >= 3 && correct == tested, "pitch jumps land on the scale: %d of %d within 2%%", correct, tested);
    CHECK (liveCnt >= 2 && liveOk == liveCnt, "steps without a pitch event stay at the input pitch: %d of %d", liveOk, liveCnt);
    // scale selection
    bool ok = true; for (int s = 0; s < 200; ++s) { auto e = pattern::eventAt (9, s, 0, 1, 0); if (e.pitch != pattern::Pitch::none && std::fmod (std::abs (e.semitones), 12.f) != 0) ok = false; }
    CHECK (ok, "OCTAVE scale only ever produces octave intervals");
    // clicks
    { Rig k (sr); k.set (ids::glitchOn, 1); k.set (ids::glitchRhythm, 70); k.set (ids::glitchPitch, 0); k.set (ids::glitchSeed, 3); auto s2 = sine (220, sr, 144000); auto o2 = k.run (s2); float d = maxDelta (o2, 4000), base = maxDelta (s2);
      CHECK (d < base * 4.f && allFinite (o2), "events are declicked on a sine: max step %.4f vs %.4f", d, base); }
}
static void testGrains()
{
    puts ("\n[grains] density, size, freeze, reverse");
    const double sr = 44100;
    auto count = [&] (float amount, float size)
    { Rig r; r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 0); r.set (ids::glitchGrains, amount); r.set (ids::glitchSize, size); auto in = noise (88200); auto o = r.run (in); return std::make_pair ((int) r.p.grains.spawned.load(), rmsOf (o, 44100, 88200)); };
    auto [n0, r0] = count (0, 80); auto [n1, r1] = count (35, 80); auto [n2, r2] = count (100, 80);
    CHECK (n0 == 0 && n1 > 10 && n2 > n1 * 2, "the grains knob sets density over 2 s: %d, %d, %d grains at 0/35/100%%", n0, n1, n2);
    CHECK (r2 > .03 && r2 < .6, "full grains level stays sane: rms %.3f (input rms ~.23)", r2);
    { Rig r; r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 0); r.set (ids::glitchGrains, 100); r.set (ids::glitchSize, 100); r.set (ids::glitchMode, 2); r.set (ids::glitchSpray, 40);
      auto in = noise (66150); auto silence = std::vector<float> (220500, 0.f);
      r.set (ids::glitchMode, 0); r.run (in); r.set (ids::glitchMode, 2); auto o = r.run (silence);
      CHECK (rmsOf (o, 176400, 220500) > .02, "FREEZE keeps the cloud alive 4+ s after the input stops: rms %.3f", rmsOf (o, 176400, 220500)); }
    { Rig r; r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 0); r.set (ids::glitchGrains, 100); r.set (ids::glitchSize, 100); r.set (ids::glitchSpray, 10); r.set (ids::glitchMode, 0);
      auto in = noise (66150); auto silence = std::vector<float> (330000, 0.f); r.run (in); auto o = r.run (silence);
      CHECK (rmsOf (o, 280000, 330000) < 1e-4, "CLOUD fades out once recent audio has gone: rms %.6f", rmsOf (o, 280000, 330000)); }
    { // reversed grains on a rising chirp: spectral centroid proxy — read a tone-specific check
      Rig r; r.set (ids::glitchOn, 1); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 0); r.set (ids::glitchGrains, 100); r.set (ids::glitchSize, 60); r.set (ids::glitchMode, 1); r.set (ids::glitchSpray, 0);
      auto in = sine (440, sr, 88200); auto o = r.run (in); CHECK (std::abs (peakFreq (o, 44100, 88200, sr, 80, 2000) - 440) < 8 && allFinite (o), "REVERSE grains keep pitch on a steady tone (peak at %.0f Hz)", peakFreq (o, 44100, 88200, sr, 80, 2000)); }
    { Rig r; r.set (ids::glitchOn, 1); r.set (ids::glitchGrains, 100); r.set (ids::glitchSize, 100); r.set (ids::glitchRhythm, 0); r.set (ids::glitchPitch, 100); r.set (ids::glitchScale, 0); r.set (ids::glitchSpray, 10);
      auto in = sine (440, sr, 132300); auto o = r.run (in); double up = tone (o, 66150, 132300, 880, sr), base = tone (o, 66150, 132300, 440, sr);
      CHECK (up > .01 && base > .01, "pitch knob detunes grains onto the scale: 440 Hz %.3f, 880 Hz %.3f", base, up); }
}
static void testPitch()
{
    puts ("\n[pitch] transpose and harmonizer");
    const double sr = 44100;
    for (int semis : { 12, 7, -5, -12, 24 })
    {
        Rig r; r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, (float) semis); r.set (ids::pitchMix, 100); r.set (ids::pitchBlend, 0);
        auto in = sine (440, sr, 132300); auto o = r.run (in); double want = 440 * std::pow (2.0, semis / 12.0); double got = want, best = -1; for (double f = want * .97; f < want * 1.03; f += .05) { double m = tone (o, 66150, 132300, f, sr); if (m > best) { best = m; got = f; } }
        double cents = 1200 * std::log2 (got / want);
        CHECK (std::abs (cents) < 4.0 && best > .3, "transpose %+d st: %.2f Hz, want %.2f (%+.1f cents), level %.2f", semis, got, want, cents, best);
    }
    { Rig r; r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, 0); r.set (ids::pitchBlend, 100); r.set (ids::pitchChord, 8);
      auto in = sine (440, sr, 132300); auto o = r.run (in); auto near_ = [&] (double f) { double b = 0; for (double x = f * .996; x < f * 1.004; x += .1) b = std::max (b, tone (o, 66150, 132300, x, sr)); return b; }; double root = near_ (440), third = near_ (554.37), fifth = near_ (659.26), off = near_ (500);
      CHECK (root > .2 && third > .1 && fifth > .1 && off < .03, "major harmony on 440 Hz: root %.2f, 3rd %.2f, 5th %.2f, between-notes %.3f", root, third, fifth, off); }
    { Rig r; r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, 5); r.set (ids::pitchBlend, 80); r.set (ids::pitchChord, 2);
      auto in = sine (440, sr, 132300); auto o = r.run (in); auto near_ = [&] (double f) { double bb = 0; for (double x = f * .996; x < f * 1.004; x += .1) bb = std::max (bb, tone (o, 66150, 132300, x, sr)); return bb; }; double a = near_ (587.33), b = near_ (880);
      CHECK (a > .1 && b > .05, "harmony follows transpose: +5 main %.2f, +5+7 voice %.2f", a, b); }
    { // latency is constant and honest
      Rig r; r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, 0); r.set (ids::pitchBlend, 0); int lat = r.p.getLatencySamples(); std::vector<float> in (44100, 0.f); in[8000] = 1; auto o = r.run (in); size_t peak = 0; float m = 0; for (size_t i = 0; i < o.size(); ++i) if (std::abs (o[i]) > m) { m = std::abs (o[i]); peak = i; }
      CHECK ((int) peak == 8000 + lat, "pitch on at 0 st stays sample-aligned: impulse at +%d (reported %d)", (int) peak - 8000, lat); }
    { // transposed signal arrives at the same time as dry
      Rig r; r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, 12); r.set (ids::pitchBlend, 0); int lat = r.p.getLatencySamples(); std::vector<float> in (88200, 0.f); auto b = sine (880, sr, 88200); for (size_t i = 44100; i < 88200; ++i) in[i] = b[i]; auto o = r.run (in);
      size_t onset = 0; for (size_t i = 0; i < o.size(); ++i) if (std::abs (o[i]) > .08f && i > 40000) { onset = i; break; } int err = (int) onset - 44100 - lat;
      CHECK (std::abs (err) < 44100 / 50, "reported latency matches the transposed path: onset error %.1f ms", 1000.0 * err / 44100); }
    { // toggling mid-stream stays clean
      Rig r; auto in = sine (440, sr, 44100); r.run (in); r.set (ids::pitchOn, 1); r.set (ids::pitchTranspose, 12); auto a = r.run (in); r.set (ids::pitchOn, 0); auto b = r.run (in);
      CHECK (maxDelta (a) < .28f && maxDelta (b) < .28f && allFinite (a) && allFinite (b), "engaging and bypassing fades instead of clicking: max step %.3f / %.3f", maxDelta (a), maxDelta (b)); }
}
static void testHostBehaviour()
{
    puts ("\n[host behaviour] state, presets, odd buffers and rates, realtime safety");
    { Rig r; r.set (ids::stutterInterval, 8); r.set (ids::pitchTranspose, -7); r.set (ids::glitchSeed, 4242); juce::MemoryBlock mb; r.p.getStateInformation (mb);
      Rig q; q.p.setStateInformation (mb.getData(), (int) mb.getSize());
      CHECK ((int) *q.p.apvts.getRawParameterValue (ids::stutterInterval) == 8 && (int) *q.p.apvts.getRawParameterValue (ids::pitchTranspose) == -7 && (int) *q.p.apvts.getRawParameterValue (ids::glitchSeed) == 4242, "state round trips through save and load"); }
    { Rig r; bool ok = true; for (int i = 0; i < r.p.getNumPrograms(); ++i) { r.p.setCurrentProgram (i); auto o = r.run (noise (22050)); ok = ok && allFinite (o); for (auto x : o) ok = ok && std::abs (x) <= 1.0f; }
      CHECK (ok, "all %d scenes run with finite output inside +/-1.0", r.p.getNumPrograms()); }
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int bs : { 32, 256, 1024, 4096 })
        {
            Rig r (sr, bs); r.p.setCurrentProgram (3); auto o = r.run (noise ((int) sr / 4));
            if (! allFinite (o)) CHECK (false, "%.0f Hz / %d samples produced non-finite audio", sr, bs);
        }
    CHECK (true, "44.1 to 192 kHz at 32 to 4096 sample blocks all run clean");
    { Rig r (44100, 512); r.head.playing = false; r.p.setCurrentProgram (0); auto in = chirp (200, 2000, 44100, 88200); auto o = r.run (in); double same = 0; for (size_t i = 8000; i < in.size(); ++i) same += std::abs (o[i] - in[i - (size_t) r.p.getLatencySamples()]);
      CHECK (same > 10 && allFinite (o), "with the transport stopped the free-running clock still stutters"); }
    { // tempo sync
      Rig r; r.head.bpm = 90; r.set (ids::stutterOn, 1); r.set (ids::stutterInterval, 1); r.set (ids::stutterRepeats, 2); r.set (ids::stutterRelease, 2000); int S = (int) std::llround (60.0 / 90.0 * 44100); auto in = chirp (200, 2000, 44100, S * 12); auto o = r.run (in); const float* c = o.data() + r.p.getLatencySamples();
      CHECK (corr (c + S * 4 + 300, c + S * 5 + 300, (size_t) S - 600) > .99, "1/4 interval follows the host tempo at 90 bpm (%d samples)", S); }
    { // no allocation
      Rig r (44100, 512); r.p.setCurrentProgram (3); r.set (ids::stutterOn, 1); auto warm = r.run (noise (44100));
      juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; std::mt19937 g (2); std::uniform_real_distribution<float> d (-.4f, .4f); allocCount = 0; int blocks = 400;
      for (int k = 0; k < blocks; ++k) { for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, d (g)); trackAlloc = true; r.p.processBlock (b, m); trackAlloc = false; r.head.ppq += 512 * 2.0 / 44100; }
      CHECK (allocCount.load() == 0, "no heap allocation on the audio thread over %d blocks (%ld allocations)", blocks, allocCount.load()); }
    { // CPU
      for (auto scene : { 0, 3, 6 }) { Rig r (48000, 256); r.p.setCurrentProgram (scene); auto in = noise (48000 * 5); auto t0 = juce::Time::getMillisecondCounterHiRes(); r.run (in); auto ms = juce::Time::getMillisecondCounterHiRes() - t0; printf ("  info  scene '%s': %.1f%% of one core on this machine\n", r.p.getProgramName (scene).toRawUTF8(), ms / 50.0); }
    }
}
// ---- GUI checks: drive the real editor with mouse events, then save what it draws -----------------
static juce::MouseEvent mouseEv (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods, bool dragged, juce::Point<float> downPos)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                             juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY,
                             &c, &c, now, downPos, now, 1, dragged);
}
static void saveSnap (HiccupEditor& ed, const juce::String& path, float scale = 2.0f)
{
    auto img = ed.getContent().createComponentSnapshot (ed.getContent().getLocalBounds(), true, scale);
    juce::File f (path); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
}
static void runGuiSnapshots (const char* outDir)
{
    using namespace hiccup::gui;
    const juce::String dir (outDir);
    puts ("\n[gui] editor driven with mouse events");
    HiccupProcessor p; TestHead head; p.setPlayHead (&head); p.prepareToPlay (44100, 512);
    std::unique_ptr<HiccupEditor> ed (dynamic_cast<HiccupEditor*> (p.createEditor()));
    ed->setVisible (true);
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; std::mt19937 g (3); std::uniform_real_distribution<float> d (-.4f, .4f);
    auto pump = [&] (int frames) { for (int f = 0; f < frames; ++f) { for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, d (g)); p.processBlock (b, m); head.ppq += 512 * 2.0 / 44100; ed->advance (1.0f / 60.0f); } };
    auto val = [&] (const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); };
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    ed->getPortrait()->clear();
    pump (40);
    saveSnap (*ed, dir + "/01_default.png");
    CHECK (p.getName() == "Interval", "the plugin reports its new name: %s", p.getName().toRawUTF8());

    { // every parameter of the plugin has a control on screen
      int missing = 0; for (auto* par : p.getParameters()) { auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par); const auto id = rp->getParameterID(); const bool isEngage = id.endsWith ("_on"), isSeed = id == ids::glitchSeed; if (! isEngage && ! isSeed && ed->findRow (id) == nullptr && ed->findChoice (id) == nullptr) { printf ("    no control for %s\n", id.toRawUTF8()); missing++; } }
      CHECK (missing == 0, "all %d parameters are reachable from the new GUI", p.getParameters().size()); }

    { // drag a row: the value must fall and the spring must follow smoothly
      auto& k = *ed->findRow (ids::stutterMix); const float before = val (ids::stutterMix); const juce::Point<float> c ((float) k.getWidth() * .5f, (float) k.getHeight() * .5f);
      k.mouseDown (mouseEv (k, c, left, false, c));
      for (int i = 1; i <= 12; ++i) k.mouseDrag (mouseEv (k, c + juce::Point<float> (0, (float) i * 6.0f), left, true, c));
      const float after = val (ids::stutterMix);
      CHECK (after < before - 15, "dragging a row changes its parameter: mix %.0f -> %.0f", before, after);
      float last = k.getDisplayedProportion(); float worst = 0; const float tgt = (float) k.valueToProportionOfLength (k.getValue()); bool mono = true; float prevGap = std::abs (last - tgt);
      for (int f = 0; f < 40; ++f) { ed->advance (1.0f / 60.0f); const float now = k.getDisplayedProportion(); worst = std::max (worst, std::abs (now - last)); const float gap = std::abs (now - tgt); mono = mono && gap <= prevGap + 1e-4f; prevGap = gap; last = now; }
      CHECK (prevGap < .001f && mono, "the thumb glides to its new position with no overshoot (largest frame step %.3f)", worst);
      k.mouseUp (mouseEv (k, c, juce::ModifierKeys(), true, c)); }
    { // horizontal drag works too, and stepped parameters step
      auto& k = *ed->findRow (ids::stutterInterval); const int before = (int) val (ids::stutterInterval); const juce::Point<float> c ((float) k.getWidth() * .5f, (float) k.getHeight() * .5f);
      k.mouseDown (mouseEv (k, c, left, false, c));
      k.mouseDrag (mouseEv (k, c + juce::Point<float> (80, 0), left, true, c)); k.mouseUp (mouseEv (k, c, juce::ModifierKeys(), true, c));
      CHECK ((int) val (ids::stutterInterval) != before, "intervals steps through divisions on a horizontal drag: index %d -> %d", before, (int) val (ids::stutterInterval));
      k.setValue (5.0, juce::sendNotificationSync); }
    { auto& f = *ed->getEngage (2); const bool before = val (ids::pitchOn) > .5f; const juce::Point<float> c (38, 20);
      f.mouseDown (mouseEv (f, c, left, false, c)); f.mouseUp (mouseEv (f, c, juce::ModifierKeys(), false, c));
      CHECK ((val (ids::pitchOn) > .5f) != before, "a quick click on the engage button latches the module");
      const bool latched = val (ids::pitchOn) > .5f;
      f.mouseDown (mouseEv (f, c, left, false, c)); const bool during = val (ids::pitchOn) > .5f; juce::Thread::sleep (450); f.mouseUp (mouseEv (f, c, juce::ModifierKeys(), false, c));
      CHECK (during != latched && (val (ids::pitchOn) > .5f) == latched, "a long press is momentary: it flips while held and back on release"); }
    { auto& f = *ed->getEngage (3); const int before = (int) val (ids::glitchSeed); const juce::Point<float> c (38, 20); f.mouseDown (mouseEv (f, c, left, false, c)); f.mouseUp (mouseEv (f, c, juce::ModifierKeys(), false, c));
      CHECK ((int) val (ids::glitchSeed) != before, "reroll draws a new pattern: seed %d -> %d", before, (int) val (ids::glitchSeed));
      CHECK (ed->getBar (1)->getShownText().startsWith ("reroll"), "the search bar shows it: '%s'", ed->getBar (1)->getShownText().toRawUTF8()); }
    { auto& t = *ed->findChoice (ids::glitchGrid); const juce::Point<float> c ((float) t.getWidth() - 20.0f, 17); t.mouseDown (mouseEv (t, c, left, false, c));
      CHECK ((int) val (ids::glitchGrid) == 2, "clicking the last option of a 3-way row selects it");
      auto& r = *ed->findChoice (ids::stutterReverse); const bool before = val (ids::stutterReverse) > .5f; r.mouseDown (mouseEv (r, c, left, false, c));
      CHECK ((val (ids::stutterReverse) > .5f) != before, "the 2-way row flips reverse"); r.mouseDown (mouseEv (r, c, left, false, c)); }
    pump (30);
    { auto& k = *ed->findRow (ids::pitchTranspose); k.setValue (7.0, juce::sendNotificationSync); ed->getBar (2)->showMessage ("transpose +7 st"); pump (30); }
    saveSnap (*ed, dir + "/03_after_edits.png");
    { ed->pressArrow (1, 1); ed->pressArrow (1, 1); pump (4); const auto mid = ed->getBar (1)->getShownText(); pump (20); CHECK (val (ids::glitchGrains) > 30 && ed->getBar (1)->getShownText() == "scene tape dream", "scene arrows load scenes and type the name: '%s' then '%s'", mid.toRawUTF8(), ed->getBar (1)->getShownText().toRawUTF8()); saveSnap (*ed, dir + "/04_scene_message.png"); pump (200); saveSnap (*ed, dir + "/05_tape_dream.png"); }
    { ed->pressArrow (1, 1); ed->pressArrow (2, 1); pump (160); saveSnap (*ed, dir + "/06_crystal_all_on.png"); }
    { // bypass: strips cover the values, and all three together
      ed->getContent(); for (const auto* id : { &ids::stutterOn, &ids::glitchOn, &ids::pitchOn }) { auto* par = p.apvts.getParameter (*id); par->setValueNotifyingHost (0.0f); } pump (6); saveSnap (*ed, dir + "/08_strips_moving.png"); pump (60); saveSnap (*ed, dir + "/09_all_bypassed.png"); }
    { for (const auto* id : { &ids::stutterOn, &ids::glitchOn, &ids::pitchOn }) { auto* par = p.apvts.getParameter (*id); par->setValueNotifyingHost (1.0f); } pump (40); }
    { ed->setSize (1000, juce::roundToInt (1000.0 * 580.0 / 1396.0)); ed->advance (.016f); auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f); juce::File f (dir + "/07_small_900.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
      CHECK (ed->getWidth() == 1000, "window resizes with its proportions locked"); ed->setSize (1396, 580); }
    { // portrait panel, exercised with a neutral generated picture
      auto* pp = ed->getPortrait(); CHECK (! pp->hasImage(), "the plugin ships with an empty portrait panel");
      juce::Image test (juce::Image::RGB, 1600, 900, true); { juce::Graphics tg (test); juce::ColourGradient cg (juce::Colour (0xff8fb0c8), 0, 0, juce::Colour (0xff2b3a55), 1600, 900, false); tg.setGradientFill (cg); tg.fillAll(); tg.setColour (juce::Colour (0xffe2c47e)); tg.fillEllipse (640, 200, 320, 320); tg.setColour (juce::Colour (0xff1e2336)); tg.fillEllipse (420, 560, 760, 700); }
      juce::File tf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("interval_test_picture.png"); tf.deleteFile(); { juce::FileOutputStream os (tf); juce::PNGImageFormat().writeImageToStream (test, os); }
      juce::StringArray dropped; dropped.add (tf.getFullPathName());
      CHECK (pp->isInterestedInFileDrag (dropped), "the panel accepts a dragged image file");
      juce::StringArray notImage; notImage.add ("/tmp/notes.txt"); CHECK (! pp->isInterestedInFileDrag (notImage), "and ignores other files");
      pp->filesDropped (dropped, 10, 10); CHECK (pp->hasImage(), "dropping the file shows it"); pump (10); saveSnap (*ed, dir + "/10_with_test_picture.png");
      pp->nudgePan (-40.0f, 0.0f);
      HiccupProcessor p2; p2.prepareToPlay (44100, 512); std::unique_ptr<HiccupEditor> ed2 (dynamic_cast<HiccupEditor*> (p2.createEditor()));
      CHECK (ed2->getPortrait()->hasImage(), "a new instance restores the picture from the settings folder"); ed2.reset();
      juce::MemoryBlock st; p.getStateInformation (st); CHECK (st.getSize() < 8000, "the picture is not written into the session state (%d bytes)", (int) st.getSize());
      pp->clear(); CHECK (! pp->hasImage() && PortraitPanel::storageFolder().findChildFiles (juce::File::findFiles, false, "portrait.*").isEmpty(), "removing it deletes the private copy"); tf.deleteFile(); }
    { // paint timing on this machine, software rendered
      auto* row = ed->findRow (ids::stutterMix); auto t0 = juce::Time::getMillisecondCounterHiRes(); for (int i = 0; i < 20; ++i) { auto img = row->createComponentSnapshot (row->getLocalBounds(), false, 2.0f); } double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / 20.0;
      auto t1 = juce::Time::getMillisecondCounterHiRes(); for (int i = 0; i < 5; ++i) { auto img = ed->getContent().createComponentSnapshot (ed->getContent().getLocalBounds(), true, 2.0f); } double ms2 = (juce::Time::getMillisecondCounterHiRes() - t1) / 5.0;
      printf ("  info  repainting at retina scale: one row %.2f ms, the whole window %.1f ms (software renderer on this machine)\n", ms, ms2); }
    printf ("\n%d passed, %d failed\n", passes, fails);
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "snap") { runGuiSnapshots (argv[2]); return fails == 0 ? 0 : 1; }
    testBypass(); testStutter(); testRhythmPitchGlitch(); testGrains(); testPitch(); testHostBehaviour();
    printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
