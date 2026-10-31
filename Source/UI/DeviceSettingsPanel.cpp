// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeviceSettingsPanel.h"
#include "../Engine/DeviceSetup.h"
#include "Theme.h"

namespace opensession
{

DeviceSettingsPanel::DeviceSettingsPanel (te::Engine& e)
    : engine (e),
      selector (e.getDeviceManager().deviceManager, 0, 64, 0, 64, false, false, false, false),
      calibrator (e)
{
    addAndMakeVisible (selector);
    addAndMakeVisible (summary);
    addAndMakeVisible (guidance);
    addAndMakeVisible (monoButton);
    addAndMakeVisible (stereoButton);
    addAndMakeVisible (measureButton);
    addAndMakeVisible (applyButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (adjustLabel);
    addAndMakeVisible (adjustSlider);
    addAndMakeVisible (measureResult);

    for (auto* ed : { &summary, &guidance })
    {
        ed->setMultiLine (true, true);
        ed->setReadOnly (true);
        ed->setCaretVisible (false);
        ed->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
    }

    guidance.setFont (juce::Font (juce::FontOptions (12.0f)));
    guidance.setText (DeviceSetup::getMonitoringGuidance() + "\n\n" + DeviceSetup::getInputPermissionGuidance()
                      + "\n\nLatency terms: buffer duration = one block of audio (e.g. 128 frames at 48 kHz = 2.67 ms). "
                        "Driver-reported latency = what the driver claims for input + output; the engine already shifts recordings by this. "
                        "Measured round trip = what a loopback cable actually shows; the difference is the residual you can apply below.");

    monoButton.onClick = [this]
    {
        engine.getDeviceManager().setAllWaveInputsToNumChannels (1);
        engine.getDeviceManager().dispatchPendingUpdates();
        refreshSummary();
    };

    stereoButton.onClick = [this]
    {
        auto inputs = engine.getDeviceManager().getWaveInputDevices();

        if (! inputs.empty() && inputs[0] != nullptr)
            DeviceSetup::setInputChannelCount (engine, *inputs[0], 2);

        refreshSummary();
    };

    adjustSlider.setRange (-100.0, 100.0, 0.1);
    adjustSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    adjustSlider.setTextValueSuffix (" ms");
    adjustSlider.setValue (LatencyCalibrator::getCurrentManualAdjustmentMs (engine), juce::dontSendNotification);
    adjustSlider.setTooltip ("Positive values move recordings earlier (compensate extra latency). Applied on top of the driver-reported figure, so leave at 0 unless a loopback test shows a residual.");
    adjustSlider.onValueChange = [this]
    {
        for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
            if (wi != nullptr)
                wi->setRecordAdjustmentMs (adjustSlider.getValue());
    };

    measureResult.setJustificationType (juce::Justification::topLeft);
    measureResult.setFont (juce::Font (juce::FontOptions (12.0f)));
    measureResult.setText ("Connect an output to an input with a cable (or point the mic at the speaker), then measure.", juce::dontSendNotification);

    measureButton.onClick = [this]
    {
        measureButton.setEnabled (false);
        measureResult.setText ("Measuring... (plays a short click pattern)", juce::dontSendNotification);
        calibrator.start ([this] (LatencyCalibrator::Result r)
        {
            lastMeasurement = r;
            measureResult.setText (r.describe(), juce::dontSendNotification);
            measureResult.setColour (juce::Label::textColourId, r.success ? theme::text : theme::record);
            measureButton.setEnabled (true);
            applyButton.setEnabled (r.success);
            juce::Logger::writeToLog ("[latency] " + r.describe().replace ("\n", " | "));
        });
    };

    applyButton.setEnabled (false);
    applyButton.onClick = [this]
    {
        if (lastMeasurement && lastMeasurement->success)
        {
            LatencyCalibrator::applyResidual (engine, *lastMeasurement);
            adjustSlider.setValue (lastMeasurement->residualMs, juce::dontSendNotification);
            refreshSummary();
        }
    };

    clearButton.onClick = [this]
    {
        LatencyCalibrator::clearAdjustment (engine);
        adjustSlider.setValue (0.0, juce::dontSendNotification);
        refreshSummary();
    };

    engine.getDeviceManager().deviceManager.addChangeListener (this);
    refreshSummary();
    startTimer (1000);
    setSize (980, 720);
}

DeviceSettingsPanel::~DeviceSettingsPanel()
{
    engine.getDeviceManager().deviceManager.removeChangeListener (this);
}

void DeviceSettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Device changed: give any new input our recording defaults.
    DeviceSetup::configureNewInputs (engine);
    refreshSummary();
}

void DeviceSettingsPanel::refreshSummary()
{
    auto s = DeviceSetup::getSummary (engine);
    s.engineRecordAdjustmentMs = engine.getDeviceManager().getRecordAdjustmentMs();
    juce::String text = s.toMultiLineString();
    text << "Manual record adjustment (all inputs): " << juce::String (LatencyCalibrator::getCurrentManualAdjustmentMs (engine), 2) << " ms\n";
    summary.setText (text, false);
}

void DeviceSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void DeviceSettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto left = r.removeFromLeft (r.getWidth() / 2 - 5);
    r.removeFromLeft (10);

    selector.setBounds (left);

    summary.setBounds (r.removeFromTop (190));
    r.removeFromTop (8);

    auto row = r.removeFromTop (26);
    monoButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    stereoButton.setBounds (row);
    r.removeFromTop (12);

    adjustLabel.setBounds (r.removeFromTop (20));
    adjustSlider.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);
    measureButton.setBounds (r.removeFromTop (26));
    r.removeFromTop (4);
    row = r.removeFromTop (26);
    applyButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    clearButton.setBounds (row);
    r.removeFromTop (4);
    measureResult.setBounds (r.removeFromTop (76));
    r.removeFromTop (8);
    guidance.setBounds (r);
}

void DeviceSettingsPanel::show (te::Engine& engine)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new DeviceSettingsPanel (engine));
    o.dialogTitle = "Audio Settings";
    o.dialogBackgroundColour = theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

} // namespace opensession
