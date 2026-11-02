#include "PluginEditor.h"
#include "BinaryData.h"

using namespace hiccup;
using namespace hiccup::gui;
namespace D = hiccup::design;
using juce::Colour;
using juce::Graphics;
using juce::Image;
using juce::String;

namespace
{
juce::Typeface::Ptr typeface (const char* name)
{
    int size = 0;
    const auto* data = BinaryData::getNamedResource (name, size);
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}
juce::Font regularFont (float h)
{
    static const juce::Font f { juce::FontOptions { typeface ("Nunito500_ttf") } };
    return f.withHeight (h);
}
juce::Font boldFont (float h)
{
    static const juce::Font f { juce::FontOptions { typeface ("Nunito800_ttf") } };
    return f.withHeight (h);
}
float textWidth (const juce::Font& font, const String& text)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0, 0);
    return ga.getBoundingBox (0, -1, true).getWidth();
}
constexpr float rowFont = 15.5f, heroValueFont = 34.0f;
constexpr float dialStart = -2.35619449f, dialEnd = 2.35619449f;   // 270 degree sweep
}

// ==== paper strip =============================================================================
void hiccup::gui::drawPaperStrip (Graphics& g, juce::Rectangle<float> area, float reveal, int seed)
{
    if (reveal >= 0.995f) return;

    // pull away to the right: the left end travels, the right end stays
    const float eased = reveal * reveal * (3.0f - 2.0f * reveal);
    const auto r = area.withTrimmedLeft ((area.getWidth() - area.getHeight()) * eased);

    // each capsule sits a fraction of a degree off true, the same every time, as if laid by hand
    FastRandom rng; rng.seed ((uint32_t) seed * 977u + 13u);
    const float tilt = rng.bipolar() * 0.0055f;
    const auto t = juce::AffineTransform::rotation (tilt, r.getCentreX(), r.getCentreY());
    const float radius = r.getHeight() * 0.5f;
    juce::Path p;
    p.addRoundedRectangle (r, radius);
    p.applyTransform (t);

    const float opacity = juce::jlimit (0.0f, 1.0f, (1.0f - eased) * 5.0f);
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (D::navy.withAlpha (0.04f * opacity));
        g.fillPath (p, juce::AffineTransform::translation (0.0f, (float) i * 0.9f + 0.3f));
    }
    juce::ColourGradient paper (D::strip.withAlpha (opacity), r.getX(), r.getY(),
                                Colour (0xffeff3f5).withAlpha (opacity), r.getRight(), r.getBottom(), false);
    g.setGradientFill (paper);
    g.fillPath (p);
}

// ==== arrows ==================================================================================
ArrowButton::ArrowButton (bool pointsUp, std::function<void()> a) : up (pointsUp), action (std::move (a))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}
void ArrowButton::tick (float dt)
{
    bool dirty = ease (hoverAnim, hover ? 1.0f : 0.0f, dt, 0.08f);
    if (pressed > 0.0f) { pressed = juce::jmax (0.0f, pressed - dt * 5.0f); dirty = true; }
    if (dirty) repaint();
}
void ArrowButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = b.getCentre();
    g.setColour (Colour (0xfff4f7f9).interpolatedWith (D::shirt, juce::jlimit (0.0f, 1.0f, hoverAnim * 0.75f + pressed * 0.5f)));
    g.fillEllipse (b);
    juce::Path chevron;
    const float w = 4.6f, h = up ? -2.6f : 2.6f;
    chevron.startNewSubPath (c.x - w, c.y - h);
    chevron.lineTo (c.x, c.y + h);
    chevron.lineTo (c.x + w, c.y - h);
    g.setColour (D::greyInk.interpolatedWith (D::ink, hoverAnim));
    g.strokePath (chevron, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ==== search bar ==============================================================================
SearchBar::SearchBar (const String& moduleName) : module (moduleName), text (moduleName), shownChars (moduleName.length())
{
    setInterceptsMouseClicks (false, true);
}
void SearchBar::showMessage (const String& t, float seconds, bool typeOut)
{
    text = t;
    messageTimer = seconds;
    caretClock = 0.0f;
    shownChars = typeOut ? juce::jmin (module.length(), t.length()) : t.length();
    typeClock = 0.0f;
    repaint();
}
void SearchBar::tick (float dt)
{
    bool dirty = false;
    if (messageTimer > 0.0f)
    {
        messageTimer -= dt;
        if (messageTimer <= 0.0f)
        {
            text = module;
            shownChars = module.length();
            dirty = true;
        }
    }
    if (shownChars < text.length())
    {
        typeClock += dt * 70.0f;   // letters per second
        const int add = (int) typeClock;
        if (add > 0)
        {
            typeClock -= (float) add;
            shownChars = juce::jmin (text.length(), shownChars + add);
            dirty = true;
        }
    }
    dirty = ease (lensLevel, lensTarget, dt, 0.06f) || dirty;
    if (flashLevel > 0.0f) { flashLevel = juce::jmax (0.0f, flashLevel - dt * 7.5f); dirty = true; }

    caretClock += dt;
    const bool caretOn = std::fmod (caretClock, 1.06f) < 0.56f;
    if (caretOn != lastCaretOn) { lastCaretOn = caretOn; dirty = true; }
    if (dirty) repaint();
}
void SearchBar::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    // the field is a soft recessed capsule
    g.setColour (D::tray.withAlpha (0.85f));
    g.fillRoundedRectangle (b, h * 0.5f);
    juce::ColourGradient inner (D::navy.withAlpha (0.07f), 0.0f, 0.0f, D::navy.withAlpha (0.0f), 0.0f, 6.0f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (b, h * 0.5f);

    // magnifier: the lens fills while the module is engaged and pulses with its activity
    const float cx = 20.0f, cy = h * 0.5f - 1.5f, r = 8.8f;
    const float fill = juce::jlimit (0.0f, 1.0f, lensLevel * 0.62f + flashLevel * 0.38f);
    if (fill > 0.003f)
    {
        juce::ColourGradient lensGrad (D::shirt.withAlpha (fill), cx - 3.0f, cy - 4.0f, D::lens.withAlpha (fill * 0.75f), cx + 5.0f, cy + 6.0f, false);
        g.setGradientFill (lensGrad);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }
    const float hx = cx + r * 0.72f, hy = cy + r * 0.72f;
    juce::Path handle;
    handle.startNewSubPath (hx, hy);
    handle.lineTo (hx + 5.6f, hy + 5.6f);
    g.setColour (D::lensDark);
    g.strokePath (handle, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (D::lens);
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 2.3f);

    // the text
    const float x = 40.0f;
    const auto font = regularFont (16.0f);
    const auto shown = text.substring (0, shownChars);
    g.setFont (font);
    g.setColour (D::greyInk.darker (0.1f));
    g.drawText (shown, juce::Rectangle<float> (x, 0.0f, b.getWidth() - x - 66.0f, h), juce::Justification::centredLeft, false);
    if (lastCaretOn)
    {
        const float cw = textWidth (font, shown);
        g.setColour (D::ink.withAlpha (0.75f));
        g.fillRoundedRectangle (x + cw + 2.0f, h * 0.5f - 9.0f, 1.6f, 18.0f, 0.8f);
    }
}

// ==== parameter row ===========================================================================
ParamRow::ParamRow (Kind k, const String& pre, const String& lab, juce::RangedAudioParameter& par, SearchBar& b, int stripSeed)
    : kind (k), prefix (pre), label (lab), param (par), bar (b), seed (stripSeed)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (240);
    setScrollWheelEnabled (true);
    setVelocityBasedMode (false);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setTitle (pre + " " + lab);
    prefixWidth = textWidth (regularFont (rowFont), prefix);
    // detents are drawn for parameters with a modest number of positions
    if (const auto n = par.getNumSteps(); n >= 2 && n <= 49) steps = n;
}
String ParamRow::valueText() const { return param.getText (param.convertTo0to1 ((float) getValue()), 64); }

