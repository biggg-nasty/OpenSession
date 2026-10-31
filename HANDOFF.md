# OpenSession — handoff

_Last updated 2026-10-31, session 2 (session 1 notes below still apply)._ Everything below was done on one machine: MacBook Pro (Apple Silicon, arm64), macOS 27.0.1, Command Line Tools only (no Xcode.app), built-in microphone + speakers, no external audio interface._

## 1. Original goal

Build an original, open-source, recording-first multitrack DAW ("OpenSession") for the Autonomous SG 2026 / NUS Hackers track, following `docs/brief/DAW_Claude_Code_Brief.md` (copy of the brief we were given). Core workflow to deliver and prove:

select audio-interface inputs → arm tracks → record → overdub → audition takes → edit non-destructively → mix → save/reopen → export stereo mix and aligned stems.

Constraints from the brief: native desktop app on JUCE + Tracktion Engine (pinned, licence-compatible), linear timeline, preserve original recordings/takes/offsets, session saving and recovery as core, real recording first (no loop launching or generative features), honest documentation separating documented / observed / inferred / product-chosen behaviour, measured limits instead of claims. Rubric: status-quo cost 20 %, reach 15 %, coolness of the build 25 %, cost reduction 25 %, open licence 15 %.

## 2. What is done

### Code (all original, GPL-3.0-or-later; `Source/`)
| Area | File(s) | Status |
|---|---|---|
| Engine wrapper (behaviours, recording-file placement in `<session>/Audio`, headless mode for tests, MIDI rescan disabled) | `Engine/OpenSessionEngine.*` | done |
| Device setup: mono inputs, 24-bit WAV, 48 kHz preference, buffer selection, actual-settings summary, permission + monitoring guidance | `Engine/DeviceSetup.*` | done |
| Recording controller: one input per track, arm, OFF/AUTO/ON monitoring, write-ability pre-check, "nothing captured" detection, take policy (overlapping clips = take group, newest active, exactly one unmuted), in-progress take protection via hard link + WAV header repair | `Engine/Recording.*`, `Session/WavRepair.*` | done |
| Loopback latency calibrator (measured minus driver-reported = residual; no double compensation) | `Engine/LatencyCalibration.*` | done, **not yet successfully measured on hardware** |
| Session document: folder format, atomic save with previous snapshot, autosave + recovery, collect-and-save, import (copies into Audio/), missing-file and orphan/incomplete-take detection and recovery, schema version | `Session/SessionDocument.*` | done |
| Exporter: stereo mix + per-track stems, same range (0 → last clip end + tail; tail per session, default 2 s), 24-bit, no normalisation, clipping reported | `Session/Exporter.*` | done |
| Synthetic demo/soak content (Karplus-Strong plucks, sine bass, noise drums, formant tone; all ours) | `Session/DemoContent.*` | done |
| UI: transport (play/stop/rec/to-start, clock + bars, tempo, click, count-in, snap, take lanes), track headers (name, input chooser, R, MON, M, S, input meter, output meter), timeline (ruler, grid, zoom/scroll, playhead, recording overlay, take lanes), clips (real waveform, trim/move/fades/gain, context menu, double-click = activate take), mixer (fader, pan, M/S, EQ/compressor/reverb inserts with generic parameter editor, master), audio settings panel, status bar, menus + shortcuts, recovery/missing-file prompts | `UI/*`, `App/*` | done |
| Command line: `--run-tests`, `--export-mix`, `--export-stems`, `--print-audio-settings`, `--smoke-record`, `--measure-latency`, `--make-demo`, `--soak` | `App/CommandLine.*`, `Main.cpp` | done |
| Self-test suite (doctest, hosted audio device = synthetic input, deterministic) | `Tests/*` | **34 cases / 526 assertions passing** |

