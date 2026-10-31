// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/** Export policy (documented in docs/recording-behavior.md, "Export"):
    - Range: timeline 0 to the end of the last clip, plus a fixed tail (default 2 s) so
      reverb/compressor release is captured. Stems use exactly the same range, so every stem
      starts at timeline zero and has identical length (leading silence preserved).
    - Mix: all unmuted tracks through their inserts and faders, then the master chain.
    - Stems: one file per audio track, post its own inserts/fader/pan, pre-master, stereo.
      Mute/solo are respected (a muted track yields a silent stem rather than being skipped).
    - Sample rate = the session's current device rate, bit depth 24 (or 16/32 float) as chosen.
    - Nothing is normalised. Peak level is measured and reported; clipping is flagged, not hidden. */
struct ExportOptions
{
    juce::File destination;             // file (mix) or folder (stems)
    double sampleRate = 48000.0;
    int bitDepth = 24;
    te::TimeDuration tail = te::TimeDuration::fromSeconds (2.0);
    bool includeMasterPlugins = true;   // mix only; stems are always pre-master
};

struct ExportResult
{
    juce::Result result { juce::Result::ok() };
    juce::File file;
    juce::String trackName;
    float peakLinear = 0.0f;            // measured before writing
    bool clipped = false;               // peak >= 0 dBFS
    double durationSeconds = 0.0;
    int64_t lengthInSamples = 0;
    int numChannels = 0;

    juce::String describe() const;
};

namespace Exporter
{
    te::TimeRange getExportRange (te::Edit&, te::TimeDuration tail);

    ExportResult exportStereoMix (te::Edit&, const ExportOptions&);

    /** One stem per audio track, named "<nn>_<TrackName>.wav" inside options.destination. */
    std::vector<ExportResult> exportStems (te::Edit&, const ExportOptions&);

    /** Reads back basic facts about a written file (used for reporting and tests). */
    ExportResult inspectFile (te::Engine&, const juce::File&);
}

} // namespace opensession