void ParamRow::tick (float dt)
{
    const auto target = (float) valueToProportionOfLength (getValue());
    bool dirty = spring.step (target, dt);
    const bool active = isMouseOver() || isMouseButtonDown();
    dirty = ease (hoverAmount, active ? 1.0f : 0.0f, dt, 0.08f) || dirty;
    dirty = ease (reveal, (covered && ! active) ? 0.0f : 1.0f, dt, covered && ! active ? 0.09f : 0.07f) || dirty;
    if (dirty) repaint();
}

void ParamRow::paintDial (Graphics& g, juce::Point<float> c, float radius, float alpha)
{
    const float thick = 7.5f;
    const float angle = dialStart + spring.value * (dialEnd - dialStart);
    auto arc = [&] (float from, float to)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, from, to, true);
        return p;
    };
    const juce::PathStrokeType stroke (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (D::faint.withMultipliedAlpha (0.4f + 0.6f * alpha));
    g.strokePath (arc (dialStart, dialEnd), stroke);
    if (spring.value > 0.001f)
    {
        g.setColour (D::ink.withMultipliedAlpha (alpha));
        g.strokePath (arc (dialStart, angle), stroke);
    }
    // a round cap sits on the end of the arc, and the middle of the dial is a soft raised button
    const float bx = c.x + radius * std::sin (angle), by = c.y - radius * std::cos (angle);
    const float tr = 8.0f + hoverAmount * 1.4f;
    g.setColour (D::navy.withAlpha (0.2f * alpha));
    g.fillEllipse (bx - tr - 0.5f, by - tr + 1.0f, (tr + 0.5f) * 2.0f, (tr + 0.5f) * 2.0f);
    g.setColour (D::navy.withMultipliedAlpha (alpha));
    g.fillEllipse (bx - tr, by - tr, tr * 2.0f, tr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
    g.fillEllipse (bx - 2.2f, by - 2.2f, 4.4f, 4.4f);
}

void ParamRow::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (hoverAmount > 0.003f)
    {
        g.setColour (D::shirt.withAlpha (0.34f * hoverAmount));
        g.fillRoundedRectangle (bounds.reduced (0.0f, 1.0f), juce::jmin (22.0f, bounds.getHeight() * 0.5f));
    }

    const bool hero = kind == Kind::hero;
    const float left = 10.0f, right = bounds.getRight() - 10.0f;
    const float trackAlpha = reveal * reveal;

    // text: grey prefix, bold completion, like a search suggestion
    auto line = hero ? juce::Rectangle<float> (left, 8.0f, right - left, 24.0f)
                     : juce::Rectangle<float> (left, 2.0f, right - left, bounds.getHeight() - 22.0f);
    g.setFont (regularFont (rowFont));
    g.setColour (D::greyInk);
    g.drawText (prefix, line, juce::Justification::centredLeft, false);
    const float afterPrefix = left + prefixWidth + 6.0f;
    g.setFont (boldFont (rowFont));
    g.setColour (D::ink);
    g.drawText (hero ? label : label + "  " + valueText(), line.withLeft (afterPrefix), juce::Justification::centredLeft, false);

    if (hero)
    {
        const float dialR = 29.0f;
        const juce::Point<float> dc (bounds.getRight() - dialR - 14.0f, bounds.getCentreY() + 1.0f);
        paintDial (g, dc, dialR, trackAlpha);

        g.setFont (boldFont (heroValueFont));
        g.setColour (D::navy);
        const auto valueArea = juce::Rectangle<float> (left - 1.0f, 34.0f, dc.x - dialR - left - 8.0f, 46.0f);
        g.drawFittedText (valueText(), valueArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        drawPaperStrip (g, juce::Rectangle<float> (left - 3.0f, 38.0f, valueArea.getWidth() + 3.0f, 38.0f), reveal, seed);
        return;
    }

    // capsule track with a round thumb; a covered row is only its prefix and strip
    const float trackH = 6.0f, trackY = bounds.getBottom() - 12.0f;
    const float thumbX = left + 2.0f + spring.value * (right - left - 4.0f);
    g.setColour (D::faint.withMultipliedAlpha (trackAlpha));
    g.fillRoundedRectangle (left, trackY - trackH * 0.5f, right - left, trackH, trackH * 0.5f);
    g.setColour (D::ink.withMultipliedAlpha (trackAlpha));
    g.fillRoundedRectangle (left, trackY - trackH * 0.5f, juce::jmax (trackH, thumbX - left), trackH, trackH * 0.5f);
    if (steps > 0 && steps <= 17)
    {
        for (int i = 0; i < steps; ++i)
        {
            const float tx = left + 2.0f + (right - left - 4.0f) * (float) i / (float) (steps - 1);
            g.setColour ((tx < thumbX ? juce::Colours::white.withAlpha (0.55f) : D::greyInk.withAlpha (0.55f)).withMultipliedAlpha (trackAlpha));
            g.fillEllipse (tx - 1.1f, trackY - 1.1f, 2.2f, 2.2f);
        }
    }
    const float tr = 7.5f + hoverAmount * 1.3f;
    g.setColour (D::navy.withAlpha (0.2f * trackAlpha));
    g.fillEllipse (thumbX - tr - 0.5f, trackY - tr + 1.0f, (tr + 0.5f) * 2.0f, (tr + 0.5f) * 2.0f);
    g.setColour (D::navy.withMultipliedAlpha (trackAlpha));
    g.fillEllipse (thumbX - tr, trackY - tr, tr * 2.0f, tr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f * trackAlpha));
    g.fillEllipse (thumbX - 2.0f, trackY - 2.0f, 4.0f, 4.0f);

    drawPaperStrip (g, juce::Rectangle<float> (afterPrefix - 3.0f, line.getCentreY() - 13.5f, bounds.getRight() - afterPrefix + 1.0f, 27.0f), reveal, seed);
}

