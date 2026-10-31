// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CommandLine.h"

namespace opensession
{

static juce::File resolve (const juce::String& path)
{
    if (path.isEmpty())
        return {};

    if (juce::File::isAbsolutePath (path))
        return juce::File (path);

    return juce::File::getCurrentWorkingDirectory().getChildFile (path);
}

CommandLine CommandLine::parse (const juce::String& commandLine)
{
    CommandLine cl;
    auto tokens = juce::StringArray::fromTokens (commandLine, true);
    tokens.removeEmptyStrings();

    for (auto& t : tokens)
        t = t.unquoted();

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& t = tokens[i];

        if (t == "--run-tests")
        {
            cl.mode = Mode::runTests;
            for (int j = i + 1; j < tokens.size(); ++j)
                cl.passthroughArgs.add (tokens[j]);
            break;
        }
        else if (t == "--export-mix" && i + 2 < tokens.size())
        {
            cl.mode = Mode::exportMix;
            cl.sessionToOpen = resolve (tokens[i + 1]);
            cl.exportDestination = resolve (tokens[i + 2]);
            i += 2;
        }
        else if (t == "--export-stems" && i + 2 < tokens.size())
        {
            cl.mode = Mode::exportStems;
            cl.sessionToOpen = resolve (tokens[i + 1]);
            cl.exportDestination = resolve (tokens[i + 2]);
            i += 2;
        }
        else if (t == "--print-audio-settings")
        {
            cl.mode = Mode::printAudioSettings;
        }
        else if (t == "--measure-latency")
        {
            cl.mode = Mode::measureLatency;
        }
        else if (t == "--make-demo" && i + 1 < tokens.size())
        {
            cl.mode = Mode::makeDemo;
            cl.sessionToOpen = resolve (tokens[++i]);

            if (i + 1 < tokens.size() && ! tokens[i + 1].startsWith ("-"))
                cl.demoSeconds = juce::jlimit (8.0, 600.0, tokens[++i].getDoubleValue());
        }
        else if (t == "--soak" && i + 2 < tokens.size())
        {
            cl.mode = Mode::soak;
            cl.sessionToOpen = resolve (tokens[i + 1]);
            cl.soakMinutes = juce::jlimit (0.1, 120.0, tokens[i + 2].getDoubleValue());
            i += 2;

            if (i + 1 < tokens.size() && ! tokens[i + 1].startsWith ("-"))
                cl.soakTracks = juce::jlimit (1, 64, tokens[++i].getIntValue());
        }
        else if (t == "--smoke-record" && i + 2 < tokens.size())
        {
            cl.mode = Mode::smokeRecord;
            cl.sessionToOpen = resolve (tokens[i + 1]);
            cl.smokeSeconds = juce::jlimit (0.5, 600.0, tokens[i + 2].getDoubleValue());
            i += 2;

            if (i + 1 < tokens.size() && ! tokens[i + 1].startsWith ("-"))
                cl.smokeInputIndex = tokens[++i].getIntValue();
        }
        else if (t.startsWith ("-NSDocumentRevisionsDebugMode") || t.startsWith ("-psn"))
        {
            // macOS launcher noise
            if (t.startsWith ("-NSDocumentRevisionsDebugMode")) ++i;
        }
        else if (! t.startsWith ("-"))
        {
            cl.sessionToOpen = resolve (t);
        }
    }

    return cl;
}

juce::String CommandLine::usage()
{
    return "OpenSession [session.osession]\n"
           "OpenSession --run-tests [doctest args]\n"
           "OpenSession --export-mix <session.osession> <out.wav>\n"
           "OpenSession --export-stems <session.osession> <out-folder>\n"
           "OpenSession --print-audio-settings\n"
           "OpenSession --smoke-record <new-session.osession> <seconds> [input-index]\n"
           "OpenSession --measure-latency\n"
           "OpenSession --make-demo <new-session.osession> [seconds]\n"
           "OpenSession --soak <new-session.osession> <minutes> [tracks]\n";
}

} // namespace opensession
