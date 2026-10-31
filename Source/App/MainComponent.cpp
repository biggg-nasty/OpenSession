// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainComponent.h"
#include "../UI/PluginWindow.h"
#include "../Engine/DeviceSetup.h"
#include "../Session/Exporter.h"
#include "../UI/DeviceSettingsPanel.h"

namespace opensession
{

//==============================================================================
EditorView::EditorView (te::Engine& engine, SessionDocument& doc, std::function<void (const juce::String&, bool)> statusFn)
{
    recording = std::make_unique<RecordingController> (engine, doc.getEdit());
    ctx = std::make_unique<SessionContext> (engine, doc, *recording);
    ctx->setStatus = std::move (statusFn);

    recording->onTakeRecorded = [this] (te::Clip& c)
    {
        auto f = c.getSourceFileReference().getFile();
        te::AudioFile af (ctx->engine, f);
        juce::String s;
        s << "Recorded take: " << f.getFileName() << " (" << juce::String (af.getInfo().getLengthInSeconds(), 2) << " s, "
          << af.getNumChannels() << " ch, " << af.getBitsPerSample() << "-bit, " << juce::String (af.getSampleRate(), 0) << " Hz)";
        ctx->status (s);
        timeline->updateClipBounds();
    };

    recording->onError = [this] (const juce::String& e) { ctx->status (e, true); };

    transport = std::make_unique<TransportBar> (*ctx);
    headers = std::make_unique<TrackHeaderList> (*ctx);
    timeline = std::make_unique<TimelineComponent> (*ctx);
    mixer = std::make_unique<MixerComponent> (*ctx);

    transport->onAddTrack = [this] { addTrack(); };
    timeline->onVerticalScroll = [this] (int y) { headers->setVerticalOffset (y); };

    addAndMakeVisible (*transport);
    addAndMakeVisible (*headers);
    addAndMakeVisible (*timeline);
    addAndMakeVisible (*mixer);
}

EditorView::~EditorView()
{
    mixer.reset();
    timeline.reset();
    headers.reset();
    transport.reset();
    ctx.reset();
    recording.reset();
}

void EditorView::setMixerVisible (bool shouldShow)
{
    mixerVisible = shouldShow;
    mixer->setVisible (shouldShow);
    resized();
}

void EditorView::addTrack()
{
    auto& edit = ctx->edit;
    edit.getUndoManager().beginNewTransaction ("Add track");
    auto t = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, te::getAudioTracks (edit).getLast()), nullptr);

    if (t != nullptr)
    {
        t->setName ("Track " + juce::String (te::getAudioTracks (edit).size()));
        ctx->selectTrack (t.get());
    }
}

void EditorView::resized()
{
    auto r = getLocalBounds();
    transport->setBounds (r.removeFromTop (52));

    if (mixerVisible)
        mixer->setBounds (r.removeFromBottom (juce::jlimit (220, 300, r.getHeight() / 3)));

    headers->setBounds (r.removeFromLeft (SessionContext::headerWidth));
    timeline->setBounds (r);
}

//==============================================================================
MainComponent::MainComponent (OpenSessionEngine& eo, juce::DocumentWindow& w)
    : engineOwner (eo), engine (eo.getEngine()), window (w)
{
    setLookAndFeel (nullptr);
    engineOwner.setMessageSink (this);

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #else
    menuBar = std::make_unique<juce::MenuBarComponent> (this);
    addAndMakeVisible (*menuBar);
   #endif

    addAndMakeVisible (statusBar);
    addAndMakeVisible (welcome);
    addAndMakeVisible (welcomeNew);
    addAndMakeVisible (welcomeOpen);
    addAndMakeVisible (welcomeDemo);
    addAndMakeVisible (welcomeAudio);

    welcome.setJustificationType (juce::Justification::centred);
    welcome.setFont (juce::Font (juce::FontOptions (18.0f)));
    welcome.setText ("OpenSession - recording-first DAW\n\n"
                     "1. Audio settings: pick your interface, 48 kHz, a small buffer.\n"
                     "2. New session, choose an input on a track, press R to arm, watch the input meter.\n"
                     "3. Rec. Overdub on the next track. Double-click a take to make it active.\n"
                     "4. Mix, save, export the stereo mix and aligned stems.",
                     juce::dontSendNotification);

    welcomeNew.onClick = [this] { newSession(); };
    welcomeOpen.onClick = [this] { openSessionDialog(); };
    welcomeDemo.onClick = [this] { commandManager.invokeDirectly (cmdOpenDemo, true); };
    welcomeAudio.onClick = [this] { DeviceSettingsPanel::show (engine); };

    lastSessionDir = getDefaultSessionsFolder();
    engine.getDeviceManager().addChangeListener (this);

    // Device summary in the status bar + autosave (60 s)
    startTimer (1000);
    DeviceSetup::configureNewInputs (engine);
    setStatus ("Ready. " + DeviceSetup::getInputPermissionGuidance(), false);
    resized();
}