void ParamRow::mouseDown (const juce::MouseEvent& e)
{
    // hold shift for fine control
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1200 : 240);
    juce::Slider::mouseDown (e);
}

void ParamRow::valueChanged()
{
    if (isMouseOver() || isMouseButtonDown())
        bar.showMessage (label + " " + valueText());
}

// ==== choice row ==============================================================================
ChoiceRow::ChoiceRow (const String& pre, const String& lab, const juce::StringArray& opts, juce::RangedAudioParameter& par, SearchBar& b, int stripSeed)
    : prefix (pre), label (lab), options (opts), param (par), bar (b), seed (stripSeed)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    prefixWidth = textWidth (regularFont (rowFont), prefix);
    labelWidth = textWidth (boldFont (rowFont), label);
    pillX.omega = pillW.omega = 30.0f;
    attachment = std::make_unique<juce::ParameterAttachment> (par, [this] (float v)
    {
        current = juce::jlimit (0, options.size() - 1, juce::roundToInt (v));
    }, nullptr);
    attachment->sendInitialUpdate();
}
void ChoiceRow::resized()
{
    // options sit in equal cells inside a capsule tray, to the right of the label
    optionAreas.clear();
    const float left = 10.0f + prefixWidth + 6.0f + labelWidth + 12.0f;
    const float right = (float) getWidth() - 2.0f;
    const float h = (float) getHeight();
    const float cell = (right - left) / (float) options.size();
    for (int i = 0; i < options.size(); ++i)
        optionAreas.push_back ({ left + cell * (float) i, 0.0f, cell, h });
    if (! optionAreas.empty())
    {
        pillX.value = optionAreas[(size_t) current].getCentreX();
        pillW.value = optionAreas[(size_t) current].getWidth();
    }
}
void ChoiceRow::tick (float dt)
{
    if (optionAreas.empty()) return;
    bool dirty = pillX.step (optionAreas[(size_t) current].getCentreX(), dt);
    dirty = pillW.step (optionAreas[(size_t) current].getWidth(), dt) || dirty;
    dirty = ease (reveal, (covered && ! hover) ? 0.0f : 1.0f, dt, covered && ! hover ? 0.09f : 0.07f) || dirty;
    if (dirty) repaint();
}
void ChoiceRow::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float left = 10.0f;
    g.setFont (regularFont (rowFont));
    g.setColour (D::greyInk);
    g.drawText (prefix, b.withLeft (left), juce::Justification::centredLeft, false);
    const float afterPrefix = left + prefixWidth + 6.0f;
    g.setFont (boldFont (rowFont));
    g.setColour (D::ink);
    g.drawText (label, b.withLeft (afterPrefix), juce::Justification::centredLeft, false);

    if (! optionAreas.empty())
    {
        const float th = b.getHeight() - 5.0f;
        const auto trayArea = juce::Rectangle<float> (optionAreas.front().getX(), b.getCentreY() - th * 0.5f,
                                                      optionAreas.back().getRight() - optionAreas.front().getX(), th);
        g.setColour (D::tray.withAlpha (0.85f));
        g.fillRoundedRectangle (trayArea, th * 0.5f);

        const float pw = pillW.value - 3.0f, ph = th - 4.0f;
        const auto pill = juce::Rectangle<float> (pillX.value - pw * 0.5f, b.getCentreY() - ph * 0.5f, pw, ph);
        g.setColour (D::navy.withAlpha (0.12f));
        g.fillRoundedRectangle (pill.translated (0.0f, 1.0f), ph * 0.5f);
        g.setColour (Colour (0xfff7fafc));
        g.fillRoundedRectangle (pill, ph * 0.5f);

        for (int i = 0; i < options.size(); ++i)
        {
            const bool on = i == current;
            g.setFont (on ? boldFont (13.5f) : regularFont (13.5f));
            g.setColour (on ? D::ink : D::greyInk);
            g.drawText (options[i], optionAreas[(size_t) i], juce::Justification::centred, false);
        }
    }
    drawPaperStrip (g, juce::Rectangle<float> (afterPrefix - 3.0f, 2.5f, b.getRight() - afterPrefix + 1.0f, b.getHeight() - 5.0f), reveal, seed);
}
void ChoiceRow::mouseDown (const juce::MouseEvent& e)
{
    int next = current;
    if (options.size() == 2)
    {
        // a two-way setting flips wherever it is clicked, as the toggle did
        next = current == 0 ? 1 : 0;
    }
    else
    {
        for (int i = 0; i < (int) optionAreas.size(); ++i)
            if (optionAreas[(size_t) i].contains (e.position))
                next = i;
    }
    attachment->setValueAsCompleteGesture ((float) next);
    bar.showMessage (label + " " + options[next]);
}

