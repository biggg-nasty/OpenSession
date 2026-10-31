// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Timeline.h"
#include "Theme.h"

namespace opensession
{

TimelineComponent::TimelineComponent (SessionContext& c) : ctx (c)
{
    ctx.addChangeListener (this);
    ctx.edit.state.addListener (this);
    ctx.edit.getTransport().addListener (this);
    setWantsKeyboardFocus (false);
    rebuildClips();
    startTimerHz (30);
}

TimelineComponent::~TimelineComponent()
{
    ctx.edit.getTransport().removeListener (this);
    ctx.edit.state.removeListener (this);
    ctx.removeChangeListener (this);
}

//==============================================================================
juce::Rectangle<int> TimelineComponent::getLaneBounds (int trackIndex) const
{
    return { 0, SessionContext::rulerHeight + trackIndex * SessionContext::trackHeight - yOffset,
             getWidth(), SessionContext::trackHeight };
}

void TimelineComponent::rebuildClips()
{
    rebuildPending = false;
    clips.clear();

    for (auto* t : te::getAudioTracks (ctx.edit))
        for (auto* c : t->getClips())
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c))
                addAndMakeVisible (clips.add (new ClipComponent (ctx, *wc, *this)));

    updateClipBounds();
}

void TimelineComponent::updateClipBounds()
{
    auto tracks = te::getAudioTracks (ctx.edit);

    for (auto* cc : clips)
    {
        auto& clip = cc->getClip();
        auto* track = dynamic_cast<te::AudioTrack*> (clip.getTrack());
        const int trackIndex = tracks.indexOf (track);

        if (trackIndex < 0)
        {
            cc->setVisible (false);
            continue;
        }

        auto lane = getLaneBounds (trackIndex).reduced (0, 3);
        const auto pos = clip.getPosition();
        const int x1 = ctx.timeToX (pos.getStart());
        const int x2 = ctx.timeToX (pos.getEnd());

        // Take lanes: overlapping clips stack, active take on top.
        const int groupSize = RecordingController::getTakeGroupSize (clip);

        if (ctx.showTakeLanes && groupSize > 1)
        {
            const int shown = juce::jmin (groupSize, 4);
            const int subHeight = lane.getHeight() / shown;
            int subIndex = 0;

            if (clip.isMuted())
            {
                // order muted takes by recording order after the active one
                int rank = 1;

                for (auto* other : RecordingController::getOverlappingClips (clip))
                    if (auto* oa = dynamic_cast<te::AudioClipBase*> (other))
                        if (oa->isMuted() && RecordingController::getTakeIndexInGroup (*other) < RecordingController::getTakeIndexInGroup (clip))
                            ++rank;

                subIndex = juce::jmin (rank, shown - 1);
            }

            lane = lane.withY (lane.getY() + subIndex * subHeight).withHeight (subHeight - 1);
        }

        cc->setVisible (true);
        cc->setBounds (x1, lane.getY(), std::max (2, x2 - x1), lane.getHeight());

        if (! clip.isMuted())
            cc->toFront (false);
    }

    repaint();
}

void TimelineComponent::resized()
{
    updateClipBounds();
}

//==============================================================================
te::TimeDuration TimelineComponent::getGridStep() const
{
    const double beatSeconds = 60.0 / ctx.edit.tempoSequence.getBpmAt (te::TimePosition());
    double step = beatSeconds;

    while (step * ctx.pixelsPerSecond < 14.0)
        step *= 2.0;

    return te::TimeDuration::fromSeconds (step);
}

void TimelineComponent::drawGrid (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& ts = ctx.edit.tempoSequence;
    const auto startBeat = ts.toBeats (ctx.xToTime (area.getX()));
    const auto endBeat = ts.toBeats (ctx.xToTime (area.getRight()));
    const int numerator = juce::jmax (1, ts.getTimeSig (0)->numerator.get());
    const double beatPx = ctx.pixelsPerSecond * 60.0 / ts.getBpmAt (te::TimePosition());
    const bool drawBeats = beatPx >= 10.0;

    for (int b = (int) std::floor (startBeat.inBeats()); b <= (int) std::ceil (endBeat.inBeats()); ++b)
    {
        if (b < 0) continue;
        const bool isBar = (b % numerator) == 0;

        if (! isBar && ! drawBeats)
            continue;

        const int x = ctx.timeToX (ts.toTime (te::BeatPosition::fromBeats ((double) b)));
        g.setColour (isBar ? theme::gridBar : theme::gridBeat);
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
    }
}

