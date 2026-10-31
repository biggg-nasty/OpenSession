// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OpenSessionEngine.h"

namespace opensession
{

//==============================================================================
struct OpenSessionEngine::Behaviour : public te::EngineBehaviour
{
    explicit Behaviour (bool headless_) : headless (headless_) {}

    bool autoInitialiseDeviceManager() override             { return ! headless; }
    bool addSystemAudioIODeviceTypes() override             { return ! headless; }

    // Recordings are placed by us, inside the session folder, with a readable name.
    juce::File getFileForNewAudioRecording (te::Track& track, const juce::String& fileExtension) override
    {
        return OpenSessionEngine::makeRecordingFile (track, fileExtension);
    }

    // We never want the engine to silently remap audio when the tempo changes
    // (the brief: preserve performance timing unless explicitly edited).
    bool areAudioClipsRemappedWhenTempoChanges() override   { return false; }

    // Plugin scans run in a child copy of the app (see Main.cpp), so a plugin that crashes
    // while being scanned is blacklisted instead of taking the session down.
    bool canScanPluginsOutOfProcess() override              { return ! headless; }

    bool headless;
};

//==============================================================================
struct OpenSessionEngine::UI : public te::UIBehaviour
{
    explicit UI (OpenSessionEngine& o) : owner (o) {}

    void showWarningAlert (const juce::String& title, const juce::String& message) override
    {
        post (title, message, true);
    }

    void showWarningMessage (const juce::String& message) override   { post ("Warning", message, true); }
    void showInfoMessage (const juce::String& message) override      { post ("Info", message, false); }

    // Renders (export) run as a ThreadPoolJobWithProgress; we pump the message
    // loop while they run so the UI stays responsive without blocking audio.
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& task) override
    {
        struct Runner : public juce::Thread
        {
            explicit Runner (te::ThreadPoolJobWithProgress& t) : juce::Thread (t.getJobName()), task (t) { startThread(); }
            ~Runner() override { task.signalJobShouldExit(); waitForThreadToExit (20000); }
            void run() override
            {
                while (! threadShouldExit())
                    if (task.runJob() == juce::ThreadPoolJob::jobHasFinished)
                        break;
            }
            te::ThreadPoolJobWithProgress& task;
        };

        Runner runner (task);

        while (runner.isThreadRunning())
            if (! juce::MessageManager::getInstance()->runDispatchLoopUntil (10))
                break;
    }

    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&pws); ws != nullptr && owner.pluginWindowFactory)
            return owner.pluginWindowFactory (ws->plugin);

        return {};
    }

    void post (const juce::String& title, const juce::String& body, bool isError)
    {
        juce::Logger::writeToLog ((isError ? "[engine warning] " : "[engine info] ") + title + ": " + body);

        if (owner.messageSink != nullptr)
            owner.messageSink->postMessage (title, body, isError);
    }

    OpenSessionEngine& owner;
};

//==============================================================================
OpenSessionEngine::OpenSessionEngine (bool headless_)
    : headless (headless_)
{
    engine = std::make_unique<te::Engine> (juce::String ("OpenSession"),
                                           std::make_unique<UI> (*this),
                                           std::make_unique<Behaviour> (headless));

    // The engine rescans MIDI hardware every 4 s by default and, when the list changes, rebuilds
    // every Edit's input devices - which aborts a recording in progress (observed on macOS 27 with
    // the built-in devices: the take stopped ~4 s after launch). OpenSession does not record MIDI
    // yet, so we only rescan on demand (Audio Settings) and never while recording.
    engine->getDeviceManager().setMidiDeviceScanIntervalSeconds (0);
}

OpenSessionEngine::~OpenSessionEngine()
{
    messageSink = nullptr;
    engine.reset();
}

void OpenSessionEngine::setMessageSink (MessageSink* sink)
{
    messageSink = sink;
}

juce::File OpenSessionEngine::getAudioDirectoryForEdit (te::Edit& edit)
{
    if (edit.editFileRetriever)
    {
        auto editFile = edit.editFileRetriever();

        if (editFile != juce::File())
            return editFile.getParentDirectory().getChildFile ("Audio");
    }

    return {};
}

juce::String OpenSessionEngine::sanitiseFileName (const juce::String& s)
{
    auto cleaned = juce::File::createLegalFileName (s.trim()).replaceCharacters (" /\\:", "____");
    cleaned = cleaned.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.");
    return cleaned.isEmpty() ? juce::String ("Track") : cleaned;
}

juce::File OpenSessionEngine::makeRecordingFile (te::Track& track, const juce::String& extension)
{
    auto dir = getAudioDirectoryForEdit (track.edit);

    if (dir == juce::File())
        return {};   // engine falls back to its own naming (temp dir)

    dir.createDirectory();

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    const auto base = sanitiseFileName (track.getName()) + "_" + stamp;
    const auto ext = extension.startsWithChar ('.') ? extension : "." + extension;

    for (int n = 1; n < 10000; ++n)
    {
        auto f = dir.getChildFile (base + "_" + juce::String (n).paddedLeft ('0', 2) + ext);

        if (! f.exists())
            return f;
    }

    return dir.getNonexistentChildFile (base, ext, false);
}

juce::String OpenSessionEngine::getBuildDescription()
{
    juce::String s;
    s << "OpenSession " << JUCE_APPLICATION_VERSION_STRING
      << " / Tracktion Engine " << OPENSESSION_ENGINE_REVISION
      << " / JUCE " << JUCE_MAJOR_VERSION << "." << JUCE_MINOR_VERSION << "." << JUCE_BUILDNUMBER
      << " / " << juce::SystemStats::getOperatingSystemName()
      << " (" << (juce::SystemStats::isOperatingSystem64Bit() ? "64-bit" : "32-bit") << ")";
    return s;
}

} // namespace opensession
