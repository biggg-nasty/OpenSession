# OpenSession

A recording-first, open-source multitrack DAW for musicians who record live instruments and vocals. Native macOS app (C++20) built on [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) and [JUCE](https://github.com/juce-framework/JUCE). GPL-3.0-or-later.

Record from your audio interface, overdub, audition takes, edit non-destructively, mix with built-in or VST3 / Audio Unit effects, and export a stereo mix and aligned stems. Original recordings are never modified.

![OpenSession after a live take](docs/images/screenshot-smoke-take.png)

## Setup

Requires macOS with Xcode Command Line Tools, CMake ≥ 3.22 and Ninja (`brew install cmake ninja`).

```bash
git clone https://github.com/biggg-nasty/OpenSession.git && cd OpenSession
tools/bootstrap.sh          # fetches the pinned engine + JUCE, configures build/
cmake --build build -j
```

## Open the demo session

The demo session is synthesised by the app, so generate it once, then open it:

```bash
APP=build/OpenSession_artefacts/Debug/OpenSession.app/Contents/MacOS/OpenSession
$APP --make-demo demo/DemoSession.osession
$APP demo/DemoSession.osession
```

Once generated, it can also be opened from the app with **File > Open demo session**.

## More

- `HANDOFF.md`: current state, known problems, all command-line modes.
- `docs/architecture.md`: what comes from the engine and what is ours.
- `docs/test-results.md`: test and hardware results (run the suite with `$APP --run-tests`).

## Licence

GNU GPL v3 or later. Combines Tracktion Engine (GPLv3) and JUCE (AGPLv3). See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
