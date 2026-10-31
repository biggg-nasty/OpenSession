// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entry point. Normal launch opens the GUI; --run-tests runs the deterministic self-test
// suite against a headless engine; --export-* render a session from the command line.
#include <JuceHeader.h>
#include <iostream>
#include "App/CommandLine.h"
#include "App/MainWindow.h"
#include "Engine/OpenSessionEngine.h"
#include "Engine/DeviceSetup.h"
#include "Session/SessionDocument.h"
#include "Session/Exporter.h"
#include "Engine/Recording.h"

#if OPENSESSION_BUILD_TESTS
 #include "Tests/TestMain.h"
#endif

namespace opensession
{

/** Logs to a file in the user's log folder and to stderr. */
struct TeeLogger : public juce::Logger
{
    TeeLogger()
    {
        fileLogger.reset (juce::FileLogger::createDefaultAppLogger ("OpenSession", "opensession.log",
                                                                    "OpenSession log - " + OpenSessionEngine::getBuildDescription()));
    }

    void logMessage (const juce::String& m) override
    {
        if (fileLogger != nullptr)
            fileLogger->logMessage (m);

        std::cerr << m << std::endl;
    }

    std::unique_ptr<juce::FileLogger> fileLogger;
};

class OpenSessionApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "OpenSession"; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override
    {
        auto cl = CommandLine::parse (commandLine);

        if (cl.mode == CommandLine::Mode::runTests)
        {
           #if OPENSESSION_BUILD_TESTS
            const int failures = tests::runAllTests (cl.passthroughArgs);
            setApplicationReturnValue (failures == 0 ? 0 : 1);
           #else
            std::cerr << "This build has no tests compiled in (OPENSESSION_BUILD_TESTS=OFF)\n";
            setApplicationReturnValue (2);
           #endif
            quit();
            return;
        }

        logger = std::make_unique<TeeLogger>();
        juce::Logger::setCurrentLogger (logger.get());
        juce::Logger::writeToLog (OpenSessionEngine::getBuildDescription());

        engine = std::make_unique<OpenSessionEngine> (false);
        const auto rateMessage = DeviceSetup::applyPreferredSampleRate (engine->getEngine());

        if (rateMessage.isNotEmpty())
            juce::Logger::writeToLog ("Sample rate: " + rateMessage);

        DeviceSetup::configureInputsForRecording (engine->getEngine());
        juce::Logger::writeToLog (DeviceSetup::getSummary (engine->getEngine()).toMultiLineString());

        if (cl.mode == CommandLine::Mode::printAudioSettings)
        {
            std::cout << DeviceSetup::getSummary (engine->getEngine()).toMultiLineString();
            quit();
            return;
        }

        if (cl.mode == CommandLine::Mode::exportMix || cl.mode == CommandLine::Mode::exportStems)
        {
            setApplicationReturnValue (runHeadlessExport (cl));
            quit();
            return;
        }

        if (cl.mode == CommandLine::Mode::smokeRecord)
        {
            startSmokeRecord (cl);
            return;
        }

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> ("OpenSession", *engine);

        if (cl.sessionToOpen != juce::File())
            mainWindow->getMainComponent().openSession (cl.sessionToOpen);
    }

    int runHeadlessExport (const CommandLine& cl)
    {
        juce::String error;
        auto doc = SessionDocument::open (engine->getEngine(), cl.sessionToOpen, error);

        if (doc == nullptr)
        {
            std::cerr << "Couldn't open session: " << error << "\n";
            return 1;
        }

        ExportOptions o;
        o.destination = cl.exportDestination;
        o.sampleRate = engine->getEngine().getDeviceManager().getSampleRate() > 0 ? engine->getEngine().getDeviceManager().getSampleRate() : 48000.0;

        if (cl.mode == CommandLine::Mode::exportMix)
        {
            auto r = Exporter::exportStereoMix (doc->getEdit(), o);
            std::cout << r.describe() << "\n";
            return r.result.wasOk() ? 0 : 1;
        }

        int failures = 0;

        for (auto& r : Exporter::exportStems (doc->getEdit(), o))
        {
            std::cout << r.describe() << "\n";
            failures += r.result.failed() ? 1 : 0;
        }

        return failures == 0 ? 0 : 1;
    }

