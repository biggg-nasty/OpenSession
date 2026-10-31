// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrackHeader.h"
#include "Theme.h"

namespace opensession
{

TrackHeader::TrackHeader (SessionContext& c, te::AudioTrack& t) : ctx (c), track (t)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (inputBox);
    addAndMakeVisible (armButton);
    addAndMakeVisible (monitorButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (inputMeterLabel);
    addAndMakeVisible (channelsLabel);

    nameLabel.setText (track.getName(), juce::dontSendNotification);
    nameLabel.setEditable (false, true, false);
    nameLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    nameLabel.onTextChange = [this]
    {
        if (nameLabel.getText().trim().isNotEmpty())
            track.setName (nameLabel.getText().trim());
        else
            nameLabel.setText (track.getName(), juce::dontSendNotification);
    };

    inputMeterLabel.setColour (juce::Label::textColourId, theme::textDim);
    inputMeterLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    channelsLabel.setColour (juce::Label::textColourId, theme::textDim);
    channelsLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    channelsLabel.setJustificationType (juce::Justification::centredRight);

    armButton.setColour (juce::TextButton::buttonOnColourId, theme::record);
    armButton.setTooltip ("Arm for recording (needs an input)");
    armButton.onClick = [this]
    {
        auto r = ctx.recording.setArmed (track, ! ctx.recording.isArmed (track));

        if (r.failed())
            ctx.status (r.getErrorMessage(), true);

        refresh();
    };

    monitorButton.setColour (juce::TextButton::buttonOnColourId, theme::monitorOn);
    monitorButton.setTooltip ("Input monitoring: OFF (hear nothing) / AUTO (hear input while armed) / ON (always). Independent of arming.");
    monitorButton.onClick = [this] { cycleMonitorMode(); };

    muteButton.setColour (juce::TextButton::buttonOnColourId, theme::warning);
    muteButton.onClick = [this] { track.setMute (! track.isMuted (false)); refresh(); };
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::accent);
    soloButton.onClick = [this] { track.setSolo (! track.isSolo (false)); refresh(); };

    inputBox.onChange = [this] { applyInputChoice(); };
    inputBox.setTooltip ("Which audio interface input feeds this track");

    if (auto* lm = track.getLevelMeterPlugin())
        outputMeter.setMeasurer (&lm->measurer);

    rebuildInputList();
    refresh();
    startTimerHz (10);
}

TrackHeader::~TrackHeader() = default;

void TrackHeader::rebuildInputList()
{
    inputBox.clear (juce::dontSendNotification);
    inputBox.addItem ("No input", 1);
    int id = 2;

    for (auto* wi : ctx.recording.getAvailableInputs())
    {
        auto label = wi->getName() + (wi->getChannels().getNumChannels() == 2 ? " (stereo)" : " (mono)");
        inputBox.addItem (label, id++);
    }

    auto* current = ctx.recording.getInputForTrack (track);
    int selected = 1;
    id = 2;

    for (auto* wi : ctx.recording.getAvailableInputs())
    {
        if (wi == current)
            selected = id;
        ++id;
    }

    inputBox.setSelectedId (selected, juce::dontSendNotification);
}

void TrackHeader::applyInputChoice()
{
    const int choice = inputBox.getSelectedId();
    te::WaveInputDevice* chosen = nullptr;

    if (choice >= 2)
    {
        auto inputs = ctx.recording.getAvailableInputs();
        const size_t index = (size_t) (choice - 2);

        if (index < inputs.size())
            chosen = inputs[index];
    }

    auto r = ctx.recording.assignInput (track, chosen);

    if (r.failed())
        ctx.status (r.getErrorMessage(), true);
    else if (chosen != nullptr)
        ctx.status (track.getName() + " <- " + chosen->getName() + ". Monitoring is " + RecordingController::monitorModeToString (ctx.recording.getMonitorMode (track)) + ".");

    refresh();
}

void TrackHeader::cycleMonitorMode()
{
    using MM = te::InputDevice::MonitorMode;
    auto mode = ctx.recording.getMonitorMode (track);
    auto next = mode == MM::off ? MM::automatic : mode == MM::automatic ? MM::on : MM::off;
    ctx.recording.setMonitorMode (track, next);

    ctx.status ("Monitoring for " + track.getName() + ": " + RecordingController::monitorModeToString (next)
                + (next == MM::off ? " (use your interface's direct monitoring to hear yourself)"
                   : " (software monitoring - you hear the input through the track, delayed by the round-trip latency)"));
    refresh();
}

void TrackHeader::refresh()
{
    nameLabel.setText (track.getName(), juce::dontSendNotification);
    armButton.setToggleState (ctx.recording.isArmed (track), juce::dontSendNotification);
    muteButton.setToggleState (track.isMuted (false), juce::dontSendNotification);
    soloButton.setToggleState (track.isSolo (false), juce::dontSendNotification);

    auto mode = ctx.recording.getMonitorMode (track);
    monitorButton.setButtonText (RecordingController::monitorModeToString (mode));
    monitorButton.setToggleState (mode != te::InputDevice::MonitorMode::off, juce::dontSendNotification);
    monitorButton.setEnabled (ctx.recording.getInputForTrack (track) != nullptr);

    if (auto* wi = ctx.recording.getInputForTrack (track))
    {
        inputMeter.setMeasurer (&wi->levelMeasurer);
        channelsLabel.setText (wi->getChannels().getNumChannels() == 2 ? "stereo in" : "mono in", juce::dontSendNotification);
    }
    else
    {
        inputMeter.setMeasurer (nullptr);
        channelsLabel.setText ("", juce::dontSendNotification);
    }

    repaint();
}

