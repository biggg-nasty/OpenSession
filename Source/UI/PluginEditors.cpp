// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditors.h"
#include <complex>

namespace opensession
{

namespace
{
    const juce::Colour faceplateTop    { 0xff30333a };
    const juce::Colour faceplateBottom { 0xff1c1e22 };
    const juce::Colour pointerCream    { 0xfff2e8cf };
    const juce::Colour scaleColour     { 0xffb9bec6 };
    const juce::Colour compAccent      { 0xffffa53d };
    const juce::Colour reverbAccent    { 0xff39c0c7 };

    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8f97), c.x - 3, c.y - 3, juce::Colour (0xff3a3d42), c.x + 3, c.y + 3, false));
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        g.setColour (juce::Colour (0xff1a1b1e));
        g.drawLine (c.x - 2.5f, c.y + 2.5f, c.x + 2.5f, c.y - 2.5f, 1.2f);
    }
}

//==============================================================================
void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto area = bounds.withSizeKeepingCentre (size, size);
    const auto centre = area.getCentre();
    const float outer = size * 0.5f - 1.0f;
    const bool large = outer > 30.0f;
    const float arcRadius = outer - (large ? 9.0f : 3.0f);
    const float bodyRadius = arcRadius - (large ? 5.0f : 4.0f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;

    // Scale: 11 ticks (0..10, like an amp panel), numbers on the larger knobs.
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
        const float r1 = arcRadius + 2.0f, r2 = arcRadius + (i % 5 == 0 ? 6.0f : 4.0f);
        g.setColour (scaleColour.withAlpha (0.7f * alpha));
        g.drawLine ({ centre + dir * r1, centre + dir * r2 }, i % 5 == 0 ? 1.4f : 1.0f);

        if (large && i % 2 == 0)
        {
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            const auto p = centre + dir * (arcRadius + 12.0f);
            g.drawText (juce::String (i), juce::Rectangle<float> (14, 10).withCentre (p), juce::Justification::centred);
        }
    }

    // Value arc
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (large ? 3.0f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (track, stroke);
    g.setColour (fill.withAlpha (alpha));
    g.strokePath (value, stroke);

    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre.translated (0.0f, bodyRadius * 0.12f)));

    // Knurled skirt
    const auto skirt = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d53), skirt.getX(), skirt.getY(),
                                             juce::Colour (0xff121315), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse (skirt);

    const int ridges = large ? 40 : 28;

    for (int i = 0; i < ridges; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
        g.setColour ((i % 2 == 0 ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.35f)));
        g.drawLine ({ centre + dir * (bodyRadius * 0.80f), centre + dir * (bodyRadius - 0.5f) }, 1.0f);
    }

    // Cap with a top-left highlight
    const float capRadius = bodyRadius * 0.74f;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5c6068), cap.getX() + capRadius * 0.4f, cap.getY() + capRadius * 0.3f,
                                             juce::Colour (0xff17181b), cap.getRight(), cap.getBottom(), true));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);

    // Pointer
    const auto dir = juce::Point<float> (std::sin (angle), -std::cos (angle));
    g.setColour (pointerCream.withAlpha (alpha));
    g.drawLine ({ centre + dir * (capRadius * 0.25f), centre + dir * (bodyRadius - 1.5f) }, juce::jmax (2.0f, bodyRadius * 0.11f));
}

juce::Label* KnobLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = theme::LookAndFeel::createSliderTextBox (slider);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    l->setColour (juce::Label::textColourId, theme::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colour (0xff121316));
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff121316));
    return l;
}