    //==============================================================================
    // Hardware smoke test: real device -> new session -> record N seconds -> stop -> save -> report.
    struct SmokeState : private juce::Timer,
                        private te::TransportControl::Listener,
                        private juce::ValueTree::Listener
    {
        ~SmokeState() override
        {
            stopTimer();

            if (doc != nullptr)
            {
                doc->getEdit().getTransport().removeListener (this);
                doc->getEdit().state.removeListener (this);
            }
        }

        void startTracing()
        {
            doc->getEdit().getTransport().addListener (this);
            doc->getEdit().state.addListener (this);
            startTime = juce::Time::getMillisecondCounterHiRes();
            startTimer (100);
        }

        juce::String stamp() const { return "  [t+" + juce::String ((juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0, 2) + "s] "; }

        void timerCallback() override
        {
            auto& tc = doc->getEdit().getTransport();
            auto* epc = tc.getCurrentPlaybackContext();
            const bool playing = tc.isPlaying(), rec = tc.isRecording();
            const bool epcPlaying = epc != nullptr && epc->isPlaying();

            if (playing != lastPlaying || rec != lastRec || epcPlaying != lastEpcPlaying)
            {
                std::cout << stamp() << "transport playing=" << (playing ? 1 : 0) << " recording=" << (rec ? 1 : 0)
                          << " graphPlayhead=" << (epcPlaying ? 1 : 0) << " pos=" << tc.getPosition().inSeconds() << "\n";
                lastPlaying = playing; lastRec = rec; lastEpcPlaying = epcPlaying;
            }
        }

        void playbackContextChanged() override      { std::cout << stamp() << "playbackContextChanged (context " << (doc->getEdit().getCurrentPlaybackContext() ? "exists" : "freed") << ")\n"; }
        void recordingStarted (te::SyncPoint sp, std::optional<te::TimeRange>) override { std::cout << stamp() << "recordingStarted at " << sp.time.inSeconds() << "\n"; }
        void recordingStopped (te::SyncPoint sp, bool discard) override { std::cout << stamp() << "recordingStopped at " << sp.time.inSeconds() << (discard ? " (discard)" : "") << "\n"; }
        void recordingAboutToStop (te::InputDeviceInstance&, te::EditItemID) override { std::cout << stamp() << "recordingAboutToStop\n"; }
        void recordingFinished (te::InputDeviceInstance&, te::EditItemID, const juce::ReferenceCountedArray<te::Clip>& clips) override { std::cout << stamp() << "recordingFinished: " << clips.size() << " clip(s)\n"; }

        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
        {
            if (! v.hasType (te::IDs::TRANSPORT) && id != te::IDs::position)
                std::cout << stamp() << "state change: " << v.getType().toString() << "." << id.toString() << " = " << v[id].toString() << "\n";
        }
        void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c) override   { std::cout << stamp() << "child added: " << p.getType().toString() << " > " << c.getType().toString() << "\n"; }
        void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int) override { std::cout << stamp() << "child removed: " << p.getType().toString() << " > " << c.getType().toString() << "\n"; }