MainComponent::~MainComponent()
{
    engine.getDeviceManager().removeChangeListener (this);
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    closeSession();
    engineOwner.setMessageSink (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    if (menuBar != nullptr)
        menuBar->setBounds (r.removeFromTop (24));

    statusBar.setBounds (r.removeFromBottom (24));

    if (editor != nullptr)
    {
        editor->setBounds (r);
        editor->setVisible (true);
    }

    const bool showWelcome = editor == nullptr;
    welcome.setVisible (showWelcome);
    welcomeNew.setVisible (showWelcome);
    welcomeOpen.setVisible (showWelcome);
    welcomeDemo.setVisible (showWelcome);
    welcomeAudio.setVisible (showWelcome);

    if (showWelcome)
    {
        auto centre = r.withSizeKeepingCentre (640, 260);
        welcome.setBounds (centre.removeFromTop (180));
        auto buttons = centre.removeFromTop (34).reduced (20, 0);
        const int w = buttons.getWidth() / 4;
        welcomeNew.setBounds (buttons.removeFromLeft (w).reduced (4, 0));
        welcomeOpen.setBounds (buttons.removeFromLeft (w).reduced (4, 0));
        welcomeDemo.setBounds (buttons.removeFromLeft (w).reduced (4, 0));
        welcomeAudio.setBounds (buttons.reduced (4, 0));
    }
}

//==============================================================================
void MainComponent::setStatus (const juce::String& s, bool isError)
{
    statusBar.setMessage (s, isError);
}

void MainComponent::postMessage (const juce::String& title, const juce::String& body, bool isError)
{
    setStatus (title + ": " + body, isError);

    if (isError)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, body);
}

void MainComponent::timerCallback()
{
    static int tick = 0;

    if (document != nullptr && ++tick % 60 == 0)
    {
        if (auto r = document->saveAutosave(); r.failed())
            setStatus ("Autosave failed: " + r.getErrorMessage(), true);
    }

    updateTitle();

    auto& dm = engine.getDeviceManager();
    juce::String info;

    if (auto* dev = dm.deviceManager.getCurrentAudioDevice())
        info << dev->getName() << " | " << juce::String (dev->getCurrentSampleRate(), 0) << " Hz | "
             << dev->getCurrentBufferSizeSamples() << " frames (" << juce::String (1000.0 * dev->getCurrentBufferSizeSamples() / dev->getCurrentSampleRate(), 1) << " ms) | "
             << "CPU " << juce::String (dm.getCpuUsage() * 100.0f, 0) << "%";
    else
        info << "No audio device open";

    statusBar.setEngineInfo (info);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The engine's wave devices may have been rebuilt (device change): give any new input our defaults.
    DeviceSetup::configureNewInputs (engine);
}

void MainComponent::updateTitle()
{
    juce::String title = "OpenSession";

    if (document != nullptr)
        title << " - " << document->getName() << (document->hasUnsavedChanges() ? " *" : "");

    if (window.getName() != title)
        window.setName (title);
}

juce::File MainComponent::getDefaultSessionsFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OpenSession Sessions");
    f.createDirectory();
    return f;
}

