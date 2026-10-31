# Demo session

The fallback demo session is **generated, not stored**, to keep the repository small (33 MB of WAV):

```bash
build/OpenSession_artefacts/Debug/OpenSession.app/Contents/MacOS/OpenSession --make-demo demo/DemoSession.osession 48
```

It is deterministic (seeded synthesis): four tracks of material the app synthesises itself (Karplus-Strong plucked strings, sine bass, noise drums, a formant "vocal" tone), a muted alternate guitar take, a compressor on the vocal and a reverb on the master, 100 bpm, click and count-in on. It contains no recordings of people and no third-party material. Say so when you use it in a demo; see `docs/demo-script.md`.
