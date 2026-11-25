#include "PreviewPlayer.h"

#include <cmath>

namespace
{
double repeatInterval(const juce::String& rate, int meterNum)
{
    if (rate == "1/16")
        return 0.25;
    if (rate == "1/8")
        return 0.5;
    if (rate == "1/4")
        return 1.0;
    if (rate == "1/2")
        return 2.0;
    const auto bars = juce::jmax(1, meterNum);
    if (rate == "1bar")
        return (double) bars;
    if (rate == "2bar")
        return 2.0 * bars;
    if (rate == "4bar")
        return 4.0 * bars;
    return 0.0;
}

float readSample(const juce::AudioBuffer<float>& audio, int channel, double pos, bool loop)
{
    const auto frames = audio.getNumSamples();
    if (frames <= 0 || channel < 0 || channel >= audio.getNumChannels())
        return 0.0f;
    if (loop)
    {
        pos = std::fmod(pos, (double) frames);
        if (pos < 0.0)
            pos += frames;
        const auto i0 = (int) pos;
        const auto i1 = i0 + 1 >= frames ? 0 : i0 + 1;
        const auto frac = (float) (pos - i0);
        const auto a = audio.getSample(channel, i0);
        const auto b = audio.getSample(channel, i1);
        return a + frac * (b - a);
    }
    if (pos < 0.0 || pos >= frames)
        return 0.0f;
    const auto i0 = (int) pos;
    const auto i1 = i0 + 1;
    const auto frac = (float) (pos - i0);
    const auto a = audio.getSample(channel, i0);
    const auto b = i1 < frames ? audio.getSample(channel, i1) : 0.0f;
    return a + frac * (b - a);
}

double wrapped(double value, double length)
{
    if (length <= 0.0)
        return 0.0;
    auto phase = std::fmod(value, length);
    if (phase < 0.0)
        phase += length;
    return phase;
}
} // namespace

void PreviewPlayer::load(const juce::var& message)
{
    const auto path = message.getProperty("path", "").toString();
    if (path.isEmpty())
        return;
    juce::File file(path);
    if (!file.existsAsFile())
        return;

    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    if (stream == nullptr)
        return;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(stream.release(), true));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    auto loaded = Clip::Ptr(new Clip());
    loaded->fileRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    loaded->audio.setSize((int) juce::jmax(1, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read(&loaded->audio, 0, (int) reader->lengthInSamples, 0, true, true);
    loaded->loop = (bool) message.getProperty("loop", false);
    const auto beatsVar = message.getProperty("beats", {});
    if (!beatsVar.isVoid())
        loaded->beats = (double) beatsVar;
    const auto bpmVar = message.getProperty("bpm", {});
    if (!bpmVar.isVoid())
        loaded->renderedBpm = (double) bpmVar;
    loaded->repeat = message.getProperty("repeat", "").toString();
    loaded->name = file.getFileName();

    const juce::ScopedLock guard(lock);
    clip = loaded;
    playing = true;
    heldByTransport.store(false);
    ++epoch;
}

void PreviewPlayer::stop()
{
    const juce::ScopedLock guard(lock);
    playing = false;
    heldByTransport.store(false);
}

void PreviewPlayer::clear()
{
    const juce::ScopedLock guard(lock);
    clip = nullptr;
    playing = false;
    heldByTransport.store(false);
    ++epoch;
}

PreviewPlayer::Clip::Ptr PreviewPlayer::current() const
{
    const juce::ScopedLock guard(lock);
    return clip;
}

juce::String PreviewPlayer::label() const
{
    const auto loaded = current();
    if (loaded == nullptr)
        return {};
    return loaded->name;
}

void PreviewPlayer::process(juce::AudioBuffer<float>& buffer, double hostRate, double bpm, double ppq, bool transport, int meterNum)
{
    if (sawTransport != transport)
    {
        const juce::ScopedLock guard(lock);
        if (sawTransport && !transport)
        {
            if (playing)
                heldByTransport.store(true);
            playing = false;
        }
        else if (transport && heldByTransport.exchange(false) && clip != nullptr)
            playing = true;
        else
            heldByTransport.store(false);
    }
    sawTransport = transport;

    Clip::Ptr loaded;
    bool audible = false;
    uint32_t token = 0;
    {
        const juce::ScopedLock guard(lock);
        loaded = clip;
        audible = playing && clip != nullptr;
        token = epoch;
    }
    if (!audible || loaded == nullptr || hostRate <= 0.0)
        return;
    if (token != seenEpoch)
    {
        seenEpoch = token;
        freePos = 0.0;
        freeSamples = 0.0;
        coasting = false;
    }

    const auto frames = loaded->audio.getNumSamples();
    const auto fileChannels = loaded->audio.getNumChannels();
    if (frames <= 0 || fileChannels <= 0)
        return;

    auto loopBeats = loaded->beats;
    if (loopBeats <= 0.0 && bpm > 0.0)
        loopBeats = (double) frames / loaded->fileRate * bpm / 60.0;
    const auto interval = repeatInterval(loaded->repeat, meterNum);
    // A chosen repeat restarts at that grid point, including when the file itself is a loop.
    const auto gridRepeat = transport && interval > 0.0 && bpm > 0.0;
    const auto gridLoop = transport && !gridRepeat && loaded->loop && loopBeats > 0.0;
    const auto rendered = loaded->renderedBpm;
    const auto tempoHolds = rendered <= 0.0 || bpm <= 0.0 || std::abs(bpm - rendered) < 0.51;
    const auto coast = gridLoop && !tempoHolds;
    if (coast && !coasting)
    {
        freePos = wrapped(ppq, loopBeats) / loopBeats * (double) frames;
        coasting = true;
    }
    if (!coast)
        coasting = false;
    const auto freeRepeat = !transport && interval > 0.0 && bpm > 0.0;
    const auto outChannels = buffer.getNumChannels();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        auto pos = 0.0;
        auto voice = true;
        if (coast)
        {
            pos = wrapped(freePos, (double) frames);
            freePos += loaded->fileRate / hostRate;
        }
        else if (gridLoop)
        {
            const auto at = ppq + (bpm > 0.0 ? (double) i * bpm / (hostRate * 60.0) : 0.0);
            pos = wrapped(at, loopBeats) / loopBeats * (double) frames;
        }
        else if (gridRepeat)
        {
            const auto at = ppq + (double) i * bpm / (hostRate * 60.0);
            const auto seconds = wrapped(at, interval) * 60.0 / bpm;
            pos = seconds * loaded->fileRate;
            voice = pos < frames;
        }
        else if (freeRepeat)
        {
            const auto beatsIn = freeSamples / hostRate * bpm / 60.0;
            const auto seconds = wrapped(beatsIn, interval) * 60.0 / bpm;
            pos = seconds * loaded->fileRate;
            voice = pos < frames;
            freeSamples += 1.0;
        }
        else
        {
            pos = loaded->loop ? wrapped(freePos, (double) frames) : freePos;
            voice = loaded->loop || pos < frames;
            freePos += loaded->fileRate / hostRate;
        }

        if (!voice)
            continue;
        for (int channel = 0; channel < outChannels; ++channel)
        {
            const auto source = fileChannels == 1 ? 0 : juce::jmin(channel, fileChannels - 1);
            buffer.addSample(channel, i, readSample(loaded->audio, source, pos, loaded->loop && !freeRepeat && !gridRepeat));
        }
    }
}
