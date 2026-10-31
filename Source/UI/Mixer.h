// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionContext.h"
#include "Meters.h"

namespace opensession
{

/** Generic editor for a built-in plugin: one slider per automatable parameter, plus bypass. */
class PluginParameterPanel : public juce::Component
{
public:
    explicit PluginParameterPanel (te::Plugin&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    te::Plugin::Ptr plugin;
    juce::ToggleButton enabledButton { "Enabled (untick to bypass)" };
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::Slider> sliders;
};

/** One mixer channel: name, inserts (EQ / Comp / Reverb), pan, fader, mute/solo, meter.
    track == nullptr means the master. */
class ChannelStrip : public juce::Component,
                     private juce::Timer
{
public:
    ChannelStrip (SessionContext&, te::AudioTrack*);
    ~ChannelStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    static constexpr int stripWidth = 96;

private:
    SessionContext& ctx;
    te::AudioTrack* track;   // nullptr = master

    juce::Label nameLabel;
    juce::TextButton eqButton { "EQ" }, compButton { "CMP" }, reverbButton { "REV" }, fxEditButton { "FX..." };
    juce::Slider fader, pan;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    LevelMeterComponent meter { true };
    juce::Label dbLabel;

    te::VolumeAndPanPlugin* getVolumePlugin() const;
    te::PluginList* getPluginList() const;
    template <typename PluginType> te::Plugin* findPlugin() const;
    template <typename PluginType> void togglePlugin (juce::TextButton&);
    template <typename PluginType> void refreshPluginButton (juce::TextButton&);
    void openPluginEditor();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

class MixerComponent : public juce::Component,
                       private juce::ValueTree::Listener
{
public:
    explicit MixerComponent (SessionContext&);
    ~MixerComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void rebuild();

private:
    SessionContext& ctx;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> master;
    juce::Viewport viewport;
    juce::Component stripContainer;
    bool rebuildPending = false;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};

} // namespace opensession
