#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String detectedHost()
{
    auto name = juce::String(juce::PluginHostType().getHostDescription()).trim();
    if (name.isEmpty())
        return "VST3 host";
    return name;
}
} // namespace

HoarderLinkProcessor::HoarderLinkProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , link(detectedHost())
{
    link.setHandler([this](const juce::var& message) { handleCommand(message); });
    link.start();
}

void HoarderLinkProcessor::handleCommand(const juce::var& message)
{
    const auto type = message.getProperty("type", "").toString();
    if (type == "preview")
        player.load(message);
    else if (type == "stop")
        player.stop();
    else if (type == "clear")
        player.clear();
}

HoarderLinkProcessor::~HoarderLinkProcessor()
{
    link.stop();
}

void HoarderLinkProcessor::prepareToPlay(double sampleRate, int)
{
    link.setSampleRate(sampleRate);
}

void HoarderLinkProcessor::releaseResources() {}

bool HoarderLinkProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HoarderLinkProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();

    auto tempo = 0.0;
    auto ppq = 0.0;
    auto isPlaying = false;
    auto isRecording = false;
    auto numerator = 4;
    auto denominator = 4;
    if (auto* head = getPlayHead())
    {
        if (auto position = head->getPosition())
        {
            if (auto bpm = position->getBpm())
                tempo = *bpm;
            if (auto positionPpq = position->getPpqPosition())
                ppq = *positionPpq;
            isPlaying = position->getIsPlaying();
            isRecording = position->getIsRecording();
            if (auto signature = position->getTimeSignature())
            {
                numerator = signature->numerator;
                denominator = signature->denominator;
            }
        }
    }
    link.publish((float) tempo, (float) ppq, isPlaying, isRecording, numerator, denominator, getSampleRate());
    player.process(buffer, getSampleRate(), tempo, ppq, isPlaying, numerator);
}

juce::AudioProcessorEditor* HoarderLinkProcessor::createEditor()
{
    return new HoarderLinkEditor(*this);
}

bool HoarderLinkProcessor::hasEditor() const { return true; }
const juce::String HoarderLinkProcessor::getName() const { return "Hoarder Link"; }
bool HoarderLinkProcessor::acceptsMidi() const { return true; }
bool HoarderLinkProcessor::producesMidi() const { return false; }
bool HoarderLinkProcessor::isMidiEffect() const { return false; }
double HoarderLinkProcessor::getTailLengthSeconds() const { return 0.0; }
int HoarderLinkProcessor::getNumPrograms() { return 1; }
int HoarderLinkProcessor::getCurrentProgram() { return 0; }
void HoarderLinkProcessor::setCurrentProgram(int) {}
const juce::String HoarderLinkProcessor::getProgramName(int) { return {}; }
void HoarderLinkProcessor::changeProgramName(int, const juce::String&) {}
void HoarderLinkProcessor::getStateInformation(juce::MemoryBlock&) {}
void HoarderLinkProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoarderLinkProcessor();
}
