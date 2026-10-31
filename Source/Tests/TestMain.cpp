// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <JuceHeader.h>
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/tracktion_doctest.hpp>
#include <iostream>
#include "TestMain.h"
#include "../Engine/OpenSessionEngine.h"

namespace opensession::tests
{

static OpenSessionEngine* currentEngine = nullptr;

OpenSessionEngine& getTestEngineOwner()
{
    jassert (currentEngine != nullptr);
    return *currentEngine;
}

te::Engine& getTestEngine()
{
    return getTestEngineOwner().getEngine();
}

struct CoutLogger : public juce::Logger
{
    void logMessage (const juce::String& m) override { std::cout << "    [log] " << m << "\n"; }
};

int runAllTests (const juce::StringArray& doctestArgs)
{
    CoutLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    std::cout << "OpenSession self-test suite\n" << OpenSessionEngine::getBuildDescription() << "\n"
              << "Engine mode: headless (HostedAudioDeviceInterface - synthetic input, no system devices)\n\n";

    OpenSessionEngine engineOwner (true);
    currentEngine = &engineOwner;
    engineOwner.getEngine().getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

    doctest::Context context;
    context.setOption ("duration", true);
    context.setOption ("no-breaks", true);

    std::vector<std::string> storage;
    std::vector<const char*> argv;
    storage.push_back ("OpenSession");

    for (auto& a : doctestArgs)
        storage.push_back (a.toStdString());

    for (auto& s : storage)
        argv.push_back (s.c_str());

    context.applyCommandLine ((int) argv.size(), argv.data());
    context.addFilter ("test-suite", "opensession");

    const int result = context.run();

    currentEngine = nullptr;
    juce::Logger::setCurrentLogger (nullptr);
    return result;
}

} // namespace opensession::tests
