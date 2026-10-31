// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionDocument.h"
#include "WavRepair.h"
#include "../Engine/OpenSessionEngine.h"

namespace opensession
{

namespace
{
    const juce::Identifier openSessionTag ("OPENSESSION");
    const juce::Identifier schemaProp ("schemaVersion");
    const juce::Identifier nameProp ("name");

    /** Relative source paths are resolved against the session folder. We write "Audio/x.wav";
        the engine's own helpers may write "../Audio/x.wav" (relative to the session.xml node),
        so both forms are accepted. */
    te::Edit::FilePathResolver makeResolver (const juce::File& folder)
    {
        return [folder] (const juce::String& path) -> juce::File
        {
            if (path.isEmpty())
                return {};

            if (juce::File::isAbsolutePath (path))
                return juce::File (path);

            if (path.startsWith ("../") || path.startsWith ("..\\"))
                return folder.getChildFile ("session.xml").getChildFile (path);

            return folder.getChildFile (path);
        };
    }

    void setRelativeSource (te::WaveAudioClip& clip, const juce::File& file, const juce::File& folder)
    {
        clip.getSourceFileReference().source = file.getRelativePathFrom (folder);
    }
}

SessionDocument::SessionDocument (te::Engine& e, juce::File f)
    : engine (e), folder (std::move (f))
{
}

SessionDocument::~SessionDocument()
{
    if (edit != nullptr)
    {
        edit->state.removeListener (this);
        edit->getTransport().stop (false, true);
    }

    edit.reset();
}

//==============================================================================
void SessionDocument::attachToEdit()
{
    if (edit != nullptr)
        edit->state.addListener (this);
}

static bool isVolatileSessionProperty (const juce::ValueTree& v, const juce::Identifier& id)
{
    // Transport position/scrubbing and the engine's own bookkeeping are not "unsaved work".
    if (v.hasType (te::IDs::TRANSPORT))
        return true;

    return id == te::IDs::position || id == te::IDs::lastSignificantChange || id == te::IDs::appVersion
        || id == te::IDs::modifiedBy || id == te::IDs::creationTime;
}

void SessionDocument::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (edit != nullptr && ! edit->isLoading() && ! isVolatileSessionProperty (v, id))
        dirty = true;
}

void SessionDocument::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (edit != nullptr && ! edit->isLoading() && ! parent.hasType (te::IDs::TRANSPORT))
        dirty = true;
}

void SessionDocument::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (edit != nullptr && ! edit->isLoading() && ! parent.hasType (te::IDs::TRANSPORT))
        dirty = true;
}

void SessionDocument::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    if (edit != nullptr && ! edit->isLoading())
        dirty = true;
}

//==============================================================================
bool SessionDocument::looksLikeSession (const juce::File& f)
{
    auto folder = getSessionFolderFor (f);
    return folder.isDirectory() && folder.getChildFile ("session.xml").existsAsFile();
}

juce::File SessionDocument::getSessionFolderFor (const juce::File& f)
{
    if (f.isDirectory())
        return f;

    if (f.getFileName() == "session.xml" || f.getFileName() == "manifest.json")
        return f.getParentDirectory();

    if (f.getParentDirectory().getFileName() == "Backups")
        return f.getParentDirectory().getParentDirectory();

    return f;
}

//==============================================================================
std::unique_ptr<SessionDocument> SessionDocument::createNew (te::Engine& engine, const juce::File& sessionFolder,
                                                             const juce::String& name, juce::String& error)
{
    if (sessionFolder.exists())
    {
        error = "A file or folder already exists at " + sessionFolder.getFullPathName();
        return {};
    }

    if (! sessionFolder.createDirectory())
    {
        error = "Couldn't create the session folder " + sessionFolder.getFullPathName();
        return {};
    }

    std::unique_ptr<SessionDocument> doc (new SessionDocument (engine, sessionFolder));
    doc->getAudioDirectory().createDirectory();
    doc->getBackupsDirectory().createDirectory();

    auto state = te::createEmptyEdit (engine);
    auto id = te::ProjectItemID::fromProperty (state, te::IDs::projectID);

    if (! id.isValid())
        id = te::ProjectItemID::createNewID (te::ProjectID{});

    auto os = state.getOrCreateChildWithName (openSessionTag, nullptr);
    os.setProperty (schemaProp, schemaVersion, nullptr);
    os.setProperty (nameProp, name, nullptr);

    const auto sessionFile = doc->getSessionFile();
    const auto folderCopy = sessionFolder;

    te::Edit::Options options { engine, state, id };
    options.role = te::Edit::forEditing;
    options.editFileRetriever = [sessionFile] { return sessionFile; };
    options.filePathResolver = makeResolver (folderCopy);
    options.numAudioTracks = 0;
    options.defaultMasterVolumedB = 0.0f;

    doc->edit = te::Edit::createEdit (std::move (options));

    if (doc->edit == nullptr)
    {
        error = "The audio engine could not create a new session";
        return {};
    }

    doc->edit->ensureNumberOfAudioTracks (2);
    doc->attachToEdit();
    doc->edit->tempoSequence.getTempo (0)->setBpm (120.0);
    doc->edit->clickTrackEnabled = true;
    doc->edit->clickTrackRecordingOnly = false;
    doc->edit->setCountInMode (te::Edit::CountIn::oneBar);
    doc->edit->setTimecodeFormat (te::TimecodeDisplayFormat (te::TimecodeType::barsBeats));

    doc->manifest = juce::var (new juce::DynamicObject());
    doc->touchManifest();
    doc->manifest.getDynamicObject()->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));

    if (auto r = doc->save(); r.failed())
    {
        error = r.getErrorMessage();
        return {};
    }

    return doc;
}