//==============================================================================
void PowerSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (highlighted ? theme::panelLight.brighter (0.1f) : theme::panelLight);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const bool on = getToggleState();
    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
    const auto ledColour = on ? theme::play : juce::Colour (0xff3a3f49);

    if (on)
    {
        g.setColour (ledColour.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }

    g.setColour (ledColour);
    g.fillEllipse (led);
    g.setColour (on ? theme::text : theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText (on ? "ON" : "BYPASS", r.withTrimmedRight (4.0f), juce::Justification::centred);
}

//==============================================================================
ParameterKnob::ParameterKnob (te::AutomatableParameter& p, juce::Colour accent, const juce::String& titleText)
    : param (&p)
{
    addAndMakeVisible (slider);
    addAndMakeVisible (title);

    title.setText (titleText.isNotEmpty() ? titleText : p.getParameterName().toUpperCase(), juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, theme::textDim);
    title.setMinimumHorizontalScale (0.7f);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 17);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);

    const auto& range = p.valueRange;
    slider.setRange (range.start, range.end, 0.0);

    // Ranges spanning decades (frequency, linear threshold gain, attack) feel right on a log-ish taper.
    if (range.start > 0.0f && range.end / range.start >= 50.0f)
        slider.setSkewFactorFromMidPoint (std::sqrt ((double) range.start * range.end));
    else
        slider.setSkewFactor (range.skew);

    if (auto def = p.getDefaultValue())
        slider.setDoubleClickReturnValue (true, *def);

    slider.textFromValueFunction = [this] (double v) { return param->valueToString ((float) v); };
    slider.valueFromTextFunction = [this] (const juce::String& s) { return (double) param->stringToValue (s); };
    slider.onDragStart = [this] { param->parameterChangeGestureBegin(); };
    slider.onDragEnd = [this] { param->parameterChangeGestureEnd(); };
    slider.onValueChange = [this] { param->setParameter ((float) slider.getValue(), juce::sendNotification); };
    slider.setTooltip (p.getParameterName() + " - drag, or double-click to reset");
    refresh();
}

void ParameterKnob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (15));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (72, r.getWidth() - 4), 17);
    slider.setBounds (r);
}

void ParameterKnob::refresh()
{
    if (! slider.isMouseButtonDown() && std::abs (slider.getValue() - param->getCurrentValue()) > 1.0e-6)
        slider.setValue (param->getCurrentValue(), juce::dontSendNotification);

    slider.updateText();
}

//==============================================================================
BuiltInEditorBase::BuiltInEditorBase (te::Plugin& p, const juce::String& t, juce::Colour a)
    : plugin (&p), title (t), accent (a)
{
    setLookAndFeel (&*knobLookAndFeel);
    addAndMakeVisible (power);
    power.setTooltip ("Bypass: the insert stays in place with its settings, audio passes through untouched");
    power.setToggleState (plugin->isEnabled(), juce::dontSendNotification);
    power.onClick = [this] { plugin->setEnabled (power.getToggleState()); };
    startTimerHz (15);
}

BuiltInEditorBase::~BuiltInEditorBase()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BuiltInEditorBase::layoutHeader()
{
    power.setBounds (getWidth() - 14 - 84, 9, 84, 22);
}

void BuiltInEditorBase::refreshFromPlugin()
{
    power.setToggleState (plugin->isEnabled(), juce::dontSendNotification);

    for (auto* k : knobs)
    {
        k->refresh();
        k->slider.setEnabled (plugin->isEnabled());
    }
}

