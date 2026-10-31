// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHelpers.h"
#include "../UI/SessionContext.h"

namespace opensession::tests
{

static double secs (te::TimePosition t)  { return t.inSeconds(); }
static double secs (te::TimeDuration t)  { return t.inSeconds(); }

TEST_SUITE ("opensession")
{
    TEST_CASE ("Editing: split keeps both halves pointing at the same source with correct offsets")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (4.0, 440.0));
        auto clip = insertClip (s.track (0), file, 0.0, 4.0);

        auto* right = s.track (0).splitClip (*clip, te::TimePosition::fromSeconds (1.5));
        REQUIRE (right != nullptr);
        auto clips = s.track (0).getClips();
        REQUIRE_EQ (clips.size(), 2);

        auto* left = clips[0] == right ? clips[1] : clips[0];
        CHECK_EQ (secs (left->getPosition().getStart()), doctest::Approx (0.0));
        CHECK_EQ (secs (left->getPosition().getEnd()), doctest::Approx (1.5));
        CHECK_EQ (secs (left->getPosition().getOffset()), doctest::Approx (0.0));
        CHECK_EQ (secs (right->getPosition().getStart()), doctest::Approx (1.5));
        CHECK_EQ (secs (right->getPosition().getEnd()), doctest::Approx (4.0));
        CHECK_EQ (secs (right->getPosition().getOffset()), doctest::Approx (1.5));
        CHECK_EQ (left->getSourceFileReference().getFile(), file);
        CHECK_EQ (right->getSourceFileReference().getFile(), file);
        CHECK (file.existsAsFile());
        CHECK_EQ (readWav (file).lengthInSamples, (int64_t) (4.0 * kSampleRate));   // source untouched
    }

    TEST_CASE ("Editing: trimming the start preserves where the audio sits (offset grows), moving does not")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (4.0, 440.0));
        auto clip = insertClip (s.track (0), file, 1.0, 3.0);

        clip->setStart (te::TimePosition::fromSeconds (2.0), true, false);     // trim
        CHECK_EQ (secs (clip->getPosition().getStart()), doctest::Approx (2.0));
        CHECK_EQ (secs (clip->getPosition().getEnd()), doctest::Approx (4.0));
        CHECK_EQ (secs (clip->getPosition().getOffset()), doctest::Approx (1.0));
        CHECK_EQ (secs (clip->getPosition().getStartOfSource()), doctest::Approx (1.0));

        clip->setStart (te::TimePosition::fromSeconds (5.0), false, true);     // move
        CHECK_EQ (secs (clip->getPosition().getStart()), doctest::Approx (5.0));
        CHECK_EQ (secs (clip->getPosition().getLength()), doctest::Approx (2.0));
        CHECK_EQ (secs (clip->getPosition().getOffset()), doctest::Approx (1.0));

        clip->setEnd (te::TimePosition::fromSeconds (6.0), true);              // trim end
        CHECK_EQ (secs (clip->getPosition().getLength()), doctest::Approx (1.0));
        CHECK_EQ (secs (clip->getPosition().getOffset()), doctest::Approx (1.0));
    }

    TEST_CASE ("Editing: fades, clip gain, duplicate and delete are non-destructive and undoable")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (2.0, 440.0));
        auto& um = s.edit().getUndoManager();

        um.beginNewTransaction();
        auto clip = insertClip (s.track (0), file, 0.0, 2.0);

        um.beginNewTransaction();
        clip->setFadeIn (te::TimeDuration::fromSeconds (0.25));
        clip->setFadeOut (te::TimeDuration::fromSeconds (0.5));
        clip->setGainDB (-6.0f);
        CHECK_EQ (secs (clip->getFadeIn()), doctest::Approx (0.25));
        CHECK_EQ (secs (clip->getFadeOut()), doctest::Approx (0.5));
        CHECK_EQ (clip->getGainDB(), doctest::Approx (-6.0f));

        um.undo();
        CHECK_EQ (secs (clip->getFadeIn()), doctest::Approx (0.0));
        CHECK_EQ (clip->getGainDB(), doctest::Approx (0.0f));
        um.redo();
        CHECK_EQ (clip->getGainDB(), doctest::Approx (-6.0f));

        um.beginNewTransaction();
        auto copy = te::duplicateClip (*clip);
        REQUIRE (copy != nullptr);
        copy->setStart (clip->getPosition().getEnd(), false, true);
        CHECK_EQ (s.track (0).getClips().size(), 2);
        CHECK_EQ (copy->getSourceFileReference().getFile(), file);
        CHECK_EQ (secs (copy->getPosition().getStart()), doctest::Approx (2.0));

        um.beginNewTransaction();
        copy->removeFromParent();
        CHECK_EQ (s.track (0).getClips().size(), 1);
        CHECK (file.existsAsFile());                                            // deleting a clip never deletes audio
        um.undo();
        CHECK_EQ (s.track (0).getClips().size(), 2);

        // Undo the split too
        um.beginNewTransaction();
        s.track (0).splitClip (*s.track (0).getClips()[0], te::TimePosition::fromSeconds (1.0));
        CHECK_EQ (s.track (0).getClips().size(), 3);
        um.undo();
        CHECK_EQ (s.track (0).getClips().size(), 2);
    }

    TEST_CASE ("Editing: take groups - overlapping clips on a track, exactly one active")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (2.0, 440.0));
        auto a = insertClip (s.track (0), file, 0.0, 2.0);
        auto b = insertClip (s.track (0), file, 0.5, 1.5);
        auto c = insertClip (s.track (0), file, 1.0, 1.0);
        auto far = insertClip (s.track (0), file, 10.0, 1.0);     // not overlapping: not part of the group

        CHECK_EQ (RecordingController::getTakeGroupSize (*a), 3);
        CHECK_EQ (RecordingController::getTakeGroupSize (*far), 1);

        RecordingController::activateTake (*b);
        CHECK (a->isMuted());
        CHECK_FALSE (b->isMuted());
        CHECK (c->isMuted());
        CHECK_FALSE (far->isMuted());

        RecordingController::activateTake (*c);
        CHECK (a->isMuted());
        CHECK (b->isMuted());
        CHECK_FALSE (c->isMuted());
    }

    TEST_CASE ("Editing: snapping is optional and rounds to the beat grid at the session tempo")
    {
        Player player (1);
        Session s;
        s.edit().tempoSequence.getTempo (0)->setBpm (120.0);        // 1 beat = 0.5 s
        SessionContext ctx (getTestEngine(), *s.doc, *s.recording);

        ctx.snapToGrid = false;
        CHECK_EQ (secs (ctx.snap (te::TimePosition::fromSeconds (0.3))), doctest::Approx (0.3));

        ctx.snapToGrid = true;
        CHECK_EQ (secs (ctx.snap (te::TimePosition::fromSeconds (0.3))), doctest::Approx (0.5));
        CHECK_EQ (secs (ctx.snap (te::TimePosition::fromSeconds (0.2))), doctest::Approx (0.0));
        CHECK_EQ (secs (ctx.snap (te::TimePosition::fromSeconds (1.74))), doctest::Approx (1.5));
    }

    TEST_CASE ("Editing: changing the tempo moves the grid, not the recorded audio")
    {
        Player player (1);
        Session s;
        auto file = writeSessionAudio (s, "src", makeSine (2.0, 440.0));
        auto clip = insertClip (s.track (0), file, 1.0, 2.0);
        s.edit().tempoSequence.getTempo (0)->setBpm (90.0);
        s.edit().dispatchPendingUpdatesSynchronously();
        CHECK_EQ (secs (clip->getPosition().getStart()), doctest::Approx (1.0));
        CHECK_EQ (secs (clip->getPosition().getLength()), doctest::Approx (2.0));
    }
}

} // namespace opensession::tests
