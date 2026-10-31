# OpenSession

Recording-first multitrack DAW, C++20 on JUCE + Tracktion Engine (pinned submodule in `third_party/`). GPL-3.0-or-later.

- **State of the work, what is left, known problems**: `HANDOFF.md`. Read it at the start of a session; update it at the end.
- **Layering and what is ours vs the engine's**: `docs/architecture.md`.
- **Claims about recording behaviour** go in `docs/recording-behavior.md` with an evidence status (documented / observed / inferred / product-chosen). Measured results go in `docs/test-results.md`. Never claim something is verified on hardware unless it was run on hardware.

## Build and test

```bash
tools/bootstrap.sh            # submodules over HTTPS (engine pins JUCE via SSH) + configure build/
cmake --build build -j
build/OpenSession_artefacts/Debug/OpenSession.app/Contents/MacOS/OpenSession --run-tests   # exit 0 = pass
```

The test suite is compiled into the app; doctest filters pass through (`-tc="*Recording*"`). Run one instance at a time: tests share the engine settings file. New source files must be added to `OPENSESSION_SOURCES` in `CMakeLists.txt`.

## Gotchas

- **Never modify the submodule.** Work around engine behaviour in `Source/Engine/*` and record the workaround in `HANDOFF.md` §5.
- Engine stop listener fires twice per stop; `RecordingController` de-duplicates. Keep that guard.
- MIDI rescan interval is 0 on purpose: the engine's periodic rescan aborted takes mid-recording.
- The session path resolver accepts both `Audio/x.wav` and `../Audio/x.wav`; re-run the session tests after touching it.
- Engine `getDestinationRecordingFile` returns its error message *as a `juce::File`* (missing `tl::unexpected`), so a read-only Audio folder makes the engine write a WAV named after the error text in the current directory. `RecordingController::checkRecordingDestination` must keep running before every `record()`.
- Recordings and sessions are user data: source WAVs are never modified, saves are atomic, aborted takes are preserved in `Audio/Incomplete/`.

## Conventions

- Every source file starts with the three-line GPL header used in existing files; code lives in `namespace opensession`, engine aliased as `te`.
- Allman braces, space before call parentheses (`foo (x)`), JUCE style.
- Commits: `Area: summary` (e.g. `Recording: …`, `UI: …`, `Docs: …`).
