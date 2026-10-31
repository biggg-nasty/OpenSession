// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "../Engine/Recording.h"
#include "../Session/SessionDocument.h"

namespace opensession
{

/** Everything the editor UI shares for one open session: engine, edit, recording controller,
    view state (zoom/scroll/snap) and selection. Owned by the editor view, referenced by children. */
struct SessionContext : public juce::ChangeBroadcaster
{
    SessionContext (te::Engine& e, SessionDocument& d, RecordingController& r)
        : engine (e), document (d), edit (d.getEdit()), recording (r) {}

    te::Engine& engine;
    SessionDocument& document;
    te::Edit& edit;
    RecordingController& recording;

    // View
    double pixelsPerSecond = 60.0;
    te::TimePosition viewStart;
    bool snapToGrid = false;
    bool showTakeLanes = true;

    // Selection (IDs so dangling pointers can't happen)
    te::EditItemID selectedClip, selectedTrack;

    std::function<void (const juce::String&, bool isError)> setStatus;

    //==============================================================================
    static constexpr int rulerHeight = 34;
    static constexpr int trackHeight = 96;
    static constexpr int headerWidth = 260;

    int timeToX (te::TimePosition t) const noexcept          { return juce::roundToInt ((t - viewStart).inSeconds() * pixelsPerSecond); }
    te::TimePosition xToTime (int x) const noexcept          { return viewStart + te::TimeDuration::fromSeconds (x / pixelsPerSecond); }

    te::TimePosition snap (te::TimePosition t) const
    {
        if (! snapToGrid)
            return t;

        auto beats = edit.tempoSequence.toBeats (t);
        auto rounded = te::BeatPosition::fromBeats (std::round (beats.inBeats()));
        return std::max (te::TimePosition(), edit.tempoSequence.toTime (rounded));
    }

    te::Clip* getSelectedClip() const        { return selectedClip.isValid() ? te::findClipForID (edit, selectedClip) : nullptr; }
    te::AudioTrack* getSelectedTrack() const { return selectedTrack.isValid() ? dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, selectedTrack)) : nullptr; }

    void selectClip (te::Clip* c)
    {
        selectedClip = c != nullptr ? c->itemID : te::EditItemID();

        if (c != nullptr)
            if (auto* t = c->getTrack())
                selectedTrack = t->itemID;

        sendChangeMessage();
    }

    void selectTrack (te::Track* t)
    {
        selectedTrack = t != nullptr ? t->itemID : te::EditItemID();
        sendChangeMessage();
    }

    void viewChanged()                       { sendChangeMessage(); }

    void status (const juce::String& s, bool isError = false)
    {
        if (setStatus)
            setStatus (s, isError);
        else
            juce::Logger::writeToLog (s);
    }
};

} // namespace opensession
