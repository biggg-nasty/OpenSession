// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LatencyCalibration.h"
#include <tracktion_engine/testing/tracktion_RoundTripLatency.h>

namespace opensession
{

struct LatencyCalibrator::Impl
{
    Impl (juce::AudioDeviceManager& adm, std::function<void (std::optional<int>)> cb)
        : tester (adm, std::move (cb)) {}

    te::test_utilities::LatencyTester tester;
};

juce::String LatencyCalibrator::Result::describe() const
{
    if (! success)
        return "Measurement failed: " + message;

    juce::String s;
    s << "Measured round trip: " << measuredRoundTripSamples << " frames = " << juce::String (measuredRoundTripMs, 2) << " ms"
      << " at " << juce::String (sampleRate, 0) << " Hz, buffer " << bufferSize << " frames\n"
      << "Driver-reported (already compensated by the engine): " << driverReportedSamples << " frames = "
      << juce::String (driverReportedMs, 2) << " ms\n"
      << "Residual to apply as manual record adjustment: " << residualSamples << " frames = "
      << juce::String (residualMs, 2) << " ms";
    return s;
}

LatencyCalibrator::LatencyCalibrator (te::Engine& e) : engine (e) {}

LatencyCalibrator::~LatencyCalibrator()
{
    cancel();
}

void LatencyCalibrator::start (std::function<void (Result)> onFinished)
{
    cancel();
    callback = std::move (onFinished);

    auto& adm = engine.getDeviceManager().deviceManager;

    if (adm.getCurrentAudioDevice() == nullptr)
    {
        Result r;
        r.message = "No audio device is open";

        if (callback)
            callback (r);

        return;
    }

    te::TransportControl::stopAllTransports (engine, false, false);

    impl = std::make_unique<Impl> (adm, [this] (std::optional<int> offset)
    {
        juce::MessageManager::callAsync ([this, offset] { finish (offset); });
    });

    running = true;
    ticks = 0;
    impl->tester.beginTest();
    startTimer (250);
}

void LatencyCalibrator::cancel()
{
    stopTimer();
    running = false;
    impl.reset();
}

void LatencyCalibrator::timerCallback()
{
    if (impl == nullptr)
        return;

    if (++ticks > 40)   // 10 s safety stop
    {
        auto cb = callback;
        cancel();
        Result r;
        r.message = "Timed out waiting for the loopback signal. Is the output connected to the input?";

        if (cb)
            cb (r);
    }
}

void LatencyCalibrator::finish (std::optional<int> offset)
{
    if (! running)
        return;

    stopTimer();
    running = false;
    impl.reset();

    auto& dm = engine.getDeviceManager();
    Result r;
    r.sampleRate = dm.getSampleRate();
    r.bufferSize = dm.getBlockSize();
    r.driverReportedSamples = dm.getRecordAdjustmentSamples();
    r.driverReportedMs = r.sampleRate > 0 ? 1000.0 * r.driverReportedSamples / r.sampleRate : 0.0;

    if (! offset.has_value() || *offset < 0)
    {
        r.message = "The test signal was not detected on the input. Connect the output to the input (or hold the mic near the speaker at a clear level) and try again.";
    }
    else
    {
        r.success = true;
        r.measuredRoundTripSamples = *offset;
        r.measuredRoundTripMs = r.sampleRate > 0 ? 1000.0 * r.measuredRoundTripSamples / r.sampleRate : 0.0;
        r.residualSamples = r.measuredRoundTripSamples - r.driverReportedSamples;
        r.residualMs = r.sampleRate > 0 ? 1000.0 * r.residualSamples / r.sampleRate : 0.0;
    }

    if (callback)
        callback (r);
}

void LatencyCalibrator::applyResidual (te::Engine& engine, const Result& r)
{
    for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
        if (wi != nullptr)
            wi->setRecordAdjustmentMs (r.residualMs);
}

void LatencyCalibrator::clearAdjustment (te::Engine& engine)
{
    for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
        if (wi != nullptr)
            wi->setRecordAdjustmentMs (0.0);
}

double LatencyCalibrator::getCurrentManualAdjustmentMs (te::Engine& engine)
{
    for (auto* wi : engine.getDeviceManager().getWaveInputDevices())
        if (wi != nullptr)
            return wi->getRecordAdjustmentMs();

    return 0.0;
}

} // namespace opensession
