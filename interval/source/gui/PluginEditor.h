#pragma once
#include "../PluginProcessor.h"
#include "Design.h"
#include <memory>
#include <vector>

namespace hiccup::gui
{
/** Anything that moves. All motion is driven from one vblank callback with real elapsed time,
    so animation stays smooth at any refresh rate. */
struct Animated
{
    virtual ~Animated() = default;
    virtual void tick (float dt) = 0;
};

/** Second-order follower. Critically damped, so settings glide into place with no overshoot. */
struct Spring
{
    float value = 0.0f, velocity = 0.0f, omega = 34.0f;
    bool step (float target, float dt) noexcept
    {
        const float before = value;
        const float d = omega * dt;
        const float e = std::exp (-d);
        const float x = value - target;
        const float temp = velocity + omega * x;
        value = target + (x + temp * dt) * e;
        velocity = (velocity - omega * temp * dt) * e;
        if (std::abs (value - target) < 1.0e-4f && std::abs (velocity) < 1.0e-3f) { value = target; velocity = 0.0f; }
        return value != before;
    }
};

/** Simple one-pole ease used for fades. */
inline bool ease (float& v, float target, float dt, float seconds) noexcept
{
    const float before = v;
    v += (target - v) * (1.0f - std::exp (-dt / seconds));
    if (std::abs (v - target) < 0.002f) v = target;
    return v != before;
}

/** Small chevrons at the end of the search bar, for stepping through presets. */
class ArrowButton final : public juce::Component, public Animated
{
public:
    ArrowButton (bool pointsUp, std::function<void()> action);
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override { hover = false; }
    void mouseDown (const juce::MouseEvent&) override { pressed = 1.0f; if (action) action(); }

private:
    bool up, hover = false;
    std::function<void()> action;
    float hoverAnim = 0.0f, pressed = 0.0f;
};

/** The head of each card: a magnifier and a search field. The field doubles as the read-out, so
    moving a control or loading a preset "types" into it. The lens lights while the module is engaged. */
class SearchBar final : public juce::Component, public Animated
{
public:
    explicit SearchBar (const juce::String& moduleName);
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void showMessage (const juce::String& text, float seconds = 1.8f, bool typeOut = false);
    void flash (float amount = 1.0f) noexcept { flashLevel = juce::jmax (flashLevel, amount); }
    float lensTarget = 0.0f;
    juce::String getShownText() const { return text.substring (0, shownChars); }

private:
    juce::String module, text;
    int shownChars = 0;
    float typeClock = 0.0f, messageTimer = 0.0f, caretClock = 0.0f, lensLevel = 0.0f, flashLevel = 0.0f;
    bool lastCaretOn = false;
};

/** A soft capsule of paper, as laid over the answers on the card in the photos. It covers the region of
    a bypassed module and pulls away to the right. reveal = 0 is fully covering, 1 is gone. */
void drawPaperStrip (juce::Graphics&, juce::Rectangle<float> area, float reveal, int seed);

/** A parameter as a search suggestion: grey module name, bold label and value, and a capsule track below.
    The hero variant carries a module's main control in large type beside a round dial. */
class ParamRow final : public juce::Slider, public Animated
{
public:
    enum class Kind { row, hero };
    ParamRow (Kind, const juce::String& prefix, const juce::String& label, juce::RangedAudioParameter&, SearchBar&, int stripSeed);
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void mouseDown (const juce::MouseEvent&) override;
    void valueChanged() override;
    float getDisplayedProportion() const noexcept { return spring.value; }
    bool covered = false;      // the owning module is bypassed

private:
    juce::String valueText() const;
    Kind kind;
    juce::String prefix, label;
    juce::RangedAudioParameter& param;
    SearchBar& bar;
    int seed, steps = 0;
    float prefixWidth = 0.0f, hoverAmount = 0.0f, reveal = 1.0f;
    Spring spring;
    void paintDial (juce::Graphics&, juce::Point<float> centre, float radius, float alpha);
};

/** A parameter with a few named settings: all are printed and a soft pill glides to the selected one. */
class ChoiceRow final : public juce::Component, public Animated
{
public:
    ChoiceRow (const juce::String& prefix, const juce::String& label, const juce::StringArray& options,
               juce::RangedAudioParameter&, SearchBar&, int stripSeed);
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override { hover = false; }
    bool covered = false;

private:
    juce::String prefix, label;
    juce::StringArray options;
    juce::RangedAudioParameter& param;
    SearchBar& bar;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::vector<juce::Rectangle<float>> optionAreas;
    int current = 0, seed;
    bool hover = false;
    float prefixWidth = 0.0f, labelWidth = 0.0f, reveal = 1.0f;
    Spring pillX, pillW;
};

/** The engage switch: a navy button like the suit. Click to latch; press and hold is momentary.
    The same control in action mode rerolls the pattern. */
class EngageButton final : public juce::Component, public Animated
{
public:
    enum class Mode { latching, action };
    EngageButton (const juce::String& labelOn, const juce::String& labelOff, juce::RangedAudioParameter* param, Mode, std::function<void()> onAction = {});
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override { hover = false; }
    bool isOn() const noexcept { return on; }
    void flash() noexcept { flashLevel = 1.0f; }

private:
    juce::String labelOn, labelOff;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    Mode mode;
    std::function<void()> onAction;
    bool on = false, held = false, hover = false;
    juce::uint32 pressTime = 0;
    float pressAnim = 0.0f, onAnim = 0.0f, hoverAnim = 0.0f, flashLevel = 0.0f;
};

/** A large rounded panel for a picture of the user's choosing. Nothing ships with the plugin: the user drops
    an image on it, it is kept in their own settings folder, and every instance shows it from then on.
    Drag to reposition, double-click to replace, and the round button on hover removes it. */
class PortraitPanel final : public juce::Component, public juce::FileDragAndDropTarget, public Animated
{
public:
    PortraitPanel();
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override { hover = false; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true; }
    void fileDragExit (const juce::StringArray&) override { dropping = false; }
    void filesDropped (const juce::StringArray&, int, int) override;

