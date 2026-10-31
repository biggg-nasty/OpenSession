// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHelpers.h"
#include "../Engine/PluginHosting.h"
#include "../UI/PluginEditors.h"

namespace opensession::tests
{

static juce::File renderPluginMix (Session& s, const juce::String& name)
{
    ExportOptions o;
    o.destination = s.temp.dir.getChildFile (name + ".wav");
    o.sampleRate = kSampleRate;
    o.tail = te::TimeDuration::fromSeconds (0.25);
    auto r = Exporter::exportStereoMix (s.edit(), o);
    REQUIRE_MESSAGE (r.result.wasOk(), r.result.getErrorMessage().toStdString());
    return r.file;
}

static juce::PluginDescription makeAbsentVst3()
{
    juce::PluginDescription d;
    d.name = "Absent Test Plugin";
    d.pluginFormatName = "VST3";
    d.manufacturerName = "Nobody";
    d.fileOrIdentifier = "/nonexistent/OpenSession/AbsentTestPlugin.vst3";
    d.uniqueId = 0x4f53;
    d.deprecatedUid = 0x4f53;
    d.numInputChannels = 2;
    d.numOutputChannels = 2;
    return d;
}

TEST_SUITE ("opensession")
{
    TEST_CASE ("Plugins: this build hosts VST3 and Audio Unit formats")
    {
        auto formats = PluginHosting::getHostedFormatNames (getTestEngine());
        CHECK (formats.contains ("VST3"));
       #if JUCE_MAC
        CHECK (formats.contains ("AudioUnit"));
       #endif
        CHECK_FALSE (formats.contains ("AAX"));
        CHECK (PluginHosting::getUnsupportedFormatsNote().contains ("AAX"));
    }

    TEST_CASE ("Plugins: a missing third-party plugin keeps its slot and settings, passes audio through, and survives save/reopen")
    {
        Player player (1);
        TempDir parent ("plugins");
        juce::File folder;

        {
            Session s ("Plugins", parent.dir);
            auto file = writeSessionAudio (s, "src", makeSine (1.0, 440.0, 1, 0.5f));
            insertClip (s.track (0), file, 0.0, 1.0);
            const auto reference = readWav (renderPluginMix (s, "ref"));

            juce::String error;
            auto p = PluginHosting::insertExternalPlugin (s.edit(), s.track (0).pluginList, makeAbsentVst3(), error);
            REQUIRE (p != nullptr);
            CHECK (PluginHosting::isMissing (*p));
            CHECK (error.contains ("passes through"));
            CHECK (PluginHosting::describe (*p).contains ("MISSING"));

            // Inserted pre-fader: before the track's volume/pan plugin.
            auto plugins = s.track (0).pluginList.getPlugins();
            const int pluginIndex = plugins.indexOf (p.get());
            const int volumeIndex = plugins.indexOf (s.track (0).getVolumePlugin());
            CHECK_GE (pluginIndex, 0);
            CHECK_LT (pluginIndex, volumeIndex);
            CHECK_EQ (PluginHosting::getUserInserts (s.track (0).pluginList).size(), 1);

            const auto withMissing = readWav (renderPluginMix (s, "missing"));
            REQUIRE (withMissing.ok);
            CHECK_EQ (peakDb (withMissing.samples, 0), doctest::Approx (peakDb (reference.samples, 0)).epsilon (0.01));
            CHECK_LT (maxDiff (reference.samples, 0, withMissing.samples, 0), 1.0e-4f);

            REQUIRE (s.doc->save().wasOk());
            folder = s.doc->getFolder();
        }

        juce::String error;
        auto reopened = SessionDocument::open (getTestEngine(), folder, error);
        REQUIRE_MESSAGE (reopened != nullptr, error.toStdString());
        auto track = te::getAudioTracks (reopened->getEdit())[0];
        auto inserts = PluginHosting::getUserInserts (track->pluginList);
        REQUIRE_EQ (inserts.size(), 1);
        auto* ext = dynamic_cast<te::ExternalPlugin*> (inserts[0]);
        REQUIRE (ext != nullptr);
        CHECK_EQ (ext->desc.name, juce::String ("Absent Test Plugin"));
        CHECK_EQ (ext->desc.fileOrIdentifier, makeAbsentVst3().fileOrIdentifier);
        CHECK (ext->isMissing());
    }

    TEST_CASE ("Plugins: parametric EQ display matches the engine's filters")
    {
        using Band = ParametricEqEditor::BandType;
        // Peak band: full gain at its centre, nothing far away.
        CHECK_EQ (ParametricEqEditor::bandGainDb (Band::peak, kSampleRate, 1000.0, 1.0, 12.0, 1000.0), doctest::Approx (12.0).epsilon (0.01));
        CHECK_EQ (ParametricEqEditor::bandGainDb (Band::peak, kSampleRate, 1000.0, 1.0, 12.0, 20.0), doctest::Approx (0.0).epsilon (0.05));
        // Shelves: full gain deep in the shelf, flat on the other side.
        CHECK_EQ (ParametricEqEditor::bandGainDb (Band::lowShelf, kSampleRate, 200.0, 0.7, -9.0, 20.0), doctest::Approx (-9.0).epsilon (0.03));
        CHECK_LT (std::abs (ParametricEqEditor::bandGainDb (Band::lowShelf, kSampleRate, 200.0, 0.7, -9.0, 10000.0)), 0.1);
        CHECK_EQ (ParametricEqEditor::bandGainDb (Band::highShelf, kSampleRate, 5000.0, 0.7, 6.0, 20000.0), doctest::Approx (6.0).epsilon (0.03));
        CHECK_EQ (ParametricEqEditor::bandGainDb (Band::peak, kSampleRate, 1000.0, 1.0, 0.0, 1000.0), 0.0);

        // Against the real render: a +12 dB peak at 1 kHz on a 1 kHz sine.
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (1.0, 1000.0, 1, 0.1f));
        insertClip (s.track (0), file, 0.0, 1.0);
        const auto plain = peakDb (readWav (renderPluginMix (s, "plain")).samples, 0);

        auto eq = insertBuiltInPlugin<te::EqualiserPlugin> (s.track (0), 0);
        REQUIRE (eq != nullptr);
        eq->setMidFreq1 (1000.0f);
        eq->setMidQ1 (1.0f);
        eq->setMidGain1 (12.0f);
        const auto boosted = peakDb (readWav (renderPluginMix (s, "boost")).samples, 0);
        const auto predicted = ParametricEqEditor::bandGainDb (Band::peak, kSampleRate, 1000.0, 1.0, 12.0, 1000.0);
        CHECK_EQ (boosted - plain, doctest::Approx (predicted).epsilon (0.03));
    }
}

} // namespace opensession::tests
