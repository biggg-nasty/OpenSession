// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClipComponent.h"
#include "Timeline.h"
#include "Theme.h"

namespace opensession
{

ClipComponent::ClipComponent (SessionContext& c, te::WaveAudioClip& wc, TimelineComponent& t)
    : ctx (c), clip (wc), clipID (wc.itemID), timeline (t)
{
    clip.state.addListener (this);
    updateThumbnail();
}

ClipComponent::~ClipComponent()
{
    clip.state.removeListener (this);
}

void ClipComponent::updateThumbnail()
{
    auto af = clip.getAudioFile();

    if (af.getFile().existsAsFile() && af.isValid())
    {
        if (thumbnail == nullptr)
            thumbnail = std::make_unique<te::SmartThumbnail> (ctx.engine, af, *this, &ctx.edit);
        else
            thumbnail->setNewFile (af);
    }
    else
    {
        thumbnail.reset();
    }

    repaint();
}

void ClipComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::source)
        updateThumbnail();

    if (id == te::IDs::start || id == te::IDs::length || id == te::IDs::offset)
        timeline.updateClipBounds();

    repaint();
}

void ClipComponent::paint (juce::Graphics& g)
{
    const bool muted = clip.isMuted();
    const bool selected = ctx.selectedClip == clipID;
    const bool missing = ! clip.getAudioFile().getFile().existsAsFile();
    auto r = getLocalBounds();

    g.setColour (missing ? theme::recordDim : muted ? theme::clipFillMuted : theme::clipFill);
    g.fillRoundedRectangle (r.toFloat(), 3.0f);

    // Waveform from the real file samples; clip gain scales the drawing like it scales playback.
    if (thumbnail != nullptr && r.getWidth() > 2)
    {
        auto pos = clip.getPosition();
        auto waveArea = r.reduced (1, 14).withTrimmedTop (2);

        if (waveArea.getHeight() > 4)
        {
            const auto visibleStart = ctx.xToTime (getX());
            const auto visibleEnd = ctx.xToTime (getRight());
            const auto t1 = std::max (visibleStart, pos.getStart());
            const auto t2 = std::min (visibleEnd, pos.getEnd());

            if (t2 > t1)
            {
                const int x1 = ctx.timeToX (t1) - getX();
                const int x2 = ctx.timeToX (t2) - getX();
                auto area = waveArea.withX (x1).withWidth (std::max (1, x2 - x1));
                const auto fileStart = pos.getOffset() + (t1 - pos.getStart());
                const auto fileEnd = pos.getOffset() + (t2 - pos.getStart());
                g.setColour (muted ? theme::waveformMuted : theme::waveform);
                thumbnail->drawChannels (g, area, { te::TimePosition() + fileStart, te::TimePosition() + fileEnd }, clip.getGain());
            }
        }
    }

    // Fades
    {
        const auto fi = clip.getFadeIn().inSeconds(), fo = clip.getFadeOut().inSeconds();
        g.setColour (juce::Colours::black.withAlpha (0.35f));

        if (fi > 0.0)
        {
            const float w = (float) (fi * ctx.pixelsPerSecond);
            juce::Path p;
            p.startNewSubPath (0.0f, (float) r.getHeight());
            p.lineTo (0.0f, 0.0f);
            p.lineTo (w, 0.0f);
            p.closeSubPath();
            g.fillPath (p);
        }

        if (fo > 0.0)
        {
            const float w = (float) (fo * ctx.pixelsPerSecond);
            juce::Path p;
            p.startNewSubPath ((float) r.getWidth(), (float) r.getHeight());
            p.lineTo ((float) r.getWidth(), 0.0f);
            p.lineTo ((float) r.getWidth() - w, 0.0f);
            p.closeSubPath();
            g.fillPath (p);
        }
    }

    // Title bar
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (r.removeFromTop (14));
    g.setColour (muted ? theme::textDim : theme::text);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    juce::String title = clip.getName();
    const int groupSize = RecordingController::getTakeGroupSize (clip);

    if (groupSize > 1)
        title = "[take " + juce::String (RecordingController::getTakeIndexInGroup (clip) + 1) + "/" + juce::String (groupSize)
                + (muted ? "]  " : " ACTIVE]  ") + title;

    if (missing)
        title = "MISSING FILE: " + title;

    if (! juce::approximatelyEqual (clip.getGainDB(), 0.0f))
        title << "  " << (clip.getGainDB() > 0 ? "+" : "") << juce::String (clip.getGainDB(), 1) << " dB";

    g.drawText (title, getLocalBounds().removeFromTop (14).reduced (4, 0), juce::Justification::centredLeft, true);

    g.setColour (selected ? theme::clipSelected : juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, selected ? 2.0f : 1.0f);
}

ClipComponent::DragMode ClipComponent::hitTest (juce::Point<int> p) const
{
    const int edge = 8;

    if (p.y < 14 && p.x < 24)              return DragMode::fadeIn;
    if (p.y < 14 && p.x > getWidth() - 24) return DragMode::fadeOut;
    if (p.x < edge)                        return DragMode::trimStart;
    if (p.x > getWidth() - edge)           return DragMode::trimEnd;
    return DragMode::move;
}