std::unique_ptr<SessionDocument> SessionDocument::open (te::Engine& engine, const juce::File& folderOrFile,
                                                        juce::String& error, bool preferAutosave)
{
    auto folder = getSessionFolderFor (folderOrFile);

    if (! folder.isDirectory())
    {
        error = folder.getFullPathName() + " is not a session folder";
        return {};
    }

    std::unique_ptr<SessionDocument> doc (new SessionDocument (engine, folder));

    if (! doc->getSessionFile().existsAsFile() && ! (preferAutosave && doc->getAutosaveFile().existsAsFile()))
    {
        error = "No session.xml inside " + folder.getFullPathName();
        return {};
    }

    doc->readManifest();   // a missing/corrupt manifest is not fatal; the Edit state is the source of truth

    juce::File toLoad = doc->getSessionFile();

    if (preferAutosave && doc->getAutosaveFile().existsAsFile())
    {
        toLoad = doc->getAutosaveFile();
        doc->loadedFromAutosave = true;
    }

    if (auto r = doc->loadEditFromXml (toLoad); r.failed())
    {
        // Offer the previous snapshot as a fallback so a corrupt save is recoverable.
        auto prev = doc->getPreviousSnapshotFile();

        if (prev.existsAsFile())
        {
            if (auto r2 = doc->loadEditFromXml (prev); r2.wasOk())
            {
                juce::Logger::writeToLog ("session.xml unreadable (" + r.getErrorMessage() + "); loaded previous snapshot");
                error = "session.xml was unreadable (" + r.getErrorMessage() + "). The previous snapshot was loaded instead - save to make it current.";
                return doc;
            }
        }

        error = r.getErrorMessage();
        return {};
    }

    doc->getAudioDirectory().createDirectory();
    doc->getBackupsDirectory().createDirectory();
    return doc;
}

juce::Result SessionDocument::loadEditFromXml (const juce::File& xmlFile)
{
    if (! xmlFile.existsAsFile())
        return juce::Result::fail ("Missing file: " + xmlFile.getFullPathName());

    if (xmlFile.getSize() == 0)
        return juce::Result::fail ("Session file is empty: " + xmlFile.getFullPathName());

    juce::XmlDocument xmlDoc (xmlFile);
    auto xml = xmlDoc.getDocumentElement();

    if (xml == nullptr)
        return juce::Result::fail ("Corrupt session file (" + xmlDoc.getLastParseError() + "): " + xmlFile.getFullPathName());

    te::updateLegacyEdit (*xml);
    auto state = juce::ValueTree::fromXml (*xml);

    if (! state.hasType (te::IDs::EDIT))
        return juce::Result::fail ("Not an OpenSession/Tracktion edit: " + xmlFile.getFullPathName());

    auto os = state.getChildWithName (openSessionTag);
    const int fileSchema = os.isValid() ? (int) os.getProperty (schemaProp, 0) : 0;

    if (fileSchema > schemaVersion)
        return juce::Result::fail ("This session was saved by a newer OpenSession (schema " + juce::String (fileSchema)
                                   + ", this build reads up to " + juce::String (schemaVersion) + ")");

    auto id = te::ProjectItemID::fromProperty (state, te::IDs::projectID);

    if (! id.isValid())
        id = te::ProjectItemID::createNewID (te::ProjectID{});

    const auto sessionFile = getSessionFile();
    const auto folderCopy = folder;

    te::Edit::Options options { engine, state, id };
    options.role = te::Edit::forEditing;
    options.editFileRetriever = [sessionFile] { return sessionFile; };
    options.filePathResolver = makeResolver (folderCopy);
    options.numAudioTracks = 0;

    edit = te::Edit::createEdit (std::move (options));

    if (edit == nullptr)
        return juce::Result::fail ("The audio engine refused to load " + xmlFile.getFullPathName());

    edit->playInStopEnabled = true;
    attachToEdit();
    return juce::Result::ok();
}