juce::File MainComponent::getDemoSessionFolder() const
{
    // Look next to the app bundle (repo build) for demo/Demo Session.osession, then in ~/Music.
    auto exe = juce::File::getSpecialLocation (juce::File::currentApplicationFile);

    for (auto dir = exe.getParentDirectory(); dir != juce::File() && dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
    {
        auto candidate = dir.getChildFile ("demo").getChildFile ("DemoSession.osession");

        if (candidate.isDirectory())
            return candidate;
    }

    return getDefaultSessionsFolder().getChildFile ("DemoSession.osession");
}

//==============================================================================
void MainComponent::closeSession()
{
    if (editor != nullptr)
    {
        editor.reset();
    }

    if (document != nullptr)
    {
        document->getEdit().getTransport().stop (false, true);
        document.reset();
    }

    resized();
}

void MainComponent::setDocument (std::unique_ptr<SessionDocument> doc)
{
    closeSession();
    document = std::move (doc);

    if (document == nullptr)
        return;

    editor = std::make_unique<EditorView> (engine, *document, [this] (const juce::String& s, bool err) { setStatus (s, err); });
    addAndMakeVisible (*editor);
    resized();
    editor->getTimeline().zoomToFit();
    lastSessionDir = document->getFolder().getParentDirectory();
    updateTitle();
    grabKeyboardFocus();
    checkRecoveryAndIntegrity();
}

void MainComponent::openSession (const juce::File& f)
{
    if (! SessionDocument::looksLikeSession (f))
    {
        setStatus ("Not an OpenSession session folder: " + f.getFullPathName(), true);
        return;
    }

    auto folder = SessionDocument::getSessionFolderFor (f);

    auto load = [this, folder] (bool preferAutosave)
    {
        juce::String error;
        auto doc = SessionDocument::open (engine, folder, error, preferAutosave);

        if (doc == nullptr)
        {
            setStatus ("Couldn't open session: " + error, true);
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open session", error);
            return;
        }

        setDocument (std::move (doc));

        if (error.isNotEmpty())
            setStatus (error, true);
        else
            setStatus ("Opened " + folder.getFullPathName() + (preferAutosave ? " (recovered from autosave - save to keep it)" : ""), false);
    };

    // Recovery: an autosave newer than the last save means the app did not exit cleanly.
    auto autosave = folder.getChildFile ("Backups").getChildFile ("session.autosave.xml");
    auto saved = folder.getChildFile ("session.xml");

    if (autosave.existsAsFile() && autosave.getLastModificationTime() > saved.getLastModificationTime() + juce::RelativeTime::seconds (2))
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Recover unsaved changes?",
            "This session has an autosave newer than its last save (" + autosave.getLastModificationTime().toString (true, true)
            + " vs " + saved.getLastModificationTime().toString (true, true) + ").\n\nLoad the autosave?",
            "Load autosave", "Load last save", this,
            juce::ModalCallbackFunction::create ([load] (int result) { load (result == 1); }));
        return;
    }

    load (false);
}

void MainComponent::checkRecoveryAndIntegrity()
{
    if (document == nullptr)
        return;

    auto missing = document->findMissingFiles();

    if (! missing.isEmpty())
    {
        juce::StringArray names;
        for (auto& f : missing) names.add (f.getFullPathName());
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Missing audio files",
            juce::String (missing.size()) + " audio file(s) referenced by this session could not be found. Their clips show as MISSING and play silence:\n\n"
            + names.joinIntoString ("\n"));
    }

    auto orphans = document->findOrphanRecordings();

    if (! orphans.isEmpty())
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Recordings not on the timeline",
            juce::String (orphans.size()) + " WAV file(s) in this session's Audio folder are not used by any clip "
            "(for example from a recording interrupted by a crash). Put them back on the timeline as new tracks?",
            "Recover", "Leave them", this,
            juce::ModalCallbackFunction::create ([this] (int result)
            {
                if (result != 1 || document == nullptr)
                    return;

                int recovered = 0;

                for (auto& f : document->findOrphanRecordings())
                    if (document->recoverOrphanRecording (f).wasOk())
                        ++recovered;

                setStatus ("Recovered " + juce::String (recovered) + " recording(s) onto new tracks", false);
            }));
    }
}

void MainComponent::newSession()
{
    askToSaveThen ([this]
    {
        chooser = std::make_shared<juce::FileChooser> ("New session - choose a name and location",
                                                       lastSessionDir.getChildFile ("My Song"), "");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            auto chosen = fc.getResult();

            if (chosen == juce::File())
                return;

            auto folder = chosen.hasFileExtension ("osession") ? chosen : chosen.withFileExtension ("osession");
            juce::String error;
            auto doc = SessionDocument::createNew (engine, folder, folder.getFileNameWithoutExtension(), error);

            if (doc == nullptr)
            {
                setStatus ("Couldn't create session: " + error, true);
                return;
            }

            setDocument (std::move (doc));
            setStatus ("Created " + folder.getFullPathName() + ". Pick an input on Track 1 and arm it.", false);
        });
    });
}

void MainComponent::openSessionDialog()
{
    askToSaveThen ([this]
    {
        chooser = std::make_shared<juce::FileChooser> ("Open session (choose the .osession folder)", lastSessionDir, "*.osession;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories
                               | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                openSession (fc.getResult());
        });
    });
}

void MainComponent::saveSession (std::function<void (bool)> then)
{
    if (document == nullptr)
    {
        if (then) then (true);
        return;
    }

    auto r = document->save();

    if (r.failed())
    {
        setStatus ("SAVE FAILED: " + r.getErrorMessage() + " - your recordings are still on disk in " + document->getAudioDirectory().getFullPathName(), true);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", r.getErrorMessage());
    }
    else
    {
        setStatus ("Saved " + document->getSessionFile().getFullPathName(), false);
    }

    updateTitle();

    if (then)
        then (r.wasOk());
}