void ClipComponent::mouseMove (const juce::MouseEvent& e)
{
    switch (hitTest (e.getPosition()))
    {
        case DragMode::trimStart:
        case DragMode::trimEnd:   setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case DragMode::fadeIn:
        case DragMode::fadeOut:   setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor); break;
        default:                  setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void ClipComponent::mouseDown (const juce::MouseEvent& e)
{
    ctx.selectClip (&clip);
    toFront (true);

    if (e.mods.isPopupMenu())
    {
        dragMode = DragMode::none;
        showContextMenu();
        return;
    }

    dragMode = hitTest (e.getPosition());
    dragStartPos = clip.getPosition();
    dragStartFadeIn = clip.getFadeIn();
    dragStartFadeOut = clip.getFadeOut();
    dragStartX = e.getScreenX();
    ctx.edit.getUndoManager().beginNewTransaction();
}

void ClipComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    const auto delta = te::TimeDuration::fromSeconds ((e.getScreenX() - dragStartX) / ctx.pixelsPerSecond);
    const auto minLen = te::TimeDuration::fromSeconds (0.01);

    switch (dragMode)
    {
        case DragMode::move:
        {
            auto newStart = std::max (te::TimePosition(), ctx.snap (dragStartPos.getStart() + delta));
            clip.setStart (newStart, false, true);     // move: source offset unchanged
            break;
        }
        case DragMode::trimStart:
        {
            auto newStart = ctx.snap (dragStartPos.getStart() + delta);
            newStart = juce::jlimit (std::max (te::TimePosition(), dragStartPos.getStart() - dragStartPos.getOffset()),
                                     dragStartPos.getEnd() - minLen, newStart);
            clip.setStart (newStart, true, false);     // trim: keeps the audio in place, changes offset
            break;
        }
        case DragMode::trimEnd:
        {
            auto newEnd = ctx.snap (dragStartPos.getEnd() + delta);
            const auto sourceEnd = dragStartPos.getStart() - dragStartPos.getOffset() + clip.getSourceLength();
            newEnd = juce::jlimit (dragStartPos.getStart() + minLen, sourceEnd, newEnd);
            clip.setEnd (newEnd, true);
            break;
        }
        case DragMode::fadeIn:
            clip.setFadeIn (juce::jlimit (te::TimeDuration(), dragStartPos.getLength(), dragStartFadeIn + delta));
            break;
        case DragMode::fadeOut:
            clip.setFadeOut (juce::jlimit (te::TimeDuration(), dragStartPos.getLength(), dragStartFadeOut - delta));
            break;
        case DragMode::none:
            break;
    }

    timeline.updateClipBounds();
}

void ClipComponent::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::none;
}

void ClipComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    ctx.edit.getUndoManager().beginNewTransaction ("Select take");
    RecordingController::activateTake (clip);
    ctx.status ("Active take: " + clip.getName());
    timeline.updateClipBounds();
    timeline.repaint();
}

void ClipComponent::showContextMenu()
{
    juce::PopupMenu m;
    const bool inGroup = RecordingController::getTakeGroupSize (clip) > 1;
    m.addItem (1, "Make this the active take", inGroup, ! clip.isMuted());
    m.addSeparator();
    m.addItem (2, "Split at playhead");
    m.addItem (3, "Duplicate (place after)");
    m.addItem (4, "Delete from timeline (keeps the audio file)");
    m.addSeparator();
    m.addItem (5, "Clip gain +1 dB");
    m.addItem (6, "Clip gain -1 dB");
    m.addItem (7, "Reset gain and fades");
    m.addItem (8, clip.isMuted() ? "Unmute clip" : "Mute clip");
    m.addSeparator();
    m.addItem (9, "Show audio file in Finder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<ClipComponent> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& c = safe->clip;
        auto& um = c.edit.getUndoManager();
        um.beginNewTransaction();

        switch (result)
        {
            case 1: RecordingController::activateTake (c); break;
            case 2:
            {
                auto pos = c.edit.getTransport().getPosition();

                if (auto* track = c.getClipTrack(); track != nullptr && c.getPosition().time.contains (pos))
                    track->splitClip (c, pos);
                else
                    safe->ctx.status ("Move the playhead inside the clip to split it", true);
                break;
            }
            case 3:
                if (auto copy = te::duplicateClip (c))
                    copy->setStart (c.getPosition().getEnd(), false, true);
                break;
            case 4: c.removeFromParent(); return;   // 'safe' is now dangling
            case 5: c.setGainDB (juce::jmin (24.0f, c.getGainDB() + 1.0f)); break;
            case 6: c.setGainDB (juce::jmax (-60.0f, c.getGainDB() - 1.0f)); break;
            case 7: c.setGainDB (0.0f); c.setFadeIn ({}); c.setFadeOut ({}); break;
            case 8: c.setMuted (! c.isMuted()); break;
            case 9: c.getAudioFile().getFile().revealToUser(); break;
            default: break;
        }

        safe->timeline.updateClipBounds();
        safe->timeline.repaint();
    });
}

} // namespace opensession
