// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace opensession
{

/** Command-line modes:
      OpenSession                              normal GUI
      OpenSession <session folder>             open a session
      OpenSession --run-tests [doctest args]   run the self-test suite headlessly, exit code = failures
      OpenSession --export-mix <session> <out.wav>
      OpenSession --export-stems <session> <out folder>
      OpenSession --print-audio-settings       print what the audio device reports and exit
      OpenSession --smoke-record <new session folder> <seconds> [input index]
                                               hardware check: record the real input into a new session, save, report
      OpenSession --measure-latency            hardware check: loopback round-trip latency through the current device
      OpenSession --make-demo <folder> [seconds]   build the synthetic fallback demo session
      OpenSession --soak <folder> <minutes> [tracks]  sustained playback (N tracks) + recording of input 1, reports CPU/dropouts */
struct CommandLine
{
    enum class Mode { gui, runTests, exportMix, exportStems, printAudioSettings, smokeRecord, measureLatency, makeDemo, soak };

    Mode mode = Mode::gui;
    juce::File sessionToOpen;
    juce::File exportDestination;
    juce::StringArray passthroughArgs;
    double smokeSeconds = 5.0;
    int smokeInputIndex = 0;
    double soakMinutes = 3.0;
    int soakTracks = 8;
    double demoSeconds = 48.0;

    static CommandLine parse (const juce::String& commandLine);
    static juce::String usage();
};

} // namespace opensession
