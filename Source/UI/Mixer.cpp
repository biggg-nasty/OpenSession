// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mixer.h"
#include "Theme.h"
#include "../Engine/PluginHelpers.h"

namespace opensession
{

//==============================================================================
PluginParameterPanel::PluginParameterPanel (te::Plugin& p) : plugin (&p)
{
    addAndMakeVisible (enabledButton);
    enabledButton.setToggleState (plugin->isEnabled(), juce::dontSendNotification);
    enabledButton.onClick = [this] { plugin->setEnabled (enabledButton.getToggleState()); };

    for (auto param : plugin->getAutomatableParameters())
    {
        auto* label = labels.add (new juce::Label ({}, param->paramName));
        label->setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (label);

        auto* slider = sliders.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
        const auto& range = param->valueRange;
        slider->setRange (range.start, range.end, 0.0);
        slider->setSkewFactor (range.skew);
        slider->setValue (param->getCurrentValue(), juce::dontSendNotification);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
        slider->textFromValueFunction = [param] (double) { return param->getCurrentValueAsString(); };
        slider->onValueChange = [param, slider] { param->setParameter ((float) slider->getValue(), juce::sendNotification); };
        addAndMakeVisible (slider);
    }

    setSize (420, 40 + 30 * sliders.size());
}

void PluginParameterPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void PluginParameterPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    enabledButton.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    for (int i = 0; i < sliders.size(); ++i)
    {
        auto row = r.removeFromTop (28);
        labels[i]->setBounds (row.removeFromLeft (120));
        sliders[i]->setBounds (row);
    }
}

//==============================================================================
ChannelStrip::ChannelStrip (SessionContext& c, te::AudioTrack* t) : ctx (c), track (t)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (eqButton);
    addAndMakeVisible (compButton);
    addAndMakeVisible (reverbButton);
    addAndMakeVisible (fxEditButton);
    addAndMakeVisible (fader);
    addAndMakeVisible (pan);
    addAndMakeVisible (meter);
    addAndMakeVisible (dbLabel);

    if (track != nullptr)
    {
        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);
        muteButton.setColour (juce::TextButton::buttonOnColourId, theme::warning);
        soloButton.setColour (juce::TextButton::buttonOnColourId, theme::accent);
        muteButton.onClick = [this] { track->setMute (! track->isMuted (false)); refresh(); };
        soloButton.onClick = [this] { track->setSolo (! track->isSolo (false)); refresh(); };
    }

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    nameLabel.setMinimumHorizontalScale (0.6f);
    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    dbLabel.setColour (juce::Label::textColourId, theme::textDim);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (-60.0, 6.0, 0.1);
    fader.setSkewFactorFromMidPoint (-12.0);
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.onValueChange = [this]
    {
        if (auto* vp = getVolumePlugin())
            vp->setVolumeDb ((float) fader.getValue());

        dbLabel.setText (juce::String (fader.getValue(), 1) + " dB", juce::dontSendNotification);
    };

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setTooltip ("Pan");
    pan.onValueChange = [this]
    {
        if (auto* vp = getVolumePlugin())
            vp->setPan ((float) pan.getValue());
    };

    eqButton.onClick = [this] { togglePlugin<te::EqualiserPlugin> (eqButton); };
    compButton.onClick = [this] { togglePlugin<te::CompressorPlugin> (compButton); };
    reverbButton.onClick = [this] { togglePlugin<te::ReverbPlugin> (reverbButton); };
    fxEditButton.onClick = [this] { openPluginEditor(); };
    eqButton.setTooltip ("4-band EQ insert: click to add/enable, click again to bypass");
    compButton.setTooltip ("Compressor insert: click to add/enable, click again to bypass");
    reverbButton.setTooltip ("Reverb insert: click to add/enable, click again to bypass");

    if (track != nullptr)
    {
        if (auto* lm = track->getLevelMeterPlugin())
            meter.setMeasurer (&lm->measurer);
    }
    else
    {
        if (auto* lm = ctx.edit.getMasterPluginList().findFirstPluginOfType<te::LevelMeterPlugin>())
            meter.setMeasurer (&lm->measurer);
        else if (auto* mt = ctx.edit.getMasterTrack())
            if (auto* lm2 = mt->pluginList.findFirstPluginOfType<te::LevelMeterPlugin>())
                meter.setMeasurer (&lm2->measurer);
    }

    refresh();
    startTimerHz (4);
}

ChannelStrip::~ChannelStrip() = default;

te::VolumeAndPanPlugin* ChannelStrip::getVolumePlugin() const
{
    if (track != nullptr)
        return track->getVolumePlugin();

    return ctx.edit.getMasterVolumePlugin().get();
}

te::PluginList* ChannelStrip::getPluginList() const
{
    if (track != nullptr)
        return &track->pluginList;

    return &ctx.edit.getMasterPluginList();
}

template <typename PluginType>
te::Plugin* ChannelStrip::findPlugin() const
{
    if (auto* list = getPluginList())
        return list->findFirstPluginOfType<PluginType>();

    return nullptr;
}

template <typename PluginType>
void ChannelStrip::togglePlugin (juce::TextButton& button)
{
    ctx.edit.getUndoManager().beginNewTransaction();

    if (auto* existing = findPlugin<PluginType>())
    {
        existing->setEnabled (! existing->isEnabled());
    }
    else
    {
        // Insert pre-fader (index 0) so the fader stays the last stage before the meter.
        te::Plugin::Ptr created = insertBuiltInPlugin<PluginType> (ctx.edit, *getPluginList(), 0);

        if (created == nullptr)
            ctx.status ("Couldn't add the plugin", true);
    }

    refreshPluginButton<PluginType> (button);
}

