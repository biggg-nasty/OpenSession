// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHelpers.h"

namespace opensession::tests
{

static ExportResult renderMix (Session& s, const juce::String& name, double tailSeconds = 0.5)
{
    ExportOptions o;
    o.destination = s.temp.dir.getChildFile (name + ".wav");
    o.sampleRate = kSampleRate;
    o.bitDepth = 24;
    o.tail = te::TimeDuration::fromSeconds (tailSeconds);
    auto r = Exporter::exportStereoMix (s.edit(), o);
    REQUIRE_MESSAGE (r.result.wasOk(), r.result.getErrorMessage().toStdString());
    return r;
}

TEST_SUITE ("opensession")
{
    TEST_CASE ("Mix/export: stereo mix is 48 kHz / 24-bit / 2 ch with range = last clip end + tail, no normalisation")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (2.0, 440.0, 1, 0.5f));
        insertClip (s.track (0), file, 0.0, 2.0);

        auto r = renderMix (s, "mix", 1.0);
        auto wav = readWav (r.file);
        REQUIRE (wav.ok);
        CHECK_EQ (wav.sampleRate, kSampleRate);
        CHECK_EQ (wav.bitsPerSample, 24);
        CHECK_EQ (wav.numChannels, 2);
        CHECK_EQ (wav.lengthInSamples, doctest::Approx (3.0 * kSampleRate).epsilon (0.001));
        CHECK_FALSE (r.clipped);
        // A 0.5 amplitude mono source, centre panned, 0 dB track and master: both channels carry it at -6 dBFS (pan law 0 dB centre)
        CHECK_EQ (peakDb (wav.samples, 0), doctest::Approx (-6.0f).epsilon (0.15));
        CHECK_EQ (peakDb (wav.samples, 1), doctest::Approx (-6.0f).epsilon (0.15));
        CHECK (allFinite (wav.samples));
    }

    TEST_CASE ("Mix/export: track gain, mute, solo and pan are applied to the exported signal")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (1.0, 440.0, 1, 0.5f));
        insertClip (s.track (0), file, 0.0, 1.0);
        insertClip (s.track (1), file, 0.0, 1.0);
        s.track (1).setMute (true);

        const auto reference = peakDb (readWav (renderMix (s, "ref").file).samples, 0);

        s.track (0).getVolumePlugin()->setVolumeDb (-6.0f);
        auto gained = readWav (renderMix (s, "gain").file);
        CHECK_EQ (peakDb (gained.samples, 0), doctest::Approx (reference - 6.0f).epsilon (0.05));

        s.track (0).getVolumePlugin()->setVolumeDb (0.0f);
        s.track (0).getVolumePlugin()->setPan (-1.0f);
        auto panned = readWav (renderMix (s, "pan").file);
        CHECK_LT (peakDb (panned.samples, 1), -80.0f);                      // right channel silent
        CHECK_GT (peakDb (panned.samples, 0), reference - 0.5f);           // left at least as loud as centre

        s.track (0).getVolumePlugin()->setPan (0.0f);
        s.track (0).setMute (true);
        auto muted = readWav (renderMix (s, "mute").file);
        CHECK_LT (peakDb (muted.samples, 0), -80.0f);
        s.track (0).setMute (false);

        s.track (1).setMute (false);
        s.track (1).setSolo (true);                                         // solo track 2 -> track 1 silent, but track 2 heard
        auto soloed = readWav (renderMix (s, "solo").file);
        CHECK_GT (peakDb (soloed.samples, 0), -20.0f);
        s.track (1).setSolo (false);
        s.track (1).setMute (true);

        // Clip gain acts on the clip only
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (s.track (0).getClips()[0]))
            ac->setGainDB (-12.0f);
        auto clipGained = readWav (renderMix (s, "clipgain").file);
        CHECK_EQ (peakDb (clipGained.samples, 0), doctest::Approx (reference - 12.0f).epsilon (0.05));
    }

    TEST_CASE ("Mix/export: built-in EQ changes the signal when enabled and is transparent when bypassed")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (1.0, 100.0, 1, 0.25f));
        insertClip (s.track (0), file, 0.0, 1.0);
        auto plain = readWav (renderMix (s, "plain").file);

        auto eq = insertBuiltInPlugin<te::EqualiserPlugin> (s.track (0), 0);
        REQUIRE (eq != nullptr);
        eq->setLowFreq (100.0f);
        eq->setLowGain (12.0f);
        auto boosted = readWav (renderMix (s, "eq").file);
        CHECK_GT (peakDb (boosted.samples, 0), peakDb (plain.samples, 0) + 6.0f);

        eq->setEnabled (false);
        auto bypassed = readWav (renderMix (s, "bypass").file);
        CHECK_EQ (peakDb (bypassed.samples, 0), doctest::Approx (peakDb (plain.samples, 0)).epsilon (0.02));
    }

    TEST_CASE ("Mix/export: stems share one timeline start and length, keep leading silence, and sum to the mix")
    {
        Player player (1);
        Session s;
        auto fileA = writeSessionAudio (s, "a", makeSine (2.0, 440.0, 1, 0.3f));
        auto fileB = writeSessionAudio (s, "b", makeSine (2.0, 660.0, 1, 0.3f));
        insertClip (s.track (0), fileA, 0.0, 2.0);
        insertClip (s.track (1), fileB, 3.0, 2.0);
        s.track (0).setName ("Guitar");
        s.track (1).setName ("Vocal");

        ExportOptions o;
        o.destination = s.temp.dir.getChildFile ("stems");
        o.sampleRate = kSampleRate;
        o.tail = te::TimeDuration::fromSeconds (1.0);
        auto stems = Exporter::exportStems (s.edit(), o);
        REQUIRE_EQ (stems.size(), (size_t) 2);

        for (auto& st : stems)
        {
            REQUIRE_MESSAGE (st.result.wasOk(), st.result.getErrorMessage().toStdString());
            CHECK (st.file.existsAsFile());
            CHECK_EQ (st.lengthInSamples, doctest::Approx (6.0 * kSampleRate).epsilon (0.001));   // 5 s content + 1 s tail
            CHECK_EQ (st.numChannels, 2);
        }

        CHECK (stems[0].file.getFileName().startsWith ("01_Guitar"));
        CHECK (stems[1].file.getFileName().startsWith ("02_Vocal"));

        auto vocal = readWav (stems[1].file);
        CHECK_LT (rmsDb (vocal.samples, 0, 0, (int) (2.9 * kSampleRate)), -90.0f);          // leading silence kept
        CHECK_GT (rmsDb (vocal.samples, 0, (int) (3.1 * kSampleRate), (int) (1.5 * kSampleRate)), -20.0f);

        // Live/export routing consistency: with a neutral master, the stems sum to the mix.
        auto mix = readWav (renderMix (s, "mix", 1.0).file);
        auto guitar = readWav (stems[0].file);
        REQUIRE_EQ (mix.lengthInSamples, guitar.lengthInSamples);
        juce::AudioBuffer<float> sum (2, (int) mix.lengthInSamples);
        sum.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            sum.addFrom (ch, 0, guitar.samples, ch, 0, sum.getNumSamples());
            sum.addFrom (ch, 0, vocal.samples, ch, 0, sum.getNumSamples());
        }
        CHECK_LT (residualDb (sum, 0, mix.samples, 0), -70.0f);
        CHECK_LT (residualDb (sum, 1, mix.samples, 1), -70.0f);
    }

    TEST_CASE ("Mix/export: clipping is measured and reported, never hidden by normalisation")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "hot", makeSine (1.0, 440.0, 1, 0.9f));
        auto clip = insertClip (s.track (0), file, 0.0, 1.0);
        clip->setGainDB (12.0f);
        auto r = renderMix (s, "clipped");
        CHECK (r.clipped);
        CHECK_GE (r.peakLinear, 0.999f);

        clip->setGainDB (-12.0f);
        auto r2 = renderMix (s, "quiet");
        CHECK_FALSE (r2.clipped);
        CHECK_LT (r2.peakLinear, 0.5f);
    }

    TEST_CASE ("Mix/export: a muted track still produces a (silent) stem so stem sets stay complete")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "a", makeSine (1.0, 440.0, 1, 0.3f));
        insertClip (s.track (0), file, 0.0, 1.0);
        insertClip (s.track (1), file, 0.0, 1.0);
        s.track (1).setMute (true);

        ExportOptions o;
        o.destination = s.temp.dir.getChildFile ("stems2");
        o.sampleRate = kSampleRate;
        auto stems = Exporter::exportStems (s.edit(), o);
        REQUIRE_EQ (stems.size(), (size_t) 2);
        CHECK (stems[1].result.wasOk());
        CHECK_LT (peakDb (readWav (stems[1].file).samples, 0), -80.0f);
        CHECK_GT (peakDb (readWav (stems[0].file).samples, 0), -20.0f);
    }
}

} // namespace opensession::tests
