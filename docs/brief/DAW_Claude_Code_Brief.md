# Claude Code brief: build a recording-first multitrack DAW

Act as a senior desktop-audio engineer and product engineer. Implement a working, original, open-source DAW in this repository. Continue through implementation and verification.

## Product intent

Build **OpenSession** (working title): a recording-first DAW for amateur through professional musicians recording instruments, vocals, and other live sources through microphones and audio interfaces.

The essential workflow is:
1. Select the audio interface and input channels.
2. Create/arm tracks, check levels, and select monitoring.
3. Record a performance against a click or existing tracks.
4. Overdub additional performances with correct timing.
5. Audition takes and edit non-destructively.
6. Mix, save a portable session, and export a stereo mix or aligned stems.

Make this complete workflow the MVP. Reduce advanced features while retaining actual recording. The first demonstrated session is a musician recording guitar and vocal overdubs. This is a starting scenario, not a restriction to beginners.

Initial validation target: at least eight playback tracks, two independently mapped simultaneous recording inputs, mono/stereo tracks, and sessions lasting several minutes. These are tested capacities, not a fixed eight-slot layout. Avoid artificial song-length or bar-count restrictions.

Use a linear recording timeline as the main workspace. Recording is essential from the first milestone. Defer clip launching, scene grids, generated composition, sample packs, and loop-based arrangement features.

Run locally without an account, cloud audio processing, or paid inference API.

We are entering Autonomous SG 2026's NUS Hackers track. The rubric weights existing cost 20%, reach 15%, working build 25%, cost reduction 25%, and open-source licensing 15%. Prepare a working recording demonstration and evidence for our claims.

## Architecture and implementation strategy

Inspect repository instructions, existing work, toolchains, and the actual development host first. Preserve unrelated changes.

Default to a native desktop application using C++, JUCE, and an established sequenced-audio engine such as Tracktion Engine. Compile a minimal recording/playback example before substantial UI work. Pin compatible engine/JUCE revisions and use their documented CMake setup, submodules, and compiler standard.

Reuse engine capabilities for device management, audio files, recording, transport, rendering, and undo where suitable. Explain which components we reuse and what we implement.

Target the available team/demo desktop platform first. On macOS, use Core Audio. Elsewhere, test an available supported native backend. List tested operating systems and devices explicitly. Do not claim cross-platform or ASIO support merely because a framework supports it.

Do not silently substitute a browser-only architecture. If this repository is a web app, document a practical native-engine integration or migration path and proceed with a minimal recording slice.

Check exact pinned dependency licenses. JUCE currently offers AGPLv3/commercial licensing; Tracktion Engine has separate GPL/commercial licensing. Select a compatible open-source distribution arrangement and preserve notices/source obligations. Do not mark the entire application MIT by default. Avoid unnecessary proprietary add-ons.

## Behavioral reverse engineering

Study observable recording workflows in Ableton Arrangement View, Logic Pro, Pro Tools, and REAPER, using official manuals and tests in copies we are entitled to use. Write original code and interface styling. Use our own project format and team-owned/test audio; do not copy proprietary code, assets, or branding.

Create docs/recording-behavior.md containing reference application/version, source/access date, user action, expected output, evidence status, our implementation, and verification.

Investigate:
- Device/channel selection, mono/stereo mapping, track arming, and simultaneous recording.
- Monitoring while stopped, playing, recording, and overdubbing.
- Dry recording while hearing monitor effects.
- Count-in/click routing and placement of recordings relative to existing audio.
- Preserving and auditioning takes.
- Non-destructive edits, undo, project portability, and mix/stem export.

Distinguish documented, directly observed, inferred, and product-chosen behavior. If originals are unavailable, state that and use documentation; do not claim direct testing or knowledge of proprietary internals.

Keep discovery focused and proceed to Gate A.

## Gate A: hardware input to durable recording