void BuiltInEditorBase::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (faceplateTop, 0.0f, 0.0f, faceplateBottom, 0.0f, r.getHeight(), false));
    g.fillAll();

    // Header strip with the accent colour, like a hardware unit's nameplate.
    auto header = r.removeFromTop ((float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (header);
    g.setColour (accent);
    g.fillRect (header.removeFromBottom (2.0f));

    g.setColour (theme::text);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText (title.toUpperCase(), header.withTrimmedLeft (34.0f).withTrimmedRight (110.0f), juce::Justification::centredLeft);

    drawScrew (g, { 16.0f, header.getCentreY() });
    drawScrew (g, { (float) getWidth() - 8.0f, (float) getHeight() - 8.0f });
    drawScrew (g, { 8.0f, (float) getHeight() - 8.0f });
}

//==============================================================================
KnobPluginEditor::KnobPluginEditor (te::Plugin& p)
    : BuiltInEditorBase (p, p.getName(), dynamic_cast<te::CompressorPlugin*> (&p) != nullptr ? compAccent
                                         : dynamic_cast<te::ReverbPlugin*> (&p) != nullptr ? reverbAccent : theme::accent)
{
    compressor = dynamic_cast<te::CompressorPlugin*> (&p);

    for (auto* param : p.getAutomatableParameters())
    {
        // The app has no sidechain routing, so the sidechain gain would do nothing.
        if (compressor != nullptr && param == compressor->sidechainDb.parameter.get())
            continue;

        // The reverb's freeze "mode" is an on/off control in practice.
        if (dynamic_cast<te::ReverbPlugin*> (&p) != nullptr && param->paramID == "mode")
        {
            freezeParam = param;
            continue;
        }

        addAndMakeVisible (knobs.add (new ParameterKnob (*param, accent)));
    }

    if (freezeParam != nullptr)
    {
        addAndMakeVisible (freezeButton);
        freezeButton.setTooltip ("Freeze: holds the reverb tail indefinitely");
        freezeButton.onClick = [this] { freezeParam->setParameter (freezeButton.getToggleState() ? 1.0f : 0.0f, juce::sendNotification); };
    }

    const int perRow = compressor != nullptr ? 3 : 4;
    const int rows = (knobs.size() + perRow - 1) / perRow;
    setSize ((compressor != nullptr ? 220 : 0) + perRow * 100 + 28,
             headerHeight + 20 + rows * 112 + (freezeParam != nullptr ? 30 : 0));
    refreshFromPlugin();
}

void KnobPluginEditor::refreshFromPlugin()
{
    BuiltInEditorBase::refreshFromPlugin();

    if (freezeParam != nullptr)
        freezeButton.setToggleState (freezeParam->getCurrentValue() >= 0.5f, juce::dontSendNotification);

    if (compressor != nullptr)
        repaint (curveArea);
}

void KnobPluginEditor::resized()
{
    layoutHeader();
    auto r = getContentArea();

    if (compressor != nullptr)
    {
        curveArea = r.removeFromLeft (200).withSizeKeepingCentre (200, juce::jmin (200, r.getHeight()));
        r.removeFromLeft (20);
    }

    if (freezeParam != nullptr)
        freezeButton.setBounds (r.removeFromBottom (26).withSizeKeepingCentre (100, 24));

    const int perRow = compressor != nullptr ? 3 : 4;
    const int rows = juce::jmax (1, (knobs.size() + perRow - 1) / perRow);
    const int rowHeight = r.getHeight() / rows;

    for (int i = 0; i < knobs.size(); ++i)
    {
        const int row = i / perRow, col = i % perRow;
        const int w = r.getWidth() / perRow;
        knobs[i]->setBounds (juce::Rectangle<int> (r.getX() + col * w, r.getY() + row * rowHeight, w, rowHeight).reduced (4, 2));
    }
}

void KnobPluginEditor::paint (juce::Graphics& g)
{
    BuiltInEditorBase::paint (g);

    if (compressor != nullptr)
        paintTransferCurve (g, curveArea.toFloat());
}

void KnobPluginEditor::paintTransferCurve (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff121316));
    g.fillRoundedRectangle (r, 4.0f);

    auto plot = r.reduced (8.0f);
    constexpr float minDb = -48.0f;
    auto toX = [&] (float db) { return plot.getX() + (db - minDb) / -minDb * plot.getWidth(); };
    auto toY = [&] (float db) { return plot.getBottom() - (db - minDb) / -minDb * plot.getHeight(); };

    g.setColour (theme::outline.withAlpha (0.6f));

    for (float db = minDb; db <= 0.0f; db += 12.0f)
    {
        g.drawHorizontalLine (juce::roundToInt (toY (db)), plot.getX(), plot.getRight());
        g.drawVerticalLine (juce::roundToInt (toX (db)), plot.getY(), plot.getBottom());
    }

    g.setColour (theme::textDim.withAlpha (0.35f));
    g.drawLine (toX (minDb), toY (minDb), toX (0.0f), toY (0.0f), 1.0f);

    // Static curve of the engine's compressor: above threshold, out = thr + (in - thr) * ratioParam (linear gain).
    const float thr = compressor->thresholdGain.getCurrentValue();
    const float rat = compressor->ratio.getCurrentValue();
    const float makeup = compressor->outputDb.getCurrentValue();
    juce::Path curve;

    for (int i = 0; i <= 96; ++i)
    {
        const float inDb = minDb + -minDb * (float) i / 96.0f;
        const float in = juce::Decibels::decibelsToGain (inDb);
        const float out = in > thr ? thr + (in - thr) * rat : in;
        const float outDb = juce::jlimit (minDb, 0.0f, juce::Decibels::gainToDecibels (out) + makeup);
        i == 0 ? curve.startNewSubPath (toX (inDb), toY (outDb)) : curve.lineTo (toX (inDb), toY (outDb));
    }

    const bool enabled = plugin->isEnabled();
    g.setColour (accent.withAlpha (enabled ? 1.0f : 0.35f));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    const float thrDb = juce::Decibels::gainToDecibels (thr);
    g.setColour (accent.withAlpha (0.4f));
    g.drawVerticalLine (juce::roundToInt (toX (thrDb)), plot.getY(), plot.getBottom());

    g.setColour (theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("IN / OUT (dB)", plot.removeFromTop (12.0f), juce::Justification::centredLeft);
}

