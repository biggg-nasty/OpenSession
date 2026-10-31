// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{
class OpenSessionEngine;

namespace tests
{
    /** Runs the whole doctest suite against a headless engine. Returns the number of failed test cases. */
    int runAllTests (const juce::StringArray& doctestArgs);

    /** The engine the currently running test should use. */
    OpenSessionEngine& getTestEngineOwner();
    te::Engine& getTestEngine();
}
}
