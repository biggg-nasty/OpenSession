// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/**
    The window the engine shows for a plugin (Plugin::showWindowExplicitly). Content is the
    plugin's own GUI for a third-party plugin (a generic parameter list if it has none), our
    knob / parametric EQ editor for a built-in, or an explanation if the plugin is missing.
    Position is stored in the plugin state by the engine, so it comes back with the session.
*/
class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin&);
    ~PluginWindow() override;

    /** Factory installed into OpenSessionEngine; returns nullptr if there is nothing to show. */
    static std::unique_ptr<juce::Component> create (te::Plugin&);

private:
    te::Plugin& plugin;
    bool updateStoredBounds = false;

    void closeButtonPressed() override;
    void moved() override;
    float getDesktopScaleFactor() const override        { return 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

/** Plugins window: scan for VST3 / Audio Unit plugins (out of process), see what was found,
    remove entries, and read which formats are not supported and why. */
class PluginManagerPanel : public juce::Component
{
public:
    explicit PluginManagerPanel (te::Engine&);
    ~PluginManagerPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    static void show (te::Engine&);

private:
    std::unique_ptr<juce::PropertiesFile> scanSettings;
    juce::PluginListComponent list;
    juce::TextEditor notes;

    static juce::File getSettingsFolder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManagerPanel)
};

} // namespace opensession
