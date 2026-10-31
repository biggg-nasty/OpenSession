// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace opensession
{

/** Recovery for WAV files whose header was never finalised (recording interrupted by a crash,
    a device change, or a power loss). The audio data is on disk; only the RIFF and "data" chunk
    sizes are wrong (zero). We rewrite those from the real file size so the file reads normally. */
namespace WavRepair
{
    struct Report
    {
        bool isWav = false;
        bool needsRepair = false;       // header sizes don't match the file size
        int64_t fileSize = 0;
        int64_t dataChunkOffset = 0;    // offset of the "data" chunk's first sample byte
        int64_t headerDataSize = 0;     // what the header claims
        int64_t actualDataSize = 0;     // what is really there
        int bytesPerFrame = 0;
        double sampleRate = 0;
        int numChannels = 0, bitsPerSample = 0;
        int64_t recoverableFrames = 0;
        juce::String error;
    };

    /** Inspects a WAV file without modifying it. */
    Report inspect (const juce::File&);

    /** Fixes the RIFF/data sizes in place. Returns ok if the file now reads, or didn't need repair. */
    juce::Result repair (const juce::File&);

    /** Creates a second directory entry (hard link) for 'file' at 'linkPath', so the data survives
        if the original path is deleted. POSIX only; returns false if unsupported. */
    bool createHardLink (const juce::File& file, const juce::File& linkPath);
}

} // namespace opensession