void SessionDocument::installResolvers()
{
    const auto sessionFile = getSessionFile();
    const auto folderCopy = folder;

    edit->editFileRetriever = [sessionFile] { return sessionFile; };
    edit->filePathResolver = makeResolver (folderCopy);
}

//==============================================================================
juce::String SessionDocument::getName() const
{
    if (edit != nullptr)
    {
        auto os = edit->state.getChildWithName (openSessionTag);

        if (os.isValid() && os.getProperty (nameProp).toString().isNotEmpty())
            return os.getProperty (nameProp).toString();
    }

    auto n = folder.getFileName();
    return n.endsWith (folderExtension) ? n.dropLastCharacters ((int) std::strlen (folderExtension)) : n;
}

bool SessionDocument::hasUnsavedChanges() const
{
    return edit != nullptr && (dirty || edit->hasChangedSinceSaved());
}

bool SessionDocument::hasAutosaveNewerThanSave() const
{
    auto a = getAutosaveFile();
    auto s = getSessionFile();
    return a.existsAsFile() && (! s.existsAsFile() || a.getLastModificationTime() > s.getLastModificationTime() + juce::RelativeTime::seconds (2));
}

//==============================================================================
juce::Result SessionDocument::writeEditStateTo (const juce::File& dest) const
{
    if (edit == nullptr)
        return juce::Result::fail ("No session loaded");

    edit->flushState();

    auto os = edit->state.getOrCreateChildWithName (openSessionTag, nullptr);
    os.setProperty (schemaProp, schemaVersion, nullptr);

    auto xml = edit->state.createXml();

    if (xml == nullptr)
        return juce::Result::fail ("Couldn't serialise the session state");

    dest.getParentDirectory().createDirectory();

    if (! xml->writeTo (dest))
        return juce::Result::fail ("Couldn't write " + dest.getFullPathName() + " (is the folder writable and is there disk space?)");

    return juce::Result::ok();
}

juce::Result SessionDocument::save()
{
    // 1. Write to a temp file next to the target (same volume -> atomic rename).
    auto target = getSessionFile();
    auto temp = folder.getChildFile ("session.xml.tmp");
    temp.deleteFile();

    if (auto r = writeEditStateTo (temp); r.failed())
    {
        temp.deleteFile();
        return r;
    }

    // Sanity-check what we wrote before replacing the good copy.
    if (juce::XmlDocument::parse (temp) == nullptr)
    {
        temp.deleteFile();
        return juce::Result::fail ("Save aborted: the written session file did not read back correctly; session.xml was left untouched");
    }

    // 2. Rotate the current file into Backups/.
    if (target.existsAsFile())
    {
        getBackupsDirectory().createDirectory();
        auto prev = getPreviousSnapshotFile();
        prev.deleteFile();

        if (! target.copyFileTo (prev))
            juce::Logger::writeToLog ("Warning: couldn't write previous snapshot " + prev.getFullPathName());
    }

    // 3. Move the temp into place.
    if (! temp.moveFileTo (target))
    {
        temp.deleteFile();
        return juce::Result::fail ("Couldn't replace " + target.getFullPathName());
    }

    touchManifest();

    if (auto r = writeManifest(); r.failed())
        juce::Logger::writeToLog ("Warning: " + r.getErrorMessage());

    edit->resetChangedStatus();
    dirty = false;
    getAutosaveFile().deleteFile();
    return juce::Result::ok();
}

juce::Result SessionDocument::saveAutosave()
{
    if (edit == nullptr || ! hasUnsavedChanges())
        return juce::Result::ok();

    getBackupsDirectory().createDirectory();
    auto temp = getBackupsDirectory().getChildFile ("session.autosave.tmp");
    temp.deleteFile();

    if (auto r = writeEditStateTo (temp); r.failed())
        return r;

    auto dest = getAutosaveFile();
    dest.deleteFile();

    if (! temp.moveFileTo (dest))
        return juce::Result::fail ("Couldn't write autosave");

    return juce::Result::ok();
}