void MainComponent::saveAsCollected()
{
    if (document == nullptr)
        return;

    chooser = std::make_shared<juce::FileChooser> ("Save a collected copy as...", lastSessionDir.getChildFile (document->getName() + " copy"), "");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto chosen = fc.getResult();

        if (chosen == juce::File() || document == nullptr)
            return;

        auto folder = chosen.hasFileExtension ("osession") ? chosen : chosen.withFileExtension ("osession");
        auto r = document->saveAsCollected (folder);

        if (r.failed())
            setStatus ("Collect & save failed: " + r.getErrorMessage(), true);
        else
            setStatus ("Saved self-contained copy to " + folder.getFullPathName(), false);

        updateTitle();
    });
}

void MainComponent::importAudio()
{
    if (document == nullptr || editor == nullptr)
        return;

    chooser = std::make_shared<juce::FileChooser> ("Import audio file", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();

        if (f == juce::File() || document == nullptr || editor == nullptr)
            return;

        auto& ctx = editor->getContext();
        auto* track = ctx.getSelectedTrack();

        if (track == nullptr)
        {
            editor->addTrack();
            track = ctx.getSelectedTrack();
        }

        if (track == nullptr)
            return;

        ctx.edit.getUndoManager().beginNewTransaction ("Import audio");
        auto r = document->importAudioFile (f, *track, ctx.edit.getTransport().getPosition());
        setStatus (r.wasOk() ? "Imported " + f.getFileName() + " (copied into the session's Audio folder)" : r.getErrorMessage(), r.failed());
    });
}

static void showExportReport (const juce::String& title, const std::vector<ExportResult>& results)
{
    juce::StringArray lines;
    bool anyClip = false, anyFail = false;

    for (auto& r : results)
    {
        lines.add (r.describe());
        anyClip = anyClip || r.clipped;
        anyFail = anyFail || r.result.failed();
    }

    if (anyClip)
        lines.add ("\nAt least one file reached full scale. Nothing was normalised or limited - lower levels and export again if this is unwanted.");

    juce::AlertWindow::showMessageBoxAsync (anyFail ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon,
                                            title, lines.joinIntoString ("\n"));
}

void MainComponent::exportMix()
{
    if (document == nullptr)
        return;

    chooser = std::make_shared<juce::FileChooser> ("Export stereo mix", lastSessionDir.getChildFile (document->getName() + " mix.wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();

        if (f == juce::File() || document == nullptr)
            return;

        ExportOptions o;
        o.destination = f;
        o.sampleRate = engine.getDeviceManager().getSampleRate() > 0 ? engine.getDeviceManager().getSampleRate() : 48000.0;
        o.bitDepth = 24;
        o.tail = te::TimeDuration::fromSeconds (document->getExportTailSeconds());
        setStatus ("Exporting mix...", false);
        auto r = Exporter::exportStereoMix (document->getEdit(), o);
        setStatus (r.describe(), r.result.failed() || r.clipped);
        showExportReport ("Export mix", { r });
    });
}

void MainComponent::exportStems()
{
    if (document == nullptr)
        return;

    chooser = std::make_shared<juce::FileChooser> ("Export stems into folder", lastSessionDir, "");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        auto dir = fc.getResult();

        if (dir == juce::File() || document == nullptr)
            return;

        ExportOptions o;
        o.destination = dir.getChildFile (OpenSessionEngine::sanitiseFileName (document->getName()) + "_stems");
        o.sampleRate = engine.getDeviceManager().getSampleRate() > 0 ? engine.getDeviceManager().getSampleRate() : 48000.0;
        o.bitDepth = 24;
        o.tail = te::TimeDuration::fromSeconds (document->getExportTailSeconds());
        setStatus ("Exporting stems...", false);
        auto results = Exporter::exportStems (document->getEdit(), o);
        setStatus ("Exported " + juce::String (results.size()) + " stems to " + o.destination.getFullPathName(), false);
        showExportReport ("Export stems (all start at 0:00, same length)", results);
    });
}

void MainComponent::askToSaveThen (std::function<void()> continuation)
{
    if (document == nullptr || ! document->hasUnsavedChanges())
    {
        continuation();
        return;
    }

    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save changes?",
        "Save changes to \"" + document->getName() + "\" before continuing?", "Save", "Don't save", "Cancel", this,
        juce::ModalCallbackFunction::create ([this, continuation] (int result)
        {
            if (result == 1)
                saveSession ([continuation] (bool ok) { if (ok) continuation(); });
            else if (result == 2)
                continuation();
        }));
}

void MainComponent::tryToQuit()
{
    if (document != nullptr && document->getEdit().getTransport().isRecording())
    {
        setStatus ("Stop the recording before quitting", true);
        return;
    }

    askToSaveThen ([this]
    {
        closeSession();
        juce::JUCEApplication::getInstance()->quit();
    });
}

