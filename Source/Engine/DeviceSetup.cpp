// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeviceSetup.h"

namespace opensession
{

juce::String AudioSettingsSummary::toMultiLineString() const
{
    juce::String s;
    s << "Driver: " << deviceTypeName << "\n"
      << "Input device: " << (inputDeviceName.isEmpty() ? "(none)" : inputDeviceName)
      << "  (" << numActiveInputChannels << " active channel" << (numActiveInputChannels == 1 ? "" : "s") << ")\n"
      << "Output device: " << (outputDeviceName.isEmpty() ? "(none)" : outputDeviceName)
      << "  (" << numActiveOutputChannels << " active channel" << (numActiveOutputChannels == 1 ? "" : "s") << ")\n"
      << "Sample rate: " << juce::String (sampleRate, 0) << " Hz, recording bit depth: " << recordBitDepth << "-bit WAV\n"
      << "Buffer: " << bufferSizeSamples << " frames = " << juce::String (bufferDurationMs, 2) << " ms (this is NOT the round-trip latency)\n"
      << "Driver-reported latency: in " << driverInputLatencySamples << " + out " << driverOutputLatencySamples
      << " frames = " << juce::String (driverRoundTripMs, 2) << " ms round trip (estimate from the driver, not measured)\n"
      << "Engine record adjustment from driver figures: " << juce::String (engineRecordAdjustmentMs, 2) << " ms\n"
      << "Engine inputs: " << (enabledInputNames.isEmpty() ? juce::String ("(none)") : enabledInputNames.joinIntoString (", ")) << "\n";

    if (! deviceOpen)
        s << "DEVICE NOT OPEN" << (lastError.isNotEmpty() ? ": " + lastError : juce::String()) << "\n";

    return s;
}

namespace DeviceSetup
{

static juce::StringArray& configuredInputNames()
{
    static juce::StringArray names;   // one engine per process
    return names;
}

static void applyInputDefaults (te::Engine& engine, bool onlyNewInputs)
{
    auto& dm = engine.getDeviceManager();

    for (auto* wi : dm.getWaveInputDevices())
    {
        if (wi == nullptr || (onlyNewInputs && configuredInputNames().contains (wi->getName())))
            continue;

        configuredInputNames().addIfNotAlreadyThere (wi->getName());

        // The engine rebuilds its whole device list on every setDeviceEnabled call, even when nothing
        // changes, so only call it when the input really is disabled.
        if (! wi->isEnabled())
            dm.setDeviceEnabled (*wi, true);

        if (wi->getBitDepth() != preferredBitDepth && wi->getAvailableBitDepths().contains (preferredBitDepth))
            wi->setBitDepth (preferredBitDepth);

        wi->setOutputFormat ("WAV file");
        wi->setMergeMode (te::WaveInputDevice::getMergeModes()[0]);   // overlay: never delete older takes
        wi->setMonitorMode (te::InputDevice::MonitorMode::off);        // explicit: monitoring is off until the user asks
        wi->setInputGainDb (0.0f);
        wi->setRecordTriggerDb (-100.0f);                              // never gate recordings on level
    }

    dm.dispatchPendingUpdates();
}

void configureInputsForRecording (te::Engine& engine)
{
    auto& dm = engine.getDeviceManager();

    // One mono engine input per hardware channel: a track then picks "Input 1", "Input 2"...
    // Stereo pairs can be created from the settings panel when wanted.
    dm.setAllWaveInputsToNumChannels (1);
    dm.dispatchPendingUpdates();
    applyInputDefaults (engine, false);
}

void configureNewInputs (te::Engine& engine)
{
    applyInputDefaults (engine, true);
}

juce::String applyPreferredSampleRate (te::Engine& engine)
{
    auto& adm = engine.getDeviceManager().deviceManager;

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        if (juce::approximatelyEqual (dev->getCurrentSampleRate(), preferredSampleRate))
            return {};

        if (dev->getAvailableSampleRates().contains (preferredSampleRate))
        {
            auto setup = adm.getAudioDeviceSetup();
            setup.sampleRate = preferredSampleRate;
            auto err = adm.setAudioDeviceSetup (setup, true);
            engine.getDeviceManager().dispatchPendingUpdates();
            return err;
        }

        return "Device does not offer " + juce::String (preferredSampleRate, 0) + " Hz; using "
                + juce::String (dev->getCurrentSampleRate(), 0) + " Hz";
    }

