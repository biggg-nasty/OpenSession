// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace opensession
{

/** One line of status at the bottom: last message (errors in red, sticky until replaced)
    plus a right-aligned engine summary (device, rate, buffer, CPU). */
class StatusBar : public juce::Component
{
public:
    StatusBar()
    {
        addAndMakeVisible (message);
        addAndMakeVisible (engineInfo);
        message.setJustificationType (juce::Justification::centredLeft);
        engineInfo.setJustificationType (juce::Justification::centredRight);
        engineInfo.setColour (juce::Label::textColourId, theme::textDim);
        message.setMinimumHorizontalScale (0.7f);
    }

    void setMessage (const juce::String& text, bool isError)
    {
        message.setText (text, juce::dontSendNotification);
        message.setColour (juce::Label::textColourId, isError ? theme::record : theme::text);
        juce::Logger::writeToLog ((isError ? "[error] " : "[status] ") + text);
    }

    void setEngineInfo (const juce::String& text)   { engineInfo.setText (text, juce::dontSendNotification); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::panel);
        g.setColour (theme::outline);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 0);
        engineInfo.setBounds (r.removeFromRight (juce::jmin (520, r.getWidth() / 2)));
        message.setBounds (r);
    }

private:
    juce::Label message, engineInfo;
};

} // namespace opensession
