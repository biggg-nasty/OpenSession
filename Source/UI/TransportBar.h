// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionContext.h"

namespace opensession
{

/** Play / stop / record / return-to-start, position readout (bars|beats and clock), tempo,
    click, count-in, snap, and a plain-language recording status. */
class TransportBar : public juce::Component,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit TransportBar (SessionContext&);
    ~TransportBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onAddTrack;

private:
    SessionContext& ctx;

    juce::TextButton toStartButton { "|<" }, playButton { "Play" }, stopButton { "Stop" }, recordButton { "Rec" },
                     clickButton { "Click" }, countInButton { "Count-in" }, snapButton { "Snap" }, addTrackButton { "+ Track" },
                     takeLanesButton { "Takes" };
    juce::Label positionLabel, barsLabel, tempoLabel { {}, "Tempo" }, statusLabel;
    juce::Slider tempoSlider;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateButtons();
    void doRecord();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace opensession