Implement:
- Device, channel, supported sample-rate, and buffer-size selection.
- OS input-permission handling.
- A mono track with selectable input, arm, record, stop, and an actual input meter.
- Hardware input captured to a valid WAV in the project audio directory.
- Dry source preservation independent of playback fader/effects.
- Recorded-file playback and a waveform derived from its samples.
- Visible recording/write failure handling.

Prefer 24-bit WAV recording, a 48 kHz default where supported, and floating-point processing. Expose the actual settings. Prove capture, finalization, and playback before building an elaborate editor.

## Gate B: multitrack tracking and overdubbing

Add:
- Named mono/stereo tracks with independent input mapping.
- At least two separately recorded simultaneous inputs.
- Existing-track playback while recording a new track.
- Play/stop/record/seek, recording status, tempo, click, and one-bar count-in.
- Per-track input monitoring independent of record enable.
- Explicit stopped/playback/record monitoring semantics.
- Monitoring off for initial setup, with concise software/direct-monitoring guidance.
- Separate input capture, monitoring, playback, and master routing.

Keep the internal click and playback mix out of raw recordings unless explicitly selected as recording sources.

Use the engine's audio timeline for recording placement. Apply driver/engine recording-latency compensation consistently, without double compensation. Provide a named recording-offset control or calibration where appropriate.

Distinguish buffer duration, driver-estimated latency, and measured round-trip latency. A 128-frame buffer at 48 kHz lasts about 2.67 ms; this is not total round-trip latency.

Prepare a physical loopback calibration test. Report measured latency and residual alignment error when hardware is available. Do not promise zero latency or universal sample-perfect hardware alignment.

## Gate C: editing and takes

Implement a linear timeline with real waveforms, time/musical rulers, zoom, scroll, selection, and playhead.

Support split, trim, move, duplicate, delete, clip gain, fades, and undo/redo. Preserve source offsets and original audio through edits/undo. Make snapping optional; preserve performance timing unless explicitly edited.

Store separate takes on subsequent passes and allow whole-take audition/selection. Define overlap playback behavior so alternate takes do not accidentally double.

Whole-take selection is sufficient initially. Range-based comping, punch recording, and edit groups follow after the core works. Do not force performances into quantized loops or silently stretch/move audio when tempo changes.

## Gate D: mixing, persistence, and delivery

Add gain, pan, mute, solo, accurate meters, and a stereo master. Implement a small built-in EQ/compressor chain, with reverb if time allows. Provide neutral/bypass settings and consistent playback/export processing.

Export stereo WAV and per-track stems aligned to a common timeline start. Preserve the chosen sample rate/bit depth, duration, offsets, and a documented effect-tail policy. Report clipping; do not silently normalize or claim mastering.

Introduce persistence early and extend it throughout implementation:
- Session-local recordings, relative asset paths, and stable IDs.
- Track setup, takes, clip offsets, edits, tempo, routing, mix/effect state, and schema version.
- Atomic metadata saves with a recoverable previous snapshot.
- Audio preservation when saving metadata fails.
- Collect/copy operation including imported assets.
- Session reopening from another directory without original asset locations.
- Clear missing-device, missing-file, corrupt-project, and write-error handling.

Deleting a timeline region must not erase its source recording.

## Real-time requirements and expansion

Separate audio/recording, session commands/model, storage, rendering, and UI.

Keep disk I/O, allocation, blocking locks, file encoding, UI work, logging, and network operations out of real-time audio callbacks. Use established background writers and bounded queues. Detect recording-buffer overflow/disk failure; preserve recoverable audio and mark incomplete takes.

Stream/cache long sessions appropriately. Synchronize edits safely. GUI timers must not determine recording timestamps. Repeated stop/start or device changes must not duplicate callbacks or leave dangling writers.

Handle mono/stereo mapping, sample-rate conversion, disconnection, and output changes coherently. A playback fader/monitor effect must not inadvertently change dry recorded audio. Match live and export routing semantics.

