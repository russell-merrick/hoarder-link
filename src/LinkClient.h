#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

/** One localhost socket to Hoarder. The audio thread only stores atomics. */
class LinkClient : private juce::Thread
{
public:
    explicit LinkClient(juce::String hostName);
    ~LinkClient() override;

    void start();
    void stop();

    void publish(float bpm, float ppq, bool playing, bool recording, int meterNum, int meterDen, double sampleRate);
    void setSampleRate(double sampleRate);
    bool isConnected() const;
    void setHandler(std::function<void(const juce::var&)> next);

private:
    struct Snapshot
    {
        float bpm = 0.0f;
        float ppq = 0.0f;
        bool playing = false;
        bool recording = false;
        int meterNum = 4;
        int meterDen = 4;
        double sampleRate = 0.0;
    };

    void run() override;
    bool connectSocket();
    bool handshake();
    bool sendHello();
    bool sendHost();
    bool sendBye();
    bool sendText(const juce::String& text);
    bool sendFrame(uint8_t opcode, const void* data, size_t size);
    bool readExact(void* dest, int bytes, int timeoutMs);
    bool pump(int timeoutMs);
    void handleText(const juce::String& text);
    Snapshot readSnapshot() const;

    juce::String hostName;
    juce::String sessionId;
    std::unique_ptr<juce::StreamingSocket> socket;
    std::function<void(const juce::var&)> handler;
    juce::MemoryBlock pendingText;
    std::atomic<bool> connected { false };

    std::atomic<float> bpm { 0.0f };
    std::atomic<float> ppq { 0.0f };
    std::atomic<int> playing { 0 };
    std::atomic<int> recording { 0 };
    std::atomic<int> meterNum { 4 };
    std::atomic<int> meterDen { 4 };
    std::atomic<float> sampleRate { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinkClient)
};
