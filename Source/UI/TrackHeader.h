// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionContext.h"
#include "Meters.h"

namespace opensession
{

/** The left-hand control block of one track: name, input chooser, arm, monitor, mute, solo,
    an input meter (actual live level from the device) and the track's output meter. */
class TrackHeader : public juce::Component,
                    private juce::Timer
{
public:
    TrackHeader (SessionContext&, te::AudioTrack&);
    ~TrackHeader() override;

    te::AudioTrack& getTrack() noexcept     { return track; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void refresh();

private:
    SessionContext& ctx;
    te::AudioTrack& track;

    juce::Label nameLabel;
    juce::ComboBox inputBox;
    juce::TextButton armButton { "R" }, monitorButton { "MON" }, muteButton { "M" }, soloButton { "S" };
    LevelMeterComponent inputMeter { false }, outputMeter { true };
    juce::Label inputMeterLabel { {}, "in" }, channelsLabel;

    void rebuildInputList();
    void applyInputChoice();
    void cycleMonitorMode();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};

/** Vertical stack of TrackHeaders aligned with the timeline lanes. */
class TrackHeaderList : public juce::Component,
                        private juce::ChangeListener,
                        private juce::ValueTree::Listener
{
public:
    explicit TrackHeaderList (SessionContext&);
    ~TrackHeaderList() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void rebuild();
    void setVerticalOffset (int y)   { yOffset = y; resized(); }
    int getContentHeight() const;

private:
    SessionContext& ctx;
    juce::OwnedArray<TrackHeader> headers;
    int yOffset = 0;
    bool rebuildPending = false;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void triggerRebuild();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeaderList)
};

} // namespace opensession
