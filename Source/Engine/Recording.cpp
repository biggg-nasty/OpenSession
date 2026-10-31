// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Recording.h"
#include "../Session/WavRepair.h"
#include "OpenSessionEngine.h"

namespace opensession
{

RecordingController::RecordingController (te::Engine& e, te::Edit& ed)
    : engine (e), edit (ed)
{
    // Keep the graph running while stopped so input monitoring and meters work
    // before the first take (Logic/Pro Tools-style "input monitoring while stopped").
    edit.playInStopEnabled = true;
    edit.getTransport().ensureContextAllocated();
    edit.getTransport().addListener (this);
}

RecordingController::~RecordingController()
{
    edit.getTransport().removeListener (this);
}

//==============================================================================
std::vector<te::WaveInputDevice*> RecordingController::getAvailableInputs() const
{
    std::vector<te::WaveInputDevice*> result;

    for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
        if (wi != nullptr && wi->isEnabled())
            result.push_back (wi);

    return result;
}

te::InputDeviceInstance* RecordingController::getInputInstanceForTrack (const te::AudioTrack& track) const
{
    for (auto* instance : edit.getEditInputDevices().getDevicesForTargetTrack (track))
        if (instance != nullptr && instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            return instance;

    return nullptr;
}

te::WaveInputDevice* RecordingController::getInputForTrack (const te::AudioTrack& track) const
{
    if (auto* instance = getInputInstanceForTrack (track))
        return dynamic_cast<te::WaveInputDevice*> (&instance->getInputDevice());

    return nullptr;
}

juce::Result RecordingController::assignInput (te::AudioTrack& track, te::WaveInputDevice* device)
{
    auto& um = edit.getUndoManager();
    edit.getTransport().ensureContextAllocated();

    // Remove whatever wave input currently feeds this track.
    for (auto* existing : edit.getEditInputDevices().getDevicesForTargetTrack (track))
        if (existing != nullptr && existing->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            if (auto r = existing->removeTarget (track.itemID, &um); r.failed())
                return r;

    if (device == nullptr)
        return juce::Result::ok();

    if (auto* context = edit.getCurrentPlaybackContext())
    {
        if (auto* instance = context->getInputFor (device))
        {
            // moveToTrack = true: an input feeds exactly one track in OpenSession.
            auto res = instance->setTarget (track.itemID, true, &um, 0);

            if (! res)
                return juce::Result::fail (res.error());

            return juce::Result::ok();
        }

        return juce::Result::fail ("Input '" + device->getName() + "' is not available to this session (is it enabled?)");
    }

    return juce::Result::fail ("Audio engine is not running for this session");
}

//==============================================================================
bool RecordingController::isArmed (const te::AudioTrack& track) const
{
    if (auto* instance = getInputInstanceForTrack (track))
        return instance->isRecordingEnabled (track.itemID);

    return false;
}

juce::Result RecordingController::setArmed (te::AudioTrack& track, bool shouldBeArmed)
{
    auto* instance = getInputInstanceForTrack (track);

    if (instance == nullptr)
        return shouldBeArmed ? juce::Result::fail ("Choose an input for '" + track.getName() + "' before arming it")
                             : juce::Result::ok();

    instance->setRecordingEnabled (track.itemID, shouldBeArmed);
    return juce::Result::ok();
}

int RecordingController::getNumArmedTracks() const
{
    int n = 0;

    for (auto* t : te::getAudioTracks (edit))
        if (isArmed (*t))
            ++n;

    return n;
}

//==============================================================================
te::InputDevice::MonitorMode RecordingController::getMonitorMode (const te::AudioTrack& track) const
{
    if (auto* dev = getInputForTrack (track))
        return dev->getMonitorMode();

    return te::InputDevice::MonitorMode::off;
}

void RecordingController::setMonitorMode (te::AudioTrack& track, te::InputDevice::MonitorMode mode)
{
    if (auto* dev = getInputForTrack (track))
    {
        dev->setMonitorMode (mode);
        edit.restartPlayback();
    }
}

juce::String RecordingController::monitorModeToString (te::InputDevice::MonitorMode m)
{
    switch (m)
    {
        case te::InputDevice::MonitorMode::off:         return "OFF";
        case te::InputDevice::MonitorMode::automatic:   return "AUTO";
        case te::InputDevice::MonitorMode::on:          return "ON";
    }

    return "?";
}

bool RecordingController::isInputAudible (const te::AudioTrack& track) const
{
    if (auto* instance = getInputInstanceForTrack (track))
        return instance->isLivePlayEnabled (track);

    return false;
}

//==============================================================================
juce::Result RecordingController::startRecording()
{
    lastError.clear();
    stopReported = false;

    if (getNumArmedTracks() == 0)
        return juce::Result::fail ("No track is armed. Pick an input on a track and press its R button.");

    auto& dm = engine.getDeviceManager();

    if (dm.deviceManager.getCurrentAudioDevice() == nullptr)
        return juce::Result::fail ("No audio device is open. Open Audio > Audio Settings.");

    for (auto* t : te::getAudioTracks (edit))
        if (isArmed (*t))
            if (auto* dev = getInputForTrack (*t))
                if (dev->getMergeMode() == te::WaveInputDevice::getMergeModes()[2])
                    return juce::Result::fail ("Input '" + dev->getName() + "' is set not to record");

    if (auto r = checkRecordingDestination(); r.failed())
    {
        lastError = r.getErrorMessage();
        return r;
    }

    edit.getTransport().record (false);
    return juce::Result::ok();
}

juce::Result RecordingController::checkRecordingDestination() const
{
    if (! edit.editFileRetriever)
        return juce::Result::fail ("This session has no folder to record into");

    auto editFile = edit.editFileRetriever();

    if (editFile == juce::File())
        return juce::Result::fail ("This session has no folder to record into");

    auto audioDir = editFile.getParentDirectory().getChildFile ("Audio");

    if (! audioDir.isDirectory() && ! audioDir.createDirectory())
        return juce::Result::fail ("Can't create the recording folder " + audioDir.getFullPathName());

    if (! audioDir.hasWriteAccess())
        return juce::Result::fail ("The recording folder is not writable: " + audioDir.getFullPathName()
                                   + ". Check the folder's permissions or move the session.");

    // Prove it: create and delete a probe file (catches full disks and odd volumes too).
    auto probe = audioDir.getChildFile (".opensession-write-probe");

    if (! probe.replaceWithText ("probe") || ! probe.deleteFile())
        return juce::Result::fail ("Couldn't write a test file into " + audioDir.getFullPathName() + " (disk full or read-only?)");

    return juce::Result::ok();
}

int RecordingController::countWaveClips (te::Edit& e)
{
    int n = 0;

    for (auto* t : te::getAudioTracks (e))
        for (auto* c : t->getClips())
            if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr)
                ++n;

    return n;
}

juce::File RecordingController::getIncompleteTakesFolder (te::Edit& e)
{
    if (e.editFileRetriever)
        if (auto f = e.editFileRetriever(); f != juce::File())
            return f.getParentDirectory().getChildFile ("Audio").getChildFile ("Incomplete");

    return {};
}

void RecordingController::recordingStarted (te::SyncPoint, std::optional<te::TimeRange>)
{
    clipsWhenRecordingStarted = countWaveClips (edit);
    armedWhenRecordingStarted = getNumArmedTracks();
    clipsRecordedThisPass = 0;
    stopReported = false;
    audioFilesBeforePass.clear();

    if (auto dir = OpenSessionEngine::getAudioDirectoryForEdit (edit); dir.isDirectory())
        audioFilesBeforePass = dir.findChildFiles (juce::File::findFiles, false, "*.wav");

    protectInProgressFiles();
}

void RecordingController::removeEmptyLeftovers()
{
    // The engine can open a recording file more than once while preparing a pass (we have seen a
    // header-only "_01.wav" next to the real "_02.wav"). A WAV with no sample data that no clip
    // references is noise for the user, so it is removed here.
    auto dir = OpenSessionEngine::getAudioDirectoryForEdit (edit);

    if (! dir.isDirectory())
        return;

    juce::Array<juce::File> referenced;

    for (auto* t : te::getAudioTracks (edit))
        for (auto* c : t->getClips())
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c))
                referenced.add (wc->getSourceFileReference().getFile());

    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))
    {
        if (audioFilesBeforePass.contains (f) || referenced.contains (f))
            continue;

        if (f.getSize() < 4096 && WavRepair::inspect (f).actualDataSize <= 0)
            f.deleteFile();
    }
}