// ==== engage button ===========================================================================
EngageButton::EngageButton (const String& on_, const String& off_, juce::RangedAudioParameter* par, Mode m, std::function<void()> act)
    : labelOn (on_), labelOff (off_), mode (m), onAction (std::move (act))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (par != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*par, [this] (float v) { on = v > 0.5f; }, nullptr);
        attachment->sendInitialUpdate();
    }
    onAnim = on ? 1.0f : 0.0f;
}
void EngageButton::tick (float dt)
{
    bool dirty = ease (pressAnim, held ? 1.0f : 0.0f, dt, held ? 0.018f : 0.05f);
    dirty = ease (onAnim, (mode == Mode::latching && on) ? 1.0f : 0.0f, dt, 0.06f) || dirty;
    dirty = ease (hoverAnim, hover ? 1.0f : 0.0f, dt, 0.08f) || dirty;
    if (flashLevel > 0.0f) { flashLevel = juce::jmax (0.0f, flashLevel - dt * 3.5f); dirty = true; }
    if (dirty) repaint();
}
void EngageButton::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f, 1.5f);
    b = b.translated (0.0f, pressAnim * 1.2f);
    const float corner = b.getHeight() * 0.5f;
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (D::navy.withAlpha (0.05f * (1.0f - pressAnim)));
        g.fillRoundedRectangle (b.translated (0.0f, (float) i * 1.1f), corner);
    }
    // navy when engaged; plain paper when not
    const auto fill = Colour (0xfff5f8fa).interpolatedWith (D::navy, onAnim);
    g.setColour (fill.interpolatedWith (D::shirt, juce::jlimit (0.0f, 1.0f, flashLevel * 0.8f + hoverAnim * (1.0f - onAnim) * 0.35f)));
    g.fillRoundedRectangle (b, corner);
    if (onAnim > 0.01f)
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.12f * onAnim), 0.0f, b.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, b.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (b, corner);
    }
    // a small round status light at the start of the capsule
    if (mode == Mode::latching)
    {
        const float d = 9.0f, x = b.getX() + 18.0f, y = b.getCentreY() - d * 0.5f;
        g.setColour (D::greyInk.withAlpha (0.45f).interpolatedWith (Colour (0xffd9e8ff), onAnim));
        g.fillEllipse (x, y, d, d);
    }
    g.setFont (boldFont (15.0f));
    g.setColour (D::navy.interpolatedWith (Colour (0xffeef3f8), onAnim));
    g.drawText (on || mode == Mode::action ? labelOn : labelOff, b, juce::Justification::centred, false);
}
void EngageButton::mouseDown (const juce::MouseEvent&)
{
    held = true;
    pressTime = juce::Time::getMillisecondCounter();
    if (mode == Mode::latching && attachment != nullptr)
        attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    if (onAction) onAction();
}
void EngageButton::mouseUp (const juce::MouseEvent&)
{
    held = false;
    // a long press acts as a momentary switch: let go and the module flips back
    if (mode == Mode::latching && attachment != nullptr && juce::Time::getMillisecondCounter() - pressTime > 400)
        attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

// ==== pattern =================================================================================
PatternPanel::PatternPanel (HiccupProcessor& p) : proc (p) {}

void PatternPanel::tick (float dt)
{
    // playhead glides between steps instead of jumping
    const int step = proc.rhythmPitch.currentStep.load (std::memory_order_relaxed);
    const int len = juce::jlimit (1, 64, proc.rhythmPitch.patternLength.load (std::memory_order_relaxed));
    const auto t = (float) step;
    if (t < playPos - (float) len * 0.5f) playPos -= (float) len;
    playPos += (t - playPos) * (1.0f - std::exp (-dt * 26.0f));
    if (step != lastStep)
    {
        if (juce::isPositiveAndBelow (step, 64)) glow[(size_t) step] = 1.0f;
        lastStep = step;
    }
    for (auto& g : glow) g = juce::jmax (0.0f, g - dt * 3.6f);

    // each new grain leaves a little spark on the chart
    const auto spawned = proc.grains.spawned.load (std::memory_order_relaxed);
    auto fresh = (int) (spawned - lastSpawned);
    lastSpawned = spawned;
    for (auto& s : sparks)
    {
        s.life = juce::jmax (0.0f, s.life - dt * 2.0f);
        if (s.life <= 0.0f && fresh > 0)
        {
            s = { rng.unit(), rng.unit(), 1.0f };
            --fresh;
        }
    }
    ease (reveal, (covered && ! hover) ? 0.0f : 1.0f, dt, covered && ! hover ? 0.1f : 0.07f);
    repaint();
}

void PatternPanel::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto rawFloat = [&] (const String& id) { return proc.apvts.getRawParameterValue (id)->load (std::memory_order_relaxed); };
    const float rhythm = rawFloat (ids::glitchRhythm) * 0.01f, pitchAmt = rawFloat (ids::glitchPitch) * 0.01f;
    const auto seed = (uint32_t) rawFloat (ids::glitchSeed);
    const int scale = (int) rawFloat (ids::glitchScale), grid = (int) rawFloat (ids::glitchGrid);
    const int len = juce::jlimit (1, 64, proc.rhythmPitch.patternLength.load (std::memory_order_relaxed));

    g.setColour (D::tray.withAlpha (0.6f));
    g.fillRoundedRectangle (b.reduced (0.0f, 2.0f), 20.0f);
    const float left = 14.0f, right = b.getRight() - 14.0f;
    auto header = juce::Rectangle<float> (left, 7.0f, right - left, 24.0f);
    g.setFont (regularFont (rowFont));
    g.setColour (D::greyInk);
    const auto word = String ("glitch");
    g.drawText (word, header, juce::Justification::centredLeft, false);
    const float after = left + textWidth (regularFont (rowFont), word) + 6.0f;
    g.setFont (boldFont (rowFont));
    g.setColour (D::ink);
    g.drawText ("pattern", header.withLeft (after), juce::Justification::centredLeft, false);
    g.setFont (regularFont (13.5f));
    g.setColour (D::greyInk);
    const auto bpm = proc.uiBpm.load (std::memory_order_relaxed);
    g.drawText (String (gridDivisions[grid].name) + "   " + String (bpm, 1) + " BPM", header, juce::Justification::centredRight, false);

    const auto area = juce::Rectangle<float> (left, 35.0f, right - left, b.getHeight() - 45.0f);
    const float gap = area.getWidth() / (float) len;
    const float pillW = juce::jmax (1.8f, gap - juce::jmin (2.2f, gap * 0.28f));
    const float rowH = (area.getHeight() - 4.0f) * 0.5f;

    for (int i = 0; i < len; ++i)
    {
        const auto ev = pattern::eventAt (seed, i, rhythm, pitchAmt, scale);
        const float x = area.getX() + gap * (float) i + (gap - pillW) * 0.5f;
        const bool hasR = ev.rhythm != pattern::Rhythm::none, hasP = ev.pitch != pattern::Pitch::none;
        const float lit = glow[(size_t) i];

        // top row: rhythm events, in suit navy
        g.setColour (hasR ? D::navy : D::faint.interpolatedWith (D::greyInk, lit * 0.8f));
        const float rh = hasR ? rowH : juce::jmin (rowH * 0.28f, 4.0f);
        g.fillRoundedRectangle (x, area.getY() + (rowH - rh) * 0.5f, pillW, rh, juce::jmin (pillW, rh) * 0.5f);

        // bottom row: pitch events, in tie olive, sitting higher or lower according to the interval
        g.setColour (hasP ? D::olive : D::faint.interpolatedWith (D::greyInk, lit * 0.8f));
        const float py = area.getY() + rowH + 4.0f;
        const float ph = hasP ? rowH * 0.62f : juce::jmin (rowH * 0.28f, 4.0f);
        const float shift = hasP ? juce::jlimit (-1.0f, 1.0f, ev.semitones / 24.0f) * rowH * 0.19f : 0.0f;
        g.fillRoundedRectangle (x, py + (rowH - ph) * 0.5f - shift, pillW, ph, juce::jmin (pillW, ph) * 0.5f);
    }
    // playhead: a soft band of shirt blue
    const float px = area.getX() + gap * (playPos + 0.5f);
    const float halfW = juce::jmax (8.0f, gap * 0.8f);
    juce::ColourGradient band (D::lens.withAlpha (0.30f), px, area.getCentreY(), D::lens.withAlpha (0.0f), px + halfW, area.getCentreY(), true);
    g.setGradientFill (band);
    g.fillRect (area.expanded (0.0f, 2.0f).withX (px - halfW).withWidth (halfW * 2.0f));

    for (const auto& s : sparks)
        if (s.life > 0.0f)
        {
            g.setColour (D::straw.darker (0.15f).withAlpha (s.life));
            const float x = area.getX() + s.x * area.getWidth(), y = area.getY() + s.y * area.getHeight();
            const float d = 1.8f + s.life * 2.2f;
            g.fillEllipse (x - d * 0.5f, y - d * 0.5f, d, d);
        }

    drawPaperStrip (g, juce::Rectangle<float> (after + 58.0f, 8.0f, right - after - 56.0f, 23.0f), reveal, 91);
    drawPaperStrip (g, area.expanded (5.0f, 1.0f), reveal, 92);
}