void SessionDocument::touchManifest()
{
    if (! manifest.isObject())
        manifest = juce::var (new juce::DynamicObject());

    auto* obj = manifest.getDynamicObject();
    obj->setProperty ("format", "OpenSession session");
    obj->setProperty ("schemaVersion", schemaVersion);
    obj->setProperty ("name", getName());
    obj->setProperty ("savedBy", OpenSessionEngine::getBuildDescription());
    obj->setProperty ("modifiedAt", juce::Time::getCurrentTime().toISO8601 (true));
    obj->setProperty ("sampleRate", engine.getDeviceManager().getSampleRate());
    obj->setProperty ("sessionFile", "session.xml");
    obj->setProperty ("audioDirectory", "Audio");

    if (! obj->hasProperty ("createdAt"))
        obj->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));
}

juce::Result SessionDocument::writeManifest()
{
    auto f = getManifestFile();

    if (! f.replaceWithText (juce::JSON::toString (manifest, false)))
        return juce::Result::fail ("Couldn't write " + f.getFullPathName());

    return juce::Result::ok();
}

juce::Result SessionDocument::readManifest()
{
    auto f = getManifestFile();

    if (! f.existsAsFile())
        return juce::Result::fail ("No manifest");

    auto parsed = juce::JSON::parse (f);

    if (! parsed.isObject())
        return juce::Result::fail ("Manifest is not valid JSON");

    manifest = parsed;
    return juce::Result::ok();
}

//==============================================================================
bool SessionDocument::isInside (const juce::File& file, const juce::File& dir)
{
    return file.isAChildOf (dir);
}

juce::File SessionDocument::makeUniqueAudioFile (const juce::String& baseName, const juce::String& extension) const
{
    auto dir = getAudioDirectory();
    dir.createDirectory();
    auto base = OpenSessionEngine::sanitiseFileName (baseName);
    auto candidate = dir.getChildFile (base + extension);
    int n = 1;

    while (candidate.exists())
        candidate = dir.getChildFile (base + "_" + juce::String (++n) + extension);

    return candidate;
}

juce::Array<te::WaveAudioClip*> SessionDocument::getAllWaveClips() const
{
    juce::Array<te::WaveAudioClip*> result;

    if (edit != nullptr)
        for (auto* t : te::getAudioTracks (*edit))
            for (auto* c : t->getClips())
                if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c))
                    result.add (wc);

    return result;
}

juce::Result SessionDocument::saveAsCollected (const juce::File& newFolder)
{
    if (edit == nullptr)
        return juce::Result::fail ("No session loaded");

    if (newFolder.exists())
        return juce::Result::fail ("Destination already exists: " + newFolder.getFullPathName());

    if (! newFolder.createDirectory())
        return juce::Result::fail ("Couldn't create " + newFolder.getFullPathName());

    auto newAudio = newFolder.getChildFile ("Audio");
    newAudio.createDirectory();
    newFolder.getChildFile ("Backups").createDirectory();

    // Copy everything already in Audio/ (keeps orphans/alternate takes too).
    if (getAudioDirectory().isDirectory())
        for (const auto& f : getAudioDirectory().findChildFiles (juce::File::findFiles, false))
            if (! f.copyFileTo (newAudio.getChildFile (f.getFileName())))
                return juce::Result::fail ("Couldn't copy " + f.getFileName());

    // Switch the document to the new location, then re-point every clip.
    const auto oldAudio = getAudioDirectory();
    folder = newFolder;
    installResolvers();

    for (auto* wc : getAllWaveClips())
    {
        auto& ref = wc->getSourceFileReference();
        auto src = ref.getFile();

        // Paths were relative to the old folder; resolve them against the old Audio dir if needed.
        if (! src.existsAsFile() && ! juce::File::isAbsolutePath (ref.source.get()))
            src = oldAudio.getParentDirectory().getChildFile (ref.source.get());

        juce::File dest;

        if (isInside (src, oldAudio))
            dest = newAudio.getChildFile (src.getFileName());
        else if (src.existsAsFile())
        {
            dest = newAudio.getChildFile (src.getFileName());

            if (dest.exists() && dest.getSize() != src.getSize())
                dest = makeUniqueAudioFile (src.getFileNameWithoutExtension(), src.getFileExtension());

            if (! dest.exists() && ! src.copyFileTo (dest))
                return juce::Result::fail ("Couldn't collect " + src.getFullPathName());
        }
        else
        {
            juce::Logger::writeToLog ("Collect: missing source " + src.getFullPathName() + " (reference kept)");
            continue;
        }

        setRelativeSource (*wc, dest, folder);
    }

    edit->sendSourceFileUpdate();
    manifest.getDynamicObject()->removeProperty ("createdAt");
    return save();
}