//==============================================================================
ParametricEqEditor::ParametricEqEditor (te::EqualiserPlugin& e)
    : BuiltInEditorBase (e, "Parametric EQ", theme::accent), eq (e)
{
    bands = {
        { "LOW SHELF",  BandType::lowShelf,  eq.loFreq,   eq.loGain,   eq.loQ,   juce::Colour (0xffff6b6b) },
        { "MID 1",      BandType::peak,      eq.midFreq1, eq.midGain1, eq.midQ1, juce::Colour (0xffffc145) },
        { "MID 2",      BandType::peak,      eq.midFreq2, eq.midGain2, eq.midQ2, juce::Colour (0xff4cd18a) },
        { "HIGH SHELF", BandType::highShelf, eq.hiFreq,   eq.hiGain,   eq.hiQ,   juce::Colour (0xff5aa9ff) },
    };

    for (auto& b : bands)
    {
        addAndMakeVisible (knobs.add (new ParameterKnob (*b.freq, b.colour, "FREQ")));
        addAndMakeVisible (knobs.add (new ParameterKnob (*b.gain, b.colour, "GAIN")));
        addAndMakeVisible (knobs.add (new ParameterKnob (*b.q,    b.colour, "Q")));
    }

    const auto deviceRate = eq.edit.engine.getDeviceManager().getSampleRate();
    sampleRate = deviceRate > 0.0 ? deviceRate : 48000.0;

    setSize (720, 486);
    refreshFromPlugin();
}

void ParametricEqEditor::resized()
{
    layoutHeader();
    auto r = getContentArea();
    graphArea = r.removeFromTop (290);
    r.removeFromTop (8);

    auto knobRow = r.withTrimmedTop (18);
    const int bandWidth = knobRow.getWidth() / (int) bands.size();

    for (size_t b = 0; b < bands.size(); ++b)
    {
        auto col = knobRow.removeFromLeft (bandWidth).reduced (4, 0);
        const int w = col.getWidth() / 3;

        for (int k = 0; k < 3; ++k)
            knobs[(int) b * 3 + k]->setBounds (col.removeFromLeft (w));
    }
}

void ParametricEqEditor::refreshFromPlugin()
{
    BuiltInEditorBase::refreshFromPlugin();
    repaint (graphArea);
}

float ParametricEqEditor::freqToX (double f) const
{
    return (float) graphArea.getX() + (float) (std::log (f / minHz) / std::log (maxHz / minHz)) * (float) graphArea.getWidth();
}

double ParametricEqEditor::xToFreq (float x) const
{
    const double t = juce::jlimit (0.0, 1.0, (double) (x - (float) graphArea.getX()) / graphArea.getWidth());
    return minHz * std::pow (maxHz / minHz, t);
}

float ParametricEqEditor::gainToY (double db) const
{
    return (float) graphArea.getCentreY() - (float) (db / displayDb) * (float) graphArea.getHeight() * 0.5f;
}

double ParametricEqEditor::yToGain (float y) const
{
    return ((float) graphArea.getCentreY() - y) / ((float) graphArea.getHeight() * 0.5f) * displayDb;
}

juce::Point<float> ParametricEqEditor::nodePosition (const Band& b) const
{
    return { freqToX (b.freq->getCurrentValue()), gainToY (b.gain->getCurrentValue()) };
}

int ParametricEqEditor::bandAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDistance = 14.0f;

    for (size_t i = 0; i < bands.size(); ++i)
    {
        const float d = nodePosition (bands[i]).getDistanceFrom (p);

        if (d < bestDistance)
        {
            best = (int) i;
            bestDistance = d;
        }
    }

    return best;
}

