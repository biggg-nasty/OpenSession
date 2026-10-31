// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "../Engine/OpenSessionEngine.h"
#include "../Engine/Recording.h"
#include "../Session/SessionDocument.h"
#include "../UI/SessionContext.h"
#include "../UI/Theme.h"
#include "../UI/TransportBar.h"
#include "../UI/TrackHeader.h"
#include "../UI/Timeline.h"
#include "../UI/Mixer.h"
#include "../UI/StatusBar.h"

namespace opensession
{

/** The editor for one open session: transport, track headers + timeline, optional mixer. */
class EditorView : public juce::Component
{
public:
    EditorView (te::Engine&, SessionDocument&, std::function<void (const juce::String&, bool)> statusFn);
    ~EditorView() override;

    SessionContext& getContext()        { return *ctx; }
    RecordingController& getRecording() { return *recording; }
    TimelineComponent& getTimeline()    { return *timeline; }
    void setMixerVisible (bool);
    bool isMixerVisible() const         { return mixerVisible; }
    void resized() override;
    void addTrack();

private:
    std::unique_ptr<RecordingController> recording;
    std::unique_ptr<SessionContext> ctx;
    std::unique_ptr<TransportBar> transport;
    std::unique_ptr<TrackHeaderList> headers;
    std::unique_ptr<TimelineComponent> timeline;
    std::unique_ptr<MixerComponent> mixer;
    bool mixerVisible = true;
};

/** Top-level: menus, commands, session lifecycle (new/open/save/export/recover), status bar. */
class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget,
                      public MessageSink,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent (OpenSessionEngine&, juce::DocumentWindow&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override {}

    // ApplicationCommandTarget
    ApplicationCommandTarget* getNextCommandTarget() override   { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    // MessageSink
    void postMessage (const juce::String& title, const juce::String& body, bool isError) override;

    //==============================================================================
    void openSession (const juce::File&);
    void tryToQuit();
    bool hasSession() const                                     { return document != nullptr; }
    SessionDocument* getDocument()                              { return document.get(); }

    enum CommandIDs
    {
        cmdNew = 0x2000, cmdOpen, cmdOpenDemo, cmdSave, cmdSaveAsCollected, cmdClose, cmdImportAudio, cmdExportMix, cmdExportStems,
        cmdRevealSession, cmdQuit,
        cmdUndo, cmdRedo, cmdSplit, cmdDelete, cmdDuplicate, cmdSelectTake, cmdClipGainUp, cmdClipGainDown, cmdToggleSnap,
        cmdAddTrack, cmdAddStereoTrack, cmdDeleteTrack, cmdArmSelected, cmdMonitorSelected,
        cmdPlayStop, cmdRecord, cmdToStart, cmdToggleClick, cmdToggleCountIn,
        cmdToggleMixer, cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdToggleTakeLanes,
        cmdAudioSettings, cmdRecoverOrphans, cmdCheckMissing, cmdAbout, cmdShowLog
    };

private:
    OpenSessionEngine& engineOwner;
    te::Engine& engine;
    juce::DocumentWindow& window;
    juce::ApplicationCommandManager commandManager;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<SessionDocument> document;
    std::unique_ptr<EditorView> editor;
    StatusBar statusBar;
    juce::Label welcome;
    juce::TextButton welcomeNew { "New session" }, welcomeOpen { "Open session..." }, welcomeDemo { "Open demo session" }, welcomeAudio { "Audio settings" };
    std::shared_ptr<juce::FileChooser> chooser;
    juce::File lastSessionDir;

    void setStatus (const juce::String&, bool isError);
    void closeSession();
    void setDocument (std::unique_ptr<SessionDocument>);
    void newSession();
    void openSessionDialog();
    void saveSession (std::function<void (bool)> then = {});
    void saveAsCollected();
    void importAudio();
    void exportMix();
    void exportStems();
    void askToSaveThen (std::function<void()> continuation);
    void updateTitle();
    void checkRecoveryAndIntegrity();
    void addTrack (bool stereo);
    void showAbout();
    juce::File getDemoSessionFolder() const;
    juce::File getDefaultSessionsFolder() const;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace opensession
