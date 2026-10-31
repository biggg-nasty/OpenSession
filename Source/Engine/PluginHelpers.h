// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/** Creates one of the engine's built-in plugins (EqualiserPlugin, CompressorPlugin, ReverbPlugin...)
    and inserts it into a plugin list at the given index (0 = first, i.e. pre-fader). */
template <typename PluginType>
inline juce::ReferenceCountedObjectPtr<PluginType> insertBuiltInPlugin (te::Edit& edit, te::PluginList& list, int index)
{
    if (auto p = edit.getPluginCache().createNewPlugin (PluginType::xmlTypeName, {}))
    {
        if (auto* typed = dynamic_cast<PluginType*> (p.get()))
        {
            list.insertPlugin (p, index, nullptr);
            return typed;
        }
    }

    return {};
}

template <typename PluginType>
inline juce::ReferenceCountedObjectPtr<PluginType> insertBuiltInPlugin (te::Track& track, int index = 0)
{
    return insertBuiltInPlugin<PluginType> (track.edit, track.pluginList, index);
}

} // namespace opensession