void MainComponent::addTrack (bool stereo)
{
    if (editor == nullptr)
        return;

    editor->addTrack();

    if (stereo)
    {
        // Stereo = a stereo input pair feeding the track. Make the first input a pair if needed and route it.
        auto& ctx = editor->getContext();
        auto inputs = ctx.recording.getAvailableInputs();
        te::WaveInputDevice* stereoInput = nullptr;

        for (auto* wi : inputs)
            if (wi->getChannels().getNumChannels() == 2)
                stereoInput = wi;

        if (stereoInput == nullptr && ! inputs.empty())
        {
            DeviceSetup::setInputChannelCount (engine, *inputs.front(), 2);
            inputs = ctx.recording.getAvailableInputs();

            for (auto* wi : inputs)
                if (wi->getChannels().getNumChannels() == 2)
                    stereoInput = wi;
        }

        if (auto* t = ctx.getSelectedTrack())
        {
            t->setName ("Stereo " + juce::String (te::getAudioTracks (ctx.edit).size()));

            if (stereoInput != nullptr)
            {
                auto r = ctx.recording.assignInput (*t, stereoInput);
                setStatus (r.wasOk() ? "Stereo track added, fed by " + stereoInput->getName() : r.getErrorMessage(), r.failed());
            }
            else
                setStatus ("Stereo track added. No stereo input pair is available on this device (needs 2 input channels).", true);
        }
    }
}

void MainComponent::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "About OpenSession",
        OpenSessionEngine::getBuildDescription() + "\n\n"
        "OpenSession is free software under the GNU GPL v3 or later.\n"
        "Built on Tracktion Engine (GPLv3) and JUCE (AGPLv3). See THIRD_PARTY_NOTICES.md.\n\n"
        "Source: " OPENSESSION_HOMEPAGE_URL);
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Transport", "View", "Audio", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (int id) { m.addCommandItem (&commandManager, id); };

    switch (index)
    {
        case 0:
            add (cmdNew); add (cmdOpen); add (cmdOpenDemo); m.addSeparator();
            add (cmdSave); add (cmdSaveAsCollected); add (cmdClose); m.addSeparator();
            add (cmdImportAudio); m.addSeparator();
            add (cmdExportMix); add (cmdExportStems);
            {
                juce::PopupMenu tail;
                for (auto id : { cmdExportTail0, cmdExportTail2, cmdExportTail5, cmdExportTail10 })
                    tail.addCommandItem (&commandManager, id);
                m.addSubMenu ("Export tail after last clip", tail, document != nullptr);
            }
            m.addSeparator();
            add (cmdRevealSession); add (cmdRecoverOrphans); add (cmdCheckMissing);
           #if ! JUCE_MAC
            m.addSeparator(); add (cmdQuit);
           #endif
            break;
        case 1:
            add (cmdUndo); add (cmdRedo); m.addSeparator();
            add (cmdSplit); add (cmdDuplicate); add (cmdDelete); add (cmdSelectTake); m.addSeparator();
            add (cmdClipGainUp); add (cmdClipGainDown); m.addSeparator(); add (cmdToggleSnap);
            break;
        case 2:
            add (cmdAddTrack); add (cmdAddStereoTrack); add (cmdDeleteTrack); m.addSeparator();
            add (cmdArmSelected); add (cmdMonitorSelected);
            break;
        case 3:
            add (cmdPlayStop); add (cmdRecord); add (cmdToStart); m.addSeparator();
            add (cmdToggleClick); add (cmdToggleClickRecordingOnly); add (cmdToggleCountIn);
            break;
        case 4:
            add (cmdToggleMixer); add (cmdToggleTakeLanes); m.addSeparator();
            add (cmdZoomIn); add (cmdZoomOut); add (cmdZoomFit);
            break;
        case 5:
            add (cmdAudioSettings); add (cmdPluginManager);
            break;
        case 6:
            add (cmdAbout); add (cmdShowLog);
            break;
        default: break;
    }

    return m;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ cmdNew, cmdOpen, cmdOpenDemo, cmdSave, cmdSaveAsCollected, cmdClose, cmdImportAudio, cmdExportMix, cmdExportStems,
                  cmdRevealSession, cmdQuit,
                  cmdUndo, cmdRedo, cmdSplit, cmdDelete, cmdDuplicate, cmdSelectTake, cmdClipGainUp, cmdClipGainDown, cmdToggleSnap,
                  cmdAddTrack, cmdAddStereoTrack, cmdDeleteTrack, cmdArmSelected, cmdMonitorSelected,
                  cmdPlayStop, cmdRecord, cmdToStart, cmdToggleClick, cmdToggleClickRecordingOnly, cmdToggleCountIn,
                  cmdExportTail0, cmdExportTail2, cmdExportTail5, cmdExportTail10,
                  cmdToggleMixer, cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdToggleTakeLanes,
                  cmdAudioSettings, cmdPluginManager, cmdRecoverOrphans, cmdCheckMissing, cmdAbout, cmdShowLog });
}

