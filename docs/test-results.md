# Test results

_Run on 2026-10-31. Machine: MacBook Pro (arm64), macOS 27.0.1, Apple clang 21, Debug build. Audio: CoreAudio, MacBook Pro Microphone (1 input channel) + MacBook Pro Speakers, 48 000 Hz, 512-frame buffer (10.67 ms). No external audio interface was attached. Nothing below is simulated or estimated unless the row says so._

## 1. Automated, deterministic (`OpenSession --run-tests`)

The suite runs the real engine with its `HostedAudioDeviceInterface`: we push synthetic input buffers through the actual recording pipeline, files are written to a temporary session folder, and results are read back from disk. Log: `logs/tests5.log` (local).

```
[doctest] test cases:  35 |  35 passed | 0 failed | 0 skipped
[doctest] assertions: 535 | 535 passed | 0 failed |
[doctest] Status: SUCCESS!
```

| Brief check | Test case(s) | Result |
|---|---|---|
| Known-signal capture: frame count, channels, WAV header, finite samples | Recording: known signal is captured to a valid 48 kHz / 24-bit mono WAV in the session's Audio folder | pass; max sample error < 0.001 (24-bit quantisation), lag 0 samples, BWAV time reference = 0 |
| Two distinct input signals → independent files, channel isolation | Recording: two simultaneous inputs land in independent files with channel isolation | pass; residual vs own signal < −80 dBFS, vs the other signal > −20 dBFS |
| Overdub routing: playback/click absent from dry capture | Recording: existing-track playback, click and software monitoring never reach the dry recording | pass; recorded RMS < −90 dBFS while output RMS > −30 dBFS |
| Dry source unchanged when fader / monitor effects change | Recording: fader, pan and insert effects do not alter the dry recording | pass (−20 dB fader, hard-left pan, +12 dB EQ) |
| Stereo (two-channel) input mapping | Recording: a stereo input pair records a two-channel file with correct channel order | pass |
| Takes: separate files per pass, whole-take selection, no doubling | Recording: repeated passes over the same range become takes…; Editing: take groups… | pass; 4 passes → 4 files, exactly 1 unmuted |
| Monitoring semantics independent of arm | Recording: monitoring mode is explicit and independent of arming | pass (OFF default; AUTO audible only when armed; ON always) |
| Arm/record refusals with clear messages | Recording: arming needs an input… | pass |
| Write failure reported, no clip | Recording: an unwritable Audio folder produces a reported failure and no clip | pass |
| Abort keeps nothing, reports nothing | Recording: aborting a take mid-way… | pass |
| Split / trim / move source-offset math | Editing: split…; Editing: trimming the start… | pass |
| Fades, gain, duplicate, delete, undo/redo | Editing: fades, clip gain, duplicate and delete are non-destructive and undoable | pass; source file untouched, deleted clip keeps its file |
| Snap optional; tempo change does not move audio | Editing: snapping is optional…; Editing: changing the tempo moves the grid, not the recorded audio | pass |
| Gain/mute/solo/pan/effects applied to the right path and export | Mix/export: track gain, mute, solo and pan…; Mix/export: built-in EQ… | pass; −6 dB fader → −6 dB peak, hard-left → right channel < −80 dBFS, bypassed EQ identical to none |
| Stereo mix/stem duration and shared start incl. leading silence | Mix/export: stereo mix is 48 kHz / 24-bit…; Mix/export: stems share one timeline start… | pass; both stems 6.000 s, 2.9 s leading silence < −90 dBFS, stems sum to mix within −70 dBFS |
| Clipping reported, not normalised | Mix/export: clipping is measured and reported… | pass |
| Muted track → silent stem (complete stem set) | Mix/export: a muted track still produces a (silent) stem | pass |
| Save/reopen preserves session/assets; relative paths; moved folder | Session: save writes session.xml + manifest + relative audio refs… | pass (names, fader, pan, mute, compressor insert, tempo, clip position/fades/gain) |
| Atomic save + previous snapshot | Session: saves are atomic and keep the previous snapshot | pass |
| Malformed file handling | Session: a corrupt session.xml is reported… | pass (snapshot fallback, clear errors, newer-schema refusal) |
| Failed write leaves session and audio intact | Session: a failed metadata save… | pass |
| Autosave recovery | Session: autosave is written separately… | pass |
| Collect/copy/reopen | Session: collect-and-save copies external audio… | pass (original deleted afterwards, copy stands alone) |
| Import copies into the session | Session: importing an audio file… | pass |
| Missing file detection | Session: missing audio is detected and the session still opens | pass |
| Interrupted-recording recovery (unfinalised header) | Session: a WAV whose header was never finalised… ; Session: an orphaned recording… | pass; header repaired, audio intact within −80 dBFS, placed at BWAV time |
| Export tail and click-while-recording-only persist with the session | Session: export tail and click-while-recording-only are stored in the session and survive reopening | pass (5 s tail and flag restored; tail clamped to 0..30 s) |
| Device changes do not loop the device list or stop playback | Devices: re-applying input defaults on device changes settles and leaves playback running | pass (≤ 4 notifications after a provoked change, transport still playing and advancing); with the old behaviour restored it fails: 3106 notifications, transport stopped at 0 s |
| Deleting a region never erases the source | Session: deleting a clip from the timeline never deletes its recording | pass |

