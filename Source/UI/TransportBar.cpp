// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TransportBar.h"
#include "Theme.h"

namespace opensession
{

static juce::String formatClock (te::TimePosition t)
{
    auto ms = juce::roundToInt (t.inSeconds() * 1000.0);
    const bool neg = ms < 0;
    ms = std::abs (ms);
    return juce::String (neg ? "-" : "") + juce::String::formatted ("%02d:%02d.%03d", (ms / 60000) % 60, (ms / 1000) % 60, ms % 1000);
}

static juce::String formatBars (te::Edit& edit, te::TimePosition t)
{
    auto bb = edit.tempoSequence.toBarsAndBeats (t);
    const int beat = (int) std::floor (bb.beats.inBeats()) + 1;
    const int ticks = juce::roundToInt ((bb.beats.inBeats() - std::floor (bb.beats.inBeats())) * 960.0);
    return juce::String::formatted ("%d|%d|%03d", bb.bars + 1, beat, ticks);
}

TransportBar::TransportBar (SessionContext& c) : ctx (c)
{
    for (auto* b : { &toStartButton, &playButton, &stopButton, &recordButton, &clickButton, &countInButton,
                     &snapButton, &addTrackButton, &takeLanesButton })
        addAndMakeVisible (b);

    addAndMakeVisible (positionLabel);
    addAndMakeVisible (barsLabel);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (statusLabel);

    positionLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    barsLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    positionLabel.setJustificationType (juce::Justification::centred);
    barsLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, theme::textDim);
    tempoLabel.setJustificationType (juce::Justification::centredRight);

    tempoSlider.setSliderStyle (juce::Slider::IncDecButtons);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 24);
    tempoSlider.setRange (20.0, 300.0, 0.1);
    tempoSlider.setValue (ctx.edit.tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" bpm");
    tempoSlider.onValueChange = [this]
    {
        // Changing tempo only moves the grid and the click; audio clips stay exactly where
        // they were recorded (EngineBehaviour::areAudioClipsRemappedWhenTempoChanges = false).
        ctx.edit.tempoSequence.getTempo (0)->setBpm (tempoSlider.getValue());
        ctx.viewChanged();
    };

    recordButton.setColour (juce::TextButton::buttonOnColourId, theme::record);
    recordButton.setColour (juce::TextButton::buttonColourId, theme::recordDim);
    playButton.setColour (juce::TextButton::buttonOnColourId, theme::play);

    toStartButton.onClick = [this] { ctx.edit.getTransport().setPosition (te::TimePosition()); };
    playButton.onClick = [this]
    {
        auto& tc = ctx.edit.getTransport();
        if (tc.isPlaying()) tc.stop (false, false); else tc.play (false);
    };
    stopButton.onClick = [this] { ctx.edit.getTransport().stop (false, false); };
    recordButton.onClick = [this] { doRecord(); };

    clickButton.setClickingTogglesState (true);
    clickButton.setToggleState (ctx.edit.clickTrackEnabled.get(), juce::dontSendNotification);
    clickButton.onClick = [this] { ctx.edit.clickTrackEnabled = clickButton.getToggleState(); };

    countInButton.setClickingTogglesState (true);
    countInButton.setToggleState (ctx.edit.getCountInMode() != te::Edit::CountIn::none, juce::dontSendNotification);
    countInButton.onClick = [this]
    {
        ctx.edit.setCountInMode (countInButton.getToggleState() ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none);
    };
    countInButton.setTooltip ("One-bar count-in before recording starts (click plays during the count-in)");

    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (ctx.snapToGrid, juce::dontSendNotification);
    snapButton.onClick = [this] { ctx.snapToGrid = snapButton.getToggleState(); ctx.viewChanged(); };
    snapButton.setTooltip ("Snap clip moves/trims to the beat grid. Off by default: recordings keep their exact timing.");

    takeLanesButton.setClickingTogglesState (true);
    takeLanesButton.setToggleState (ctx.showTakeLanes, juce::dontSendNotification);
    takeLanesButton.onClick = [this] { ctx.showTakeLanes = takeLanesButton.getToggleState(); ctx.viewChanged(); };
    takeLanesButton.setTooltip ("Show alternate takes stacked under the active take");

    addTrackButton.onClick = [this] { if (onAddTrack) onAddTrack(); };

    ctx.edit.getTransport().addChangeListener (this);
    startTimerHz (30);
    updateButtons();
}

TransportBar::~TransportBar()
{
    ctx.edit.getTransport().removeChangeListener (this);
}

void TransportBar::doRecord()
{
    auto& tc = ctx.edit.getTransport();

    if (tc.isRecording())
    {
        ctx.recording.stopRecording();
        ctx.status ("Recording stopped");
        return;
    }

    auto r = ctx.recording.startRecording();

    if (r.failed())
        ctx.status (r.getErrorMessage(), true);
    else
        ctx.status (ctx.edit.getCountInMode() != te::Edit::CountIn::none ? "Count-in..." : "Recording");
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (6, 6);
    const int h = r.getHeight();

    toStartButton.setBounds (r.removeFromLeft (40));    r.removeFromLeft (4);
    playButton.setBounds (r.removeFromLeft (64));       r.removeFromLeft (4);
    stopButton.setBounds (r.removeFromLeft (64));       r.removeFromLeft (4);
    recordButton.setBounds (r.removeFromLeft (64));     r.removeFromLeft (12);

    positionLabel.setBounds (r.removeFromLeft (120));
    barsLabel.setBounds (r.removeFromLeft (110));       r.removeFromLeft (12);

    tempoLabel.setBounds (r.removeFromLeft (50));
    tempoSlider.setBounds (r.removeFromLeft (130).withHeight (h));  r.removeFromLeft (12);

    clickButton.setBounds (r.removeFromLeft (60));      r.removeFromLeft (4);
    countInButton.setBounds (r.removeFromLeft (80));    r.removeFromLeft (4);
    snapButton.setBounds (r.removeFromLeft (60));       r.removeFromLeft (4);
    takeLanesButton.setBounds (r.removeFromLeft (60));  r.removeFromLeft (12);

    addTrackButton.setBounds (r.removeFromRight (80));
    statusLabel.setBounds (r);
}

void TransportBar::timerCallback()
{
    auto& tc = ctx.edit.getTransport();
    const auto pos = tc.getPosition();
    positionLabel.setText (formatClock (pos), juce::dontSendNotification);
    barsLabel.setText (formatBars (ctx.edit, pos), juce::dontSendNotification);

    juce::String s;

    if (tc.isRecording())
        s = "RECORDING " + juce::String (ctx.recording.getNumArmedTracks()) + " track(s)";
    else if (tc.isPlaying())
        s = "Playing";
    else
    {
        const int armed = ctx.recording.getNumArmedTracks();
        s = armed > 0 ? "Stopped - " + juce::String (armed) + " track(s) armed, ready to record" : "Stopped";
    }

    statusLabel.setText (s, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, tc.isRecording() ? theme::record : theme::textDim);
}

void TransportBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateButtons();
}

void TransportBar::updateButtons()
{
    auto& tc = ctx.edit.getTransport();
    playButton.setToggleState (tc.isPlaying() && ! tc.isRecording(), juce::dontSendNotification);
    playButton.setButtonText (tc.isPlaying() ? "Pause" : "Play");
    recordButton.setToggleState (tc.isRecording(), juce::dontSendNotification);
    recordButton.setButtonText (tc.isRecording() ? "Stop" : "Rec");
    clickButton.setToggleState (ctx.edit.clickTrackEnabled.get(), juce::dontSendNotification);
}

} // namespace opensession
