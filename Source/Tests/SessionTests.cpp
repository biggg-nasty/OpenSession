// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHelpers.h"
#include "../Session/WavRepair.h"

namespace opensession::tests
{

static double secs (te::TimePosition t)  { return t.inSeconds(); }
static double secs (te::TimeDuration t)  { return t.inSeconds(); }

TEST_SUITE ("opensession")
{
    TEST_CASE ("Session: save writes session.xml + manifest + relative audio refs, and reopening restores everything")
    {
        Player player (1);
        TempDir elsewhere ("moved");
        juce::File folder;

        {
            Session s ("Song");
            s.assignAndArm (0, 0);
            auto input = makeSine (1.0, 440.0);
            auto clips = s.record (player, input);
            REQUIRE_EQ (clips.size(), 1);

            auto& t0 = s.track (0);
            auto& t1 = s.track (1);
            t0.setName ("Guitar DI");
            t1.setName ("Vocal");
            t0.getVolumePlugin()->setVolumeDb (-4.5f);
            t0.getVolumePlugin()->setPan (0.25f);
            t1.setMute (true);
            s.edit().tempoSequence.getTempo (0)->setBpm (97.0);
            clips[0]->setStart (te::TimePosition::fromSeconds (1.25), false, true);
            clips[0]->setFadeIn (te::TimeDuration::fromSeconds (0.1));
            clips[0]->setGainDB (-3.0f);
            insertBuiltInPlugin<te::CompressorPlugin> (t0, 0);

            auto r = s.doc->save();
            REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
            CHECK_FALSE (s.doc->hasUnsavedChanges());
            folder = s.doc->getFolder();

            CHECK (s.doc->getSessionFile().existsAsFile());
            CHECK (s.doc->getManifestFile().existsAsFile());
            auto manifest = juce::JSON::parse (s.doc->getManifestFile());
            CHECK_EQ ((int) manifest["schemaVersion"], SessionDocument::schemaVersion);
            CHECK_EQ (manifest["name"].toString(), juce::String ("Song"));

            const auto source = clips[0]->getSourceFileReference().source.get();
            CHECK_FALSE (juce::File::isAbsolutePath (source));
            CHECK (source.startsWith ("Audio"));

            // Move the whole folder somewhere else before reopening: nothing may depend on the old path.
            REQUIRE (s.doc->getFolder().moveFileTo (elsewhere.dir.getChildFile ("Song.osession")));
        }

        auto moved = elsewhere.dir.getChildFile ("Song.osession");
        juce::String error;
        auto doc = SessionDocument::open (getTestEngine(), moved, error);
        REQUIRE_MESSAGE (doc != nullptr, error.toStdString());
        CHECK (error.isEmpty());

        auto tracks = te::getAudioTracks (doc->getEdit());
        REQUIRE_EQ (tracks.size(), 2);
        CHECK_EQ (tracks[0]->getName(), juce::String ("Guitar DI"));
        CHECK_EQ (tracks[1]->getName(), juce::String ("Vocal"));
        CHECK_EQ (tracks[0]->getVolumePlugin()->getVolumeDb(), doctest::Approx (-4.5f).epsilon (0.01));
        CHECK_EQ (tracks[0]->getVolumePlugin()->getPan(), doctest::Approx (0.25f).epsilon (0.01));
        CHECK (tracks[1]->isMuted (false));
        CHECK (tracks[0]->pluginList.findFirstPluginOfType<te::CompressorPlugin>() != nullptr);
        CHECK_EQ (doc->getEdit().tempoSequence.getTempo (0)->getBpm(), doctest::Approx (97.0));

        auto clips = doc->getAllWaveClips();
        REQUIRE_EQ (clips.size(), 1);
        CHECK_EQ (secs (clips[0]->getPosition().getStart()), doctest::Approx (1.25));
        CHECK_EQ (secs (clips[0]->getPosition().getLength()), doctest::Approx (1.0).epsilon (0.01));
        CHECK_EQ (secs (clips[0]->getFadeIn()), doctest::Approx (0.1));
        CHECK_EQ (clips[0]->getGainDB(), doctest::Approx (-3.0f));
        CHECK (clips[0]->getSourceFileReference().getFile().existsAsFile());
        CHECK (clips[0]->getSourceFileReference().getFile().isAChildOf (moved));
        CHECK (doc->findMissingFiles().isEmpty());
        CHECK_EQ (doc->getName(), juce::String ("Song"));
    }

    TEST_CASE ("Session: export tail and click-while-recording-only are stored in the session and survive reopening")
    {
        Player player (1);
        TempDir parent ("tail");
        juce::File folder;

        {
            Session s ("Tail", parent.dir);
            CHECK_EQ (s.doc->getExportTailSeconds(), doctest::Approx (SessionDocument::defaultExportTailSeconds));
            CHECK_FALSE (s.edit().clickTrackRecordingOnly.get());

            s.doc->setExportTailSeconds (5.0);
            s.edit().clickTrackRecordingOnly = true;
            CHECK (s.doc->hasUnsavedChanges());
            REQUIRE (s.doc->save().wasOk());
            folder = s.doc->getFolder();

            s.doc->setExportTailSeconds (-1.0);
            CHECK_EQ (s.doc->getExportTailSeconds(), doctest::Approx (0.0));
            s.doc->setExportTailSeconds (1000.0);
            CHECK_EQ (s.doc->getExportTailSeconds(), doctest::Approx (SessionDocument::maxExportTailSeconds));
        }

        juce::String error;
        auto reopened = SessionDocument::open (getTestEngine(), folder, error);
        REQUIRE_MESSAGE (reopened != nullptr, error.toStdString());
        CHECK_EQ (reopened->getExportTailSeconds(), doctest::Approx (5.0));
        CHECK (reopened->getEdit().clickTrackRecordingOnly.get());
    }

    TEST_CASE ("Session: saves are atomic and keep the previous snapshot")
    {
        Player player (1);
        Session s;
        REQUIRE (s.doc->save().wasOk());
        const auto first = s.doc->getSessionFile().loadFileAsString();
        s.track (0).setName ("Renamed");
        REQUIRE (s.doc->save().wasOk());
        CHECK (s.doc->getPreviousSnapshotFile().existsAsFile());
        CHECK_EQ (s.doc->getPreviousSnapshotFile().loadFileAsString(), first);
        CHECK (s.doc->getSessionFile().loadFileAsString().contains ("Renamed"));
        CHECK_FALSE (s.doc->getFolder().getChildFile ("session.xml.tmp").exists());
    }

    TEST_CASE ("Session: a corrupt session.xml is reported, and the previous snapshot is used when available")
    {
        Player player (1);
        TempDir outer ("corrupt");
        juce::File folder;
        {
            Session s ("Test", outer.dir);
            s.track (0).setName ("Good");
            REQUIRE (s.doc->save().wasOk());
            s.track (0).setName ("Later");
            REQUIRE (s.doc->save().wasOk());
            folder = s.doc->getFolder();
        }

        // Case 1: corrupt main file, good previous snapshot -> loads snapshot with a warning
        REQUIRE (folder.getChildFile ("session.xml").replaceWithText ("<EDIT this is not xml"));
        juce::String error;
        auto doc = SessionDocument::open (getTestEngine(), folder, error);
        REQUIRE (doc != nullptr);
        CHECK (error.containsIgnoreCase ("previous snapshot"));
        CHECK_EQ (te::getAudioTracks (doc->getEdit())[0]->getName(), juce::String ("Good"));
        doc.reset();

        // Case 2: no snapshot either -> clear failure, nothing crashes
        folder.getChildFile ("Backups").getChildFile ("session.previous.xml").deleteFile();
        error.clear();
        doc = SessionDocument::open (getTestEngine(), folder, error);
        CHECK (doc == nullptr);
        CHECK (error.containsIgnoreCase ("corrupt"));

        // Case 3: wrong kind of XML
        REQUIRE (folder.getChildFile ("session.xml").replaceWithText ("<NOTANEDIT/>"));
        error.clear();
        doc = SessionDocument::open (getTestEngine(), folder, error);
        CHECK (doc == nullptr);
        CHECK (error.isNotEmpty());

        // Case 4: newer schema
        REQUIRE (folder.getChildFile ("session.xml").replaceWithText ("<EDIT appVersion=\"x\"><OPENSESSION schemaVersion=\"99\"/></EDIT>"));
        error.clear();
        doc = SessionDocument::open (getTestEngine(), folder, error);
        CHECK (doc == nullptr);
        CHECK (error.containsIgnoreCase ("newer"));
    }

    TEST_CASE ("Session: a failed metadata save leaves the previous session.xml and all audio intact")
    {
        if (juce::SystemStats::getEnvironmentVariable ("USER", "") == "root")
            return;

        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "take", makeSine (0.5, 440.0));
        insertClip (s.track (0), file, 0.0, 0.5);
        REQUIRE (s.doc->save().wasOk());
        const auto before = s.doc->getSessionFile().loadFileAsString();

        REQUIRE (s.doc->getFolder().setReadOnly (true, false));
        s.track (0).setName ("Unsaved change");
        auto r = s.doc->save();
        s.doc->getFolder().setReadOnly (false, false);

        CHECK (r.failed());
        CHECK (r.getErrorMessage().isNotEmpty());
        CHECK_EQ (s.doc->getSessionFile().loadFileAsString(), before);
        CHECK (file.existsAsFile());
        CHECK_EQ (readWav (file).lengthInSamples, (int64_t) (0.5 * kSampleRate));
        CHECK (s.doc->hasUnsavedChanges());
    }

