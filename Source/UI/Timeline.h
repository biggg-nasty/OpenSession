// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionContext.h"
#include "ClipComponent.h"

namespace opensession
{

/** Linear timeline: ruler (clock + bars/beats), one lane per track, clips, playhead,
    recording-in-progress indicator, zoom and scroll. */
class TimelineComponent : public juce::Component,
                          private juce::Timer,
                          private juce::ChangeListener,
                          private juce::ValueTree::Listener,
                          private te::TransportControl::Listener
{
public:
    explicit TimelineComponent (SessionContext&);
    ~TimelineComponent() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    void rebuildClips();
    void updateClipBounds();
    void zoom (double factor, int anchorX);
    void zoomToFit();
    void scrollToPlayheadIfNeeded (bool force);

    int getVerticalOffset() const            { return yOffset; }
    std::function<void (int)> onVerticalScroll;

    /** Lane rectangle for a track (in this component's coordinates). */
    juce::Rectangle<int> getLaneBounds (int trackIndex) const;

private:
    SessionContext& ctx;
    juce::OwnedArray<ClipComponent> clips;
    int yOffset = 0;
    bool rebuildPending = false, draggingRuler = false;
    std::optional<te::TimePosition> recordStart;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void recordingStarted (te::SyncPoint, std::optional<te::TimeRange>) override;
    void recordingStopped (te::SyncPoint, bool) override;
    void triggerRebuild();
    void drawRuler (juce::Graphics&, juce::Rectangle<int>);
    void drawGrid (juce::Graphics&, juce::Rectangle<int>);
    te::TimeDuration getGridStep() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace opensession
