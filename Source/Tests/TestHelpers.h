// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include <doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include "TestMain.h"
#include "../Engine/OpenSessionEngine.h"
#include "../Engine/DeviceSetup.h"
#include "../Engine/Recording.h"
#include "../Engine/PluginHelpers.h"
#include "../Session/SessionDocument.h"
#include "../Session/Exporter.h"

namespace opensession::tests
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

//==============================================================================
/** A unique scratch folder, deleted when the test ends. */
struct TempDir
{
    TempDir (const juce::String& label = "opensession-test")
    {
        dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("OpenSessionTests")
                .getChildFile (label + "-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();
    }

    ~TempDir()
    {
        dir.deleteRecursively();
    }

    juce::File dir;
};

//==============================================================================
/** Pumps the message loop so the engine's async updates run. */
inline void pump (int milliseconds = 20)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

//==============================================================================
/** Hosted audio device for deterministic tests: we push input buffers, the engine records them. */
struct Player
{
    explicit Player (int inputChannels, int outputChannels = 2)
        : player (getTestEngine(), { .sampleRate = kSampleRate, .blockSize = kBlockSize, .useMidiDevices = false,
                                     .inputChannels = inputChannels, .outputChannels = outputChannels,
                                     .inputNames = {}, .outputNames = {}, .inputLatencyNumSamples = 0, .outputLatencyNumSamples = 0 })
    {
        DeviceSetup::configureInputsForRecording (getTestEngine());
        getTestEngine().getDeviceManager().dispatchPendingUpdates();
        pump();
    }

    te::test_utilities::EnginePlayer player;
};

//==============================================================================
/** A new session in a temp folder, with a RecordingController attached. */
struct Session
{
    /** If parentDir is given the session folder is created there and survives this object. */
    explicit Session (const juce::String& name = "Test", const juce::File& parentDir = {})
    {
        juce::String error;
        auto parent = parentDir != juce::File() ? parentDir : temp.dir;
        doc = SessionDocument::createNew (getTestEngine(), parent.getChildFile (name + SessionDocument::folderExtension), name, error);
        REQUIRE_MESSAGE (doc != nullptr, error.toStdString());
        edit().setCountInMode (te::Edit::CountIn::none);
        edit().clickTrackEnabled = false;
        recording = std::make_unique<RecordingController> (getTestEngine(), edit());
        edit().dispatchPendingUpdatesSynchronously();
    }

    ~Session()
    {
        recording.reset();
        doc.reset();
    }

    te::Edit& edit()                                   { return doc->getEdit(); }
    te::AudioTrack& track (int index)                  { auto tracks = te::getAudioTracks (edit()); REQUIRE (index < tracks.size()); return *tracks[index]; }
    te::WaveInputDevice* input (int index)
    {
        auto inputs = recording->getAvailableInputs();
        REQUIRE ((int) inputs.size() > index);
        return inputs[(size_t) index];
    }

    void assignAndArm (int trackIndex, int inputIndex)
    {
        auto r = recording->assignInput (track (trackIndex), input (inputIndex));
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        r = recording->setArmed (track (trackIndex), true);
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        edit().dispatchPendingUpdatesSynchronously();
        pump();
    }

    /** Records the given input buffer (channels = player inputs) from the current position. Returns new clips. */
    juce::Array<te::WaveAudioClip*> record (Player& p, juce::AudioBuffer<float>& input, int extraSamplesToFlush = 0)
    {
        juce::Array<te::WaveAudioClip*> before;
        for (auto* wc : doc->getAllWaveClips()) before.add (wc);

        auto r = recording->startRecording();
        REQUIRE_MESSAGE (r.wasOk(), r.getErrorMessage().toStdString());
        pump();
        lastOutput = p.player.process (input);

        if (extraSamplesToFlush > 0)
            p.player.process (extraSamplesToFlush);

        edit().getTransport().stop (false, true);
        pump (50);

        juce::Array<te::WaveAudioClip*> created;
        for (auto* wc : doc->getAllWaveClips())
            if (! before.contains (wc))
                created.add (wc);

        return created;
    }

    TempDir temp;
    std::unique_ptr<SessionDocument> doc;
    std::unique_ptr<RecordingController> recording;
    juce::AudioBuffer<float> lastOutput;
};

//==============================================================================
// Signals and analysis

inline juce::AudioBuffer<float> makeSine (double seconds, double freq, int channels = 1, float amplitude = 0.5f, double sampleRate = kSampleRate)
{
    const int n = (int) std::llround (seconds * sampleRate);
    juce::AudioBuffer<float> b (channels, n);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sampleRate);
    }

    return b;
}

inline juce::AudioBuffer<float> makeSquare (double seconds, double freq, int channels = 1, float amplitude = 0.5f, double sampleRate = kSampleRate)
{
    auto b = makeSine (seconds, freq, channels, 1.0f, sampleRate);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, b.getSample (ch, i) >= 0 ? amplitude : -amplitude);
    return b;
}

