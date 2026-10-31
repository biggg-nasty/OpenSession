// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "SessionDocument.h"

namespace opensession
{

/** Synthesised material we own outright, used for the fallback demo session and the soak test.
    Nothing here is a recording of a person or a copyrighted work: plucks are Karplus-Strong strings,
    drums are filtered noise and sine thumps, the "vocal" is a formant-shaped tone. */
namespace DemoContent
{
    juce::AudioBuffer<float> pluckedPattern (double seconds, double bpm, double sampleRate, int seed);
    juce::AudioBuffer<float> bassLine (double seconds, double bpm, double sampleRate);
    juce::AudioBuffer<float> drumPattern (double seconds, double bpm, double sampleRate);
    juce::AudioBuffer<float> formantVoice (double seconds, double bpm, double sampleRate, int seed);
    juce::AudioBuffer<float> steadyTone (double seconds, double freq, float amplitude, double sampleRate);

    bool writeWav (const juce::File&, const juce::AudioBuffer<float>&, double sampleRate, int bitDepth = 24);

    /** Builds the demo session: 4 synthetic tracks, a muted alternate take, click on, 100 bpm. */
    juce::Result makeDemoSession (te::Engine&, const juce::File& sessionFolder, double sampleRate, double seconds);

    /** Builds a session with N steady-tone tracks of the given length for a playback+record soak test. */
    juce::Result makeSoakSession (te::Engine&, SessionDocument&, int numPlaybackTracks, double seconds, double sampleRate);
}

} // namespace opensession
