# Architecture: what we reuse, what we wrote

OpenSession is a native macOS desktop application in C++20 built on two pinned open-source frameworks. This document says exactly which parts come from them and which parts are ours, and how the layers are separated.

## Pinned dependencies

| Component | Revision | Licence we use it under | Role |
|---|---|---|---|
| Tracktion Engine | `third_party/tracktion_engine` @ develop `a5e44996f5f08dfa6af5f5d02d900fb3cfa37422` (VERSION.md 3.5.0, 2026-10-24) | GPL-3.0-or-later | Sequenced audio engine: Edit model, tracks, clips, transport, recording pipeline, graph, rendering, built-in plugins, undo |
| JUCE | engine submodule `modules/juce` @ `7ff644f65d7daa1428c34b9738ebb5c449dc9172` (9.0.5) | AGPL-3.0 | Audio device I/O (CoreAudio), WAV read/write, GUI toolkit, threading, files |
| doctest (bundled with the engine) | 2.4.11 | MIT | Self-test framework |

Combination: GPLv3 section 13 allows a GPLv3 work to be combined with an AGPLv3 work; the resulting binary must be distributed under both sets of terms. OpenSession's own code is GPL-3.0-or-later. See `THIRD_PARTY_NOTICES.md` for every bundled library.

## Layering

```
UI (Source/UI, Source/App)          JUCE components, menus, commands. Reads engine state, issues commands. No audio.
   │
Session commands / model            Source/Engine/Recording.*   Source/Session/SessionDocument.*  Source/Session/Exporter.*
   │                                (our rules on top of the engine: input mapping, monitoring semantics,
   │                                 take policy, session folder format, atomic saves, export policy)
   │
Engine (Tracktion Engine)           Edit / TransportControl / EditPlaybackContext / WaveInputDevice /
   │                                WaveInputRecordingThread / Renderer / PluginList
   │
Device layer (JUCE)                 AudioDeviceManager + CoreAudio (or the HostedAudioDeviceInterface in tests)
```

Real-time rules: all disk I/O for recordings happens on the engine's `WaveInputRecordingThread` through a bounded block queue (the engine logs "Audio recording can't keep up!" if it ever exceeds 500 pending blocks; we count those lines in the soak test). Our code never touches the audio callback. GUI timers only read state for display; recording timestamps come from the engine's sync points, not from GUI timers.

## Reused from the engine (not reimplemented)

| Capability | Engine API we call | Notes |
|---|---|---|
| Device enumeration, sample rate, buffer size | `te::DeviceManager`, `juce::AudioDeviceManager`, `juce::AudioDeviceSelectorComponent` | We force mono wave inputs by default and 24-bit WAV output |
| Input → track routing, arming | `InputDeviceInstance::setTarget / setRecordingEnabled` | One input per track is our rule (`moveToTrack = true`) |
| Monitoring | `InputDevice::MonitorMode { off, automatic, on }` | Exposed per track as OFF / AUTO / ON |
| Recording to disk, latency adjustment, clip creation | `TransportControl::record`, `WaveInputDeviceInstance`, BWAV time stamps | Driver-reported latency applied by the engine; our calibrator adds only the measured residual |
| Timeline model: tracks, clips, offsets, fades, gain, split, duplicate, undo | `AudioTrack`, `WaveAudioClip`, `ClipTrack::splitClip`, `te::duplicateClip`, `juce::UndoManager` | Every edit is a state change on the Edit ValueTree, so it is undoable and the source file is never modified |
| Mixing: volume, pan, mute, solo, meters | `VolumeAndPanPlugin`, `LevelMeterPlugin`, `Track::setMute/setSolo` | |
| Built-in effects | `EqualiserPlugin`, `CompressorPlugin`, `ReverbPlugin` | Inserted pre-fader; bypass via `Plugin::setEnabled` |
| Third-party plugin hosting | `PluginManager` (format manager, `KnownPluginList`, out-of-process `CustomScanner`), `ExternalPlugin`, `PluginWindowState`, `juce::PluginListComponent` | VST3 + Audio Unit compiled in (`JUCE_PLUGINHOST_VST3/AU=1`); the engine saves plugin state in the Edit, compensates reported latency and passes audio through a plugin it cannot load |
| Offline render | `te::Renderer::renderToFile` | Used for mix and stems with the same time range |
| Waveform display | `te::SmartThumbnail` | Thumbnails are generated from the actual file samples |
| Click, count-in, tempo | `Edit::clickTrackEnabled`, `Edit::setCountInMode`, `TempoSequence` | Audio clips are not remapped when tempo changes (`EngineBehaviour::areAudioClipsRemappedWhenTempoChanges = false`) |
| Headless deterministic testing | `HostedAudioDeviceInterface`, `test_utilities::EnginePlayer` | We push synthetic input buffers through the real recording pipeline |

## Original work (ours)

