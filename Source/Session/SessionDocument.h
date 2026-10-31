// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/**
    An OpenSession session is a folder:

        MySong.osession/
            session.xml                  Edit state (Tracktion Engine ValueTree, our properties under "OPENSESSION")
            manifest.json                schema version, app/engine version, name, sample rate, timestamps
            Audio/                       every recording and imported file (relative references)
            Backups/session.previous.xml the snapshot that was current before the last save
            Backups/session.autosave.xml periodic autosave (recovery)

    Saves are atomic: write to a temp file, rotate the previous session.xml into Backups/,
    then move the temp file into place. Audio files are never touched by a metadata save,
    so a failed save cannot lose recordings.
*/
class SessionDocument : private juce::ValueTree::Listener
{
public:
    static constexpr int schemaVersion = 1;
    static constexpr const char* folderExtension = ".osession";

    ~SessionDocument() override;

    /** Creates a brand-new session folder. Fails if the folder exists. */
    static std::unique_ptr<SessionDocument> createNew (te::Engine&, const juce::File& sessionFolder,
                                                       const juce::String& name, juce::String& error);

    /** Opens an existing session from its folder (or its session.xml). If preferAutosave is true and an
        autosave exists, that is loaded instead of session.xml. */
    static std::unique_ptr<SessionDocument> open (te::Engine&, const juce::File& folderOrFile,
                                                  juce::String& error, bool preferAutosave = false);

    static bool looksLikeSession (const juce::File& folderOrFile);
    static juce::File getSessionFolderFor (const juce::File& folderOrFile);

    //==============================================================================
    te::Edit& getEdit() noexcept                            { return *edit; }
    te::Engine& getEngine() noexcept                        { return engine; }
    juce::String getName() const;

    /** Silence appended after the last clip end when exporting (captures reverb/release).
        Stored in the session; 2 s when unset, clamped to 0..maxExportTailSeconds. */
    static constexpr double defaultExportTailSeconds = 2.0, maxExportTailSeconds = 30.0;
    double getExportTailSeconds() const;
    void setExportTailSeconds (double);
    juce::File getFolder() const                            { return folder; }
    juce::File getSessionFile() const                       { return folder.getChildFile ("session.xml"); }
    juce::File getManifestFile() const                      { return folder.getChildFile ("manifest.json"); }
    juce::File getAudioDirectory() const                    { return folder.getChildFile ("Audio"); }
    juce::File getBackupsDirectory() const                  { return folder.getChildFile ("Backups"); }
    juce::File getPreviousSnapshotFile() const              { return getBackupsDirectory().getChildFile ("session.previous.xml"); }
    juce::File getAutosaveFile() const                      { return getBackupsDirectory().getChildFile ("session.autosave.xml"); }

    bool hasUnsavedChanges() const;
    bool hasAutosaveNewerThanSave() const;
    bool wasLoadedFromAutosave() const                      { return loadedFromAutosave; }

    //==============================================================================
    /** Atomic save with previous-snapshot rotation. */
    juce::Result save();
    /** Writes Backups/session.autosave.xml without touching session.xml. */
    juce::Result saveAutosave();
    /** Copies the whole session to a new folder, collecting any audio referenced from outside
        the session's Audio folder, and switches this document to the new location. */
    juce::Result saveAsCollected (const juce::File& newFolder);

    //==============================================================================
    /** Copies an external audio file into Audio/ and adds a clip for it. */
    juce::Result importAudioFile (const juce::File& source, te::AudioTrack&, te::TimePosition at);

    /** Audio files referenced by clips but not present on disk. */
    juce::Array<juce::File> findMissingFiles() const;

    /** WAV files inside Audio/ that no clip references (e.g. from an interrupted recording). */
    juce::Array<juce::File> findOrphanRecordings() const;

    /** Puts an orphan recording back on the timeline: a new track, clip at the file's BWAV
        time stamp (or 0 if absent). Marks the clip name with "(recovered)". */
    juce::Result recoverOrphanRecording (const juce::File&);

    /** All WAV clips in the session, in track order. */
    juce::Array<te::WaveAudioClip*> getAllWaveClips() const;

    /** Writes a standard set of session properties (tempo etc. are in the Edit already). */
    void touchManifest();

private:
    SessionDocument (te::Engine&, juce::File folder);

    te::Engine& engine;
    juce::File folder;
    std::unique_ptr<te::Edit> edit;
    juce::var manifest;
    bool loadedFromAutosave = false;
    bool dirty = false;

    void attachToEdit();
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    juce::Result loadEditFromXml (const juce::File& xmlFile);
    juce::Result writeEditStateTo (const juce::File&) const;
    juce::Result writeManifest();
    juce::Result readManifest();
    void installResolvers();
    juce::File makeUniqueAudioFile (const juce::String& baseName, const juce::String& extension) const;
    static bool isInside (const juce::File& file, const juce::File& dir);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionDocument)
};

} // namespace opensession