static double exportTailSecondsFor (juce::CommandID id)
{
    switch (id)
    {
        case MainComponent::cmdExportTail0:  return 0.0;
        case MainComponent::cmdExportTail5:  return 5.0;
        case MainComponent::cmdExportTail10: return 10.0;
        default:                             return SessionDocument::defaultExportTailSeconds;
    }
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using Mod = juce::ModifierKeys;
    const bool hasDoc = document != nullptr;
    const bool hasClip = editor != nullptr && editor->getContext().getSelectedClip() != nullptr;
    const bool hasTrack = editor != nullptr && editor->getContext().getSelectedTrack() != nullptr;

    auto set = [&] (const juce::String& name, const juce::String& category, bool active, int key = 0, int mods = 0)
    {
        info.setInfo (name, name, category, active ? 0 : juce::ApplicationCommandInfo::isDisabled);
        if (key != 0)
            info.addDefaultKeypress (key, Mod (mods));
    };

    switch (id)
    {
        case cmdNew:            set ("New session...", "File", true, 'n', Mod::commandModifier); break;
        case cmdOpen:           set ("Open session...", "File", true, 'o', Mod::commandModifier); break;
        case cmdOpenDemo:       set ("Open demo session", "File", true); break;
        case cmdSave:           set ("Save", "File", hasDoc, 's', Mod::commandModifier); break;
        case cmdSaveAsCollected:set ("Save collected copy as...", "File", hasDoc, 's', Mod::commandModifier | Mod::shiftModifier); break;
        case cmdClose:          set ("Close session", "File", hasDoc, 'w', Mod::commandModifier); break;
        case cmdImportAudio:    set ("Import audio file...", "File", hasDoc, 'i', Mod::commandModifier); break;
        case cmdExportMix:      set ("Export stereo mix...", "File", hasDoc, 'e', Mod::commandModifier); break;
        case cmdExportStems:    set ("Export stems...", "File", hasDoc, 'e', Mod::commandModifier | Mod::shiftModifier); break;
        case cmdRevealSession:  set ("Show session folder", "File", hasDoc); break;
        case cmdQuit:           set ("Quit", "File", true, 'q', Mod::commandModifier); break;
        case cmdUndo:           set ("Undo", "Edit", hasDoc, 'z', Mod::commandModifier); break;
        case cmdRedo:           set ("Redo", "Edit", hasDoc, 'z', Mod::commandModifier | Mod::shiftModifier); break;
        case cmdSplit:          set ("Split clip at playhead", "Edit", hasDoc, 't', Mod::commandModifier); break;
        case cmdDelete:         set ("Delete selected clip", "Edit", hasClip, juce::KeyPress::deleteKey); break;
        case cmdDuplicate:      set ("Duplicate selected clip", "Edit", hasClip, 'd', Mod::commandModifier); break;
        case cmdSelectTake:     set ("Make selected clip the active take", "Edit", hasClip, 'k', Mod::commandModifier); break;
        case cmdClipGainUp:     set ("Clip gain +1 dB", "Edit", hasClip, '=', Mod::commandModifier | Mod::altModifier); break;
        case cmdClipGainDown:   set ("Clip gain -1 dB", "Edit", hasClip, '-', Mod::commandModifier | Mod::altModifier); break;
        case cmdToggleSnap:     set ("Snap to grid", "Edit", hasDoc, 'g', Mod::commandModifier); info.setTicked (editor != nullptr && editor->getContext().snapToGrid); break;
        case cmdAddTrack:       set ("Add mono track", "Track", hasDoc, 't', Mod::commandModifier | Mod::shiftModifier); break;
        case cmdAddStereoTrack: set ("Add stereo track (stereo input pair)", "Track", hasDoc); break;
        case cmdDeleteTrack:    set ("Delete selected track", "Track", hasTrack); break;
        case cmdArmSelected:    set ("Arm/disarm selected track", "Track", hasTrack, 'r', Mod::shiftModifier); break;
        case cmdMonitorSelected:set ("Cycle monitoring on selected track", "Track", hasTrack, 'i', Mod::shiftModifier); break;
        case cmdPlayStop:       set ("Play / Stop", "Transport", hasDoc, juce::KeyPress::spaceKey); break;
        case cmdRecord:         set ("Record / Stop recording", "Transport", hasDoc, 'r'); break;
        case cmdToStart:        set ("Go to start", "Transport", hasDoc, juce::KeyPress::returnKey); break;
        case cmdToggleClick:    set ("Click (metronome)", "Transport", hasDoc, 'c'); info.setTicked (hasDoc && document->getEdit().clickTrackEnabled.get()); break;
        case cmdToggleClickRecordingOnly: set ("Click only while recording", "Transport", hasDoc); info.setTicked (hasDoc && document->getEdit().clickTrackRecordingOnly.get()); break;
        case cmdExportTail0: case cmdExportTail2: case cmdExportTail5: case cmdExportTail10:
        {
            const auto seconds = exportTailSecondsFor (id);
            set (seconds == 0 ? juce::String ("None (end at last clip)") : juce::String (seconds) + " s", "File", hasDoc);
            info.setTicked (hasDoc && std::abs (document->getExportTailSeconds() - seconds) < 0.01);
            break;
        }
        case cmdToggleCountIn:  set ("One-bar count-in", "Transport", hasDoc); info.setTicked (hasDoc && document->getEdit().getCountInMode() != te::Edit::CountIn::none); break;
        case cmdToggleMixer:    set ("Show mixer", "View", hasDoc, 'm', Mod::commandModifier); info.setTicked (editor != nullptr && editor->isMixerVisible()); break;
        case cmdToggleTakeLanes:set ("Show take lanes", "View", hasDoc); info.setTicked (editor != nullptr && editor->getContext().showTakeLanes); break;
        case cmdZoomIn:         set ("Zoom in", "View", hasDoc, '=', Mod::commandModifier); break;
        case cmdZoomOut:        set ("Zoom out", "View", hasDoc, '-', Mod::commandModifier); break;
        case cmdZoomFit:        set ("Zoom to fit", "View", hasDoc, '0', Mod::commandModifier); break;
        case cmdAudioSettings:  set ("Audio settings...", "Audio", true, ',', Mod::commandModifier); break;
        case cmdPluginManager:  set ("Plugins (VST3 / Audio Unit)...", "Audio", true); break;
        case cmdRecoverOrphans: set ("Recover recordings not on the timeline...", "File", hasDoc); break;
        case cmdCheckMissing:   set ("Check for missing audio files", "File", hasDoc); break;
        case cmdAbout:          set ("About OpenSession", "Help", true); break;
        case cmdShowLog:        set ("Show log folder", "Help", true); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    auto* ctx = editor != nullptr ? &editor->getContext() : nullptr;
    auto* clip = ctx != nullptr ? ctx->getSelectedClip() : nullptr;
    auto* track = ctx != nullptr ? ctx->getSelectedTrack() : nullptr;

    switch (info.commandID)
    {
        case cmdNew:            newSession(); break;
        case cmdOpen:           openSessionDialog(); break;
        case cmdOpenDemo:
        {
            auto demo = getDemoSessionFolder();
            if (SessionDocument::looksLikeSession (demo)) askToSaveThen ([this, demo] { openSession (demo); });
            else setStatus ("No demo session found at " + demo.getFullPathName(), true);
            break;
        }
        case cmdSave:           saveSession(); break;
        case cmdSaveAsCollected:saveAsCollected(); break;
        case cmdClose:          askToSaveThen ([this] { closeSession(); setStatus ("Session closed", false); }); break;
        case cmdImportAudio:    importAudio(); break;
        case cmdExportMix:      exportMix(); break;
        case cmdExportStems:    exportStems(); break;
        case cmdRevealSession:  if (document) document->getFolder().revealToUser(); break;
        case cmdQuit:           tryToQuit(); break;

        case cmdUndo:           if (ctx) { ctx->edit.getUndoManager().undo(); setStatus ("Undo", false); } break;
        case cmdRedo:           if (ctx) { ctx->edit.getUndoManager().redo(); setStatus ("Redo", false); } break;
        case cmdSplit:
            if (ctx)
            {
                auto pos = ctx->edit.getTransport().getPosition();
                ctx->edit.getUndoManager().beginNewTransaction ("Split");
                int n = 0;

                if (clip != nullptr)
                {
                    if (auto* t = clip->getClipTrack(); t != nullptr && clip->getPosition().time.contains (pos)) { t->splitClip (*clip, pos); ++n; }
                }
                else if (track != nullptr)
                {
                    for (auto* c : track->getClips())
                        if (c->getPosition().time.contains (pos)) { track->splitClip (*c, pos); ++n; }
                }

                setStatus (n > 0 ? "Split " + juce::String (n) + " clip(s) at playhead" : "Nothing under the playhead to split (select a clip or track)", n == 0);
            }
            break;
        case cmdDelete:
            if (clip) { ctx->edit.getUndoManager().beginNewTransaction ("Delete clip"); clip->removeFromParent(); ctx->selectClip (nullptr); setStatus ("Clip removed from the timeline; its audio file stays in Audio/", false); }
            break;
        case cmdDuplicate:
            if (clip)
            {
                ctx->edit.getUndoManager().beginNewTransaction ("Duplicate");
                if (auto copy = te::duplicateClip (*clip)) { copy->setStart (clip->getPosition().getEnd(), false, true); ctx->selectClip (copy.get()); }
            }
            break;
        case cmdSelectTake:     if (clip) { ctx->edit.getUndoManager().beginNewTransaction ("Select take"); RecordingController::activateTake (*clip); editor->getTimeline().updateClipBounds(); } break;
        case cmdClipGainUp:     if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip)) ac->setGainDB (juce::jmin (24.0f, ac->getGainDB() + 1.0f)); break;
        case cmdClipGainDown:   if (auto* ac = dynamic_cast<te::AudioClipBase*> (clip)) ac->setGainDB (juce::jmax (-60.0f, ac->getGainDB() - 1.0f)); break;
        case cmdToggleSnap:     if (ctx) { ctx->snapToGrid = ! ctx->snapToGrid; ctx->viewChanged(); } break;

        case cmdAddTrack:       addTrack (false); break;
        case cmdAddStereoTrack: addTrack (true); break;
        case cmdDeleteTrack:
            if (track && ctx)
            {
                ctx->edit.getUndoManager().beginNewTransaction ("Delete track");
                ctx->selectClip (nullptr);
                ctx->edit.deleteTrack (track);
                ctx->selectTrack (nullptr);
                setStatus ("Track deleted (audio files kept)", false);
            }
            break;
        case cmdArmSelected:
            if (track && ctx) { auto r = ctx->recording.setArmed (*track, ! ctx->recording.isArmed (*track)); if (r.failed()) setStatus (r.getErrorMessage(), true); }
            break;
        case cmdMonitorSelected:
            if (track && ctx)
            {
                using MM = te::InputDevice::MonitorMode;
                auto mode = ctx->recording.getMonitorMode (*track);
                ctx->recording.setMonitorMode (*track, mode == MM::off ? MM::automatic : mode == MM::automatic ? MM::on : MM::off);
            }
            break;

        case cmdPlayStop:
            if (ctx)
            {
                auto& tc = ctx->edit.getTransport();
                if (tc.isRecording()) ctx->recording.stopRecording();
                else if (tc.isPlaying()) tc.stop (false, false);
                else tc.play (false);
            }
            break;
        case cmdRecord:
            if (ctx)
            {
                if (ctx->edit.getTransport().isRecording()) ctx->recording.stopRecording();
                else { auto r = ctx->recording.startRecording(); if (r.failed()) setStatus (r.getErrorMessage(), true); }
            }
            break;
        case cmdToStart:        if (ctx) ctx->edit.getTransport().setPosition ({}); break;
        case cmdToggleClick:    if (ctx) ctx->edit.clickTrackEnabled = ! ctx->edit.clickTrackEnabled.get(); break;
        case cmdToggleClickRecordingOnly: if (ctx) ctx->edit.clickTrackRecordingOnly = ! ctx->edit.clickTrackRecordingOnly.get(); break;
        case cmdExportTail0: case cmdExportTail2: case cmdExportTail5: case cmdExportTail10:
            if (document != nullptr) document->setExportTailSeconds (exportTailSecondsFor (info.commandID));
            break;
        case cmdToggleCountIn:  if (ctx) ctx->edit.setCountInMode (ctx->edit.getCountInMode() == te::Edit::CountIn::none ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none); break;

        case cmdToggleMixer:    if (editor) editor->setMixerVisible (! editor->isMixerVisible()); break;
        case cmdToggleTakeLanes:if (ctx) { ctx->showTakeLanes = ! ctx->showTakeLanes; ctx->viewChanged(); } break;
        case cmdZoomIn:         if (editor) editor->getTimeline().zoom (1.25, editor->getTimeline().getWidth() / 2); break;
        case cmdZoomOut:        if (editor) editor->getTimeline().zoom (0.8, editor->getTimeline().getWidth() / 2); break;
        case cmdZoomFit:        if (editor) editor->getTimeline().zoomToFit(); break;

        case cmdAudioSettings:  DeviceSettingsPanel::show (engine); break;
        case cmdPluginManager:  PluginManagerPanel::show (engine); break;
        case cmdRecoverOrphans: checkRecoveryAndIntegrity(); break;
        case cmdCheckMissing:
            if (document)
            {
                auto missing = document->findMissingFiles();
                setStatus (missing.isEmpty() ? "All audio files present" : juce::String (missing.size()) + " missing audio file(s)", ! missing.isEmpty());
                if (! missing.isEmpty()) checkRecoveryAndIntegrity();
            }
            break;
        case cmdAbout:          showAbout(); break;
        case cmdShowLog:        juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("OpenSession").revealToUser(); break;
        default: return false;
    }

    commandManager.commandStatusChanged();
    return true;
}

} // namespace opensession
