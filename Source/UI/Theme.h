// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace opensession::theme
{
    inline const juce::Colour background      { 0xff1b1d21 };
    inline const juce::Colour panel           { 0xff24272d };
    inline const juce::Colour panelLight      { 0xff2e323a };
    inline const juce::Colour outline         { 0xff3a3f49 };
    inline const juce::Colour text            { 0xffe6e8eb };
    inline const juce::Colour textDim         { 0xff9aa0a8 };
    inline const juce::Colour accent          { 0xff4fa3ff };
    inline const juce::Colour record          { 0xffe5484d };
    inline const juce::Colour recordDim       { 0xff7a2a2d };
    inline const juce::Colour play            { 0xff46c36b };
    inline const juce::Colour warning         { 0xfff5b942 };
    inline const juce::Colour clipFill        { 0xff2f6ea8 };
    inline const juce::Colour clipFillMuted   { 0xff3a4250 };
    inline const juce::Colour clipSelected    { 0xff6fb3ff };
    inline const juce::Colour waveform        { 0xffd7e9ff };
    inline const juce::Colour waveformMuted   { 0xff7d8794 };
    inline const juce::Colour lane            { 0xff202329 };
    inline const juce::Colour laneAlt         { 0xff23262c };
    inline const juce::Colour gridBar         { 0xff3c424d };
    inline const juce::Colour gridBeat        { 0xff2b2f37 };
    inline const juce::Colour playhead        { 0xffffffff };
    inline const juce::Colour monitorOn       { 0xff39c0c7 };

    struct LookAndFeel : public juce::LookAndFeel_V4
    {
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, background);
            setColour (juce::TextButton::buttonColourId, panelLight);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            setColour (juce::ComboBox::backgroundColourId, panelLight);
            setColour (juce::ComboBox::outlineColourId, outline);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.6f));
            setColour (juce::Label::textColourId, text);
            setColour (juce::Slider::thumbColourId, accent);
            setColour (juce::Slider::trackColourId, panelLight);
            setColour (juce::Slider::backgroundColourId, panel);
            setColour (juce::Slider::rotarySliderFillColourId, accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, outline);
            setColour (juce::Slider::textBoxTextColourId, text);
            setColour (juce::Slider::textBoxBackgroundColourId, panel);
            setColour (juce::Slider::textBoxOutlineColourId, outline);
            setColour (juce::TextEditor::backgroundColourId, panel);
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::TextEditor::outlineColourId, outline);
            setColour (juce::ToggleButton::textColourId, text);
            setColour (juce::ToggleButton::tickColourId, accent);
            setColour (juce::ScrollBar::thumbColourId, outline);
            setColour (juce::AlertWindow::backgroundColourId, panel);
            setColour (juce::AlertWindow::textColourId, text);
            setColour (juce::ListBox::backgroundColourId, panel);
            setColour (juce::TabbedComponent::backgroundColourId, panel);
        }

        juce::Font getTextButtonFont (juce::TextButton& b, int buttonHeight) override
        {
            // Narrow buttons (mixer insert slots) get a smaller face so "CMP"/"REV" are not elided.
            const int cap = b.getWidth() < 40 ? 10 : 14;
            return juce::Font (juce::FontOptions ((float) juce::jmin (cap, buttonHeight - 4)));
        }
    };
}
