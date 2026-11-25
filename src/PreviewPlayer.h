#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

/** Plays the latest preview. The audio thread never reads the file. */
class PreviewPlayer
{
public:
    void load(const juce::var& message);
    void stop();
    void clear();

    void process(juce::AudioBuffer<float>& buffer, double hostRate, double bpm, double ppq, bool transport, int meterNum);
    juce::String label() const;

private:
    struct Clip : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<Clip>;
        juce::AudioBuffer<float> audio;
        double fileRate = 44100.0;
        bool loop = false;
        double beats = 0.0;
        double renderedBpm = 0.0;
        juce::String repeat;
        juce::String name;
    };

    Clip::Ptr current() const;

    juce::CriticalSection lock;
    Clip::Ptr clip;
    bool playing = false;
    uint32_t epoch = 0;

    uint32_t seenEpoch = 0;
    double freePos = 0.0;
    double freeSamples = 0.0;
    bool sawTransport = false;
    bool coasting = false;
    // Set only when transport itself stops a playing preview, so the next play resumes it.
    std::atomic<bool> heldByTransport { false };
};
