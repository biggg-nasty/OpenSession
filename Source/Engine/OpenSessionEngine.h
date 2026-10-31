// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace opensession
{

/** Where a user-facing message from the engine or the app should go. */
struct MessageSink
{
    virtual ~MessageSink() = default;
    virtual void postMessage (const juce::String& title, const juce::String& body, bool isError) = 0;
};

/**
    Owns the Tracktion Engine instance and the behaviours OpenSession installs into it.

    Reused from the engine: device management, the audio graph, recording to disk,
    transport, undo, rendering, built-in plugins.
    Original here: where recordings are written (session Audio/ folder with our naming),
    how engine messages reach the UI, and the headless mode used by the self-test suite.
*/
class OpenSessionEngine
{
public:
    /** @param headless  If true, the engine does not open system audio devices.
                         The self-test suite uses this together with the engine's
                         HostedAudioDeviceInterface so audio can be pushed in deterministically. */
    explicit OpenSessionEngine (bool headless);
    ~OpenSessionEngine();

    te::Engine& getEngine() noexcept                    { return *engine; }
    bool isHeadless() const noexcept                    { return headless; }

    /** The UI registers itself here to receive engine warnings (e.g. record failures). */
    void setMessageSink (MessageSink* sink);

    /** Returns the folder recordings for this Edit go into: <session>/Audio. Empty if the
        Edit has no file yet. */
    static juce::File getAudioDirectoryForEdit (te::Edit&);

    /** Builds the file a new recording for the given track should be written to.
        Pattern: Audio/<TrackName>_<yyyymmdd-HHMMSS>_<n>.<ext>, never overwriting. */
    static juce::File makeRecordingFile (te::Track&, const juce::String& extension);

    static juce::String sanitiseFileName (const juce::String&);

    /** Build/version information shown in the About box and written into session manifests. */
    static juce::String getBuildDescription();

private:
    struct Behaviour;
    struct UI;

    bool headless = false;
    std::unique_ptr<te::Engine> engine;
    MessageSink* messageSink = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenSessionEngine)
};

} // namespace opensession
