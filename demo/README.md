# Demo session

The fallback demo session is **generated, not stored**, to keep the repository small (33 MB of WAV):

```bash
tools/demo-prep.sh      # builds Release, runs the tests, regenerates this session, checks export
# or just the session:
build-release/OpenSession_artefacts/Release/OpenSession.app/Contents/MacOS/OpenSession --make-demo demo/DemoSession.osession 48
```

The app's File > Open demo session finds `demo/DemoSession.osession` by walking up from the app bundle, so it works from either build folder. `tools/demo-prep.sh` deletes and regenerates the session, discarding anything recorded into it while rehearsing.

It is deterministic (seeded synthesis): four tracks of material the app synthesises itself (Karplus-Strong plucked strings, sine bass, noise drums, a formant "vocal" tone), a muted alternate guitar take, a compressor on the vocal and a reverb on the master, 100 bpm, click and count-in on. It contains no recordings of people and no third-party material. Say so when you use it in a demo; see `docs/demo-script.md`.

## Live demo session (stored)

`demo/LiveDemo.osession` is a stored session (about 32 MB) holding the synthesised tracks plus one **live take recorded with a real microphone**, so you can open it and hear/edit an actual recording without an audio interface. That take is a recording of its author's voice, shared under the repository's licence. Open it with File > Open and pick the folder. `tools/demo-prep.sh` never touches it.