| Module | What it adds |
|---|---|
| `Engine/OpenSessionEngine` | Engine construction with our `EngineBehaviour` (recordings go to `<session>/Audio/<Track>_<timestamp>_<n>.wav`; headless mode; **MIDI rescan interval set to 0** because the engine's 4 s rescan rebuilt input devices mid-take and destroyed recordings) and `UIBehaviour` (engine warnings routed to the status bar; render jobs run while pumping the message loop) |
| `Engine/DeviceSetup` | Recording defaults (mono inputs, 24-bit, overlay merge mode, monitoring off, no record trigger), 48 kHz preference, a summary that separates buffer duration, driver-reported latency and record adjustment, permission and monitoring guidance text |
| `Engine/Recording` (`RecordingController`) | Input/track mapping API; explicit monitoring semantics; pre-flight checks (armed track, device open, Audio folder writable via a probe file); detection of passes that produced no clip; **take policy**: overlapping clips on one track form a take group, newest take active, exactly one unmuted; audition by activating a clip; **in-progress take protection**: hard link of the recording file into `Audio/Incomplete/`, header repair if the engine aborts and deletes its copy; removal of header-only leftovers |
| `Engine/LatencyCalibration` | Loopback workflow around the engine's spike-pattern tester; reports measured vs driver-reported vs residual; applies only the residual to every input |
| `Session/SessionDocument` | Session folder format (`session.xml`, `manifest.json`, `Audio/`, `Backups/`), schema version, relative paths with a resolver that accepts both `Audio/x` and `../Audio/x`, atomic save with previous-snapshot rotation and read-back check, autosave + recovery choice, collect-and-save, import-by-copy, missing-file list, orphan and incomplete-take recovery (BWAV time stamp → timeline position), synchronous dirty tracking |
| `Session/WavRepair` | RIFF/data size inspection and repair for WAVs whose header was never finalised; POSIX hard links |
| `Session/Exporter` | Export policy: range 0 → last clip end + tail, stems pre-master and stereo with identical start/length, no normalisation, peak measurement and clipping report |
| `Session/DemoContent` | Synthesised demo and soak material we own (Karplus-Strong plucks, sine bass, noise drums, formant tone, steady tones) |
| `Engine/PluginHosting` | Which formats are built, the AAX/RTAS/VST2 explanation, effects-only plugin list, pre-fader insert position (after existing inserts, before volume/pan and meter), missing/latency/bypass descriptions; the scan child process is started from `Main.cpp` and enabled by `EngineBehaviour::canScanPluginsOutOfProcess` |
| `UI/PluginEditors`, `UI/PluginWindow` | Knob look-and-feel (amp-style knobs with 0-10 scale and value arc), compressor editor with static transfer curve, reverb editor, parametric view of the engine's 4-band EQ (response computed from the same JUCE coefficient formulas the engine runs; draggable band nodes, wheel = Q), plugin window (plugin's own GUI, JUCE generic editor if it has none, or a missing-plugin notice) installed through `OpenSessionEngine::setPluginWindowFactory`, Plugins window (scan / list / format notes) |
| `UI/*` | All components: transport bar, track headers (input chooser, R, MON, M, S, input and output meters), timeline with ruler/grid/zoom/scroll/playhead/recording overlay/take lanes, clip component (waveform, trim, move, fades, gain, context menu), mixer strips with generic plugin parameter editor, audio settings panel with calibration, status bar, theme |
| `App/*`, `Main.cpp` | Menus, commands and shortcuts, session lifecycle with save prompts, export dialogs, recovery prompts, command-line modes (`--run-tests`, `--export-mix`, `--export-stems`, `--print-audio-settings`, `--smoke-record`, `--measure-latency`, `--make-demo`, `--soak`), file + stderr logging |
| `Tests/*` | 38 deterministic test cases (581 assertions) covering recording, editing, mix/export, session and plugin-hosting behaviour through the hosted device |

## Data flow for one take

1. User picks an input on a track (`RecordingController::assignInput` → `InputDeviceInstance::setTarget`), arms it (`setRecordingEnabled`), optionally sets MON.
2. `startRecording()` checks: armed tracks > 0, device open, Audio folder writable (probe file), then `TransportControl::record()`.
3. Engine: count-in (if enabled) → punch-in sync point → `WaveInputDeviceInstance::prepareToRecord` asks our `EngineBehaviour::getFileForNewAudioRecording` for the path → `AudioFileWriter` opened with BWAV time reference = punch-in sample → audio thread pushes blocks to `WaveInputRecordingThread`.
4. On `recordingStarted` we hard-link the in-progress file into `Audio/Incomplete/`.
5. Stop → engine finalises the file, adjusts for latency, creates a `WaveAudioClip` at the punch-in time → `recordingFinished`: we activate the new take and mute overlapping older takes, release the safety link, delete header-only leftovers.
6. Save → `SessionDocument::save()` writes `session.xml.tmp`, verifies it parses, rotates the previous file into `Backups/`, moves the temp into place, updates `manifest.json`.

## Extension points (deferred, not claimed)

- Plugin formats not hosted: **AAX** (Avid restricts AAX hosting to its own products; the public AAX SDK is for building plugins), **RTAS** (retired by Avid in 2013, 32-bit only), **VST2** (SDK no longer licensed, not GPL-distributable; `-DOPENSESSION_VST2_SDK_DIR` enables it for private builds). LV2 is available in JUCE but not enabled. Instruments are filtered out of the insert menu: tracks are audio-only.
- Buses/sends: engine `AuxSend/AuxReturn` plugins exist; no UI yet.
- Automation, MIDI recording, virtual instruments: engine-supported; out of MVP scope.
- Range comping / punch in-out: engine has `recordingPunchInOut` and comp managers; our take model is whole-take only for now.

## Tested platform

macOS 27.0.1 (arm64), CoreAudio, MacBook Pro built-in microphone and speakers (JUCE 9 combines them into a private aggregate device), Apple clang 21, CMake 4.4.5, Ninja 1.13.2. Session 2 also ran the Release build on macOS 26.6.2 (MacBook Air Mac15,12, built-in mic and speakers, Apple clang 17.0.0). No other OS, driver (ASIO, ALSA, WASAPI) or interface has been tested; the code paths exist in JUCE but we do not claim them.