void TimelineComponent::drawRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme::panel);
    g.fillRect (area);

    auto& ts = ctx.edit.tempoSequence;
    const int numerator = juce::jmax (1, ts.getTimeSig (0)->numerator.get());
    const double barPx = ctx.pixelsPerSecond * 60.0 / ts.getBpmAt (te::TimePosition()) * numerator;
    int barStep = 1;

    while (barPx * barStep < 60.0)
        barStep *= 2;

    const auto startBeat = ts.toBeats (ctx.xToTime (area.getX()));
    const auto endBeat = ts.toBeats (ctx.xToTime (area.getRight()));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (int bar = juce::jmax (0, (int) std::floor (startBeat.inBeats() / numerator)); bar <= (int) std::ceil (endBeat.inBeats() / numerator); bar += barStep)
    {
        const auto t = ts.toTime (te::BeatPosition::fromBeats ((double) bar * numerator));
        const int x = ctx.timeToX (t);
        g.setColour (theme::gridBar);
        g.drawVerticalLine (x, (float) area.getY() + 18.0f, (float) area.getBottom());
        g.setColour (theme::text);
        g.drawText (juce::String (bar + 1), x + 3, area.getY() + 16, 60, 16, juce::Justification::centredLeft);
    }

    // Clock scale on top
    double secStep = 1.0;
    while (secStep * ctx.pixelsPerSecond < 70.0) secStep *= 2.0;
    while (secStep * ctx.pixelsPerSecond > 220.0 && secStep > 0.1) secStep /= 2.0;

    const double tStart = std::floor (ctx.xToTime (area.getX()).inSeconds() / secStep) * secStep;

    for (double t = std::max (0.0, tStart); ctx.timeToX (te::TimePosition::fromSeconds (t)) < area.getRight(); t += secStep)
    {
        const int x = ctx.timeToX (te::TimePosition::fromSeconds (t));
        g.setColour (theme::textDim);
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getY() + 6.0f);
        const int mins = (int) t / 60;
        const double secs = t - mins * 60;
        g.drawText (juce::String::formatted ("%d:%05.2f", mins, secs), x + 3, area.getY(), 80, 16, juce::Justification::centredLeft);
    }

    g.setColour (theme::outline);
    g.drawHorizontalLine (area.getBottom() - 1, 0.0f, (float) getWidth());
}

void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    auto tracks = te::getAudioTracks (ctx.edit);
    auto lanesArea = getLocalBounds().withTrimmedTop (SessionContext::rulerHeight);

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto lane = getLaneBounds (i);
        g.setColour (i % 2 == 0 ? theme::lane : theme::laneAlt);

        if (ctx.selectedTrack == tracks[i]->itemID)
            g.setColour (theme::panelLight.withAlpha (0.6f));

        g.fillRect (lane);
        g.setColour (theme::outline);
        g.drawHorizontalLine (lane.getBottom() - 1, 0.0f, (float) getWidth());
    }

    drawGrid (g, lanesArea);

    // Recording in progress: live red region on each armed track from the punch-in point.
    if (recordStart.has_value() && ctx.edit.getTransport().isRecording())
    {
        const int x1 = ctx.timeToX (*recordStart);
        const int x2 = ctx.timeToX (ctx.edit.getTransport().getPosition());

        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! ctx.recording.isArmed (*tracks[i]))
                continue;

            auto lane = getLaneBounds (i).reduced (0, 3);
            g.setColour (theme::record.withAlpha (0.45f));
            g.fillRoundedRectangle (juce::Rectangle<int> (x1, lane.getY(), std::max (2, x2 - x1), lane.getHeight()).toFloat(), 3.0f);
            g.setColour (theme::text);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("recording...", x1 + 4, lane.getY(), 120, 14, juce::Justification::centredLeft);
        }
    }
}

void TimelineComponent::paintOverChildren (juce::Graphics& g)
{
    drawRuler (g, getLocalBounds().removeFromTop (SessionContext::rulerHeight));

    const int x = ctx.timeToX (ctx.edit.getTransport().getPosition());

    if (x >= 0 && x < getWidth())
    {
        g.setColour (ctx.edit.getTransport().isRecording() ? theme::record : theme::playhead);
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
        juce::Path tri;
        tri.addTriangle ((float) x - 6.0f, 0.0f, (float) x + 6.0f, 0.0f, (float) x, 8.0f);
        g.fillPath (tri);
    }
}