template <typename PluginType>
void ChannelStrip::refreshPluginButton (juce::TextButton& button)
{
    auto* p = findPlugin<PluginType>();
    button.setToggleState (p != nullptr && p->isEnabled(), juce::dontSendNotification);
    button.setColour (juce::TextButton::buttonColourId, p == nullptr ? theme::panelLight : theme::recordDim.withAlpha (0.6f));
    button.setAlpha (p == nullptr ? 0.7f : 1.0f);
}

void ChannelStrip::openPluginEditor()
{
    auto* list = getPluginList();

    if (list == nullptr)
        return;

    juce::PopupMenu m;
    int id = 1;
    juce::Array<te::Plugin*> plugins;

    for (auto p : list->getPlugins())
    {
        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr)
            continue;

        plugins.add (p);
        m.addItem (id++, p->getName() + (p->isEnabled() ? "" : " (bypassed)"));
    }

    if (plugins.isEmpty())
    {
        ctx.status ("No inserts on this channel yet - click EQ, Comp or Verb to add one");
        return;
    }

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fxEditButton),
                     [this, plugins] (int result)
    {
        if (result <= 0 || result > plugins.size())
            return;

        auto* p = plugins[result - 1];
        auto* panel = new PluginParameterPanel (*p);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (panel);
        o.dialogTitle = (track != nullptr ? track->getName() : juce::String ("Master")) + " - " + p->getName();
        o.dialogBackgroundColour = theme::panel;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    });
}

void ChannelStrip::refresh()
{
    nameLabel.setText (track != nullptr ? track->getName() : juce::String ("MASTER"), juce::dontSendNotification);

    if (auto* vp = getVolumePlugin())
    {
        fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
        pan.setValue (vp->getPan(), juce::dontSendNotification);
        dbLabel.setText (juce::String (vp->getVolumeDb(), 1) + " dB", juce::dontSendNotification);
    }

    if (track != nullptr)
    {
        muteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
        soloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);
    }

    refreshPluginButton<te::EqualiserPlugin> (eqButton);
    refreshPluginButton<te::CompressorPlugin> (compButton);
    refreshPluginButton<te::ReverbPlugin> (reverbButton);
}

void ChannelStrip::timerCallback()
{
    if (! fader.isMouseButtonDown() && ! pan.isMouseButtonDown())
        refresh();
}

void ChannelStrip::paint (juce::Graphics& g)
{
    g.fillAll (track == nullptr ? theme::panelLight : theme::panel);
    g.setColour (theme::outline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    if (track != nullptr)
    {
        g.setColour (track->getColour());
        g.fillRect (0, 0, getWidth(), 3);
    }
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 6);
    nameLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);

    auto fx = r.removeFromTop (22);
    const int w = fx.getWidth() / 3;
    eqButton.setBounds (fx.removeFromLeft (w).reduced (1, 0));
    compButton.setBounds (fx.removeFromLeft (w).reduced (1, 0));
    reverbButton.setBounds (fx.reduced (1, 0));
    r.removeFromTop (2);
    fxEditButton.setBounds (r.removeFromTop (20));
    r.removeFromTop (6);

    pan.setBounds (r.removeFromTop (40).withSizeKeepingCentre (40, 40));
    r.removeFromTop (4);

    auto bottom = r.removeFromBottom (24);

    if (track != nullptr)
    {
        muteButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (1, 0));
        soloButton.setBounds (bottom.reduced (1, 0));
    }

    dbLabel.setBounds (r.removeFromBottom (16));
    meter.setBounds (r.removeFromRight (16));
    r.removeFromRight (4);
    fader.setBounds (r);
}

//==============================================================================
MixerComponent::MixerComponent (SessionContext& c) : ctx (c)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&stripContainer, false);
    viewport.setScrollBarsShown (false, true);
    ctx.edit.state.addListener (this);
    rebuild();
}

MixerComponent::~MixerComponent()
{
    ctx.edit.state.removeListener (this);
}

void MixerComponent::rebuild()
{
    rebuildPending = false;
    strips.clear();
    master.reset();

    for (auto* t : te::getAudioTracks (ctx.edit))
        stripContainer.addAndMakeVisible (strips.add (new ChannelStrip (ctx, t)));

    master = std::make_unique<ChannelStrip> (ctx, nullptr);
    addAndMakeVisible (*master);
    resized();
}

void MixerComponent::resized()
{
    auto r = getLocalBounds();
    master->setBounds (r.removeFromRight (ChannelStrip::stripWidth + 10).reduced (4, 0));
    viewport.setBounds (r);

    stripContainer.setSize (std::max (r.getWidth(), strips.size() * ChannelStrip::stripWidth), r.getHeight() - (viewport.isHorizontalScrollBarShown() ? 8 : 0));
    int x = 0;

    for (auto* s : strips)
    {
        s->setBounds (x, 0, ChannelStrip::stripWidth, stripContainer.getHeight());
        x += ChannelStrip::stripWidth;
    }
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setColour (theme::outline);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void MixerComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::TRACK) && ! rebuildPending)
    {
        rebuildPending = true;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MixerComponent> (this)] { if (safe != nullptr) safe->rebuild(); });
    }
}

void MixerComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::TRACK) && ! rebuildPending)
    {
        rebuildPending = true;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MixerComponent> (this)] { if (safe != nullptr) safe->rebuild(); });
    }
}

void MixerComponent::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (te::IDs::EDIT) && ! rebuildPending)
    {
        rebuildPending = true;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MixerComponent> (this)] { if (safe != nullptr) safe->rebuild(); });
    }
}

} // namespace opensession