        std::unique_ptr<SessionDocument> doc;
        std::unique_ptr<RecordingController> recording;
        juce::File file;
        int returnValue = 1;
        double startTime = 0;
        bool lastPlaying = false, lastRec = false, lastEpcPlaying = false;
    };

    std::unique_ptr<SmokeState> smoke;

    void startSmokeRecord (const CommandLine& cl)
    {
        smoke = std::make_unique<SmokeState>();
        auto& te_engine = engine->getEngine();
        std::cout << "== OpenSession hardware smoke record ==\n" << DeviceSetup::getSummary (te_engine).toMultiLineString() << "\n";

        juce::String error;
        smoke->doc = SessionDocument::createNew (te_engine, cl.sessionToOpen, cl.sessionToOpen.getFileNameWithoutExtension(), error);

        if (smoke->doc == nullptr)
        {
            std::cerr << "Couldn't create session: " << error << "\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        auto& edit = smoke->doc->getEdit();
        edit.setCountInMode (te::Edit::CountIn::none);
        edit.clickTrackEnabled = false;
        smoke->recording = std::make_unique<RecordingController> (te_engine, edit);
        auto inputs = smoke->recording->getAvailableInputs();

        if (inputs.empty() || (size_t) cl.smokeInputIndex >= inputs.size())
        {
            std::cerr << "No enabled input at index " << cl.smokeInputIndex << " (" << inputs.size() << " available)\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        auto& track = *te::getAudioTracks (edit)[0];
        track.setName ("Smoke input");
        std::cout << "Routing input '" << inputs[(size_t) cl.smokeInputIndex]->getName() << "' -> track '" << track.getName() << "'\n";

        if (auto r = smoke->recording->assignInput (track, inputs[(size_t) cl.smokeInputIndex]); r.failed())
        {
            std::cerr << r.getErrorMessage() << "\n"; setApplicationReturnValue (1); quit(); return;
        }

        if (auto r = smoke->recording->setArmed (track, true); r.failed())
        {
            std::cerr << r.getErrorMessage() << "\n"; setApplicationReturnValue (1); quit(); return;
        }

        edit.dispatchPendingUpdatesSynchronously();
        smoke->startTracing();

        smoke->recording->onTakeRecorded = [this] (te::Clip& c) { smoke->file = c.getSourceFileReference().getFile(); };
        smoke->recording->onError = [] (const juce::String& e) { std::cerr << "Recording error: " << e << "\n"; };

        // Give the input meter a moment, then report the live input level before recording.
        juce::Timer::callAfterDelay (1500, [this, seconds = cl.smokeSeconds]
        {
            if (smoke == nullptr) return;
            auto& track = *te::getAudioTracks (smoke->doc->getEdit())[0];
            auto* input = smoke->recording->getInputForTrack (track);
            te::LevelMeasurer::Client client;
            input->levelMeasurer.addClient (client);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            const auto level = client.getAndClearAudioLevel (0).dB;
            input->levelMeasurer.removeClient (client);
            std::cout << "Live input level before recording: " << juce::String (level, 1) << " dBFS"
                      << (level <= -99.0f ? "  (silent - if you expected signal, check microphone permission / input gain)" : "") << "\n";

            auto r = smoke->recording->startRecording();

            if (r.failed())
            {
                std::cerr << "Couldn't start recording: " << r.getErrorMessage() << "\n";
                setApplicationReturnValue (1); quit(); return;
            }

            std::cout << "Recording for " << seconds << " s... (make some noise)\n";

            juce::Timer::callAfterDelay (1000, [this]
            {
                if (smoke == nullptr) return;
                auto& ed = smoke->doc->getEdit();
                auto& tc = ed.getTransport();
                auto& tr = *te::getAudioTracks (ed)[0];
                auto* inst = smoke->recording->getInputInstanceForTrack (tr);
                std::cout << "  [1 s] transport playing=" << (tc.isPlaying() ? "yes" : "no") << " recording=" << (tc.isRecording() ? "yes" : "no")
                          << " position=" << tc.getPosition().inSeconds() << " s"
                          << " | track armed=" << (smoke->recording->isArmed (tr) ? "yes" : "no")
                          << " | instance=" << (inst != nullptr ? "ok" : "NULL")
                          << " active=" << (inst != nullptr && inst->isRecordingActive() ? "yes" : "no")
                          << " recording=" << (inst != nullptr && inst->isRecording() ? "yes" : "no")
                          << " file=" << (inst != nullptr ? inst->getRecordingFile (tr.itemID).getFullPathName() : juce::String ("-")) << "\n";
            });

            juce::Timer::callAfterDelay (juce::roundToInt (seconds * 1000.0), [this]
            {
                if (smoke == nullptr) return;
                auto& tc = smoke->doc->getEdit().getTransport();
                auto* epc = tc.getCurrentPlaybackContext();
                std::cout << "  [stop] before: position=" << tc.getPosition().inSeconds()
                          << " unlooped=" << (epc != nullptr ? epc->getUnloopedPosition().inSeconds() : -1.0)
                          << " epcPlaying=" << (epc != nullptr && epc->isPlaying() ? "yes" : "no") << "\n";

                if (juce::SystemStats::getEnvironmentVariable ("OPENSESSION_SMOKE_CLEARDEVICES", "0") == "1")
                    tc.stop (false, true);
                else
                    smoke->recording->stopRecording();

                juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                std::cout << "  [stop] after: position=" << tc.getPosition().inSeconds() << "\n";
                finishSmokeRecord();
            });
        });
    }

    void finishSmokeRecord()
    {
        auto& edit = smoke->doc->getEdit();
        auto clips = smoke->doc->getAllWaveClips();

        if (clips.isEmpty())
        {
            std::cerr << "FAILED: no clip was created\n";
            setApplicationReturnValue (1);
        }
        else
        {
            auto f = clips[0]->getSourceFileReference().getFile();
            auto info = Exporter::inspectFile (engine->getEngine(), f);
            te::AudioFile af (engine->getEngine(), f);
            std::cout << "Recorded file: " << f.getFullPathName() << "\n"
                      << "  " << af.getSampleRate() << " Hz, " << af.getNumChannels() << " ch, " << af.getBitsPerSample() << "-bit, "
                      << info.lengthInSamples << " frames = " << juce::String (info.durationSeconds, 3) << " s\n"
                      << "  peak " << (info.peakLinear > 0 ? juce::String (juce::Decibels::gainToDecibels (info.peakLinear), 1) + " dBFS" : juce::String ("silent"))
                      << (info.clipped ? "  CLIPPED" : "") << "\n"
                      << "  clip on timeline: start " << clips[0]->getPosition().getStart().inSeconds() << " s, length "
                      << clips[0]->getPosition().getLength().inSeconds() << " s\n"
                      << "  record adjustment applied by engine (driver-derived): " << engine->getEngine().getDeviceManager().getRecordAdjustmentMs() << " ms\n";
            setApplicationReturnValue (info.lengthInSamples > 0 ? 0 : 1);
        }

        auto r = smoke->doc->save();
        std::cout << (r.wasOk() ? "Session saved: " + smoke->doc->getSessionFile().getFullPathName() : "SAVE FAILED: " + r.getErrorMessage()) << "\n";
        edit.getTransport().stop (false, true);
        smoke.reset();
        quit();
    }

    void shutdown() override
    {
        smoke.reset();
        mainWindow.reset();
        engine.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::Logger::setCurrentLogger (nullptr);
        logger.reset();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->getMainComponent().tryToQuit();
        else
            quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto cl = CommandLine::parse (commandLine);

        if (mainWindow != nullptr && cl.sessionToOpen != juce::File())
            mainWindow->getMainComponent().openSession (cl.sessionToOpen);
    }

private:
    std::unique_ptr<TeeLogger> logger;
    theme::LookAndFeel lookAndFeel;
    std::unique_ptr<OpenSessionEngine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace opensession

START_JUCE_APPLICATION (opensession::OpenSessionApplication)
