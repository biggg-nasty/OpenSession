# Hardware verification procedures (manual)

These are the checks the brief asks for that could not be run on the development machine (only a built-in 1-channel microphone was available). Each has a pass criterion. Record results in `docs/test-results.md` with device, OS, sample rate, buffer size and date.

## A. Two simultaneous hardware inputs
1. Connect a ≥2-input interface. Audio > Audio Settings: pick it as input and output, 48 kHz, 128 or 256 frames. Confirm "Engine inputs" lists Input 1, Input 2 (mono each).
2. New session. Track 1 input = Input 1, Track 2 input = Input 2. Arm both. Plug a guitar into 1 and a mic into 2.
3. Check both input meters move independently (mute the guitar: meter 1 stops, meter 2 does not).
4. Rec, play both for 20 s, Stop. Expect two clips, two files in `Audio/`, same length ±1 block. Solo each track and listen: guitar only / voice only.
**Pass:** two files, no bleed when solo'd beyond acoustic spill.

## B. Loopback latency measurement and overdub alignment
1. Cable: interface output 1 → interface input 1 (line level, low gain). Audio Settings > "Measure round-trip latency (loopback)".
2. Record the three numbers shown: buffer ms, driver-reported ms, measured ms, residual ms. Click "Apply measured residual".
3. Alignment check: on Track 1 import a click/drum WAV (or record a few sharp hits). Arm Track 2 (input 1, still cabled). Rec from 0, let it run 10 s, Stop.
4. Zoom in on a transient in both tracks; the engine also gives exact numbers: export stems and compute the lag between stem 1 and stem 2 (any audio editor or `python -c` cross-correlation).
**Report:** measured round-trip, driver-reported, residual applied, and the remaining alignment error in samples and ms. Do not describe it as zero unless it measured zero.

## C. Monitoring with headphones
1. Headphones on the interface. Track 1 input = mic, MON = OFF, not armed: expect silence from the app (use the interface's direct monitor knob to hear yourself).
2. MON = AUTO: silence until R is pressed; armed → input audible through the app (delayed by round-trip latency).
3. MON = ON: audible regardless of R. Press Play: input still audible over playback. Press Rec: still audible; the recording stays dry (verify by inserting a reverb on the track while recording, then playing the clip with the reverb bypassed: no reverb in the file).
**Pass:** each state behaves as labelled; recorded file has no effects.

## D. Sustained session
Run `OpenSession --soak scratch/Soak.osession 10 8` with the interface (10 min, 8 tracks). Then in the GUI: open the saved session, add 2 more tracks with real inputs, record a 5-minute overdub while all 8 tones play.
**Report:** OS, interface, rate, buffer, track count, duration, CPU avg/max from the soak output, any "can't keep up" or engine warning count, dropouts heard.

## E. Export and independent playback
File > Export stereo mix → play in QuickTime / `afplay`. File > Export stems → import the stems into any other DAW at 0:00 and confirm they line up with each other and with the mix.

## F. Musician test (hypotheses)
Two musicians each: select interface → arm guitar → record to click with count-in → overdub a vocal → audition takes → split/trim → mix → save, quit, reopen → export. Time each step, log every error message they hit and whether they recovered without help, collect three sentences of feedback. Hypotheses: clearer input setup, faster take workflow, straightforward portability (reopen on another Mac from the folder), predictable cost (zero).