// ==== portrait ================================================================================
juce::File PortraitPanel::storageFolder()
{
   #if JUCE_MAC
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return base.getChildFile ("hiccup audio").getChildFile ("Interval");
}

PortraitPanel::PortraitPanel()
{
    setTitle ("portrait");
    setDescription ("Drop an image here, or click to choose one");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    restore();
}

void PortraitPanel::prepareDisplay (const Image& source)
{
    // keep a 3x copy so the picture stays sharp at any window size without repainting a huge bitmap
    const float wantW = (float) D::portraitW * 3.0f, wantH = (float) D::cardH * 3.0f;
    const float s = juce::jmin (1.0f, juce::jmax (wantW / (float) source.getWidth(), wantH / (float) source.getHeight()));
    display = s < 1.0f ? source.rescaled (juce::roundToInt ((float) source.getWidth() * s), juce::roundToInt ((float) source.getHeight() * s), Graphics::highResamplingQuality)
                       : source;
    repaint();
}

void PortraitPanel::restore()
{
    const auto folder = storageFolder();
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "portrait.*"))
    {
        if (f.getFileExtension() == ".txt") continue;
        const auto img = juce::ImageFileFormat::loadFrom (f);
        if (img.isValid())
        {
            prepareDisplay (img);
            juce::StringArray tokens;
            tokens.addTokens (folder.getChildFile ("portrait.txt").loadFileAsString(), " ", "");
            if (tokens.size() >= 2)
            {
                panX = juce::jlimit (0.0f, 1.0f, tokens[0].getFloatValue());
                panY = juce::jlimit (0.0f, 1.0f, tokens[1].getFloatValue());
            }
            return;
        }
    }
}

bool PortraitPanel::loadFromFile (const juce::File& file)
{
    const auto img = juce::ImageFileFormat::loadFrom (file);
    if (! img.isValid()) return false;

    // a private copy, so the picture survives the original being moved or deleted
    const auto folder = storageFolder();
    if (folder.createDirectory().wasOk())
    {
        for (const auto& old : folder.findChildFiles (juce::File::findFiles, false, "portrait.*"))
            old.deleteFile();
        file.copyFileTo (folder.getChildFile ("portrait" + file.getFileExtension().toLowerCase()));
    }
    panX = panY = 0.5f;
    savePan();
    prepareDisplay (img);
    return true;
}