//==============================================================================
void TimelineComponent::mouseDown (const juce::MouseEvent& e)
{
    draggingRuler = e.y < SessionContext::rulerHeight;
    auto t = std::max (te::TimePosition(), ctx.xToTime (e.x));

    if (draggingRuler)
    {
        ctx.edit.getTransport().setPosition (t);
        return;
    }

    // Click on an empty lane: select the track, move the playhead, clear the clip selection.
    const int laneIndex = (e.y - SessionContext::rulerHeight + yOffset) / SessionContext::trackHeight;
    auto tracks = te::getAudioTracks (ctx.edit);

    if (laneIndex >= 0 && laneIndex < tracks.size())
        ctx.selectTrack (tracks[laneIndex]);

    ctx.selectClip (nullptr);

    if (! ctx.edit.getTransport().isRecording())
        ctx.edit.getTransport().setPosition (ctx.snap (t));
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingRuler)
        ctx.edit.getTransport().setPosition (std::max (te::TimePosition(), ctx.xToTime (e.x)));
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoom (wheel.deltaY > 0 ? 1.15 : 1.0 / 1.15, e.x);
        return;
    }

    if (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) || e.mods.isShiftDown())
    {
        const double delta = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY) * 200.0;
        ctx.viewStart = std::max (te::TimePosition(), ctx.viewStart - te::TimeDuration::fromSeconds (delta / ctx.pixelsPerSecond));
        ctx.viewChanged();
        return;
    }

    const int contentHeight = te::getAudioTracks (ctx.edit).size() * SessionContext::trackHeight;
    const int maxOffset = std::max (0, contentHeight - (getHeight() - SessionContext::rulerHeight));
    yOffset = juce::jlimit (0, maxOffset, yOffset - juce::roundToInt (wheel.deltaY * 120.0));

    if (onVerticalScroll)
        onVerticalScroll (yOffset);

    updateClipBounds();
}

void TimelineComponent::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoom (scaleFactor, e.x);
}

void TimelineComponent::zoom (double factor, int anchorX)
{
    const auto anchorTime = ctx.xToTime (anchorX);
    ctx.pixelsPerSecond = juce::jlimit (2.0, 2000.0, ctx.pixelsPerSecond * factor);
    ctx.viewStart = std::max (te::TimePosition(), anchorTime - te::TimeDuration::fromSeconds (anchorX / ctx.pixelsPerSecond));
    ctx.viewChanged();
}

void TimelineComponent::zoomToFit()
{
    const double len = std::max (4.0, ctx.edit.getLength().inSeconds() + 2.0);
    ctx.pixelsPerSecond = juce::jlimit (2.0, 2000.0, (getWidth() - 20) / len);
    ctx.viewStart = {};
    ctx.viewChanged();
}

void TimelineComponent::scrollToPlayheadIfNeeded (bool force)
{
    const int x = ctx.timeToX (ctx.edit.getTransport().getPosition());

    if (force || x < 0 || x > getWidth() - 40)
    {
        ctx.viewStart = std::max (te::TimePosition(), ctx.edit.getTransport().getPosition() - te::TimeDuration::fromSeconds (40.0 / ctx.pixelsPerSecond));
        ctx.viewChanged();
    }
}

//==============================================================================
void TimelineComponent::timerCallback()
{
    if (ctx.edit.getTransport().isPlaying())
        scrollToPlayheadIfNeeded (false);

    repaint();
}

void TimelineComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateClipBounds();
}

void TimelineComponent::triggerRebuild()
{
    if (rebuildPending)
        return;

    rebuildPending = true;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TimelineComponent> (this)]
    {
        if (safe != nullptr)
            safe->rebuildClips();
    });
}

void TimelineComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::AUDIOCLIP) || child.hasType (te::IDs::TRACK))
        triggerRebuild();
}

void TimelineComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::AUDIOCLIP) || child.hasType (te::IDs::TRACK))
        triggerRebuild();
}

void TimelineComponent::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    triggerRebuild();
}

void TimelineComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v.hasType (te::IDs::AUDIOCLIP) && (id == te::IDs::mute || id == te::IDs::start || id == te::IDs::length))
        updateClipBounds();
}

void TimelineComponent::recordingStarted (te::SyncPoint start, std::optional<te::TimeRange> punch)
{
    recordStart = punch.has_value() ? punch->getStart() : start.time;
}

void TimelineComponent::recordingStopped (te::SyncPoint, bool)
{
    recordStart.reset();
}

} // namespace opensession
