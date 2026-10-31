// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/** A snapshot of what the audio device is actually doing right now.
    These are the numbers we expose to the user instead of marketing claims:
    buffer duration, driver-reported latency and the record adjustment are three
    different things and are reported separately. */
struct AudioSettingsSummary
{
    juce::String deviceTypeName, inputDeviceName, outputDeviceName;
    double sampleRate = 0;
    int bufferSizeSamples = 0;
    double bufferDurationMs = 0;
    int driverInputLatencySamples = 0, driverOutputLatencySamples = 0;
    double driverRoundTripMs = 0;         // input + output latency as reported by the driver
    double engineRecordAdjustmentMs = 0;  // what the engine shifts recordings by from driver figures
    juce::StringArray enabledInputNames;  // engine wave input devices (one per mono channel by default)
    int numActiveInputChannels = 0, numActiveOutputChannels = 0;
    int recordBitDepth = 24;
    bool deviceOpen = false;
    juce::String lastError;

    juce::String toMultiLineString() const;
};

/** Helpers around te::DeviceManager / juce::AudioDeviceManager. */
namespace DeviceSetup
{
    /** Default sample rate we ask for when the device supports it. */
    constexpr double preferredSampleRate = 48000.0;
    constexpr int preferredBitDepth = 24;

    /** Makes every wave input a mono device, enables all of them, sets 24-bit WAV output and
        the overlay merge mode (new recordings become new clips - we handle take muting ourselves). */
    void configureInputsForRecording (te::Engine&);

    /** Tries to switch the current device to the preferred sample rate if available. Returns an
        error string if the change was requested but failed. */
    juce::String applyPreferredSampleRate (te::Engine&);

    /** Sets the buffer size if the device offers it. */
    juce::String applyBufferSize (te::Engine&, int bufferSize);

    AudioSettingsSummary getSummary (te::Engine&);

    /** All enabled wave input devices, in engine order. */
    std::vector<te::WaveInputDevice*> getEnabledInputs (te::Engine&);

    /** Set a wave input device to mono or stereo (regrouping channels). */
    void setInputChannelCount (te::Engine&, te::WaveInputDevice&, int numChannels);

    /** Human text for the OS microphone permission situation. We can't query TCC from
        JUCE on macOS, so this explains what to do if input stays silent. */
    juce::String getInputPermissionGuidance();

    /** Short explanation of software vs direct monitoring for the setup screen. */
    juce::String getMonitoringGuidance();
}

} // namespace opensession
