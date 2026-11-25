#include "LinkClient.h"

#include <cstring>

namespace
{
constexpr const char* kHost = "127.0.0.1";
constexpr int kPort = 8771;

int legalDenominator(int den)
{
    switch (den)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
        case 32:
            return den;
        default:
            return 4;
    }
}
} // namespace

LinkClient::LinkClient(juce::String name)
    : juce::Thread("Hoarder Link")
    , hostName(std::move(name))
{
    sessionId = juce::Uuid().toDashedString().removeCharacters("{}");
    if (hostName.isEmpty())
        hostName = "VST3 host";
    hostName = hostName.substring(0, 64);
}

LinkClient::~LinkClient()
{
    stop();
}

void LinkClient::start()
{
    startThread();
}

void LinkClient::stop()
{
    signalThreadShouldExit();
    stopThread(3000);
}

void LinkClient::publish(float nextBpm, float nextPpq, bool isPlaying, bool isRecording, int num, int den, double rate)
{
    bpm.store(nextBpm);
    ppq.store(nextPpq);
    playing.store(isPlaying ? 1 : 0);
    recording.store(isRecording ? 1 : 0);
    meterNum.store(num > 0 ? num : 4);
    meterDen.store(den > 0 ? den : 4);
    sampleRate.store((float) rate);
}

void LinkClient::setSampleRate(double rate)
{
    sampleRate.store((float) rate);
}

bool LinkClient::isConnected() const
{
    return connected.load();
}

void LinkClient::setHandler(std::function<void(const juce::var&)> next)
{
    handler = std::move(next);
}

void LinkClient::handleText(const juce::String& text)
{
    if (handler == nullptr)
        return;
    const auto parsed = juce::JSON::parse(text);
    if (parsed.isObject())
        handler(parsed);
}

void LinkClient::run()
{
    while (!threadShouldExit())
    {
        connected.store(false);
        pendingText.reset();
        socket = std::make_unique<juce::StreamingSocket>();
        if (!connectSocket() || !handshake() || !sendHello())
        {
            socket->close();
            socket.reset();
            wait(500);
            continue;
        }
        connected.store(true);
        auto nextHostAt = juce::Time::getMillisecondCounter();
        while (!threadShouldExit())
        {
            if (!pump(20))
                break;
            const auto now = juce::Time::getMillisecondCounter();
            if (now >= nextHostAt)
            {
                if (!sendHost())
                    break;
                nextHostAt = now + 50;
            }
        }
        const bool leaving = threadShouldExit();
        const bool wasConnected = connected.exchange(false);
        if (leaving && wasConnected && socket != nullptr)
            sendBye();
        if (socket != nullptr)
            socket->close();
        socket.reset();
        if (leaving)
            break;
        wait(500);
    }
    connected.store(false);
}

bool LinkClient::connectSocket()
{
    return socket->connect(kHost, kPort, 1000);
}

bool LinkClient::handshake()
{
    juce::Random random;
    uint8_t keyBytes[16];
    for (auto& byte : keyBytes)
        byte = (uint8_t) random.nextInt(256);
    const auto key = juce::Base64::toBase64(keyBytes, (int) sizeof(keyBytes));
    const auto request = "GET / HTTP/1.1\r\n"
                         "Host: 127.0.0.1:8771\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: "
                       + key
                       + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    const auto raw = request.toRawUTF8();
    if (socket->write(raw, (int) strlen(raw)) < 0)
        return false;

    juce::String head;
    char byte = 0;
    while (!head.contains("\r\n\r\n"))
    {
        if (!readExact(&byte, 1, 3000))
            return false;
        head += juce::String::charToString((juce::juce_wchar) (unsigned char) byte);
        if (head.length() > 4096)
            return false;
    }
    return head.contains(" 101 ");
}

bool LinkClient::sendHello()
{
    auto* object = new juce::DynamicObject();
    object->setProperty("type", "hello");
    object->setProperty("format", "vst3");
    object->setProperty("host", hostName);
    object->setProperty("session", sessionId);
    return sendText(juce::JSON::toString(juce::var(object), true));
}

bool LinkClient::sendBye()
{
    auto* object = new juce::DynamicObject();
    object->setProperty("type", "bye");
    return sendText(juce::JSON::toString(juce::var(object), true));
}

