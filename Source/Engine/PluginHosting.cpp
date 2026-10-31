// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginHosting.h"

namespace opensession
{

juce::StringArray PluginHosting::getHostedFormatNames (te::Engine& engine)
{
    juce::StringArray names;

    for (auto* f : engine.getPluginManager().pluginFormatManager.getFormats())
        if (f->getName() != te::PluginManager::builtInPluginFormatName)
            names.addIfNotAlreadyThere (f->getName());

    return names;
}

juce::String PluginHosting::getUnsupportedFormatsNote()
{
    juce::String s;
    s << "AAX and RTAS plugins cannot be loaded. AAX plugins only run inside Avid hosts (Pro Tools, "
         "Media Composer); Avid does not offer AAX hosting to other applications. RTAS was retired by "
         "Avid in 2013 and was 32-bit only. Most AAX/RTAS products are also sold as VST3 or Audio Unit: "
         "install that version and it will appear here.";
   #if ! JUCE_PLUGINHOST_VST
    s << "\n\nVST2 is not built in: Steinberg no longer licenses the VST2 SDK and it cannot be shipped "
         "in a GPL program. A private build can enable it with -DOPENSESSION_VST2_SDK_DIR=<path>.";
   #endif
    return s;
}

juce::Array<juce::PluginDescription> PluginHosting::getKnownEffects (te::Engine& engine)
{
    juce::Array<juce::PluginDescription> result;

    for (const auto& d : engine.getPluginManager().knownPluginList.getTypes())
        if (! d.isInstrument && ! te::PluginManager::isBuiltInPlugin (d))
            result.add (d);

    std::sort (result.begin(), result.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        if (a.pluginFormatName != b.pluginFormatName)   return a.pluginFormatName < b.pluginFormatName;
        if (a.manufacturerName != b.manufacturerName)   return a.manufacturerName.compareIgnoreCase (b.manufacturerName) < 0;
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    return result;
}

int PluginHosting::getPreFaderInsertIndex (te::PluginList& list)
{
    auto plugins = list.getPlugins();

    for (int i = 0; i < plugins.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugins[i].get()) != nullptr || dynamic_cast<te::LevelMeterPlugin*> (plugins[i].get()) != nullptr)
            return i;

    return plugins.size();
}

te::Plugin::Ptr PluginHosting::insertExternalPlugin (te::Edit& edit, te::PluginList& list, const juce::PluginDescription& desc, juce::String& error)
{
    auto p = edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);

    if (p == nullptr)
    {
        error = "Couldn't create " + desc.name;
        return {};
    }

    list.insertPlugin (p, getPreFaderInsertIndex (list), nullptr);

    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (p.get()); ext != nullptr && ext->isMissing())
        error = desc.name + " didn't load" + (ext->getLoadError().isNotEmpty() ? ": " + ext->getLoadError() : juce::String())
                + ". Audio passes through it unprocessed.";

    return p;
}

bool PluginHosting::isExternal (te::Plugin& p)
{
    return dynamic_cast<te::ExternalPlugin*> (&p) != nullptr;
}

bool PluginHosting::isMissing (te::Plugin& p)
{
    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&p))
        return ext->isMissing();

    return false;
}

juce::String PluginHosting::describe (te::Plugin& p)
{
    juce::String s = p.getName();

    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&p))
        s << " (" << ext->desc.pluginFormatName << ")";

    if (isMissing (p))
        s << " - MISSING, passing audio through";
    else if (! p.isEnabled())
        s << " - bypassed";

    const auto latencyMs = p.getLatencySeconds() * 1000.0;

    if (latencyMs >= 0.05)
        s << " - latency " << juce::String (latencyMs, 1) << " ms (compensated)";

    return s;
}

juce::Array<te::Plugin*> PluginHosting::getUserInserts (te::PluginList& list)
{
    juce::Array<te::Plugin*> result;

    for (auto* p : list.getPlugins())
        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) == nullptr && dynamic_cast<te::LevelMeterPlugin*> (p) == nullptr)
            result.add (p);

    return result;
}

} // namespace opensession
