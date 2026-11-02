#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/** Everything visual that is shared: window geometry and the palette taken from the reference photos. */
namespace hiccup::design
{
    inline constexpr int windowW = 1396, windowH = 580;
    inline constexpr int cardY = 22, cardH = 478, cardRadius = 30, pad = 20;
    inline constexpr int portraitX = 24, portraitW = 260;
    inline constexpr int stutterX = 300, glitchX = 616, pitchX = 1072, sideW = 300, centreW = 440;
    inline constexpr int searchY = 16, searchH = 44, heroY = 68, heroH = 92, rowsY = 166, engageY = 412, engageH = 46;
    inline constexpr int footerX = 24, footerY = 514, footerW = 1348, footerH = 52;

    inline const juce::Colour navy    { 0xff1e2336 };   // the suit
    inline const juce::Colour ink     { 0xff232a3e };   // bold print on the card
    inline const juce::Colour greyInk { 0xff7f8a96 };   // the typed part of a search
    inline const juce::Colour faint   { 0xffcdd6de };   // tracks
    inline const juce::Colour tray    { 0xffdfe6eb };   // recessed fields on the paper
    inline const juce::Colour shirt   { 0xffc9d7ea };   // pale blue shirt: selection and hover
    inline const juce::Colour olive   { 0xff6f7040 };   // the tie
    inline const juce::Colour straw   { 0xffe2c47e };   // warm accent for sparks
    inline const juce::Colour lens    { 0xff5a6bcb };   // the magnifier
    inline const juce::Colour lensDark { 0xff3e4c9e };
    inline const juce::Colour strip   { 0xfff8fafb };   // the paper strips, a little whiter than the card
}