Design extension points for third-party plugins, buses, automation, MIDI recording, and virtual instruments. Defer these until core recording/editing/saving/export passes. Plugin hosting requires latency reporting, state persistence, and missing-plugin behavior before being claimed.

Professional musicians are part of the intended audience. Report delivered capabilities and measured limits instead of an unqualified professional-grade claim.

## Verification

Use deterministic/audio-data checks:
- Known-signal capture: frame count, channels, WAV headers, finite sample values.
- Two distinct input signals: independent files and channel isolation.
- Overdub routing: playback/click absent from dry capture through internal routing.
- Dry source unchanged when playback faders/monitor effects change.
- Split/trim/move source-offset math, fades, undo, and alternate-take selection.
- Gain/mute/solo/pan/effects applied to the correct signal path and export.
- Stereo mix/stem duration and shared start, including leading silence.
- Save/reopen and collect/copy/reopen preserving session/assets.
- Malformed files, failed writes, unavailable settings, repeated recording cycles, and interrupted recording recovery.

Hardware checks:
- Record a real instrument/vocal through an available interface.
- Overdub a second performance and evaluate alignment.
- Record two simultaneous inputs.
- Verify monitoring/input routing using the team's normal headphone setup.
- Run sustained multitrack recording/playback; report OS, interface, rate, buffer, track count, duration, and dropouts.
- Distinguish measured loopback latency from alignment after compensation.

Build and run relevant tests; report commands/results. If hardware is unavailable, provide exact manual procedures and label those checks unverified. Never fabricate listening, compatibility, latency, or recording results.

## Deliverables, product evidence, and demo

Provide:
- Working native app, build/run instructions, and original demo/test session.
- docs/recording-behavior.md linking reference observations to implementations/tests.
- docs/architecture.md explaining reused engine versus original work.
- docs/test-results.md distinguishing automated and hardware results.
- docs/cost-and-competition.md with dated, comparable prices and current alternatives.
- Compatible explicit open-source license, dependency/asset notices, and repository publication steps.

Include relevant competition: REAPER currently lists US$60 discounted licensing for eligible users and US$225 commercial licensing. Verify current price/eligibility. Ardour already offers recording/editing/mixing with an open-source model. Include these alongside Logic, Ableton, Pro Tools, and Waveform. Zero license cost alone is not a unique product.

Test hypotheses such as clearer input setup, faster recording/take workflows, straightforward portability, and predictable cost. Have musicians record two performances, select a take, edit, and export; collect actual completion/errors/feedback.

Demo: select interface -> arm guitar -> record to click -> overdub vocal/instrument -> audition a take -> edit -> adjust mix -> save/reopen -> export/play WAV independently -> show our contribution, behavior evidence, measured limits, cost comparison, and license.

Keep an honestly identified prerecorded fallback session available. Only distribute recordings the team has rights to publish.

At completion, report delivered behavior, run/build commands, verification results, limitations, and the next priority.

## Primary references

- Ableton recording: https://www.ableton.com/en/manual/recording-new-clips/
- Logic guide: https://help.apple.com/logicpro/
- Pro Tools recording: https://www.avid.com/pro-tools/features/audio-recording
- REAPER features: https://www.reaper.fm/about.php
- REAPER price/eligibility: https://www.reaper.fm/purchase.php
- Ardour features: https://ardour.org/features.html
- Tracktion Engine overview: https://www.tracktion.com/develop/tracktion-engine
- Engine source/examples/license: https://github.com/Tracktion/tracktion_engine
- Engine features: https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md
- JUCE source/license: https://github.com/juce-framework/JUCE
- JUCE device management: https://juce.com/tutorials/tutorial_audio_device_manager/
- JUCE recording example: https://github.com/juce-framework/JUCE/blob/master/examples/Audio/AudioRecordingDemo.h
- JUCE background writer: https://docs.juce.com/master/classjuce_1_1AudioFormatWriter_1_1ThreadedWriter.html

Begin with repository inspection and Gate A: real input recorded to a durable WAV and played back.

