// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DemoContent.h"
#include "../Engine/PluginHelpers.h"

namespace opensession::DemoContent
{

static double midiToHz (int note)   { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

// Karplus-Strong plucked string
static void addPluck (juce::AudioBuffer<float>& out, int startSample, double freq, double lengthSeconds, float amp, juce::Random& rng, double sr)
{
    const int period = std::max (2, (int) std::round (sr / freq));
    std::vector<float> ring ((size_t) period);
    for (auto& v : ring) v = rng.nextFloat() * 2.0f - 1.0f;

    const int n = std::min ((int) (lengthSeconds * sr), out.getNumSamples() - startSample);
    size_t idx = 0;
    auto* d = out.getWritePointer (0);

    for (int i = 0; i < n; ++i)
    {
        const size_t next = (idx + 1) % ring.size();
        const float v = ring[idx];
        ring[idx] = 0.996f * 0.5f * (v + ring[next]);
        idx = next;
        d[startSample + i] += amp * v * (float) std::min (1.0, (n - i) / (0.05 * sr));
    }
}

juce::AudioBuffer<float> pluckedPattern (double seconds, double bpm, double sr, int seed)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * sr));
    b.clear();
    juce::Random rng (seed);
    const double beat = 60.0 / bpm;
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };   // Am F C G

    for (double t = 0.0; t < seconds; t += beat / 2.0)
    {
        const int bar = (int) (t / (4.0 * beat)) % 4;
        const int step = (int) std::round (t / (beat / 2.0)) % 8;
        const int note = chords[bar][step % 3] + (step >= 4 ? 12 : 0);
        addPluck (b, (int) (t * sr), midiToHz (note), 1.2, 0.35f, rng, sr);
    }

    return b;
}

juce::AudioBuffer<float> bassLine (double seconds, double bpm, double sr)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * sr));
    b.clear();
    const double beat = 60.0 / bpm;
    const int roots[4] = { 45, 41, 36, 43 };
    auto* d = b.getWritePointer (0);

    for (double t = 0.0; t < seconds; t += beat)
    {
        const int bar = (int) (t / (4.0 * beat)) % 4;
        const double f = midiToHz (roots[bar]);
        const int start = (int) (t * sr), len = std::min ((int) (beat * 0.9 * sr), b.getNumSamples() - start);

        for (int i = 0; i < len; ++i)
        {
            const double ph = 2.0 * juce::MathConstants<double>::pi * f * i / sr;
            const float env = (float) std::exp (-3.0 * i / (double) len);
            d[start + i] += 0.3f * env * (float) (std::sin (ph) + 0.3 * std::sin (2.0 * ph));
        }
    }

    return b;
}

juce::AudioBuffer<float> drumPattern (double seconds, double bpm, double sr)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * sr));
    b.clear();
    juce::Random rng (7);
    const double beat = 60.0 / bpm;
    auto* d = b.getWritePointer (0);

    for (double t = 0.0; t < seconds; t += beat / 2.0)
    {
        const int step = (int) std::round (t / (beat / 2.0)) % 8;
        const int start = (int) (t * sr);

        if (step == 0 || step == 4)     // kick
            for (int i = 0; i < (int) (0.25 * sr) && start + i < b.getNumSamples(); ++i)
                d[start + i] += 0.6f * (float) std::exp (-12.0 * i / sr) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (55.0 + 80.0 * std::exp (-40.0 * i / sr)) * i / sr);

        if (step == 2 || step == 6)     // snare: noise burst
            for (int i = 0; i < (int) (0.18 * sr) && start + i < b.getNumSamples(); ++i)
                d[start + i] += 0.35f * (float) std::exp (-18.0 * i / sr) * (rng.nextFloat() * 2.0f - 1.0f);

        for (int i = 0; i < (int) (0.04 * sr) && start + i < b.getNumSamples(); ++i)   // hat
            d[start + i] += 0.12f * (float) std::exp (-90.0 * i / sr) * (rng.nextFloat() * 2.0f - 1.0f);
    }

    return b;
}

juce::AudioBuffer<float> formantVoice (double seconds, double bpm, double sr, int seed)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * sr));
    b.clear();
    juce::Random rng (seed);
    const double beat = 60.0 / bpm;
    const int melody[8] = { 69, 72, 71, 69, 67, 69, 72, 76 };
    auto* d = b.getWritePointer (0);

    for (double t = 2.0 * beat; t < seconds; t += beat * 2.0)
    {
        const int note = melody[((int) (t / (beat * 2.0))) % 8];
        const double f0 = midiToHz (note) * (1.0 + 0.004 * (rng.nextDouble() - 0.5));
        const int start = (int) (t * sr), len = std::min ((int) (beat * 1.7 * sr), b.getNumSamples() - start);
        const double formants[3] = { 700.0, 1200.0, 2600.0 };   // "ah"-ish

        for (int i = 0; i < len; ++i)
        {
            const double tt = i / sr;
            const double vib = 1.0 + 0.006 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.5 * tt);
            double s = 0.0;

            for (int h = 1; h <= 20; ++h)
            {
                const double fh = f0 * vib * h;
                double gain = 0.0;
                for (auto F : formants) gain += std::exp (-std::pow ((fh - F) / 180.0, 2.0));
                s += gain * std::sin (2.0 * juce::MathConstants<double>::pi * fh * tt) / h;
            }

            const float env = (float) std::min ({ 1.0, tt / 0.08, (len - i) / (0.15 * sr) });
            d[start + i] += 0.22f * env * (float) s;
        }
    }

    return b;
}