## 2. Hardware (built-in microphone, this Mac)

| Check | Command / method | Result |
|---|---|---|
| Record a real input through the available interface | `OpenSession --smoke-record scratch/Smoke6.osession 6` | **pass**. Live level before recording −3.8 dBFS. File `Smoke_input_20261031-132803_01.wav`: 48 000 Hz, 1 ch, 24-bit, 283 853 frames = 5.914 s, clip at 0.000 s. Peak 0.0 dBFS (room/mic gain hot; reported as CLIPPED by the tool). Session saved and reopened. |
| Play the recorded WAV independently | `afinfo` + `afplay -t 3 <file>` | **pass**: valid WAVE, lpcm 24-bit, 5.9136 s, played back |
| Waveform derived from samples, GUI | GUI launched on `Smoke6.osession`, screenshot `logs/gui1.png` (local) | **pass**: waveform, headers, mixer, status bar render; input meter live |
| Sustained multitrack playback + recording | `OpenSession --soak scratch/Soak1.osession 3 8` | **pass**: 8 playback tracks (steady tones) + 1 live input, 180 s requested, transport stayed in record to the end, CPU avg 8.2 % / max 16.0 % (engine measure, Debug build), 0 engine warnings ("can't keep up" never logged), output clip flag no, recorded 181.210 s |
| Device facts reported | `--print-audio-settings` | CoreAudio; buffer 512 frames = 10.67 ms; driver-reported latency in 2033 + out 1346 frames = 70.40 ms round trip; engine record adjustment 70.40 ms (driver-derived, applied automatically) |
| Loopback latency measurement | `OpenSession --measure-latency` (acoustic: speakers → mic, no cable available) | **not verified**: "test signal was not detected on the input". A cable loopback on a real interface is the intended procedure (see `docs/hardware-procedures.md`). |
| Two simultaneous *hardware* inputs | — | **not verified**: only a 1-channel input device is attached. Verified in the deterministic suite only. |
| Overdub alignment on hardware (residual error after compensation) | — | **not verified** (needs loopback). The engine applies the driver-reported 70.40 ms; residual unknown. |
| Headphone/direct monitoring with the team's setup | — | **not verified** (no interface). Software monitoring semantics verified in the suite. |
| Export mix/stems of a hardware recording and play them | `--export-mix` on the smoke session | see section 3 |
| Musician sessions (two performances, take selection, edit, export; timings/errors/feedback) | — | **not run yet** |

### 2b. Release build on a second Mac (session 2, 2026-10-31)

Machine: MacBook Air (Mac15,12, arm64), macOS 26.6.2, Apple clang 17.0.0, CMake 4.4.5, Ninja 1.13.2. Built-in microphone and speakers, no interface. Run through `tools/demo-prep.sh`.