### Verified on this machine
- **Automated** (`OpenSession --run-tests`): known-signal capture to 48 kHz/24-bit mono WAV with exact frame count and sample alignment; two simultaneous inputs → independent files with channel isolation; playback/click/monitoring absent from dry capture; fader/pan/EQ do not alter recordings; stereo input pair → 2-channel file; repeated passes become takes with exactly one active; monitoring independent of arming; unwritable Audio folder → reported failure, no clip; split/trim/move/duplicate/delete/fades/gain/undo with source offsets preserved; snapping optional; tempo change does not move audio; mix and stems (format, range, gain/mute/solo/pan, EQ bypass transparency, stems sum to mix, leading silence kept, clipping flagged, muted track → silent stem); save/reopen after moving the folder; atomic save + previous snapshot; corrupt file → clear error / snapshot fallback; read-only folder → save fails, audio intact; autosave recovery; collect-and-save; import; missing-file detection; orphan + header-repair recovery; abort keeps nothing.
- **Hardware** (`--smoke-record`, built-in mic): live input level shown before recording (−3.8 to −13 dBFS), 6-second take recorded to `Audio/Smoke_input_<timestamp>_01.wav`, 48 kHz / 1 ch / 24-bit, 283 853 frames, clip placed at 0 s, session saved, reopened in the GUI with waveform drawn from the samples, played back independently with `afplay`. Device facts: CoreAudio, 512-frame buffer = 10.67 ms, driver-reported latency 2033 + 1346 frames = 70.40 ms round trip, which the engine applies as record adjustment.
- **GUI**: launched on the recorded session and screenshotted (`logs/gui1.png` locally; not committed). Renders transport, headers, waveform, mixer, status bar.

### Documents
- `docs/recording-behavior.md` — reference behaviour (Logic Pro 12.3, Pro Tools 2026.4, Live 12, REAPER 7.84 manuals, accessed 2026-10-31) vs our implementation vs verification, with evidence status per row.
- `docs/cost-and-competition.md` — dated prices, reach figures, licensing, gaps.
- `docs/research/*` — the raw research drafts the two docs were built from.
- `LICENSE` (GPL-3.0), `HANDOFF.md` (this file).

## 3. What is left

Ordered by priority.

1. ~~Remaining docs~~ done: `docs/architecture.md`, `docs/test-results.md`, `README.md`, `THIRD_PARTY_NOTICES.md`, `docs/demo-script.md`, `docs/hardware-procedures.md`, `demo/README.md` (demo session is regenerated with `--make-demo`, not stored).
2. ~~Soak result~~ done: 3 min, 8 playback tracks + live input, CPU avg 8.2 % / max 16.0 %, 0 engine warnings, take kept to the end (`docs/test-results.md`).
3. ~~Release build~~ done: `build-release/OpenSession_artefacts/Release/OpenSession.app` builds cleanly; it has not yet been exercised on hardware (all hardware runs used Debug).
4. **Hardware checks that need gear we do not have** (document as unverified until done): two simultaneous *hardware* inputs (needs ≥2-input interface), headphone monitoring with the team's setup, loopback latency measurement with a cable (the acoustic speaker→mic attempt did not detect the test signal), overdub alignment measurement after compensation, sustained multi-input session, musician sessions (two performances, take selection, edit, export; collect timings/errors/feedback).
5. ~~Publish~~ done: public at https://github.com/biggg-nasty/OpenSession (`origin`, default branch `main`); session 2 pushed with a plain `git push origin main`, `tools/publish.sh` is only needed for a fresh repo. Session-1 commits use a placeholder identity ("OpenSession contributors"); from session 2 commits use the local git identity. `tools/bootstrap.sh` handles the SSH→HTTPS submodule rewrite for cloners.
6. **Nice-to-have after the core is solid**: ~~expose `clickTrackRecordingOnly` and the export tail length in the UI~~ done in session 2 (Transport > Click only while recording; File > Export tail after last clip; both stored in the session, CLI export honours the tail); range-based comping and punch in/out; edit groups; plugin hosting (needs latency reporting, state persistence, missing-plugin behaviour before it can be claimed); automation; MIDI.

## Session 2 log (2026-10-31)

- Added `CLAUDE.md` (agent guide: pointers, build, gotchas, conventions).
- Removed the stray committed file `"The directory\n…doesn't have write-access"` and traced it to the engine bug in §5.
- Pushed to the existing public remote (handoff item 5).
- Exposed click-only-while-recording and the export tail (handoff item 6, first part); new session test; suite now 34 / 526, all passing. Not yet checked by hand in the GUI.

## 4. How to build and test

Prerequisites (macOS): Xcode Command Line Tools, Homebrew `cmake` (≥3.22; 4.4.5 used) and `ninja`.