inline juce::AudioBuffer<float> makeSilence (double seconds, int channels = 1, double sampleRate = kSampleRate)
{
    juce::AudioBuffer<float> b (channels, (int) std::llround (seconds * sampleRate));
    b.clear();
    return b;
}

struct WavInfo
{
    bool ok = false;
    double sampleRate = 0;
    int numChannels = 0, bitsPerSample = 0;
    int64_t lengthInSamples = 0;
    juce::StringPairArray metadata;
    juce::AudioBuffer<float> samples;
};

inline WavInfo readWav (const juce::File& f)
{
    WavInfo info;
    juce::WavAudioFormat wav;
    auto stream = f.createInputStream();

    if (stream == nullptr)
        return info;

    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream.release(), true));

    if (reader == nullptr)
        return info;

    info.ok = true;
    info.sampleRate = reader->sampleRate;
    info.numChannels = (int) reader->numChannels;
    info.bitsPerSample = (int) reader->bitsPerSample;
    info.lengthInSamples = reader->lengthInSamples;
    info.metadata = reader->metadataValues;
    info.samples.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&info.samples, 0, (int) reader->lengthInSamples, 0, true, true);
    return info;
}

inline bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sampleRate = kSampleRate, int bits = 24, juce::StringPairArray metadata = {})
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sampleRate, (unsigned) b.getNumChannels(), bits, metadata, 0));
    if (w == nullptr) return false;
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

inline float rmsDb (const juce::AudioBuffer<float>& b, int ch, int start = 0, int num = -1)
{
    if (num < 0) num = b.getNumSamples() - start;
    if (num <= 0) return -200.0f;
    return juce::Decibels::gainToDecibels (b.getRMSLevel (ch, start, num), -200.0f);
}

inline float peakDb (const juce::AudioBuffer<float>& b, int ch)
{
    return juce::Decibels::gainToDecibels (b.getMagnitude (ch, 0, b.getNumSamples()), -200.0f);
}

inline bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

/** Max absolute difference between a channel of 'a' and a channel of 'b' over the overlapping length. */
inline float maxDiff (const juce::AudioBuffer<float>& a, int chA, const juce::AudioBuffer<float>& b, int chB, int offsetIntoB = 0)
{
    const int n = std::min (a.getNumSamples(), b.getNumSamples() - offsetIntoB);
    float m = 0.0f;
    for (int i = 0; i < n; ++i)
        m = std::max (m, std::abs (a.getSample (chA, i) - b.getSample (chB, i + offsetIntoB)));
    return m;
}

/** RMS of the difference in dBFS (how well 'recorded' reproduces 'reference'). */
inline float residualDb (const juce::AudioBuffer<float>& reference, int chR, const juce::AudioBuffer<float>& recorded, int chRec)
{
    const int n = std::min (reference.getNumSamples(), recorded.getNumSamples());
    double acc = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double d = reference.getSample (chR, i) - recorded.getSample (chRec, i);
        acc += d * d;
    }
    return n > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (acc / n), -200.0f) : -200.0f;
}

/** Finds the lag (in samples) of 'recorded' relative to 'reference' by cross-correlation. */
inline int findLag (const juce::AudioBuffer<float>& reference, const juce::AudioBuffer<float>& recorded, int maxLag = 4096, int window = 24000)
{
    const int n = std::min ({ window, reference.getNumSamples(), recorded.getNumSamples() - maxLag });
    int bestLag = 0;
    double best = -1.0;
    auto* r = reference.getReadPointer (0);
    auto* x = recorded.getReadPointer (0);

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double acc = 0.0;
        for (int i = std::max (0, -lag); i < n; ++i)
        {
            const int j = i + lag;
            if (j >= 0 && j < recorded.getNumSamples())
                acc += r[i] * x[j];
        }
        if (acc > best) { best = acc; bestLag = lag; }
    }
    return bestLag;
}

inline te::WaveAudioClip::Ptr insertClip (te::AudioTrack& track, const juce::File& file, double startSeconds, double lengthSeconds, double offsetSeconds = 0.0)
{
    auto clip = track.insertWaveClip (file.getFileNameWithoutExtension(), file,
                                      te::ClipPosition { te::TimeRange (te::TimePosition::fromSeconds (startSeconds),
                                                                        te::TimePosition::fromSeconds (startSeconds + lengthSeconds)),
                                                         te::TimeDuration::fromSeconds (offsetSeconds) }, false);
    REQUIRE (clip != nullptr);
    clip->setUsesProxy (false);
    return clip;
}

/** Writes a WAV into the session's Audio folder and returns it. */
inline juce::File writeSessionAudio (Session& s, const juce::String& name, const juce::AudioBuffer<float>& b, juce::StringPairArray metadata = {})
{
    auto f = s.doc->getAudioDirectory().getChildFile (name + ".wav");
    REQUIRE (writeWav (f, b, kSampleRate, 24, metadata));
    return f;
}

} // namespace opensession::tests