| Check | Command / method | Result |
|---|---|---|
| Self-test suite, Release build | `--run-tests` | **pass**: 35 / 35 cases, 535 / 535 assertions (after the device-rebuild fix below) |
| Record a real input, Release build | `--smoke-record <scratch>/Smoke.osession 6` | **pass**: `Smoke_input_20261031-141123_01.wav`, 48 000 Hz, 1 ch, 24-bit, 283 145 frames = 5.899 s, peak −4.2 dBFS, clip at 0 s, session saved; `afinfo` reads a valid 24-bit WAVE of 5.8989 s |
| Device facts reported | `--print-audio-settings` | CoreAudio; buffer 512 frames = 10.67 ms; driver-reported latency in 2993 + out 1606 frames = 95.81 ms round trip; engine record adjustment 95.81 ms (driver-derived) |
| Demo session generation and export | `--make-demo demo/DemoSession.osession 48`, `--export-mix`, `--export-stems` | **pass**: mix and 4 stems all 50.00 s (48 s + 2 s default tail), 2 ch; mix peak −1.4 dBFS, no clipping |
| GUI launch on the demo session, Release build | app launched with the session path, log inspected | **first run: fail** (not noticed at the time): the log showed ~27 000 device-list rebuilds in 8 s and Play did nothing (issue 6 below). **After the fix: pass**: 1 rebuild at startup, none in the following 14 s. Pressing Play and the menus were not exercised by hand (keystrokes cannot be sent to the app from the test shell). |

## 3. Reproducing

```bash
tools/demo-prep.sh                 # Release build + suite + demo session + export check + device report
APP=build/OpenSession_artefacts/Debug/OpenSession.app/Contents/MacOS/OpenSession
$APP --run-tests
$APP --print-audio-settings
$APP --smoke-record scratch/Smoke.osession 6
$APP --soak scratch/Soak.osession 3 8
$APP --export-mix scratch/Smoke.osession /tmp/smoke-mix.wav && afplay /tmp/smoke-mix.wav
```

## 4. Issues found by testing (and what changed)

1. **Take destroyed ~4 s into recording on the real device, not in the hosted tests.** Tracing showed the Edit's input-device list being rebuilt mid-take (`INPUTDEVICES` child added) by the engine's 4-second MIDI hardware rescan, after which the engine deleted the partial file without any UI warning. Fix: MIDI scan interval set to 0 in `OpenSessionEngine`; in-progress files are hard-linked into `Audio/Incomplete/` and header-repaired if the engine aborts; a "nothing captured" error is raised to the UI. Verified by the 6 s smoke take and the 180 s soak.
2. **Header-only leftover files** (`…_01.wav`, 784 bytes, 0 audio) next to the real take: the engine opens a recording file more than once while preparing. Fix: unreferenced zero-data WAVs created during the pass are removed at stop; orphan scans ignore zero-data files.
3. **Unsaved-change tracking** in the engine is asynchronous (undo-manager change message); we added synchronous ValueTree dirty tracking so autosave and "save changes?" prompts are reliable.
4. **Relative path style mismatch** (`../Audio/x.wav` vs `Audio/x.wav`) between the engine's helper and ours; the resolver accepts both.
5. **Error message written as a file name.** An early run of the unwritable-folder test left a WAV in the working directory named `The directory\n…\n doesn't have write-access`. Cause (upstream, `tracktion_WaveInputDevice.cpp`, `getDestinationRecordingFile`): the error branches `return` a `juce::String` from a function returning `tl::expected<juce::File, juce::String>` without `tl::unexpected`, so the message converts implicitly to a `juce::File` and is treated as success. Our `checkRecordingDestination` (writability probe before every `record()`) prevents the engine from reaching that path; the current suite does not recreate the file. Worth reporting upstream.
6. **Play did nothing in the GUI; the engine rebuilt its device list thousands of times a second.** `MainComponent` (and the Audio Settings panel) re-applied all input defaults on every device-manager change notification, and the engine's `DeviceManager::setDeviceEnabled` triggers a full device rebuild even when the input is already enabled, so each rebuild caused the next. Every rebuild reset playback. It affected only the GUI (command-line modes and tests never attach that listener), and the session-1 and early session-2 GUI checks missed it because they looked for errors, not for rebuilds. Fix: `setDeviceEnabled` only when an input is really disabled, and device-change callbacks use `DeviceSetup::configureNewInputs`, which configures only inputs not seen before (so it also no longer resets a user's MON choice or stereo pair after a rebuild). Regression test added; it fails with the old behaviour (3106 notifications, transport stopped).
