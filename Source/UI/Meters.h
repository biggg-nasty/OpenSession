// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace te = tracktion;

namespace opensession
{

/** A level meter fed by a te::LevelMeasurer (input device or track LevelMeterPlugin).
    Peak with 1.5 s hold, overload indicator latched until clicked. Vertical or horizontal. */
class LevelMeterComponent : public juce::Component,
                            private juce::Timer
{
public:
    explicit LevelMeterComponent (bool vertical_ = true) : vertical (vertical_)
    {
        setInterceptsMouseClicks (true, false);
        startTimerHz (30);
    }

    ~LevelMeterComponent() override
    {
        setMeasurer (nullptr);
    }

    void setMeasurer (te::LevelMeasurer* m)
    {
        if (measurer == m)
            return;

        if (measurer != nullptr)
            measurer->removeClient (client);

        measurer = m;

        if (measurer != nullptr)
            measurer->addClient (client);

        for (auto& l : levelsDb) l = -100.0f;
        for (auto& p : peakHoldDb) p = -100.0f;
        overloaded = false;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 2.0f);

        const int numChannels = juce::jlimit (1, 2, client.getNumChannelsUsed() > 0 ? client.getNumChannelsUsed() : 1);
        const float gap = 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::Rectangle<float> bar = vertical
                ? juce::Rectangle<float> (r.getX() + ch * (r.getWidth() / numChannels), r.getY(), r.getWidth() / numChannels - gap, r.getHeight())
                : juce::Rectangle<float> (r.getX(), r.getY() + ch * (r.getHeight() / numChannels), r.getWidth(), r.getHeight() / numChannels - gap);

            const float prop = dbToProportion (levelsDb[(size_t) ch]);
            const float peakProp = dbToProportion (peakHoldDb[(size_t) ch]);

            auto filled = vertical ? bar.withTop (bar.getBottom() - bar.getHeight() * prop)
                                   : bar.withRight (bar.getX() + bar.getWidth() * prop);

            juce::ColourGradient grad = vertical
                ? juce::ColourGradient (theme::play, bar.getX(), bar.getBottom(), theme::record, bar.getX(), bar.getY(), false)
                : juce::ColourGradient (theme::play, bar.getX(), bar.getY(), theme::record, bar.getRight(), bar.getY(), false);
            grad.addColour (0.75, theme::warning);
            g.setGradientFill (grad);
            g.fillRect (filled);

            g.setColour (juce::Colours::white.withAlpha (0.8f));

            if (vertical)
                g.fillRect (bar.getX(), bar.getBottom() - bar.getHeight() * peakProp - 1.0f, bar.getWidth(), 2.0f);
            else
                g.fillRect (bar.getX() + bar.getWidth() * peakProp - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        }

        if (overloaded)
        {
            g.setColour (theme::record);
            auto led = vertical ? r.removeFromTop (6.0f) : r.removeFromRight (6.0f);
            g.fillRect (led);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        overloaded = false;
        for (auto& p : peakHoldDb) p = -100.0f;
        repaint();
    }

    float getCurrentLevelDb (int channel) const    { return levelsDb[(size_t) juce::jlimit (0, 1, channel)]; }

private:
    bool vertical;
    te::LevelMeasurer* measurer = nullptr;
    te::LevelMeasurer::Client client;
    std::array<float, 2> levelsDb { -100.0f, -100.0f }, peakHoldDb { -100.0f, -100.0f };
    std::array<juce::uint32, 2> peakHoldTime { 0, 0 };
    bool overloaded = false;

    static float dbToProportion (float dB)
    {
        return juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f);
    }

    void timerCallback() override
    {
        if (measurer == nullptr)
            return;

        const auto now = juce::Time::getMillisecondCounter();
        const int numChannels = juce::jlimit (1, 2, client.getNumChannelsUsed() > 0 ? client.getNumChannelsUsed() : 1);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto pair = client.getAndClearAudioLevel (ch);
            auto& lvl = levelsDb[(size_t) ch];
            // fast attack, ~20 dB/s release
            lvl = pair.dB > lvl ? pair.dB : juce::jmax (-100.0f, lvl - 1.2f);

            auto& hold = peakHoldDb[(size_t) ch];

            if (pair.dB >= hold || now - peakHoldTime[(size_t) ch] > 1500)
            {
                hold = pair.dB;
                peakHoldTime[(size_t) ch] = now;
            }
        }

        // Only trust the overload flag when a channel really reached full scale this tick:
        // the measurer's flag can read true before any audio has been measured.
        bool hot = false;
        for (int ch = 0; ch < numChannels; ++ch)
            hot = hot || levelsDb[(size_t) ch] >= -0.2f;

        if (client.getAndClearOverload() && hot)
            overloaded = true;

        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterComponent)
};

} // namespace opensession