void RecordingController::protectInProgressFiles()
{
    // The engine deletes a take's file when a recording is aborted before it "officially" ends
    // (device rebuild, abort, zero length). A hard link to the same data in Audio/Incomplete keeps the
    // samples recoverable; we remove the link when the take completes normally.
    inProgressFiles.clear();
    auto folder = getIncompleteTakesFolder (edit);

    if (folder == juce::File())
        return;

    for (auto* t : te::getAudioTracks (edit))
    {
        if (! isArmed (*t))
            continue;

        if (auto* instance = getInputInstanceForTrack (*t))
        {
            auto original = instance->getRecordingFile (t->itemID);

            if (original == juce::File() || ! original.existsAsFile())
                continue;

            auto link = folder.getChildFile (original.getFileNameWithoutExtension() + ".inprogress.wav");

            if (WavRepair::createHardLink (original, link))
                inProgressFiles.push_back ({ original, link });
        }
    }
}

void RecordingController::releaseOrRecoverInProgressFiles (bool recordingsWereKept)
{
    juce::StringArray recovered;

    for (auto& f : inProgressFiles)
    {
        if (! f.safetyLink.existsAsFile())
            continue;

        if (recordingsWereKept && f.original.existsAsFile())
        {
            f.safetyLink.deleteFile();           // normal completion: the engine's file is the take
            continue;
        }

        // The engine removed (or never finished) the original: fix the header and keep the audio.
        auto report = WavRepair::inspect (f.safetyLink);

        if (report.recoverableFrames > 0 && WavRepair::repair (f.safetyLink).wasOk())
        {
            auto dest = f.safetyLink.getParentDirectory().getChildFile (f.original.getFileNameWithoutExtension() + "_incomplete.wav");
            dest.deleteFile();
            f.safetyLink.moveFileTo (dest);
            recovered.add (dest.getFileName() + " (" + juce::String (report.recoverableFrames / std::max (1.0, report.sampleRate), 2) + " s)");
        }
        else
        {
            f.safetyLink.deleteFile();
        }
    }

    inProgressFiles.clear();

    if (auto folder = getIncompleteTakesFolder (edit); folder.isDirectory() && folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
        folder.deleteFile();

    if (! recovered.isEmpty())
    {
        lastError = "The take did not complete, but its audio was preserved in Audio/Incomplete: " + recovered.joinIntoString (", ")
                    + ". Use File > Recover recordings to put it on the timeline.";

        if (onError)
            onError (lastError);
    }
}

void RecordingController::recordingStopped (te::SyncPoint, bool discardRecordings)
{
    if (stopReported)
        return;

    stopReported = true;

    if (discardRecordings)
    {
        for (auto& f : inProgressFiles)
            f.safetyLink.deleteFile();

        inProgressFiles.clear();
        return;
    }

    const bool anyClipCreated = clipsRecordedThisPass > 0 || countWaveClips (edit) != clipsWhenRecordingStarted;
    releaseOrRecoverInProgressFiles (anyClipCreated);
    removeEmptyLeftovers();

    // The engine can fail to open a file (permissions, disk) or abort a take (device rebuild) without
    // raising a UI warning; we notice because armed tracks produced no clips.
    if (armedWhenRecordingStarted > 0 && ! anyClipCreated && lastError.isEmpty())
    {
        lastError = "Recording stopped but no audio was captured on " + juce::String (armedWhenRecordingStarted)
                    + " armed track(s). Check the input, the session's Audio folder permissions and free disk space.";

        if (onError)
            onError (lastError);
    }
}

void RecordingController::stopRecording()
{
    edit.getTransport().stop (false, false);
}

void RecordingController::abortRecording()
{
    edit.getTransport().stop (true, false);
}

//==============================================================================
juce::Array<te::Clip*> RecordingController::getOverlappingClips (te::Clip& clip)
{
    juce::Array<te::Clip*> result;

    if (auto* track = clip.getClipTrack())
    {
        const auto range = clip.getPosition().time;

        for (auto* other : track->getClips())
            if (other != nullptr && other != &clip && other->getPosition().time.overlaps (range))
                result.add (other);
    }

    return result;
}

void RecordingController::activateTake (te::Clip& clip)
{
    if (auto* ac = dynamic_cast<te::AudioClipBase*> (&clip))
        ac->setMuted (false);

    for (auto* other : getOverlappingClips (clip))
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (other))
            ac->setMuted (true);
}

int RecordingController::getTakeGroupSize (te::Clip& clip)
{
    return getOverlappingClips (clip).size() + 1;
}

int RecordingController::getTakeIndexInGroup (te::Clip& clip)
{
    // Order takes by their position in the track's clip list (= recording order).
    if (auto* track = clip.getClipTrack())
    {
        int index = 0;
        auto overlapping = getOverlappingClips (clip);

        for (auto* c : track->getClips())
        {
            if (c == &clip)
                return index;

            if (overlapping.contains (c))
                ++index;
        }
    }

    return 0;
}

//==============================================================================
void RecordingController::recordingFinished (te::InputDeviceInstance&, te::EditItemID,
                                             const juce::ReferenceCountedArray<te::Clip>& recordedClips)
{
    for (auto clip : recordedClips)
    {
        if (clip == nullptr)
            continue;

        ++clipsRecordedThisPass;

        // The newest take becomes the one that plays; older overlapping takes stay in
        // the session, muted, and can be re-activated from the take lane.
        activateTake (*clip);

        if (onTakeRecorded)
            onTakeRecorded (*clip);
    }

}

} // namespace opensession