bool LinkClient::sendHost()
{
    const auto snap = readSnapshot();
    if (snap.bpm < 1.0f || snap.bpm > 999.0f)
        return true;
    if (snap.sampleRate < 8000.0 || snap.sampleRate > 384000.0)
        return true;
    auto* object = new juce::DynamicObject();
    object->setProperty("type", "host");
    object->setProperty("bpm", snap.bpm);
    object->setProperty("ppq", juce::jlimit(-1000000.0f, 1000000.0f, snap.ppq));
    object->setProperty("playing", snap.playing);
    object->setProperty("recording", snap.recording);
    juce::Array<juce::var> meter;
    meter.add(juce::jlimit(1, 32, snap.meterNum));
    meter.add(legalDenominator(snap.meterDen));
    object->setProperty("meter", juce::var(meter));
    object->setProperty("sample_rate", (int) (snap.sampleRate + 0.5));
    return sendText(juce::JSON::toString(juce::var(object), true));
}

LinkClient::Snapshot LinkClient::readSnapshot() const
{
    Snapshot snap;
    snap.bpm = bpm.load();
    snap.ppq = ppq.load();
    snap.playing = playing.load() != 0;
    snap.recording = recording.load() != 0;
    snap.meterNum = meterNum.load();
    snap.meterDen = meterDen.load();
    snap.sampleRate = sampleRate.load();
    return snap;
}

bool LinkClient::sendText(const juce::String& text)
{
    const auto raw = text.toRawUTF8();
    return sendFrame(0x1, raw, strlen(raw));
}

bool LinkClient::sendFrame(uint8_t opcode, const void* data, size_t size)
{
    if (socket == nullptr || size > 65535)
        return false;
    uint8_t header[8];
    int headerBytes = 0;
    header[headerBytes++] = (uint8_t) (0x80 | opcode);
    if (size < 126)
    {
        header[headerBytes++] = (uint8_t) (0x80 | size);
    }
    else
    {
        header[headerBytes++] = (uint8_t) (0x80 | 126);
        header[headerBytes++] = (uint8_t) ((size >> 8) & 0xff);
        header[headerBytes++] = (uint8_t) (size & 0xff);
    }
    uint8_t mask[4];
    juce::Random random;
    for (auto& byte : mask)
        byte = (uint8_t) random.nextInt(256);
    juce::HeapBlock<uint8_t> frame((size_t) headerBytes + 4 + size);
    memcpy(frame.getData(), header, (size_t) headerBytes);
    memcpy(frame.getData() + headerBytes, mask, 4);
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
        frame[headerBytes + 4 + (int) i] = (uint8_t) (bytes[i] ^ mask[i % 4]);
    const auto total = headerBytes + 4 + (int) size;
    return socket->write(frame.getData(), total) == total;
}

bool LinkClient::readExact(void* dest, int bytes, int timeoutMs)
{
    auto* out = static_cast<uint8_t*>(dest);
    int got = 0;
    const auto start = juce::Time::getMillisecondCounter();
    while (got < bytes)
    {
        if (threadShouldExit() || socket == nullptr)
            return false;
        const auto elapsed = (int) (juce::Time::getMillisecondCounter() - start);
        if (elapsed >= timeoutMs)
            return false;
        if (!socket->waitUntilReady(true, juce::jmin(50, timeoutMs - elapsed)))
            continue;
        const auto n = socket->read(out + got, bytes - got, false);
        if (n <= 0)
            return false;
        got += n;
    }
    return true;
}

bool LinkClient::pump(int timeoutMs)
{
    if (socket == nullptr)
        return false;
    if (!socket->waitUntilReady(true, timeoutMs))
        return !threadShouldExit();
    uint8_t head[2];
    if (!readExact(head, 2, 1000))
        return false;
    const auto opcode = (uint8_t) (head[0] & 0x0f);
    auto length = (size_t) (head[1] & 0x7f);
    if (length == 126)
    {
        uint8_t extended[2];
        if (!readExact(extended, 2, 1000))
            return false;
        length = ((size_t) extended[0] << 8) | extended[1];
    }
    else if (length == 127)
    {
        return false;
    }
    if ((head[1] & 0x80) != 0)
        return false;
    juce::HeapBlock<uint8_t> payload(length + 1);
    if (length > 0 && !readExact(payload.getData(), (int) length, 1000))
        return false;
    if (opcode == 0x9)
        return sendFrame(0xA, payload.getData(), length);
    if (opcode == 0x8)
        return false;
    if (opcode == 0x1 || opcode == 0x0)
    {
        pendingText.append(payload.getData(), length);
        if ((head[0] & 0x80) != 0)
        {
            const auto text = juce::String::fromUTF8(static_cast<const char*>(pendingText.getData()), (int) pendingText.getSize());
            pendingText.reset();
            handleText(text);
        }
        return true;
    }
    return opcode == 0xA;
}
