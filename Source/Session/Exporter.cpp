// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Exporter.h"
#include "../Engine/OpenSessionEngine.h"

namespace opensession
{

juce::String ExportResult::describe() const
{
    if (result.failed())
        return (trackName.isNotEmpty() ? trackName + ": " : juce::String()) + "FAILED - " + result.getErrorMessage();

    juce::String s;
    s << (trackName.isNotEmpty() ? trackName + " -> " : juce::String()) << file.getFileName()
      << "  " << juce::String (durationSeconds, 2) << " s, " << numChannels << " ch, peak "
      << (peakLinear > 0.0f ? juce::String (juce::Decibels::gainToDecibels (peakLinear), 1) + " dBFS" : juce::String ("silent"));

    if (clipped)
        s << "  ** CLIPPED - lower the master or track gain and export again **";

    return s;
}

namespace Exporter
{

te::TimeRange getExportRange (te::Edit& edit, te::TimeDuration tail)
{
    auto end = te::TimePosition() + edit.getLength() + tail;

    if (end <= te::TimePosition::fromSeconds (0.01))
        end = te::TimePosition::fromSeconds (1.0);

    return { te::TimePosition(), end };
}

static juce::BigInteger bitsForTracks (te::Edit& edit, const juce::Array<te::Track*>& wanted)
{
    juce::BigInteger bits;
    auto all = te::getAllTracks (edit);

    for (int i = 0; i < all.size(); ++i)
        if (wanted.contains (all[i]))
            bits.setBit (i);

    return bits;
}

static ExportResult renderTracks (te::Edit& edit, const ExportOptions& options, const juce::File& dest,
                                  const juce::Array<te::Track*>& tracks, bool useMasterPlugins, const juce::String& label)
{
    ExportResult r;
    r.file = dest;
    r.trackName = label;

    auto& engine = edit.engine;
    const auto range = getExportRange (edit, options.tail);
    const auto bits = bitsForTracks (edit, tracks);

    if (bits.isZero())
    {
        r.result = juce::Result::fail ("Nothing to export");
        return r;
    }

    dest.getParentDirectory().createDirectory();

    if (! dest.getParentDirectory().hasWriteAccess())
    {
        r.result = juce::Result::fail ("Can't write to " + dest.getParentDirectory().getFullPathName());
        return r;
    }

    dest.deleteFile();

    te::Renderer::Parameters params (edit);
    params.destFile = dest;
    params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    params.bitDepth = options.bitDepth;
    params.sampleRateForAudio = options.sampleRate;
    params.blockSizeForAudio = 512;
    params.time = range;
    params.tracksToDo = bits;
    params.usePlugins = true;
    params.useMasterPlugins = useMasterPlugins;
    params.canRenderInMono = false;         // always stereo files so stems/mix line up
    params.shouldNormalise = false;
    params.ditheringEnabled = options.bitDepth < 24;
    params.checkNodesForAudio = false;      // a silent (muted) stem is a valid result
    params.trimSilenceAtEnds = false;
    params.endAllowance = {};

    const auto written = te::Renderer::renderToFile ("Export " + label, params);

    if (written == juce::File() || ! dest.existsAsFile() || dest.getSize() < 44)
    {
        r.result = juce::Result::fail ("The render produced no file for " + label);
        return r;
    }

    auto inspected = inspectFile (engine, dest);
    r.durationSeconds = inspected.durationSeconds;
    r.lengthInSamples = inspected.lengthInSamples;
    r.numChannels = inspected.numChannels;
    r.peakLinear = inspected.peakLinear;
    r.clipped = inspected.clipped;
    return r;
}

ExportResult exportStereoMix (te::Edit& edit, const ExportOptions& options)
{
    juce::Array<te::Track*> tracks;

    for (auto* t : te::getAudioTracks (edit))
        tracks.add (t);

    auto dest = options.destination.hasFileExtension ("wav") ? options.destination
                                                              : options.destination.withFileExtension ("wav");

    return renderTracks (edit, options, dest, tracks, options.includeMasterPlugins, "Mix");
}

std::vector<ExportResult> exportStems (te::Edit& edit, const ExportOptions& options)
{
    std::vector<ExportResult> results;
    auto folder = options.destination;
    folder.createDirectory();

    int index = 0;

    for (auto* t : te::getAudioTracks (edit))
    {
        ++index;
        auto name = juce::String (index).paddedLeft ('0', 2) + "_" + OpenSessionEngine::sanitiseFileName (t->getName()) + ".wav";
        results.push_back (renderTracks (edit, options, folder.getChildFile (name), { t }, false, t->getName()));
    }

    return results;
}

ExportResult inspectFile (te::Engine& engine, const juce::File& file)
{
    ExportResult r;
    r.file = file;
    std::unique_ptr<juce::AudioFormatReader> reader (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        r.result = juce::Result::fail ("Unreadable file " + file.getFileName());
        return r;
    }

    r.lengthInSamples = reader->lengthInSamples;
    r.numChannels = (int) reader->numChannels;
    r.durationSeconds = reader->sampleRate > 0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;

    const int blockSize = 65536;
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, blockSize);
    float peak = 0.0f;

    for (int64_t pos = 0; pos < reader->lengthInSamples; pos += blockSize)
    {
        const int n = (int) std::min<int64_t> (blockSize, reader->lengthInSamples - pos);
        reader->read (&buffer, 0, n, pos, true, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = std::max (peak, buffer.getMagnitude (ch, 0, n));
    }

    r.peakLinear = peak;
    // An integer WAV can't hold > 0 dBFS, so "at full scale" is our clipping evidence.
    r.clipped = peak >= 0.999f;
    return r;
}

} // namespace Exporter
} // namespace opensession