    TEST_CASE ("Session: autosave is written separately and can be chosen on reopen")
    {
        Player player (1);
        TempDir outer ("autosave");
        juce::File folder;
        {
            Session s ("Test", outer.dir);
            s.track (0).setName ("Saved name");
            REQUIRE (s.doc->save().wasOk());
            s.track (0).setName ("Autosaved name");
            REQUIRE (s.doc->saveAutosave().wasOk());
            CHECK (s.doc->getAutosaveFile().existsAsFile());
            CHECK (s.doc->getSessionFile().loadFileAsString().contains ("Saved name"));
            folder = s.doc->getFolder();
        }

        juce::String error;
        auto normal = SessionDocument::open (getTestEngine(), folder, error, false);
        REQUIRE (normal != nullptr);
        CHECK_EQ (te::getAudioTracks (normal->getEdit())[0]->getName(), juce::String ("Saved name"));
        normal.reset();

        auto recovered = SessionDocument::open (getTestEngine(), folder, error, true);
        REQUIRE (recovered != nullptr);
        CHECK (recovered->wasLoadedFromAutosave());
        CHECK_EQ (te::getAudioTracks (recovered->getEdit())[0]->getName(), juce::String ("Autosaved name"));
    }

    TEST_CASE ("Session: collect-and-save copies external audio into the new session and reopens from another place")
    {
        Player player (1);
        TempDir external ("external");
        TempDir target ("target");
        auto externalFile = external.dir.getChildFile ("loop.wav");
        REQUIRE (writeWav (externalFile, makeSine (1.0, 330.0)));

        juce::File newFolder = target.dir.getChildFile ("Collected.osession");
        {
            Session s;
            auto inSession = writeSessionAudio (s, "inside", makeSine (0.5, 440.0));
            insertClip (s.track (0), inSession, 0.0, 0.5);

            // Reference a file outside the session (the raw engine API, as an import without copying would).
            auto ext = insertClip (s.track (1), externalFile, 1.0, 1.0);
            ext->getSourceFileReference().setToFile (externalFile, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
            CHECK (juce::File::isAbsolutePath (ext->getSourceFileReference().source.get()));

            auto r = s.doc->saveAsCollected (newFolder);
            REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
            CHECK_EQ (s.doc->getFolder(), newFolder);
            CHECK (newFolder.getChildFile ("Audio").getChildFile ("loop.wav").existsAsFile());
            CHECK (newFolder.getChildFile ("Audio").getChildFile ("inside.wav").existsAsFile());

            for (auto* wc : s.doc->getAllWaveClips())
                CHECK_FALSE (juce::File::isAbsolutePath (wc->getSourceFileReference().source.get()));
        }

        externalFile.deleteFile();   // the original is gone; the collected copy must stand alone
        external.dir.deleteRecursively();

        juce::String error;
        auto doc = SessionDocument::open (getTestEngine(), newFolder, error);
        REQUIRE_MESSAGE (doc != nullptr, error.toStdString());

        for (auto& m : doc->findMissingFiles())
            MESSAGE ("missing after collect: ", m.getFullPathName().toStdString());

        for (auto* wc : doc->getAllWaveClips())
            MESSAGE ("clip source: ", wc->getSourceFileReference().source.get().toStdString());

        CHECK (doc->findMissingFiles().isEmpty());
        CHECK_EQ (doc->getAllWaveClips().size(), 2);
    }

    TEST_CASE ("Session: importing an audio file copies it into Audio/ and places a clip of the file's length")
    {
        Player player (1);
        TempDir external ("import");
        auto src = external.dir.getChildFile ("sample.wav");
        REQUIRE (writeWav (src, makeSine (1.5, 300.0, 2)));

        Session s;
        auto r = s.doc->importAudioFile (src, s.track (0), te::TimePosition::fromSeconds (2.0));
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        auto clips = s.doc->getAllWaveClips();
        REQUIRE_EQ (clips.size(), 1);
        CHECK (clips[0]->getSourceFileReference().getFile().isAChildOf (s.doc->getAudioDirectory()));
        CHECK_EQ (secs (clips[0]->getPosition().getStart()), doctest::Approx (2.0));
        CHECK_EQ (secs (clips[0]->getPosition().getLength()), doctest::Approx (1.5).epsilon (0.001));

        auto bad = s.doc->importAudioFile (external.dir.getChildFile ("missing.wav"), s.track (0), {});
        CHECK (bad.failed());
    }

    TEST_CASE ("Session: missing audio is detected and the session still opens")
    {
        Player player (1);
        TempDir outer ("missing");
        juce::File folder, wavFile;
        {
            Session s ("Test", outer.dir);
            wavFile = writeSessionAudio (s, "gone", makeSine (0.5, 440.0));
            insertClip (s.track (0), wavFile, 0.0, 0.5);
            REQUIRE (s.doc->save().wasOk());
            folder = s.doc->getFolder();
        }

        REQUIRE (wavFile.deleteFile());
        juce::String error;
        auto doc = SessionDocument::open (getTestEngine(), folder, error);
        REQUIRE (doc != nullptr);
        auto missing = doc->findMissingFiles();
        REQUIRE_EQ (missing.size(), 1);
        CHECK_EQ (missing[0].getFileName(), juce::String ("gone.wav"));
        CHECK_EQ (doc->getAllWaveClips().size(), 1);
    }

    TEST_CASE ("Session: an orphaned recording (e.g. after a crash) is found and recovered at its BWAV time stamp")
    {
        Player player (1);
        Session s;

        juce::StringPairArray meta;
        const int64_t startSample = (int64_t) (1.5 * kSampleRate);
        meta.addArray (juce::WavAudioFormat::createBWAVMetadata ("", "opensession-test", "", juce::Time::getCurrentTime(), startSample, ""));
        auto orphan = writeSessionAudio (s, "Guitar_crashed_take", makeSine (0.75, 440.0), meta);

        auto orphans = s.doc->findOrphanRecordings();
        REQUIRE_EQ (orphans.size(), 1);
        CHECK_EQ (orphans[0], orphan);

        const int tracksBefore = te::getAudioTracks (s.edit()).size();
        auto r = s.doc->recoverOrphanRecording (orphan);
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        auto tracks = te::getAudioTracks (s.edit());
        REQUIRE_EQ (tracks.size(), tracksBefore + 1);
        auto clips = tracks.getLast()->getClips();
        REQUIRE_EQ (clips.size(), 1);
        CHECK_EQ (secs (clips[0]->getPosition().getStart()), doctest::Approx (1.5));
        CHECK_EQ (secs (clips[0]->getPosition().getLength()), doctest::Approx (0.75).epsilon (0.001));
        CHECK (tracks.getLast()->getName().contains ("recovered"));
        CHECK (s.doc->findOrphanRecordings().isEmpty());
    }

    TEST_CASE ("Session: a WAV whose header was never finalised (crash mid-take) is repaired and recovered")
    {
        Player player (1);
        Session s;
        auto good = makeSine (1.25, 440.0);
        auto f = writeSessionAudio (s, "Vocal_crashed", good);

        // Simulate an interrupted writer: zero the RIFF and data sizes as JUCE's writer leaves them until close.
        {
            auto report = WavRepair::inspect (f);
            REQUIRE (report.isWav);
            REQUIRE_FALSE (report.needsRepair);
            juce::FileOutputStream out (f);
            REQUIRE (out.openedOk());
            out.setPosition (4);                      out.writeInt (0);
            out.setPosition (report.dataChunkOffset - 4); out.writeInt (0);
            out.flush();
        }

        auto broken = WavRepair::inspect (f);
        CHECK (broken.needsRepair);
        CHECK_EQ (broken.recoverableFrames, (int64_t) good.getNumSamples());
        CHECK_EQ (readWav (f).lengthInSamples, (int64_t) 0);          // unreadable as-is

        REQUIRE_EQ (s.doc->findOrphanRecordings().size(), 1);
        auto r = s.doc->recoverOrphanRecording (f);
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        auto fixed = readWav (f);
        CHECK_EQ (fixed.lengthInSamples, (int64_t) good.getNumSamples());
        CHECK_LT (residualDb (good, 0, fixed.samples, 0), -80.0f);     // the audio itself is intact
        auto clips = te::getAudioTracks (s.edit()).getLast()->getClips();
        REQUIRE_EQ (clips.size(), 1);
        CHECK_EQ (secs (clips[0]->getPosition().getLength()), doctest::Approx (1.25).epsilon (0.001));
    }

    TEST_CASE ("Recording: aborting a take mid-way keeps a repaired copy under Audio/Incomplete")
    {
        Player player (1);
        Session s;
        s.assignAndArm (0, 0);

        auto r = s.recording->startRecording();
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        pump();
        auto input = makeSine (1.0, 440.0);
        player.player.process (input);

        juce::String reported;
        s.recording->onError = [&reported] (const juce::String& e) { reported = e; };
        s.recording->abortRecording();                                 // engine deletes the take's file
        pump (50);

        CHECK_EQ (s.doc->getAllWaveClips().size(), 0);
        auto incomplete = s.doc->getAudioDirectory().getChildFile ("Incomplete").findChildFiles (juce::File::findFiles, false, "*_incomplete.wav");
        // An explicit abort is the user's choice: nothing is kept and nothing is reported.
        CHECK_EQ (incomplete.size(), 0);
        CHECK (reported.isEmpty());
    }

    TEST_CASE ("Session: deleting a clip from the timeline never deletes its recording")
    {
        Player player (1);
        Session s;
        s.assignAndArm (0, 0);
        auto input = makeSine (0.5, 440.0);
        auto clips = s.record (player, input);
        REQUIRE_EQ (clips.size(), 1);
        auto file = clips[0]->getSourceFileReference().getFile();
        clips[0]->removeFromParent();
        CHECK_EQ (s.doc->getAllWaveClips().size(), 0);
        CHECK (file.existsAsFile());
        CHECK_EQ (s.doc->findOrphanRecordings().size(), 1);   // and it is now recoverable
    }
}

} // namespace opensession::tests
