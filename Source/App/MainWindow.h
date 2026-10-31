// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include "MainComponent.h"

namespace opensession
{

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, OpenSessionEngine& engine)
        : DocumentWindow (name, theme::background, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        content = new MainComponent (engine, *this);
        setContentOwned (content, true);
        setResizable (true, false);
        setResizeLimits (900, 560, 10000, 10000);
        centreWithSize (1380, 860);
        setVisible (true);
        content->grabKeyboardFocus();
    }

    void closeButtonPressed() override
    {
        content->tryToQuit();
    }

    MainComponent& getMainComponent()   { return *content; }

private:
    MainComponent* content = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace opensession
