// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionContext.h"

namespace opensession
{

class TimelineComponent;

/** One audio clip on the timeline: waveform from the actual file samples, name, take badge,
    fade handles, trim edges and drag-to-move. All edits go through the engine's clip API,
    so they are undoable and never touch the source file. */
class ClipComponent : public juce::Component,
                      private juce::ValueTree::Listener
{
public:
    ClipComponent (SessionContext&, te::WaveAudioClip&, TimelineComponent&);
    ~ClipComponent() override;

    te::WaveAudioClip& getClip() noexcept   { return clip; }
    te::EditItemID getClipID() const        { return clipID; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void updateThumbnail();
    void showContextMenu();

private:
    enum class DragMode { none, move, trimStart, trimEnd, fadeIn, fadeOut };

    SessionContext& ctx;
    te::WaveAudioClip& clip;
    te::EditItemID clipID;
    TimelineComponent& timeline;
    std::unique_ptr<te::SmartThumbnail> thumbnail;

    DragMode dragMode = DragMode::none;
    te::ClipPosition dragStartPos;
    te::TimeDuration dragStartFadeIn, dragStartFadeOut;
    int dragStartX = 0;

    DragMode hitTest (juce::Point<int>) const;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipComponent)
};

} // namespace opensession
