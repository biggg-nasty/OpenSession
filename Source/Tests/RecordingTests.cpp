// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHelpers.h"

namespace opensession::tests
{

TEST_SUITE ("opensession")
{
    TEST_CASE ("Recording: known signal is captured to a valid 48 kHz / 24-bit mono WAV in the session's Audio folder")
    {
        Player player (1);
        Session s;
        s.assignAndArm (0, 0);

        auto input = makeSine (2.0, 440.0);
        auto clips = s.record (player, input);
        REQUIRE_EQ (clips.size(), 1);

        auto file = clips[0]->getSourceFileReference().getFile();
        CHECK (file.existsAsFile());
        CHECK (file.isAChildOf (s.doc->getAudioDirectory()));
        CHECK (file.hasFileExtension ("wav"));

        auto wav = readWav (file);
        REQUIRE (wav.ok);
        CHECK_EQ (wav.sampleRate, kSampleRate);
        CHECK_EQ (wav.numChannels, 1);
        CHECK_EQ (wav.bitsPerSample, 24);
        CHECK_EQ (wav.lengthInSamples, (int64_t) input.getNumSamples());
        CHECK (allFinite (wav.samples));
        CHECK_LT (maxDiff (input, 0, wav.samples, 0), 0.001f);         // bit-exact apart from 24-bit quantisation
        CHECK_EQ (findLag (input, wav.samples), 0);                      // sample-aligned with zero device latency

        // The clip sits where recording started and references the file relatively.
        CHECK_EQ (clips[0]->getPosition().getStart().inSeconds(), doctest::Approx (0.0).epsilon (0.001));
        CHECK_EQ (clips[0]->getPosition().getLength().inSeconds(), doctest::Approx (2.0).epsilon (0.01));
        CHECK_FALSE (juce::File::isAbsolutePath (clips[0]->getSourceFileReference().source.get()));

        // BWAV time reference = sample position of the recording start, so the file is self-describing.
        CHECK_EQ (wav.metadata[juce::WavAudioFormat::bwavTimeReference].getLargeIntValue(), (int64_t) 0);
    }

    TEST_CASE ("Recording: two simultaneous inputs land in independent files with channel isolation")
    {
        Player player (2);
        Session s;
        REQUIRE_GE (s.recording->getAvailableInputs().size(), (size_t) 2);
        s.assignAndArm (0, 0);
        s.assignAndArm (1, 1);
        CHECK_EQ (s.recording->getNumArmedTracks(), 2);

        auto sigA = makeSine (1.5, 440.0, 1, 0.5f);
        auto sigB = makeSine (1.5, 880.0, 1, 0.3f);
        juce::AudioBuffer<float> input (2, sigA.getNumSamples());
        input.copyFrom (0, 0, sigA, 0, 0, sigA.getNumSamples());
        input.copyFrom (1, 0, sigB, 0, 0, sigB.getNumSamples());

        auto clips = s.record (player, input);
        REQUIRE_EQ (clips.size(), 2);

        te::WaveAudioClip* clipA = nullptr;
        te::WaveAudioClip* clipB = nullptr;

        for (auto* c : clips)
        {
            if (c->getTrack() == &s.track (0)) clipA = c;
            if (c->getTrack() == &s.track (1)) clipB = c;
        }

        REQUIRE (clipA != nullptr);
        REQUIRE (clipB != nullptr);
        auto fileA = clipA->getSourceFileReference().getFile();
        auto fileB = clipB->getSourceFileReference().getFile();
        CHECK_NE (fileA, fileB);

        auto wavA = readWav (fileA);
        auto wavB = readWav (fileB);
        REQUIRE (wavA.ok);
        REQUIRE (wavB.ok);
        CHECK_EQ (wavA.numChannels, 1);
        CHECK_EQ (wavB.numChannels, 1);
        CHECK_EQ (wavA.lengthInSamples, (int64_t) sigA.getNumSamples());
        CHECK_EQ (wavB.lengthInSamples, (int64_t) sigB.getNumSamples());

        // Each file reproduces its own input...
        CHECK_LT (residualDb (sigA, 0, wavA.samples, 0), -80.0f);
        CHECK_LT (residualDb (sigB, 0, wavB.samples, 0), -80.0f);
        // ...and not the other one (the residual against the wrong signal is at signal level).
        CHECK_GT (residualDb (sigB, 0, wavA.samples, 0), -20.0f);
        CHECK_GT (residualDb (sigA, 0, wavB.samples, 0), -20.0f);
    }

    TEST_CASE ("Recording: existing-track playback, click and software monitoring never reach the dry recording")
    {
        Player player (1);
        Session s;

        // Track 1 plays back something loud; the click is on; the new track's input is monitored.
        auto loud = makeSquare (2.0, 220.0, 1, 0.9f);
        auto playbackFile = writeSessionAudio (s, "playback", loud);
        insertClip (s.track (0), playbackFile, 0.0, 2.0);
        s.edit().clickTrackEnabled = true;
        s.edit().clickTrackRecordingOnly = false;

        s.assignAndArm (1, 0);
        s.recording->setMonitorMode (s.track (1), te::InputDevice::MonitorMode::on);
        s.edit().dispatchPendingUpdatesSynchronously();
        pump();

        auto silence = makeSilence (2.0);
        auto clips = s.record (player, silence);
        REQUIRE_EQ (clips.size(), 1);
        CHECK (clips[0]->getTrack() == &s.track (1));

        auto wav = readWav (clips[0]->getSourceFileReference().getFile());
        REQUIRE (wav.ok);
        CHECK_LT (rmsDb (wav.samples, 0), -90.0f);                       // dry capture is silent
        CHECK_GT (rmsDb (s.lastOutput, 0), -30.0f);                      // ...while the output really had playback in it
    }

    TEST_CASE ("Recording: fader, pan and insert effects do not alter the dry recording")
    {
        Player player (1);
        Session s;
        s.assignAndArm (0, 0);

        auto& track = s.track (0);
        track.getVolumePlugin()->setVolumeDb (-20.0f);
        track.getVolumePlugin()->setPan (-1.0f);
        auto eq = insertBuiltInPlugin<te::EqualiserPlugin> (track, 0);
        REQUIRE (eq != nullptr);
        eq->setLowGain (12.0f);
        s.recording->setMonitorMode (track, te::InputDevice::MonitorMode::on);
        s.edit().dispatchPendingUpdatesSynchronously();
        pump();

        auto input = makeSine (1.0, 110.0, 1, 0.4f);
        auto clips = s.record (player, input);
        REQUIRE_EQ (clips.size(), 1);
        auto wav = readWav (clips[0]->getSourceFileReference().getFile());
        REQUIRE (wav.ok);
        CHECK_EQ (wav.lengthInSamples, (int64_t) input.getNumSamples());
        CHECK_LT (residualDb (input, 0, wav.samples, 0), -80.0f);
    }

    TEST_CASE ("Recording: repeated passes over the same range become takes - files kept, newest active, exactly one plays")
    {
        Player player (1);
        Session s;
        s.assignAndArm (0, 0);

        juce::Array<juce::File> files;

        for (int pass = 0; pass < 4; ++pass)
        {
            s.edit().getTransport().setPosition ({});
            auto input = makeSine (0.5, 200.0 + 100.0 * pass);
            auto clips = s.record (player, input);
            REQUIRE_EQ (clips.size(), 1);
            files.add (clips[0]->getSourceFileReference().getFile());
        }

        for (auto& f : files)
            CHECK (f.existsAsFile());

        CHECK_EQ (juce::StringArray (files[0].getFileName(), files[1].getFileName(), files[2].getFileName(), files[3].getFileName()).size(), 4);

        auto clips = s.track (0).getClips();
        REQUIRE_EQ (clips.size(), 4);
        int unmuted = 0;
        te::Clip* active = nullptr;

        for (auto* c : clips)
        {
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (c); ac != nullptr && ! ac->isMuted())
            {
                ++unmuted;
                active = c;
            }
        }

        CHECK_EQ (unmuted, 1);
        REQUIRE (active != nullptr);
        CHECK_EQ (active->getSourceFileReference().getFile(), files[3]);   // newest take plays
        CHECK_EQ (RecordingController::getTakeGroupSize (*active), 4);

        // Audition an older take: it becomes the only audible one.
        RecordingController::activateTake (*clips[1]);
        unmuted = 0;
        for (auto* c : clips)
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (c); ac != nullptr && ! ac->isMuted())
                ++unmuted;
        CHECK_EQ (unmuted, 1);
        CHECK_FALSE (dynamic_cast<te::AudioClipBase*> (clips[1])->isMuted());
    }

    TEST_CASE ("Recording: a stereo input pair records a two-channel file with correct channel order")
    {
        Player player (2);
        Session s;
        auto inputs = s.recording->getAvailableInputs();
        REQUIRE_GE (inputs.size(), (size_t) 2);
        DeviceSetup::setInputChannelCount (getTestEngine(), *inputs[0], 2);
        pump();
        s.edit().getTransport().ensureContextAllocated (true);
        s.edit().dispatchPendingUpdatesSynchronously();
        pump();

        inputs = s.recording->getAvailableInputs();
        REQUIRE_GE (inputs.size(), (size_t) 1);
        CHECK_EQ (inputs[0]->getChannels().getNumChannels(), 2);
        s.assignAndArm (0, 0);

        auto left = makeSine (1.0, 440.0, 1, 0.5f);
        auto right = makeSine (1.0, 880.0, 1, 0.25f);
        juce::AudioBuffer<float> input (2, left.getNumSamples());
        input.copyFrom (0, 0, left, 0, 0, left.getNumSamples());
        input.copyFrom (1, 0, right, 0, 0, right.getNumSamples());

        auto clips = s.record (player, input);
        REQUIRE_EQ (clips.size(), 1);
        auto wav = readWav (clips[0]->getSourceFileReference().getFile());
        REQUIRE (wav.ok);
        CHECK_EQ (wav.numChannels, 2);
        CHECK_LT (residualDb (left, 0, wav.samples, 0), -80.0f);
        CHECK_LT (residualDb (right, 0, wav.samples, 1), -80.0f);
    }

    TEST_CASE ("Recording: arming needs an input, and recording with nothing armed is refused with a clear message")
    {
        Player player (1);
        Session s;
        auto r = s.recording->setArmed (s.track (0), true);
        CHECK (r.failed());
        CHECK (r.getErrorMessage().containsIgnoreCase ("input"));
        r = s.recording->startRecording();
        CHECK (r.failed());
        CHECK (r.getErrorMessage().containsIgnoreCase ("armed"));
        CHECK_FALSE (s.edit().getTransport().isRecording());
    }

    TEST_CASE ("Recording: monitoring mode is explicit and independent of arming")
    {
        Player player (1);
        Session s;
        auto& t = s.track (0);
        REQUIRE (s.recording->assignInput (t, s.input (0)).wasOk());
        s.edit().dispatchPendingUpdatesSynchronously();

        CHECK_EQ (s.recording->getMonitorMode (t), te::InputDevice::MonitorMode::off);   // default: off
        CHECK_FALSE (s.recording->isInputAudible (t));

        s.recording->setMonitorMode (t, te::InputDevice::MonitorMode::automatic);
        s.edit().dispatchPendingUpdatesSynchronously();
        CHECK_FALSE (s.recording->isInputAudible (t));                                   // auto + not armed = silent
        REQUIRE (s.recording->setArmed (t, true).wasOk());
        s.edit().dispatchPendingUpdatesSynchronously();
        CHECK (s.recording->isInputAudible (t));                                         // auto + armed = audible

        s.recording->setMonitorMode (t, te::InputDevice::MonitorMode::on);
        REQUIRE (s.recording->setArmed (t, false).wasOk());
        s.edit().dispatchPendingUpdatesSynchronously();
        CHECK (s.recording->isInputAudible (t));                                         // on = audible even unarmed
        CHECK_FALSE (s.recording->isArmed (t));
    }

    TEST_CASE ("Recording: an unwritable Audio folder produces a reported failure and no clip")
    {
        if (juce::SystemStats::getEnvironmentVariable ("USER", "") == "root")
            return;   // root ignores permissions

        struct Sink : public MessageSink
        {
            void postMessage (const juce::String& t, const juce::String& b, bool err) override { messages.add (t + ": " + b); errors += err ? 1 : 0; }
            juce::StringArray messages;
            int errors = 0;
        } sink;

        Player player (1);
        Session s;
        getTestEngineOwner().setMessageSink (&sink);
        s.assignAndArm (0, 0);

        auto audio = s.doc->getAudioDirectory();
        REQUIRE (audio.setReadOnly (true, false));

        auto input = makeSine (0.5, 440.0);
        auto r = s.recording->startRecording();
        pump (50);
        player.player.process (input);
        s.edit().getTransport().stop (false, true);
        pump (50);

        audio.setReadOnly (false, false);
        getTestEngineOwner().setMessageSink (nullptr);

        CHECK_EQ (s.doc->getAllWaveClips().size(), 0);
        CHECK ((sink.errors > 0 || r.failed()));
        CHECK ((sink.messages.joinIntoString ("\n").containsIgnoreCase ("record") || r.failed()));
    }
}

} // namespace opensession::tests