void PortraitPanel::clear()
{
    for (const auto& old : storageFolder().findChildFiles (juce::File::findFiles, false, "portrait.*"))
        old.deleteFile();
    display = {};
    repaint();
}

void PortraitPanel::savePan() const
{
    const auto folder = storageFolder();
    if (folder.isDirectory())
        folder.getChildFile ("portrait.txt").replaceWithText (String (panX, 3) + " " + String (panY, 3));
}

void PortraitPanel::nudgePan (float dx, float dy)
{
    if (! display.isValid()) return;
    const float w = (float) getWidth(), h = (float) getHeight();
    const float s = juce::jmax (w / (float) display.getWidth(), h / (float) display.getHeight());
    const float spareX = (float) display.getWidth() * s - w, spareY = (float) display.getHeight() * s - h;
    if (spareX > 0.5f) panX = juce::jlimit (0.0f, 1.0f, panX - dx / spareX);
    if (spareY > 0.5f) panY = juce::jlimit (0.0f, 1.0f, panY - dy / spareY);
    repaint();
}

juce::Rectangle<float> PortraitPanel::removeButtonArea() const
{
    return { (float) getWidth() - 44.0f, 14.0f, 30.0f, 30.0f };
}

void PortraitPanel::tick (float dt)
{
    bool dirty = ease (hoverAnim, hover ? 1.0f : 0.0f, dt, 0.08f);
    dirty = ease (dropAnim, dropping ? 1.0f : 0.0f, dt, 0.06f) || dirty;
    if (dirty) repaint();
}

void PortraitPanel::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    juce::Path shape;
    shape.addRoundedRectangle (b, (float) D::cardRadius);

    if (display.isValid())
    {
        g.saveState();
        g.reduceClipRegion (shape);
        const float s = juce::jmax (b.getWidth() / (float) display.getWidth(), b.getHeight() / (float) display.getHeight());
        const float x = (b.getWidth() - (float) display.getWidth() * s) * panX;
        const float y = (b.getHeight() - (float) display.getHeight() * s) * panY;
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImageTransformed (display, juce::AffineTransform::scale (s).translated (x, y));
        g.restoreState();

        if (hoverAnim > 0.003f)
        {
            // round remove button
            const auto r = removeButtonArea();
            g.setColour (D::navy.withAlpha (0.78f * hoverAnim));
            g.fillEllipse (r);
            juce::Path cross;
            const auto c = r.getCentre();
            cross.startNewSubPath (c.x - 5.0f, c.y - 5.0f); cross.lineTo (c.x + 5.0f, c.y + 5.0f);
            cross.startNewSubPath (c.x + 5.0f, c.y - 5.0f); cross.lineTo (c.x - 5.0f, c.y + 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.95f * hoverAnim));
            g.strokePath (cross, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        // an empty, round invitation
        const auto c = juce::Point<float> (b.getCentreX(), b.getCentreY() - 22.0f);
        const float r = 62.0f + 3.0f * hoverAnim + 5.0f * dropAnim;
        g.setColour (D::shirt.withAlpha (0.35f + 0.3f * hoverAnim + 0.3f * dropAnim));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        juce::Path ring, dashed;
        ring.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        const float dashes[] = { 5.0f, 7.0f };
        juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createDashedStroke (dashed, ring, dashes, 2);
        g.setColour (D::lens.withAlpha (0.8f));
        g.fillPath (dashed);
        juce::Path plus;
        plus.startNewSubPath (c.x - 13.0f, c.y); plus.lineTo (c.x + 13.0f, c.y);
        plus.startNewSubPath (c.x, c.y - 13.0f); plus.lineTo (c.x, c.y + 13.0f);
        g.setColour (D::lens);
        g.strokePath (plus, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setFont (boldFont (17.0f));
        g.setColour (D::ink);
        g.drawText ("drop a photo", juce::Rectangle<float> (0.0f, c.y + r + 18.0f, b.getWidth(), 24.0f), juce::Justification::centred, false);
        g.setFont (regularFont (14.0f));
        g.setColour (D::greyInk);
        g.drawText ("or click to choose one", juce::Rectangle<float> (0.0f, c.y + r + 42.0f, b.getWidth(), 20.0f), juce::Justification::centred, false);
    }

    if (dropAnim > 0.003f)
    {
        g.setColour (D::lens.withAlpha (0.9f * dropAnim));
        g.strokePath (shape, juce::PathStrokeType (3.0f));
    }
}

void PortraitPanel::mouseDown (const juce::MouseEvent& e)
{
    pressedRemove = display.isValid() && removeButtonArea().contains (e.position);
    panAtDownX = panX;
    panAtDownY = panY;
}

void PortraitPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! display.isValid() || pressedRemove) return;
    panX = panAtDownX; panY = panAtDownY;
    nudgePan ((float) e.getDistanceFromDragStartX(), (float) e.getDistanceFromDragStartY());
}

void PortraitPanel::mouseUp (const juce::MouseEvent& e)
{
    if (pressedRemove)
    {
        pressedRemove = false;
        if (removeButtonArea().contains (e.position)) clear();
        return;
    }
    if (! display.isValid())
    {
        if (! e.mouseWasDraggedSinceMouseDown()) openChooser();
        return;
    }
    if (e.mouseWasDraggedSinceMouseDown()) savePan();
}

void PortraitPanel::mouseDoubleClick (const juce::MouseEvent&) { openChooser(); }

void PortraitPanel::openChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a photo", juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.png;*.jpg;*.jpeg;*.gif");
    juce::Component::SafePointer<PortraitPanel> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis != nullptr && fc.getResult().existsAsFile())
            safeThis->loadFromFile (fc.getResult());
    });
}

bool PortraitPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("png;jpg;jpeg;gif"))
            return true;
    return false;
}

void PortraitPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dropping = false;
    for (const auto& f : files)
        if (loadFromFile (juce::File (f)))
            return;
}