    bool loadFromFile (const juce::File&);
    void clear();
    bool hasImage() const noexcept { return display.isValid(); }
    void nudgePan (float dx, float dy);
    static juce::File storageFolder();

private:
    void restore();
    void prepareDisplay (const juce::Image&);
    void savePan() const;
    void openChooser();
    juce::Rectangle<float> removeButtonArea() const;
    juce::Image display;
    float panX = 0.5f, panY = 0.5f, panAtDownX = 0.5f, panAtDownY = 0.5f;
    bool hover = false, dropping = false, pressedRemove = false;
    float hoverAnim = 0.0f, dropAnim = 0.0f;
    std::unique_ptr<juce::FileChooser> chooser;
};

/** The live 2-bar glitch pattern, printed like a small chart: rhythm events above, pitch events below. */
class PatternPanel final : public juce::Component, public Animated
{
public:
    explicit PatternPanel (HiccupProcessor&);
    void paint (juce::Graphics&) override;
    void tick (float dt) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override { hover = false; }
    bool covered = false;

private:
    HiccupProcessor& proc;
    float playPos = 0.0f, reveal = 1.0f;
    bool hover = false;
    std::array<float, 64> glow {};
    struct Spark { float x, y, life; };
    std::array<Spark, 24> sparks {};
    uint32_t lastSpawned = 0;
    FastRandom rng;
    int lastStep = -1;
};

class Backdrop final : public juce::Component
{
public:
    Backdrop();
    void paint (juce::Graphics&) override;

private:
    juce::Image image;
};

/** Only the window's resize grip needs restyling: three soft dots instead of hard diagonal lines. */
class Look final : public juce::LookAndFeel_V4
{
public:
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};
} // namespace hiccup::gui

class HiccupEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HiccupEditor (HiccupProcessor&);
    ~HiccupEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    /** One animation step. Called by the vblank attachment, and directly by the offline tests. */
    void advance (float dt);
    juce::Component& getContent() noexcept { return content; }

    // for the offline GUI checks
    hiccup::gui::ParamRow* findRow (const juce::String& paramID) { return rows.count (paramID) ? rows[paramID] : nullptr; }
    hiccup::gui::ChoiceRow* findChoice (const juce::String& paramID) { return choices.count (paramID) ? choices[paramID] : nullptr; }
    hiccup::gui::EngageButton* getEngage (int index) { return engages[(size_t) index].get(); }
    hiccup::gui::SearchBar* getBar (int index) { return bars[(size_t) index].get(); }
    hiccup::gui::PortraitPanel* getPortrait() { return portrait.get(); }
    void pressArrow (int group, int delta) { stepPreset (group, delta); }

private:
    void stepPreset (int group, int delta);
    HiccupProcessor& proc;
    hiccup::gui::Look look;
    juce::Component content;
    std::unique_ptr<hiccup::gui::Backdrop> backdrop;
    std::unique_ptr<hiccup::gui::PatternPanel> pattern;
    std::unique_ptr<hiccup::gui::PortraitPanel> portrait;
    std::vector<std::unique_ptr<hiccup::gui::SearchBar>> bars;
    std::vector<std::unique_ptr<hiccup::gui::ParamRow>> rowStore;
    std::vector<std::unique_ptr<hiccup::gui::ChoiceRow>> choiceStore;
    std::map<juce::String, hiccup::gui::ParamRow*> rows;
    std::map<juce::String, hiccup::gui::ChoiceRow*> choices;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<hiccup::gui::EngageButton>> engages;
    std::vector<std::unique_ptr<hiccup::gui::ArrowButton>> arrows;
    std::vector<hiccup::gui::Animated*> animated;
    uint32_t lastHits = 0;
    int presetIndex[3] {};
    double lastTime = 0.0;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::VBlankAttachment> vblank;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HiccupEditor)
};

juce::AudioProcessorEditor* createHiccupEditor (HiccupProcessor&);
