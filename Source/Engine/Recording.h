// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/**
    Session-level recording control: input-to-track mapping, arming, monitoring
    semantics and our take policy.

    Reused from the engine: InputDeviceInstance targets/arming, MonitorMode, the
    recording pipeline itself (background writer, latency adjustment, clip creation).
    Original: one-input-per-track mapping, explicit monitoring semantics, the
    "takes are overlapping clips, exactly one plays" rule, and error reporting.
*/
class RecordingController : private te::TransportControl::Listener
{
public:
    RecordingController (te::Engine&, te::Edit&);
    ~RecordingController() override;

    //==============================================================================
    /** Enabled engine inputs the user can pick from. */
    std::vector<te::WaveInputDevice*> getAvailableInputs() const;

    /** The wave input currently feeding the track, or nullptr. */
    te::WaveInputDevice* getInputForTrack (const te::AudioTrack&) const;
    te::InputDeviceInstance* getInputInstanceForTrack (const te::AudioTrack&) const;

    /** Folder inside Audio/ where in-progress safety links and recovered incomplete takes live. */
    static juce::File getIncompleteTakesFolder (te::Edit&);

    /** Routes an input to a track (removing it from any other track). nullptr clears. */
    juce::Result assignInput (te::AudioTrack&, te::WaveInputDevice*);

    //==============================================================================
    bool isArmed (const te::AudioTrack&) const;
    juce::Result setArmed (te::AudioTrack&, bool);
    int getNumArmedTracks() const;

    //==============================================================================
    /** Monitoring is a property of the input device in the engine; since we map one
        input to one track, we present it per track. */
    te::InputDevice::MonitorMode getMonitorMode (const te::AudioTrack&) const;
    void setMonitorMode (te::AudioTrack&, te::InputDevice::MonitorMode);
    static juce::String monitorModeToString (te::InputDevice::MonitorMode);

    /** True if the live input is audible right now for this track, given transport state. */
    bool isInputAudible (const te::AudioTrack&) const;

    //==============================================================================
    /** Starts recording on all armed tracks (count-in applies if enabled in the Edit).
        Fails early, with a message, if there is no armed track, no device, or the session's
        Audio folder cannot be written. */
    juce::Result startRecording();
    /** Stops, keeping the recordings. */
    void stopRecording();
    /** Stops and discards the files recorded in this pass. */
    void abortRecording();

    //==============================================================================
    // Takes. A "take group" is the set of clips on one track that overlap in time.
    // Exactly one clip in a group is active (unmuted); the others are muted. A new
    // recording over existing material becomes the active take.
    static juce::Array<te::Clip*> getOverlappingClips (te::Clip&);
    static void activateTake (te::Clip&);
    static bool isActiveTake (te::Clip&)                        { return false; }
    static int getTakeIndexInGroup (te::Clip&);
    static int getTakeGroupSize (te::Clip&);

    //==============================================================================
    std::function<void (te::Clip&)> onTakeRecorded;
    std::function<void (const juce::String&)> onError;

    juce::String getLastError() const                           { return lastError; }

private:
    te::Engine& engine;
    te::Edit& edit;
    juce::String lastError;

    int clipsWhenRecordingStarted = 0, armedWhenRecordingStarted = 0, clipsRecordedThisPass = 0;
    bool stopReported = false;
    struct InProgressFile { juce::File original, safetyLink; };
    std::vector<InProgressFile> inProgressFiles;
    juce::Array<juce::File> audioFilesBeforePass;

    void removeEmptyLeftovers();

    void protectInProgressFiles();
    void releaseOrRecoverInProgressFiles (bool recordingsWereKept);

    void recordingStarted (te::SyncPoint, std::optional<te::TimeRange>) override;
    void recordingStopped (te::SyncPoint, bool discardRecordings) override;
    void recordingFinished (te::InputDeviceInstance&, te::EditItemID, const juce::ReferenceCountedArray<te::Clip>&) override;
    static int countWaveClips (te::Edit&);
    juce::Result checkRecordingDestination() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingController)
};

} // namespace opensession
