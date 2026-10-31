// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginWindow.h"
#include "PluginEditors.h"
#include "Theme.h"
#include "../Engine/PluginHosting.h"

namespace opensession
{

namespace
{
    /** Shown instead of a GUI when a third-party plugin could not be loaded. */
    struct MissingPluginPanel : public juce::Component
    {
        explicit MissingPluginPanel (te::ExternalPlugin& p)
        {
            juce::String s;
            s << p.desc.name << " (" << p.desc.pluginFormatName << ", " << p.desc.manufacturerName << ") could not be loaded.\n\n"
              << "Audio passes through this slot unprocessed. Its settings are kept in the session and come back "
                 "when the plugin is installed again (then reopen the session).\n\n"
              << "Expected at: " << p.desc.fileOrIdentifier;

            if (p.getLoadError().isNotEmpty())
                s << "\n\nError: " << p.getLoadError();

            text.setMultiLine (true);
            text.setReadOnly (true);
            text.setText (s, false);
            addAndMakeVisible (text);
            setSize (440, 200);
        }

        void resized() override     { text.setBounds (getLocalBounds().reduced (10)); }
        void paint (juce::Graphics& g) override { g.fillAll (theme::panel); }

        juce::TextEditor text;
    };

    std::unique_ptr<juce::Component> createContent (te::Plugin& plugin)
    {
        if (auto builtIn = createBuiltInPluginEditor (plugin))
            return builtIn;

        auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin);

        if (ext == nullptr)
            return {};

        if (ext->getAudioPluginInstance() == nullptr)
            return std::make_unique<MissingPluginPanel> (*ext);

        if (auto editor = ext->createEditor())
            return editor;

        // No GUI of its own: JUCE's generic list of the plugin's parameters.
        return std::make_unique<juce::GenericAudioProcessorEditor> (*ext->getAudioPluginInstance());
    }
}

//==============================================================================
PluginWindow::PluginWindow (te::Plugin& p)
    : juce::DocumentWindow (p.getName() + (PluginHosting::isExternal (p) ? juce::String() : juce::String (" - OpenSession")),
                            theme::panel, juce::DocumentWindow::closeButton, true),
      plugin (p)
{
    setUsingNativeTitleBar (true);

    JUCE_AUTORELEASEPOOL
    {
        if (auto content = createContent (plugin))
            setContentOwned (content.release(), true);
    }

    setResizable (false, false);
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setTopLeftPosition (plugin.windowState->choosePositionForPluginWindow());
    updateStoredBounds = true;
}

PluginWindow::~PluginWindow()
{
    updateStoredBounds = false;
    plugin.edit.flushPluginStateIfNeeded (plugin);
    clearContentComponent();
}

std::unique_ptr<juce::Component> PluginWindow::create (te::Plugin& plugin)
{
    auto w = std::make_unique<PluginWindow> (plugin);

    if (w->getContentComponent() == nullptr)
        return {};

    w->setVisible (true);
    w->toFront (true);
    return w;
}

void PluginWindow::closeButtonPressed()
{
    plugin.windowState->closeWindowExplicitly();   // deletes this window
}

void PluginWindow::moved()
{
    if (updateStoredBounds)
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }
}

//==============================================================================
static std::unique_ptr<juce::PropertiesFile> makeScanSettings (const juce::File& folder)
{
    juce::PropertiesFile::Options o;
    o.applicationName = "PluginScan";
    o.filenameSuffix = ".settings";
    o.folderName = folder.getFullPathName();
    return std::make_unique<juce::PropertiesFile> (folder.getChildFile ("PluginScan.settings"), o);
}

juce::File PluginManagerPanel::getSettingsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("OpenSession");
}

PluginManagerPanel::PluginManagerPanel (te::Engine& engine)
    : scanSettings (makeScanSettings (getSettingsFolder())),
      list (engine.getPluginManager().pluginFormatManager, engine.getPluginManager().knownPluginList,
            getSettingsFolder().getChildFile ("PluginScanCrashes.txt"), scanSettings.get(), true)
{
    addAndMakeVisible (list);
    list.setOptionsButtonText ("Scan / options...");

    juce::String s;
    s << "Formats in this build: " << PluginHosting::getHostedFormatNames (engine).joinIntoString (", ")
      << ". Scans run in a separate process, so a plugin that crashes while being scanned is skipped and "
         "listed as failed instead of closing OpenSession. Add a plugin to a track with the + button on its "
         "mixer strip.\n\n" << PluginHosting::getUnsupportedFormatsNote();

    notes.setMultiLine (true);
    notes.setReadOnly (true);
    notes.setText (s, false);
    addAndMakeVisible (notes);

    setSize (720, 560);
}

PluginManagerPanel::~PluginManagerPanel()
{
    if (scanSettings != nullptr)
        scanSettings->saveIfNeeded();
}

void PluginManagerPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void PluginManagerPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    notes.setBounds (r.removeFromBottom (150));
    r.removeFromBottom (8);
    list.setBounds (r);
}

void PluginManagerPanel::show (te::Engine& engine)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new PluginManagerPanel (engine));
    o.dialogTitle = "Plugins";
    o.dialogBackgroundColour = theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

} // namespace opensession