```bash
git clone https://github.com/biggg-nasty/OpenSession.git OpenSession && cd OpenSession
# the engine pins JUCE through an SSH URL; rewrite it to HTTPS while fetching
git -c url.https://github.com/.insteadOf=git@github.com: submodule update --init --recursive --depth 1

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j            # ~30 s wall on an M-series Mac for a full build

APP=build/OpenSession_artefacts/Debug/OpenSession.app/Contents/MacOS/OpenSession

$APP --run-tests                  # deterministic suite, exit code 0 = all passed (~25 s)
$APP --run-tests -tc="*Recording*"   # doctest filters pass through
$APP --print-audio-settings       # what the device really reports
$APP --smoke-record scratch/Smoke.osession 6      # real mic -> WAV -> session (needs mic permission)
$APP --measure-latency            # loopback cable or speaker->mic
$APP --make-demo demo/DemoSession.osession 48     # synthetic fallback session (~48 s of material)
$APP --soak scratch/Soak.osession 3 8             # 3 min, 8 playback tracks + live input
$APP --export-mix demo/DemoSession.osession /tmp/mix.wav
$APP --export-stems demo/DemoSession.osession /tmp/stems
open build/OpenSession_artefacts/Debug/OpenSession.app   # GUI; or: $APP path/to/Song.osession
```

Pinned revisions: `third_party/tracktion_engine` = develop `a5e44996f5f08dfa6af5f5d02d900fb3cfa37422` (VERSION.md 3.5.0, 2026-10-24), its JUCE submodule `7ff644f65d7daa1428c34b9738ebb5c449dc9172` (9.0.5). C++20, macOS deployment target 12.0.

Session format: `Song.osession/` folder with `session.xml` (Tracktion Edit ValueTree + our `OPENSESSION` node with `schemaVersion`), `manifest.json`, `Audio/` (all recordings, relative paths), `Backups/session.previous.xml`, `Backups/session.autosave.xml`, `Audio/Incomplete/` (preserved aborted takes).

Logs: `~/Library/Logs/OpenSession/opensession.log` (also stderr). Engine settings: `~/Library/OpenSession/Settings.xml`.

## 5. Known problems and caveats

- **Engine returns an error message as a file path** (upstream bug in `getDestinationRecordingFile`, `tracktion_WaveInputDevice.cpp`: missing `tl::unexpected`). With a read-only Audio folder the engine would write a WAV named after the error text in the current directory; session 1 accidentally committed one (removed in session 2). `RecordingController::checkRecordingDestination` prevents it; keep that check ahead of every `record()`. Worth reporting upstream.

- **Engine MIDI rescan aborts takes** (upstream Tracktion Engine behaviour): the default 4-second MIDI hardware rescan rebuilt the Edit's input devices mid-recording and the engine silently deleted the partial file. Fixed in `OpenSessionEngine` by setting the scan interval to 0 (rescan only on demand) and by hard-linking in-progress files into `Audio/Incomplete/` with header repair. Hot-plugging MIDI while recording is therefore not detected until Audio Settings is opened. Worth reporting upstream.
- **Engine stop listener fires twice** per stop (`recordingStopped` from both `performStop` and `performStopRecording`); `RecordingController` de-duplicates with a flag.
- **Relative path styles**: the engine's `SourceFileReference` may write `../Audio/x.wav` (relative to the `session.xml` node) while we write `Audio/x.wav`; `SessionDocument::makeResolver` accepts both. Do not change the resolver without re-running the session tests.
- **Take activation mutes whole clips**: partially overlapping takes are muted entirely, including their non-overlapping part. Whole-take selection only; no range comping.
- **Acoustic loopback did not detect the engine's spike pattern** through speakers→mic on this Mac at the current system volume; a cable loopback on a real interface is the intended path. Driver-reported latency is still applied automatically.
- **Mic input clipped** in the first hardware take (room noise + mic gain at 0 dBFS); the app reports it, does not hide it. Lower input gain in macOS Sound settings / on the interface.
- **Debug build only so far**; CPU figures in the status bar are from Debug.
- **One input per track**; stereo = a 2-channel input pair assigned to one track (Audio Settings → "Inputs 1+2 as stereo pair").
- **macOS microphone permission** cannot be queried from JUCE; the app explains what to do if the input meter stays silent. Running from a terminal attributes the permission to the terminal app.
- **Status quo**: no musician has used the app yet; hypotheses in `docs/cost-and-competition.md` are untested.
- Tests create a hosted audio device per test case; they must run on the message thread (`--run-tests` does this). They will not run concurrently with another instance using the same engine settings file safely, so run one instance at a time.