juce::Result SessionDocument::importAudioFile (const juce::File& source, te::AudioTrack& track, te::TimePosition at)
{
    if (! source.existsAsFile())
        return juce::Result::fail ("File not found: " + source.getFullPathName());

    te::AudioFile probe (engine, source);

    if (! probe.isValid())
        return juce::Result::fail ("Not a readable audio file: " + source.getFileName());

    auto dest = isInside (source, getAudioDirectory()) ? source
                                                       : makeUniqueAudioFile (source.getFileNameWithoutExtension(), source.getFileExtension());

    if (dest != source && ! source.copyFileTo (dest))
        return juce::Result::fail ("Couldn't copy into the session's Audio folder");

    te::AudioFile af (engine, dest);
    const auto length = te::TimeDuration::fromSeconds (af.getInfo().getLengthInSeconds());

    auto clip = track.insertWaveClip (source.getFileNameWithoutExtension(), dest,
                                      te::ClipPosition { te::TimeRange (at, at + length), {} }, false);

    if (clip == nullptr)
        return juce::Result::fail ("The engine couldn't create a clip for " + source.getFileName());

    setRelativeSource (*clip, dest, folder);
    return juce::Result::ok();
}

juce::Array<juce::File> SessionDocument::findMissingFiles() const
{
    juce::Array<juce::File> missing;

    for (auto* wc : getAllWaveClips())
    {
        auto f = wc->getSourceFileReference().getFile();

        if (! f.existsAsFile())
            missing.addIfNotAlreadyThere (f == juce::File() ? juce::File (wc->getSourceFileReference().source.get()) : f);
    }

    return missing;
}

juce::Array<juce::File> SessionDocument::findOrphanRecordings() const
{
    juce::Array<juce::File> referenced;

    for (auto* wc : getAllWaveClips())
        referenced.add (wc->getSourceFileReference().getFile());

    juce::Array<juce::File> orphans;

    for (const auto& f : getAudioDirectory().findChildFiles (juce::File::findFiles, false, "*.wav"))
        if (! referenced.contains (f) && ! f.getFileName().startsWithChar ('.') && WavRepair::inspect (f).actualDataSize > 0)
            orphans.add (f);

    // Takes preserved after an aborted recording (see RecordingController) live one level down.
    for (const auto& f : getAudioDirectory().getChildFile ("Incomplete").findChildFiles (juce::File::findFiles, false, "*.wav"))
        if (! referenced.contains (f) && ! f.getFileName().endsWith (".inprogress.wav"))
            orphans.add (f);

    return orphans;
}

juce::Result SessionDocument::recoverOrphanRecording (const juce::File& file)
{
    if (edit == nullptr)
        return juce::Result::fail ("No session loaded");

    // A file whose header was never finalised (crash mid-take) is repaired in place first.
    if (auto report = WavRepair::inspect (file); report.isWav && report.needsRepair)
        if (auto r = WavRepair::repair (file); r.failed())
            return r;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return juce::Result::fail ("Can't read " + file.getFileName() + " - the recording may be incomplete");

    te::TimePosition start;
    const auto bwavRef = reader->metadataValues[juce::WavAudioFormat::bwavTimeReference];

    if (bwavRef.isNotEmpty() && reader->sampleRate > 0)
        start = te::TimePosition::fromSamples (bwavRef.getLargeIntValue(), reader->sampleRate);

    const auto length = te::TimeDuration::fromSeconds ((double) reader->lengthInSamples / reader->sampleRate);
    reader.reset();

    auto track = edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, te::getAudioTracks (*edit).getLast()), nullptr);

    if (track == nullptr)
        return juce::Result::fail ("Couldn't create a track for the recovered recording");

    track->setName (file.getFileNameWithoutExtension() + " (recovered)");

    auto clip = track->insertWaveClip (file.getFileNameWithoutExtension() + " (recovered)", file,
                                       te::ClipPosition { te::TimeRange (start, start + length), {} }, false);

    if (clip == nullptr)
        return juce::Result::fail ("Couldn't create a clip for the recovered recording");

    setRelativeSource (*clip, file, folder);
    return juce::Result::ok();
}

} // namespace opensession