double ParametricEqEditor::bandGainDb (BandType type, double sr, double freq, double q, double gainDb, double f)
{
    // The engine skips a band whose gain is exactly 0 dB; the filter would be unity anyway.
    if (gainDb == 0.0)
        return 0.0;

    const auto gain = std::pow (10.0, gainDb / 20.0);
    const auto c = type == BandType::lowShelf  ? juce::IIRCoefficients::makeLowShelf (sr, freq, q, (float) gain)
                 : type == BandType::highShelf ? juce::IIRCoefficients::makeHighShelf (sr, freq, q, (float) gain)
                                               : juce::IIRCoefficients::makePeakFilter (sr, freq, q, (float) gain);

    // coefficients = { b0, b1, b2, a1, a2 } normalised by a0
    const auto w = juce::MathConstants<double>::twoPi * juce::jmin (f, sr * 0.499) / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    const auto num = (double) c.coefficients[0] + (double) c.coefficients[1] * z1 + (double) c.coefficients[2] * z2;
    const auto den = 1.0 + (double) c.coefficients[3] * z1 + (double) c.coefficients[4] * z2;
    return 20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (num / den)));
}

double ParametricEqEditor::totalGainDbAt (double f) const
{
    double total = 0.0;

    for (auto& b : bands)
        total += bandGainDb (b.type, sampleRate, b.freq->getCurrentValue(), b.q->getCurrentValue(), b.gain->getCurrentValue(), f);

    return total;
}