// ==== window grip =============================================================================
void Look::drawCornerResizer (Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    g.setColour (juce::Colours::white.withAlpha (isMouseDragging ? 0.6f : (isMouseOver ? 0.42f : 0.22f)));
    const float x = (float) w - 5.0f, y = (float) h - 5.0f, d = 2.6f, s = 5.0f;
    for (const auto o : { juce::Point<float> { 0, 0 }, { -s, 0 }, { 0, -s } })
        g.fillEllipse (x + o.x - d * 0.5f, y + o.y - d * 0.5f, d, d);
}

// ==== backdrop ================================================================================
Backdrop::Backdrop()
{
    int size = 0;
    const auto* data = BinaryData::getNamedResource ("backdrop_jpg", size);
    image = juce::ImageCache::getFromMemory (data, size);
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
    setBufferedToImage (true);
}
void Backdrop::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (image, juce::AffineTransform::scale (0.5f));

    // the maker's name, sewn into the suit
    const float y = (float) D::footerY, h = (float) D::footerH;
    g.setFont (boldFont (17.0f));
    g.setColour (Colour (0xffe7edf5));
    g.drawText ("hiccup audio", juce::Rectangle<float> ((float) D::footerX + 28.0f, y, 300.0f, h), juce::Justification::centredLeft, false);
    g.setFont (regularFont (17.0f));
    g.drawText ("Interval", juce::Rectangle<float> ((float) (D::footerX + D::footerW) - 328.0f, y, 300.0f, h), juce::Justification::centredRight, false);
}

