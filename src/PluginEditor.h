#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class HoarderLinkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HoarderLinkEditor(HoarderLinkProcessor&);
    ~HoarderLinkEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HoarderLinkProcessor& processor;
    juce::Label title;
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoarderLinkEditor)
};
