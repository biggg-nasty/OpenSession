// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace te = tracktion;

namespace opensession
{

/** Knobs drawn like outboard / amp controls: knurled skirt, dark cap, cream pointer, 0-10 scale
    and a coloured value arc (rotarySliderFillColourId). Also styles the value readouts. */
struct KnobLookAndFeel : public theme::LookAndFeel
{
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

/** Hardware-style on/off switch with an LED, used for plugin bypass. */
class PowerSwitch : public juce::Button
{
public:
    PowerSwitch() : juce::Button ("Power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** One knob bound to an engine parameter, with its name above and value readout below.
    Follows changes made elsewhere (undo, the mixer, automation) via refresh(). */
class ParameterKnob : public juce::Component
{
public:
    ParameterKnob (te::AutomatableParameter&, juce::Colour accent, const juce::String& title = {});

    void resized() override;
    void refresh();

    juce::Slider slider;
    juce::Label title;

private:
    te::AutomatableParameter::Ptr param;
};

/** Base for our built-in plugin editors: faceplate, title, power (bypass) switch, refresh timer. */
class BuiltInEditorBase : public juce::Component,
                          private juce::Timer
{
public:
    BuiltInEditorBase (te::Plugin&, const juce::String& title, juce::Colour accent);
    ~BuiltInEditorBase() override;

    void paint (juce::Graphics&) override;

protected:
    te::Plugin::Ptr plugin;
    juce::String title;
    juce::Colour accent;
    PowerSwitch power;
    juce::OwnedArray<ParameterKnob> knobs;

    static constexpr int headerHeight = 40;
    juce::Rectangle<int> getContentArea() const     { return getLocalBounds().withTrimmedTop (headerHeight).reduced (14, 10); }
    void layoutHeader();
    virtual void refreshFromPlugin();

private:
    juce::SharedResourcePointer<KnobLookAndFeel> knobLookAndFeel;
    void timerCallback() override                   { refreshFromPlugin(); }
};

/** Compressor, reverb and any other built-in: a row/grid of knobs; the compressor also shows
    its static transfer curve (threshold and ratio). */
class KnobPluginEditor : public BuiltInEditorBase
{
public:
    explicit KnobPluginEditor (te::Plugin&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    te::CompressorPlugin* compressor = nullptr;
    te::AutomatableParameter::Ptr freezeParam;
    juce::ToggleButton freezeButton { "Freeze" };
    juce::Rectangle<int> curveArea;

    void refreshFromPlugin() override;
    void paintTransferCurve (juce::Graphics&, juce::Rectangle<float>);
};

/** Parametric view of the engine's 4-band EQ (low shelf, two peaks, high shelf): the summed
    response curve with a draggable node per band (drag = frequency/gain, wheel = Q,
    double-click = flat), plus frequency / gain / Q knobs for exact values. */
class ParametricEqEditor : public BuiltInEditorBase
{
public:
    explicit ParametricEqEditor (te::EqualiserPlugin&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    enum class BandType { lowShelf, peak, highShelf };

    /** Magnitude in dB of one band at frequency f, using the same JUCE coefficient formulas the
        engine's EqualiserPlugin runs, so the drawn curve matches what is heard. */
    static double bandGainDb (BandType, double sampleRate, double freq, double q, double gainDb, double f);

private:
    struct Band
    {
        juce::String name;
        BandType type;
        te::AutomatableParameter::Ptr freq, gain, q;
        juce::Colour colour;
    };

    te::EqualiserPlugin& eq;
    std::vector<Band> bands;
    juce::Rectangle<int> graphArea;
    int hoverBand = -1, dragBand = -1;
    juce::Point<int> hoverPoint;
    double sampleRate = 48000.0;

    static constexpr double minHz = 20.0, maxHz = 20000.0, displayDb = 24.0;

    float freqToX (double) const;
    double xToFreq (float) const;
    float gainToY (double) const;
    double yToGain (float) const;
    juce::Point<float> nodePosition (const Band&) const;
    int bandAt (juce::Point<float>) const;
    double totalGainDbAt (double f) const;
    void refreshFromPlugin() override;
};

/** Our editor for a built-in plugin (EQ, compressor, reverb, ...), or nullptr for third-party plugins. */
std::unique_ptr<juce::Component> createBuiltInPluginEditor (te::Plugin&);

} // namespace opensession
