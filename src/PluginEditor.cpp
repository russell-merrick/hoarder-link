#include "PluginEditor.h"

HoarderLinkEditor::HoarderLinkEditor(HoarderLinkProcessor& owner)
    : AudioProcessorEditor(owner)
    , processor(owner)
{
    title.setText("Hoarder Link", juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    status.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
    status.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(status);

    setSize(280, 88);
    startTimerHz(4);
}

HoarderLinkEditor::~HoarderLinkEditor()
{
    stopTimer();
}

void HoarderLinkEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff14161c));
}

void HoarderLinkEditor::resized()
{
    title.setBounds(16, 14, getWidth() - 32, 24);
    status.setBounds(16, 44, getWidth() - 32, 22);
}

void HoarderLinkEditor::timerCallback()
{
    const auto up = processor.getLink().isConnected();
    const auto name = processor.previewName();
    status.setText(up ? (name.isEmpty() ? "Connected" : "Connected  " + name) : "Looking for Hoarder", juce::dontSendNotification);
}
