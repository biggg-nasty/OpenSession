// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "../Engine/LatencyCalibration.h"

namespace opensession
{

/** Audio Settings: device/channels/rate/buffer (JUCE selector), the actual numbers the device
    is running at, input layout (mono/stereo pairs), the manual record adjustment and the
    loopback latency measurement. */
class DeviceSettingsPanel : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    explicit DeviceSettingsPanel (te::Engine&);
    ~DeviceSettingsPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    static void show (te::Engine&);

private:
    te::Engine& engine;
    juce::AudioDeviceSelectorComponent selector;
    juce::TextEditor summary, guidance;
    juce::TextButton monoButton { "All inputs mono" }, stereoButton { "Inputs 1+2 as stereo pair" },
                     measureButton { "Measure round-trip latency (loopback)" }, applyButton { "Apply measured residual" },
                     clearButton { "Clear manual adjustment" };
    juce::Label adjustLabel { {}, "Manual record adjustment (ms)" }, measureResult;
    juce::Slider adjustSlider;
    LatencyCalibrator calibrator;
    std::optional<LatencyCalibrator::Result> lastMeasurement;

    void refreshSummary();
    void timerCallback() override                       { refreshSummary(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceSettingsPanel)
};

} // namespace opensession