void TrackHeader::timerCallback()
{
    // Cheap sync for state changed elsewhere (undo, tests, menu commands).
    const bool armed = ctx.recording.isArmed (track);

    if (armed != armButton.getToggleState()
         || track.isMuted (false) != muteButton.getToggleState()
         || track.isSolo (false) != soloButton.getToggleState()
         || nameLabel.getText() != track.getName())
        refresh();
}

void TrackHeader::paint (juce::Graphics& g)
{
    const bool selected = ctx.selectedTrack == track.itemID;
    g.fillAll (selected ? theme::panelLight : theme::panel);
    g.setColour (theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    g.setColour (ctx.recording.isArmed (track) ? theme::record : track.getColour().withAlpha (0.9f));
    g.fillRect (0, 0, 4, getHeight());

    if (ctx.recording.isInputAudible (track))
    {
        g.setColour (theme::monitorOn);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("LIVE INPUT AUDIBLE", getLocalBounds().removeFromBottom (14).reduced (10, 0), juce::Justification::centredLeft);
    }
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    outputMeter.setBounds (r.removeFromRight (14));
    r.removeFromRight (6);

    auto row1 = r.removeFromTop (22);
    nameLabel.setBounds (row1.removeFromLeft (row1.getWidth() - 70));
    channelsLabel.setBounds (row1);

    r.removeFromTop (4);
    auto row2 = r.removeFromTop (24);
    inputBox.setBounds (row2);

    r.removeFromTop (4);
    auto row3 = r.removeFromTop (24);
    armButton.setBounds (row3.removeFromLeft (34));     row3.removeFromLeft (4);
    monitorButton.setBounds (row3.removeFromLeft (54)); row3.removeFromLeft (4);
    muteButton.setBounds (row3.removeFromLeft (30));    row3.removeFromLeft (4);
    soloButton.setBounds (row3.removeFromLeft (30));    row3.removeFromLeft (8);
    inputMeterLabel.setBounds (row3.removeFromLeft (16));
    inputMeter.setBounds (row3.reduced (0, 6));
}

void TrackHeader::mouseDown (const juce::MouseEvent&)
{
    ctx.selectTrack (&track);
}

//==============================================================================
TrackHeaderList::TrackHeaderList (SessionContext& c) : ctx (c)
{
    ctx.addChangeListener (this);
    ctx.edit.state.addListener (this);
    rebuild();
}

TrackHeaderList::~TrackHeaderList()
{
    ctx.edit.state.removeListener (this);
    ctx.removeChangeListener (this);
}

void TrackHeaderList::rebuild()
{
    rebuildPending = false;
    headers.clear();

    for (auto* t : te::getAudioTracks (ctx.edit))
        addAndMakeVisible (headers.add (new TrackHeader (ctx, *t)));

    resized();
    repaint();
}

int TrackHeaderList::getContentHeight() const
{
    return headers.size() * SessionContext::trackHeight;
}

void TrackHeaderList::resized()
{
    int y = SessionContext::rulerHeight - yOffset;

    for (auto* h : headers)
    {
        h->setBounds (0, y, getWidth(), SessionContext::trackHeight);
        y += SessionContext::trackHeight;
    }
}

void TrackHeaderList::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setColour (theme::panel);
    g.fillRect (0, 0, getWidth(), SessionContext::rulerHeight);
    g.setColour (theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("TRACKS", getLocalBounds().removeFromTop (SessionContext::rulerHeight).reduced (10, 0), juce::Justification::centredLeft);
    g.setColour (theme::outline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void TrackHeaderList::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* h : headers)
        h->repaint();
}

void TrackHeaderList::triggerRebuild()
{
    if (rebuildPending)
        return;

    rebuildPending = true;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TrackHeaderList> (this)]
    {
        if (safe != nullptr)
            safe->rebuild();
    });
}

void TrackHeaderList::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::TRACK) || parent.hasType (te::IDs::INPUTDEVICES) || child.hasType (te::IDs::INPUTDEVICE) || child.hasType (te::IDs::INPUTDEVICEDESTINATION))
        triggerRebuild();
}

void TrackHeaderList::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::TRACK) || parent.hasType (te::IDs::INPUTDEVICES) || child.hasType (te::IDs::INPUTDEVICE) || child.hasType (te::IDs::INPUTDEVICEDESTINATION))
        triggerRebuild();
}

void TrackHeaderList::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (te::IDs::EDIT))
        triggerRebuild();
}

void TrackHeaderList::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v.hasType (te::IDs::INPUTDEVICEDESTINATION) || id == te::IDs::targetID || id == te::IDs::armed)
        for (auto* h : headers)
            h->refresh();
}

} // namespace opensession