void ParametricEqEditor::paint (juce::Graphics& g)
{
    BuiltInEditorBase::paint (g);

    const auto area = graphArea.toFloat();
    g.setColour (juce::Colour (0xff111215));
    g.fillRoundedRectangle (area, 5.0f);

    g.saveState();
    g.reduceClipRegion (graphArea);

    // Grid: decades and common frequencies, 6 dB steps.
    g.setFont (juce::Font (juce::FontOptions (9.5f)));

    for (double f : { 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0 })
    {
        const bool major = f == 100.0 || f == 1000.0 || f == 10000.0;
        const float x = freqToX (f);
        g.setColour (theme::outline.withAlpha (major ? 0.9f : 0.35f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());

        if (major || f == 50.0 || f == 500.0 || f == 5000.0)
        {
            g.setColour (theme::textDim);
            g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0),
                        juce::Rectangle<float> (x + 2.0f, area.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }
    }

    for (double db = -18.0; db <= 18.0; db += 6.0)
    {
        const float y = gainToY (db);
        g.setColour (theme::outline.withAlpha (db == 0.0 ? 1.0f : 0.35f));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
        g.setColour (theme::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (area.getX() + 4.0f, y - 12.0f, 30.0f, 11.0f),
                    juce::Justification::centredLeft);
    }

    const bool enabled = plugin->isEnabled();

    // Per-band contribution for the hovered / dragged band.
    const int highlight = dragBand >= 0 ? dragBand : hoverBand;

    if (highlight >= 0)
    {
        auto& b = bands[(size_t) highlight];
        juce::Path band;

        for (int x = graphArea.getX(); x <= graphArea.getRight(); x += 2)
        {
            const auto y = gainToY (bandGainDb (b.type, sampleRate, b.freq->getCurrentValue(), b.q->getCurrentValue(),
                                                b.gain->getCurrentValue(), xToFreq ((float) x)));
            x == graphArea.getX() ? band.startNewSubPath ((float) x, y) : band.lineTo ((float) x, y);
        }

        g.setColour (b.colour.withAlpha (0.7f));
        g.strokePath (band, juce::PathStrokeType (1.2f));
    }

    // Summed response
    juce::Path response;

    for (int x = graphArea.getX(); x <= graphArea.getRight(); ++x)
    {
        const auto y = gainToY (enabled ? totalGainDbAt (xToFreq ((float) x)) : 0.0);
        x == graphArea.getX() ? response.startNewSubPath ((float) x, y) : response.lineTo ((float) x, y);
    }

    juce::Path filled (response);
    filled.lineTo (area.getRight(), gainToY (0.0));
    filled.lineTo (area.getX(), gainToY (0.0));
    filled.closeSubPath();
    g.setColour (accent.withAlpha (enabled ? 0.18f : 0.06f));
    g.fillPath (filled);
    g.setColour (enabled ? theme::text : theme::textDim.withAlpha (0.5f));
    g.strokePath (response, juce::PathStrokeType (2.0f));

    // Band nodes
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto p = nodePosition (bands[i]);
        const bool active = (int) i == highlight;
        const float radius = active ? 9.0f : 7.5f;
        g.setColour (bands[i].colour.withAlpha (active ? 0.35f : 0.18f));
        g.fillEllipse (juce::Rectangle<float> (radius * 3.0f, radius * 3.0f).withCentre (p));
        g.setColour (bands[i].colour.withAlpha (enabled ? 1.0f : 0.4f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p));
        g.setColour (juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (juce::String ((int) i + 1), juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p), juce::Justification::centred);
    }

    // Readout
    if (highlight >= 0)
    {
        auto& b = bands[(size_t) highlight];
        const auto text = b.name + "   " + b.freq->getCurrentValueAsString() + "   " + b.gain->getCurrentValueAsString()
                          + "   Q " + juce::String (b.q->getCurrentValue(), 2);
        g.setColour (b.colour);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (text, area.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    }
    else if (graphArea.contains (hoverPoint))
    {
        g.setColour (theme::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        const auto f = xToFreq ((float) hoverPoint.x);
        g.drawText ((f >= 1000.0 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String (f, 0) + " Hz")
                        + "   " + juce::String (totalGainDbAt (f), 1) + " dB",
                    area.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    }

    g.restoreState();

    // Band names over the knob columns
    const int bandWidth = graphArea.getWidth() / (int) bands.size();

    for (size_t b = 0; b < bands.size(); ++b)
    {
        auto label = juce::Rectangle<int> (graphArea.getX() + (int) b * bandWidth, graphArea.getBottom() + 8, bandWidth, 16).reduced (6, 0);
        g.setColour (bands[b].colour);
        g.fillRect (label.removeFromLeft (3).reduced (0, 2));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText ("  " + juce::String ((int) b + 1) + "  " + bands[b].name, label, juce::Justification::centredLeft);
    }
}

void ParametricEqEditor::mouseMove (const juce::MouseEvent& e)
{
    hoverPoint = e.getPosition();
    const int band = bandAt (e.position);

    if (band != hoverBand || graphArea.contains (hoverPoint))
    {
        hoverBand = band;
        setMouseCursor (band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint (graphArea);
    }
}

void ParametricEqEditor::mouseExit (const juce::MouseEvent&)
{
    hoverBand = -1;
    hoverPoint = { -1, -1 };
    repaint (graphArea);
}

void ParametricEqEditor::mouseDown (const juce::MouseEvent& e)
{
    dragBand = bandAt (e.position);

    if (dragBand >= 0)
    {
        eq.edit.getUndoManager().beginNewTransaction ("EQ band");
        bands[(size_t) dragBand].freq->parameterChangeGestureBegin();
        bands[(size_t) dragBand].gain->parameterChangeGestureBegin();
    }
}

void ParametricEqEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;

    auto& b = bands[(size_t) dragBand];
    const auto pos = e.position.toFloat();
    b.freq->setParameter ((float) juce::jlimit ((double) te::EqualiserPlugin::minFreq, (double) te::EqualiserPlugin::maxFreq, xToFreq (pos.x)), juce::sendNotification);
    b.gain->setParameter ((float) juce::jlimit ((double) te::EqualiserPlugin::minGain, (double) te::EqualiserPlugin::maxGain, yToGain (pos.y)), juce::sendNotification);
    repaint (graphArea);
}

void ParametricEqEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
    {
        bands[(size_t) dragBand].freq->parameterChangeGestureEnd();
        bands[(size_t) dragBand].gain->parameterChangeGestureEnd();
    }

    dragBand = -1;
    repaint (graphArea);
}

void ParametricEqEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto band = bandAt (e.position); band >= 0)
    {
        eq.edit.getUndoManager().beginNewTransaction ("EQ band flat");
        bands[(size_t) band].gain->setParameter (0.0f, juce::sendNotification);
        repaint (graphArea);
    }
}

void ParametricEqEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = bandAt (e.position);

    if (band < 0)
        return;

    auto& q = *bands[(size_t) band].q;
    const auto factor = std::pow (1.5, (double) wheel.deltaY * (wheel.isReversed ? -1.0 : 1.0) * 4.0);
    q.setParameter ((float) juce::jlimit ((double) te::EqualiserPlugin::minQ, (double) te::EqualiserPlugin::maxQ, q.getCurrentValue() * factor),
                    juce::sendNotification);
    repaint (graphArea);
}

//==============================================================================
std::unique_ptr<juce::Component> createBuiltInPluginEditor (te::Plugin& p)
{
    if (dynamic_cast<te::ExternalPlugin*> (&p) != nullptr)
        return {};

    if (auto* eq = dynamic_cast<te::EqualiserPlugin*> (&p))
        return std::make_unique<ParametricEqEditor> (*eq);

    return std::make_unique<KnobPluginEditor> (p);
}

} // namespace opensession
