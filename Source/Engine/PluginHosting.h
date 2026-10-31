// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PluginHelpers.h"

namespace opensession
{

/**
    Third-party plugin hosting on top of the engine's PluginManager / ExternalPlugin.

    Reused from the engine: format manager (VST3, AudioUnit and, if built with the legacy SDK,
    VST), the known-plugin list and its out-of-process scanner, plugin instantiation, state
    save/restore inside the Edit, latency compensation, and pass-through when a plugin is missing.
    Ours: which formats we build, where new inserts go, how missing / failed plugins are reported.
*/
namespace PluginHosting
{
    /** Format names this build can host, as JUCE reports them ("VST3", "AudioUnit", "VST"). */
    juce::StringArray getHostedFormatNames (te::Engine&);

    /** Plain-language note about formats we cannot host (AAX, RTAS, VST2 without the SDK). */
    juce::String getUnsupportedFormatsNote();

    /** Scanned third-party effects (instruments excluded), sorted by format, maker, name. */
    juce::Array<juce::PluginDescription> getKnownEffects (te::Engine&);

    /** Where a new insert goes so it runs pre-fader, after the inserts already there:
        just before the track's volume/pan and level meter. */
    int getPreFaderInsertIndex (te::PluginList&);

    /** Creates an ExternalPlugin for the description and inserts it pre-fader. On failure returns
        nullptr and sets error. A plugin that is in the list but fails to load is still inserted
        (so its slot and settings survive) and reported through error. */
    te::Plugin::Ptr insertExternalPlugin (te::Edit&, te::PluginList&, const juce::PluginDescription&, juce::String& error);

    bool isExternal (te::Plugin&);

    /** True for an external plugin whose binary could not be loaded. The engine keeps its saved
        state in the session and passes audio through unprocessed. */
    bool isMissing (te::Plugin&);

    /** One line for menus and tooltips: name, format, and "missing" / "bypassed" / latency. */
    juce::String describe (te::Plugin&);

    /** Inserts on a list that the user manages (excludes volume/pan and level meters). */
    juce::Array<te::Plugin*> getUserInserts (te::PluginList&);
}

} // namespace opensession
