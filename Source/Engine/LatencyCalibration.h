// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/**
    Physical loopback measurement. Plays a spike pattern out of the current output and listens on
    the current input; the delay between them is the real round-trip latency. The engine already
    shifts recordings by the driver-reported latency, so the correction we store is
    (measured - driver reported), never the whole figure - no double compensation.

    Reused: the spike pattern and detector from the engine's test utilities (tracktion_RoundTripLatency.h).
    Original: the workflow, the reporting of the three different latency numbers, and applying the
    residual to every wave input.
*/
class LatencyCalibrator : private juce::Timer
{
public:
    struct Result
    {
        bool success = false;
        juce::String message;
        double sampleRate = 0;
        int bufferSize = 0;
        int measuredRoundTripSamples = 0;       // from loopback
        double measuredRoundTripMs = 0;
        int driverReportedSamples = 0;          // what the engine already compensates
        double driverReportedMs = 0;
        int residualSamples = 0;                // measured - driver reported
        double residualMs = 0;

        juce::String describe() const;
    };

    explicit LatencyCalibrator (te::Engine&);
    ~LatencyCalibrator() override;

    /** Starts a measurement; the callback fires on the message thread. Needs a cable (or acoustic path)
        from the output back into the input and the transport stopped. */
    void start (std::function<void (Result)> onFinished);
    bool isRunning() const                                  { return running; }
    void cancel();

    /** Writes the residual into every enabled wave input's manual record adjustment. */
    static void applyResidual (te::Engine&, const Result&);
    /** Clears manual adjustment on all wave inputs. */
    static void clearAdjustment (te::Engine&);
    static double getCurrentManualAdjustmentMs (te::Engine&);

private:
    te::Engine& engine;
    std::function<void (Result)> callback;
    bool running = false;
    int ticks = 0;
    struct Impl;
    std::unique_ptr<Impl> impl;

    void timerCallback() override;
    void finish (std::optional<int> offset);
};

} // namespace opensession