// ==== editor ==================================================================================
HiccupEditor::HiccupEditor (HiccupProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    setOpaque (true);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);
    content.setBounds (0, 0, D::windowW, D::windowH);

    backdrop = std::make_unique<Backdrop>();
    backdrop->setBounds (content.getLocalBounds());
    content.addAndMakeVisible (*backdrop);

    auto param = [this] (const String& id) { return proc.apvts.getParameter (id); };
    auto track = [this] (auto& ptrRef) -> auto& { content.addAndMakeVisible (*ptrRef); animated.push_back (ptrRef.get()); return *ptrRef; };

    const int inner = D::cardY;
    const int xs[3] = { D::stutterX + D::pad, D::glitchX + D::pad, D::pitchX + D::pad };
    const int ws[3] = { D::sideW - D::pad * 2, D::centreW - D::pad * 2, D::sideW - D::pad * 2 };
    const char* names[3] = { "stutter", "glitch", "pitch" };

    portrait = std::make_unique<PortraitPanel>();
    portrait->setBounds (D::portraitX, D::cardY, D::portraitW, D::cardH);
    track (portrait);

    // ---- search bars and preset chevrons ----
    for (int i = 0; i < 3; ++i)
    {
        const int sy = inner + D::searchY;
        bars.push_back (std::make_unique<SearchBar> (names[i]));
        bars.back()->setBounds (xs[i], sy, ws[i], D::searchH);
        track (bars.back());
        arrows.push_back (std::make_unique<ArrowButton> (true, [this, i] { stepPreset (i, -1); }));
        arrows.back()->setBounds (xs[i] + ws[i] - 66, sy + (D::searchH - 30) / 2, 30, 30);
        track (arrows.back());
        arrows.push_back (std::make_unique<ArrowButton> (false, [this, i] { stepPreset (i, 1); }));
        arrows.back()->setBounds (xs[i] + ws[i] - 35, sy + (D::searchH - 30) / 2, 30, 30);
        track (arrows.back());
    }

    int seedCounter = 1;
    auto addRow = [&] (ParamRow::Kind kind, int module, const String& id, const String& label, juce::Rectangle<int> bounds)
    {
        auto* par = param (id);
        rowStore.push_back (std::make_unique<ParamRow> (kind, names[module], label, *par, *bars[(size_t) module], seedCounter++));
        auto* row = rowStore.back().get();
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *row));
        row->setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        row->setBounds (bounds);
        track (rowStore.back());
        rows[id] = row;
    };
    auto addChoice = [&] (int module, const String& id, const String& label, const juce::StringArray& opts, juce::Rectangle<int> bounds)
    {
        choiceStore.push_back (std::make_unique<ChoiceRow> (names[module], label, opts, *param (id), *bars[(size_t) module], seedCounter++));
        auto* row = choiceStore.back().get();
        row->setBounds (bounds);
        track (choiceStore.back());
        choices[id] = row;
    };

    // ---- stutter ----
    {
        const int x = xs[0], w = ws[0], rowH = 52;
        int y = inner + D::rowsY;
        addRow (ParamRow::Kind::hero, 0, ids::stutterInterval, "intervals", { x, inner + D::heroY, w, D::heroH });
        addRow (ParamRow::Kind::row, 0, ids::stutterMix, "mix", { x, y, w, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 0, ids::stutterRelease, "release", { x, y, w, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 0, ids::stutterRepeats, "repeats", { x, y, w, rowH }); y += rowH + 8;
        addChoice (0, ids::stutterReverse, "reverse", { "off", "on" }, { x, y, w, 36 });
    }
    // ---- glitch ----
    {
        const int x = xs[1], w = ws[1], gap = 14, half = (w - gap) / 2, rowH = 44, choiceH = 33;
        int y = inner + D::rowsY;
        pattern = std::make_unique<PatternPanel> (proc);
        pattern->setBounds (x, inner + D::heroY, w, D::heroH);
        track (pattern);
        addChoice (1, ids::glitchGrid, "grid", { "1/8", "1/16", "1/32" }, { x, y, w, choiceH }); y += choiceH + 1;
        addChoice (1, ids::glitchScale, "scale", { "OCTAVE", "FIFTH", "WILD" }, { x, y, w, choiceH }); y += choiceH + 1;
        addChoice (1, ids::glitchMode, "grain", { "CLOUD", "REVERSE", "FREEZE" }, { x, y, w, choiceH }); y += choiceH + 4;
        addRow (ParamRow::Kind::row, 1, ids::glitchRhythm, "rhythm", { x, y, half, rowH });
        addRow (ParamRow::Kind::row, 1, ids::glitchPitch, "pitch", { x + half + gap, y, half, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 1, ids::glitchGrains, "grains", { x, y, half, rowH });
        addRow (ParamRow::Kind::row, 1, ids::glitchSize, "size", { x + half + gap, y, half, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 1, ids::glitchSpray, "spray", { x, y, half, rowH });
        addRow (ParamRow::Kind::row, 1, ids::glitchMix, "mix", { x + half + gap, y, half, rowH });
    }
    // ---- pitch ----
    {
        const int x = xs[2], w = ws[2], rowH = 58;
        int y = inner + D::rowsY + 6;
        addRow (ParamRow::Kind::hero, 2, ids::pitchTranspose, "transpose", { x, inner + D::heroY, w, D::heroH });
        addRow (ParamRow::Kind::row, 2, ids::pitchChord, "harmony", { x, y, w, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 2, ids::pitchBlend, "blend", { x, y, w, rowH }); y += rowH;
        addRow (ParamRow::Kind::row, 2, ids::pitchMix, "mix", { x, y, w, rowH });
    }

    // ---- engage buttons ----
    auto addEngage = [&] (const String& on, const String& off, juce::RangedAudioParameter* par, EngageButton::Mode mode, juce::Rectangle<int> bounds, std::function<void()> action = {})
    {
        engages.push_back (std::make_unique<EngageButton> (on, off, par, mode, std::move (action)));
        engages.back()->setBounds (bounds);
        track (engages.back());
    };
    const int ey = inner + D::engageY, eh = D::engageH;
    addEngage ("stutter engaged", "stutter bypassed", param (ids::stutterOn), EngageButton::Mode::latching, { xs[0], ey, ws[0], eh });
    addEngage ("glitch engaged", "glitch bypassed", param (ids::glitchOn), EngageButton::Mode::latching, { xs[1], ey, (ws[1] - 14) / 2, eh });
    addEngage ("pitch engaged", "pitch bypassed", param (ids::pitchOn), EngageButton::Mode::latching, { xs[2], ey, ws[2], eh });
    addEngage ("reroll pattern", "reroll pattern", nullptr, EngageButton::Mode::action, { xs[1] + (ws[1] + 14) / 2, ey, (ws[1] - 14) / 2, eh }, [this]
    {
        auto* seed = proc.apvts.getParameter (ids::glitchSeed);
        const int next = juce::Random::getSystemRandom().nextInt (10000);
        seed->beginChangeGesture();
        seed->setValueNotifyingHost (seed->convertTo0to1 ((float) next));
        seed->endChangeGesture();
        bars[1]->showMessage ("reroll pattern " + String (next));
        bars[1]->flash();
        engages[3]->flash();
    });

    advance (10.0f);   // start settled, nothing animates in from zero

    // ---- window ----
    const double ratio = (double) D::windowW / (double) D::windowH;
    constrainer.setFixedAspectRatio (ratio);
    constrainer.setSizeLimits (D::windowW * 6 / 10, D::windowH * 6 / 10, D::windowW * 2, D::windowH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const double scale = proc.apvts.state.getProperty ("uiScale", 0.92);
    setSize (juce::roundToInt (D::windowW * scale), juce::roundToInt (D::windowH * scale));

    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] (double t)
    {
        const float dt = lastTime > 0.0 ? juce::jlimit (0.001f, 0.1f, (float) (t - lastTime)) : 1.0f / 60.0f;
        lastTime = t;
        advance (dt);
    });
}

HiccupEditor::~HiccupEditor()
{
    setConstrainer (nullptr);
    setLookAndFeel (nullptr);
}

void HiccupEditor::resized()
{
    const float s = (float) getWidth() / (float) D::windowW;
    content.setTransform (juce::AffineTransform::scale (s));
    proc.apvts.state.setProperty ("uiScale", (double) s, nullptr);
}

void HiccupEditor::advance (float dt)
{
    const auto rawOn = [this] (const String& id) { return proc.apvts.getRawParameterValue (id)->load (std::memory_order_relaxed) > 0.5f; };
    const bool stutterOn = rawOn (ids::stutterOn), glitchOn = rawOn (ids::glitchOn), pitchOn = rawOn (ids::pitchOn);
    bars[0]->lensTarget = stutterOn ? 0.72f : 0.0f;
    bars[1]->lensTarget = glitchOn ? 0.72f : 0.0f;
    bars[2]->lensTarget = pitchOn ? 0.72f : 0.0f;

    // a bypassed module's values go under paper strips, as on the card in the photos
    const auto coverFor = [&] (const String& id)
    {
        return id.startsWith ("stutter") ? ! stutterOn : (id.startsWith ("glitch") ? ! glitchOn : ! pitchOn);
    };
    for (auto& r : rows) r.second->covered = coverFor (r.first);
    for (auto& c : choices) c.second->covered = coverFor (c.first);
    pattern->covered = ! glitchOn;

    // the stutter lens pulses with every hit
    const auto hits = proc.stutter.hitCounter.load (std::memory_order_relaxed);
    if (hits != lastHits)
    {
        lastHits = hits;
        if (stutterOn) bars[0]->flash (0.9f);
    }
    for (auto* a : animated) a->tick (dt);
}

void HiccupEditor::stepPreset (int group, int delta)
{
    const auto& list = group == 0 ? stutterPresets() : (group == 1 ? scenes() : pitchPresets());
    const int n = (int) list.size();
    presetIndex[group] = (presetIndex[group] + delta + n) % n;
    const auto& preset = list[(size_t) presetIndex[group]];
    proc.applyPreset (preset);
    const String word = group == 0 ? "stutter" : (group == 1 ? "scene" : "pitch");
    bars[(size_t) group]->showMessage (word + " " + preset.name, 2.2f, true);
}

juce::AudioProcessorEditor* createHiccupEditor (HiccupProcessor& p) { return new HiccupEditor (p); }
