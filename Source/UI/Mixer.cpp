// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mixer.h"
#include "Theme.h"
#include "../Engine/PluginHosting.h"
#include "PluginWindow.h"

namespace opensession
{

//==============================================================================
ChannelStrip::ChannelStrip (SessionContext& c, te::AudioTrack* t) : ctx (c), track (t)
{
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (eqButton);
    addAndMakeVisible (compButton);
    addAndMakeVisible (reverbButton);
    addAndMakeVisible (fxEditButton);
    addAndMakeVisible (addPluginButton);
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
    fxEditButton.onClick = [this] { showInsertsMenu(); };
    addPluginButton.onClick = [this] { showAddPluginMenu(); };
    eqButton.setTooltip ("Parametric EQ insert: click to add (opens the editor) or bypass; right-click to open the editor");
    compButton.setTooltip ("Compressor insert: click to add (opens the editor) or bypass; right-click to open the editor");
    reverbButton.setTooltip ("Reverb insert: click to add (opens the editor) or bypass; right-click to open the editor");
    addPluginButton.setTooltip ("Add an insert: built-in effects or a scanned VST3 / Audio Unit plugin");

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
    auto* existing = findPlugin<PluginType>();

    if (existing != nullptr && juce::ModifierKeys::currentModifiers.isPopupMenu())
    {
        existing->showWindowExplicitly();
        return;
    }

    ctx.edit.getUndoManager().beginNewTransaction();

    if (existing != nullptr)
    {
        existing->setEnabled (! existing->isEnabled());
    }
    else
    {
        // Insert pre-fader (index 0) so the fader stays the last stage before the meter.
        te::Plugin::Ptr created = insertBuiltInPlugin<PluginType> (ctx.edit, *getPluginList(), 0);

        if (created == nullptr)
            ctx.status ("Couldn't add the plugin", true);
        else
            created->showWindowExplicitly();
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

void ChannelStrip::showInsertsMenu()
{
    auto* list = getPluginList();

    if (list == nullptr)
        return;

    auto plugins = PluginHosting::getUserInserts (*list);

    if (plugins.isEmpty())
    {
        ctx.status ("No inserts on this channel yet - click EQ, CMP, REV or + to add one");
        return;
    }

    // Item ids: 1000 * action + index. Actions: 1 open editor, 2 bypass toggle, 3 remove.
    juce::PopupMenu m;

    for (int i = 0; i < plugins.size(); ++i)
    {
        auto* p = plugins[i];
        juce::PopupMenu sub;
        sub.addItem (1000 + i, "Open editor");
        sub.addItem (2000 + i, "Bypass", true, ! p->isEnabled());
        sub.addSeparator();
        sub.addItem (3000 + i, "Remove");
        m.addSubMenu (juce::String (i + 1) + ". " + PluginHosting::describe (*p), sub);
    }

    juce::Array<te::Plugin::Ptr> held;

    for (auto* p : plugins)
        held.add (p);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fxEditButton),
                     [safe = juce::Component::SafePointer<ChannelStrip> (this), held] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;

        const int action = result / 1000, index = result % 1000;

        if (index >= held.size())
            return;

        auto p = held[index];

        switch (action)
        {
            case 1: p->showWindowExplicitly(); break;
            case 2: safe->ctx.edit.getUndoManager().beginNewTransaction(); p->setEnabled (! p->isEnabled()); break;
            case 3:
                safe->ctx.edit.getUndoManager().beginNewTransaction ("Remove insert");
                p->hideWindowForShutdown();
                p->deleteFromParent();
                break;
            default: break;
        }

        safe->refresh();
    });
}

void ChannelStrip::showAddPluginMenu()
{
    auto* list = getPluginList();

    if (list == nullptr)
        return;

    // Ids: 1-3 built-ins, 10 = plugin manager, 100 + n = known third-party effect n.
    juce::PopupMenu m;
    m.addSectionHeader ("Built-in");
    m.addItem (1, "Parametric EQ", findPlugin<te::EqualiserPlugin>() == nullptr);
    m.addItem (2, "Compressor", findPlugin<te::CompressorPlugin>() == nullptr);
    m.addItem (3, "Reverb", findPlugin<te::ReverbPlugin>() == nullptr);

    auto known = PluginHosting::getKnownEffects (ctx.engine);
    std::map<juce::String, juce::PopupMenu> byFormat;

    for (int i = 0; i < known.size(); ++i)
    {
        const auto& d = known.getReference (i);
        byFormat[d.pluginFormatName].addItem (100 + i, d.name + (d.manufacturerName.isNotEmpty() ? "  (" + d.manufacturerName + ")" : juce::String()));
    }

    m.addSeparator();

    if (byFormat.empty())
        m.addItem (-1, "No third-party plugins scanned yet", false);

    for (auto& [format, sub] : byFormat)
        m.addSubMenu (format == "AudioUnit" ? juce::String ("Audio Unit") : format, sub);

    m.addSeparator();
    m.addItem (10, "Scan for plugins...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addPluginButton),
                     [safe = juce::Component::SafePointer<ChannelStrip> (this), known] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;

        if (result == 1) { safe->togglePlugin<te::EqualiserPlugin> (safe->eqButton); return; }
        if (result == 2) { safe->togglePlugin<te::CompressorPlugin> (safe->compButton); return; }
        if (result == 3) { safe->togglePlugin<te::ReverbPlugin> (safe->reverbButton); return; }
        if (result == 10) { PluginManagerPanel::show (safe->ctx.engine); return; }

        const int index = result - 100;

        if (index < 0 || index >= known.size())
            return;

        safe->ctx.edit.getUndoManager().beginNewTransaction ("Add plugin");
        juce::String error;
        auto p = PluginHosting::insertExternalPlugin (safe->ctx.edit, *safe->getPluginList(), known.getReference (index), error);

        if (error.isNotEmpty())
            safe->ctx.status (error, true);
        else if (p != nullptr)
            safe->ctx.status ("Added " + PluginHosting::describe (*p));

        if (p != nullptr && ! PluginHosting::isMissing (*p))
            p->showWindowExplicitly();

        safe->refresh();
    });
}

void ChannelStrip::refreshInsertsButton()
{
    auto* list = getPluginList();
    auto inserts = list != nullptr ? PluginHosting::getUserInserts (*list) : juce::Array<te::Plugin*>();
    juce::StringArray lines;
    bool anyMissing = false;

    for (auto* p : inserts)
    {
        lines.add (PluginHosting::describe (*p));
        anyMissing = anyMissing || PluginHosting::isMissing (*p);
    }

    fxEditButton.setButtonText (inserts.isEmpty() ? "FX..." : "FX (" + juce::String (inserts.size()) + ")");
    fxEditButton.setColour (juce::TextButton::buttonColourId, anyMissing ? theme::warning.withAlpha (0.6f) : theme::panelLight);
    fxEditButton.setTooltip (inserts.isEmpty() ? juce::String ("Inserts on this channel: none")
                                               : "Inserts, in signal order (click to open, bypass or remove):\n" + lines.joinIntoString ("\n"));
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
    refreshInsertsButton();
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
    auto fxRow = r.removeFromTop (20);
    addPluginButton.setBounds (fxRow.removeFromRight (24).reduced (1, 0));
    fxEditButton.setBounds (fxRow.reduced (1, 0));
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