juce::AudioBuffer<float> steadyTone (double seconds, double freq, float amplitude, double sr)
{
    juce::AudioBuffer<float> b (1, (int) (seconds * sr));
    auto* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
        d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
    return b;
}

bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sampleRate, int bitDepth)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sampleRate, (unsigned) b.getNumChannels(), bitDepth, {}, 0));
    return w != nullptr && w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static te::WaveAudioClip::Ptr addClip (te::AudioTrack& track, const juce::File& file, double start, double length)
{
    auto clip = track.insertWaveClip (file.getFileNameWithoutExtension(), file,
                                      te::ClipPosition { te::TimeRange (te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (start + length)), {} }, false);
    if (clip != nullptr)
        clip->getSourceFileReference().source = file.getRelativePathFrom (file.getParentDirectory().getParentDirectory());
    return clip;
}

juce::Result makeDemoSession (te::Engine& engine, const juce::File& folder, double sr, double seconds)
{
    juce::String error;
    auto doc = SessionDocument::createNew (engine, folder, "DemoSession", error);

    if (doc == nullptr)
        return juce::Result::fail (error);

    auto& edit = doc->getEdit();
    const double bpm = 100.0;
    edit.tempoSequence.getTempo (0)->setBpm (bpm);
    edit.ensureNumberOfAudioTracks (4);
    auto tracks = te::getAudioTracks (edit);
    auto audio = doc->getAudioDirectory();

    struct Part { const char* name; juce::AudioBuffer<float> buffer; float faderDb; float pan; };
    Part parts[] = {
        { "Guitar (synth pluck)", pluckedPattern (seconds, bpm, sr, 1), -6.0f, -0.3f },
        { "Bass (synth)",         bassLine (seconds, bpm, sr),          -4.0f,  0.0f },
        { "Drums (synth)",        drumPattern (seconds, bpm, sr),       -5.0f,  0.0f },
        { "Vocal (synth formant)",formantVoice (seconds, bpm, sr, 3),   -3.0f,  0.2f },
    };

    for (int i = 0; i < 4; ++i)
    {
        auto f = audio.getChildFile (juce::String (parts[i].name).upToFirstOccurrenceOf (" ", false, false) + "_take1.wav");

        if (! writeWav (f, parts[i].buffer, sr))
            return juce::Result::fail ("Couldn't write " + f.getFileName());

        tracks[i]->setName (parts[i].name);
        tracks[i]->getVolumePlugin()->setVolumeDb (parts[i].faderDb);
        tracks[i]->getVolumePlugin()->setPan (parts[i].pan);
        addClip (*tracks[i], f, 0.0, seconds);
    }

    // An alternate, muted take on the guitar so the take lane has something to show.
    auto alt = audio.getChildFile ("Guitar_take2.wav");
    writeWav (alt, pluckedPattern (seconds, bpm, sr, 2), sr);
    if (auto c = addClip (*tracks[0], alt, 0.0, seconds))
        c->setMuted (true);

    // A gentle compressor on the vocal and reverb on the master so the mix page shows inserts.
    insertBuiltInPlugin<te::CompressorPlugin> (*tracks[3], 0);
    if (auto rev = insertBuiltInPlugin<te::ReverbPlugin> (edit, edit.getMasterPluginList(), 0))
        rev->wetParam->setParameter (0.12f, juce::sendNotification);

    edit.clickTrackEnabled = true;
    edit.setCountInMode (te::Edit::CountIn::oneBar);

    juce::File (folder.getChildFile ("README.txt")).replaceWithText (
        "OpenSession demo session (synthetic).\n"
        "All audio here was generated by OpenSession itself (Karplus-Strong plucks, sine bass, noise drums, formant tone).\n"
        "It contains no recordings of people and no third-party material; it is the honest fallback used when live\n"
        "recording is not possible during a demo. Licence: GPL-3.0-or-later, same as the application.\n");

    return doc->save();
}

juce::Result makeSoakSession (te::Engine&, SessionDocument& doc, int numPlaybackTracks, double seconds, double sr)
{
    auto& edit = doc.getEdit();
    edit.ensureNumberOfAudioTracks (numPlaybackTracks + 1);
    auto tracks = te::getAudioTracks (edit);
    auto audio = doc.getAudioDirectory();

    for (int i = 0; i < numPlaybackTracks; ++i)
    {
        const double freq = 110.0 * std::pow (2.0, i / 4.0);
        auto f = audio.getChildFile ("tone_" + juce::String (i + 1) + ".wav");

        if (! writeWav (f, steadyTone (seconds, freq, 0.08f, sr), sr))
            return juce::Result::fail ("Couldn't write " + f.getFileName());

        tracks[i]->setName ("Tone " + juce::String (i + 1) + " (" + juce::String (freq, 0) + " Hz)");
        tracks[i]->getVolumePlugin()->setPan ((i % 2 == 0) ? -0.5f : 0.5f);
        addClip (*tracks[i], f, 0.0, seconds);
    }

    tracks[numPlaybackTracks]->setName ("Live input");
    return juce::Result::ok();
}

} // namespace opensession::DemoContent