    return "No audio device is open";
}

juce::String applyBufferSize (te::Engine& engine, int bufferSize)
{
    auto& adm = engine.getDeviceManager().deviceManager;

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        if (! dev->getAvailableBufferSizes().contains (bufferSize))
            return "Device does not offer a buffer of " + juce::String (bufferSize) + " frames";

        auto setup = adm.getAudioDeviceSetup();
        setup.bufferSize = bufferSize;
        auto err = adm.setAudioDeviceSetup (setup, true);
        engine.getDeviceManager().dispatchPendingUpdates();
        return err;
    }

    return "No audio device is open";
}

AudioSettingsSummary getSummary (te::Engine& engine)
{
    AudioSettingsSummary s;
    auto& dm = engine.getDeviceManager();
    auto& adm = dm.deviceManager;

    if (auto* type = adm.getCurrentDeviceTypeObject())
        s.deviceTypeName = type->getTypeName();

    if (auto* dev = adm.getCurrentAudioDevice())
    {
        s.deviceOpen = true;
        auto setup = adm.getAudioDeviceSetup();
        s.inputDeviceName = setup.inputDeviceName;
        s.outputDeviceName = setup.outputDeviceName;
        s.sampleRate = dev->getCurrentSampleRate();
        s.bufferSizeSamples = dev->getCurrentBufferSizeSamples();
        s.bufferDurationMs = s.sampleRate > 0 ? 1000.0 * s.bufferSizeSamples / s.sampleRate : 0.0;
        s.driverInputLatencySamples = dev->getInputLatencyInSamples();
        s.driverOutputLatencySamples = dev->getOutputLatencyInSamples();
        s.driverRoundTripMs = s.sampleRate > 0 ? 1000.0 * (s.driverInputLatencySamples + s.driverOutputLatencySamples) / s.sampleRate : 0.0;
        s.numActiveInputChannels = dev->getActiveInputChannels().countNumberOfSetBits();
        s.numActiveOutputChannels = dev->getActiveOutputChannels().countNumberOfSetBits();
    }
    else
    {
        s.lastError = adm.getAudioDeviceSetup().inputDeviceName.isEmpty() && adm.getAudioDeviceSetup().outputDeviceName.isEmpty()
                        ? juce::String ("no device selected") : juce::String ("device failed to open");
    }

    s.engineRecordAdjustmentMs = dm.getRecordAdjustmentMs();

    for (auto* wi : dm.getWaveInputDevices())
        if (wi != nullptr && wi->isEnabled())
        {
            s.enabledInputNames.add (wi->getName());
            s.recordBitDepth = wi->getBitDepth();
        }

    return s;
}

std::vector<te::WaveInputDevice*> getEnabledInputs (te::Engine& engine)
{
    std::vector<te::WaveInputDevice*> result;

    for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
        if (wi != nullptr && wi->isEnabled())
            result.push_back (wi);

    return result;
}

void setInputChannelCount (te::Engine& engine, te::WaveInputDevice& device, int numChannels)
{
    auto& dm = engine.getDeviceManager();
    dm.setDeviceNumChannels (device, (uint32_t) juce::jlimit (1, 2, numChannels));
    dm.dispatchPendingUpdates();
}

juce::String getInputPermissionGuidance()
{
    return "macOS asks for microphone permission the first time OpenSession opens an input. "
           "If you declined, or the input meter stays at silence while you play, open "
           "System Settings > Privacy & Security > Microphone and enable OpenSession, then "
           "reopen the audio device from Audio > Audio Settings.";
}

juce::String getMonitoringGuidance()
{
    return "Monitoring is OFF for every input until you turn it on (track button 'MON'). "
           "Choose one of two ways to hear yourself:\n"
           "  Direct monitoring: use your interface's own mix/monitor knob. Zero added latency; "
           "keep OpenSession monitoring OFF so you don't hear a delayed double.\n"
           "  Software monitoring: set MON to AUTO (hear input only while the track is armed) or ON. "
           "You hear the input through the track's fader and effects, delayed by the round-trip latency "
           "shown in Audio Settings. Recordings stay dry either way.";
}

} // namespace DeviceSetup
} // namespace opensession
