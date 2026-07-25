#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/ProtoReader.h"

#include <cstdio>
#include <ctime>
#if defined(WIIMESH_WII)
#include <unistd.h>
#endif

namespace wiimesh {

constexpr uint32_t PortTextMessage = 1;
constexpr uint32_t PortRouting = 5;

struct WakeMode {
    const char *name;
    const uint8_t *bytes;
    size_t size;
};

static constexpr uint8_t WakePythonC3x32[] = {
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
};
static constexpr uint8_t WakeLegacy94x4[] = {0x94, 0x94, 0x94, 0x94};
static constexpr uint8_t WakePair94c3x4[] = {0x94, 0xc3, 0x94, 0xc3, 0x94, 0xc3, 0x94, 0xc3};
static constexpr uint8_t WakeEmptyFrame[] = {0x94, 0xc3, 0x00, 0x00};
static constexpr uint8_t WakeC3x4[] = {0xc3, 0xc3, 0xc3, 0xc3};
static constexpr uint8_t WakeC3x64[] = {
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
};
static constexpr uint8_t WakeC3x32Then94x4[] = {
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0x94, 0x94, 0x94, 0x94,
};
static constexpr WakeMode WakeModes[] = {
    {"python-c3x32", WakePythonC3x32, sizeof(WakePythonC3x32)},
    {"legacy-94x4", WakeLegacy94x4, sizeof(WakeLegacy94x4)},
    {"pair-94c3x4", WakePair94c3x4, sizeof(WakePair94c3x4)},
    {"empty-frame", WakeEmptyFrame, sizeof(WakeEmptyFrame)},
    {"short-c3x4", WakeC3x4, sizeof(WakeC3x4)},
    {"long-c3x64", WakeC3x64, sizeof(WakeC3x64)},
    {"none", nullptr, 0},
    {"c3x32-plus-94x4", WakeC3x32Then94x4, sizeof(WakeC3x32Then94x4)},
};
static std::string portName(uint32_t port) {
    switch (port) {
    case 0: return "UNKNOWN_APP";
    case 1: return "TEXT_MESSAGE_APP";
    case 2: return "REMOTE_HARDWARE_APP";
    case 3: return "POSITION_APP";
    case 4: return "NODEINFO_APP";
    case 5: return "ROUTING_APP";
    case 6: return "ADMIN_APP";
    case 7: return "TEXT_MESSAGE_COMPRESSED_APP";
    case 8: return "WAYPOINT_APP";
    case 10: return "DETECTION_SENSOR_APP";
    case 11: return "ALERT_APP";
    case 12: return "KEY_VERIFICATION_APP";
    case 32: return "REPLY_APP";
    case 67: return "TELEMETRY_APP";
    case 70: return "TRACEROUTE_APP";
    case 71: return "NEIGHBORINFO_APP";
    default: return "PORT_" + std::to_string(port);
    }
}

static std::string modemPresetName(uint32_t preset) {
    switch (preset) {
    case 0: return "LongFast";
    case 1: return "LongSlow";
    case 2: return "VeryLongSlow";
    case 3: return "MediumSlow";
    case 4: return "MediumFast";
    case 5: return "ShortSlow";
    case 6: return "ShortFast";
    case 7: return "LongModerate";
    case 8: return "ShortTurbo";
    case 9: return "LongTurbo";
    case 10: return "LiteFast";
    case 11: return "LiteSlow";
    case 12: return "NarrowFast";
    case 13: return "NarrowSlow";
    case 14: return "TinyFast";
    case 15: return "TinySlow";
    case 16: return "MediumTurbo";
    default: return "LongFast";
    }
}

static std::string hexPreview(const std::vector<uint8_t> &payload, size_t maxBytes) {
    static const char *hex = "0123456789abcdef";
    std::string out;
    const size_t n = payload.size() < maxBytes ? payload.size() : maxBytes;
    for (size_t i = 0; i < n; ++i) {
        if (i) out += ' ';
        const uint8_t b = payload[i];
        out += hex[(b >> 4) & 0x0f];
        out += hex[b & 0x0f];
    }
    if (payload.size() > maxBytes) out += " ...";
    return out;
}

static std::string asciiPreview(const std::vector<uint8_t> &payload, size_t maxBytes) {
    std::string out;
    const size_t n = payload.size() < maxBytes ? payload.size() : maxBytes;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char b = payload[i];
        if (b == 0x1b) {
            out += "<esc>";
        } else if (b == '\r' || b == '\n' || b == '\t') {
            out += ' ';
        } else if (b >= 32 && b <= 126) {
            out += static_cast<char>(b);
        } else {
            out += '.';
        }
    }
    if (payload.size() > maxBytes) out += " ...";
    return out;
}

static bool looksLikeConsoleText(const std::vector<uint8_t> &payload) {
    size_t printable = 0;
    size_t considered = 0;
    for (uint8_t b : payload) {
        if (b == 0x94 || b == 0xc3) continue;
        considered++;
        if (b == 0x1b || b == '\r' || b == '\n' || b == '\t' || (b >= 32 && b <= 126)) {
            printable++;
        }
    }
    return considered > 0 && printable * 100 / considered > 80;
}

static bool parseHexField(const std::string &line, const std::string &key, uint32_t &value) {
    const size_t p = line.find(key);
    if (p == std::string::npos) return false;
    const size_t start = p + key.size();
    if (start + 2 > line.size() || line[start] != '0' || line[start + 1] != 'x') return false;
    char *end = nullptr;
    const unsigned long out = std::strtoul(line.c_str() + start + 2, &end, 16);
    value = static_cast<uint32_t>(out);
    return end != line.c_str() + start + 2;
}

static void serialWritePause(unsigned microseconds = 100000) {
#if defined(WIIMESH_WII)
    usleep(microseconds);
#else
    (void)microseconds;
#endif
}

static void addProtocolEvent(AppState &state, Logger *logger, const std::string &text) {
    state.protocolEvents.push_back(text);
    while (state.protocolEvents.size() > 8) {
        state.protocolEvents.erase(state.protocolEvents.begin());
    }
    if (logger) {
        logger->line("PROTO " + text);
    }
}

static void addStreamEvent(AppState &state, Logger *logger, const std::string &text) {
    state.streamEvents.push_back(text);
    while (state.streamEvents.size() > 30) {
        state.streamEvents.erase(state.streamEvents.begin());
    }
    if (logger) {
        logger->line("STREAM " + text);
    }
}

static void addDebugPacket(AppState &state, const DebugPacket &packet) {
    state.debugPackets.push_back(packet);
    while (static_cast<int>(state.debugPackets.size()) > MaxDebugPackets) {
        state.debugPackets.erase(state.debugPackets.begin());
    }
}

MeshtasticProtocol::MeshtasticProtocol(Logger *logger) : logger_(logger) {
    channels_[0] = primaryFallbackName_;
}

void MeshtasticProtocol::reset() {
    frameState_ = NeedStart1;
    frameLen_ = 0;
    frame_.clear();
}

void MeshtasticProtocol::setWakeMode(uint32_t mode) {
    wakeMode_ = mode % wakeModeCount();
}

uint32_t MeshtasticProtocol::wakeMode() const {
    return wakeMode_;
}

const char *MeshtasticProtocol::wakeModeName() const {
    return wakeModeName(wakeMode_);
}

uint32_t MeshtasticProtocol::wakeModeCount() {
    return static_cast<uint32_t>(sizeof(WakeModes) / sizeof(WakeModes[0]));
}

const char *MeshtasticProtocol::wakeModeName(uint32_t mode) {
    if (mode >= wakeModeCount()) {
        return "unknown";
    }
    return WakeModes[mode].name;
}

bool MeshtasticProtocol::writeWake(Transport &transport) {
    const WakeMode &mode = WakeModes[wakeMode_ % wakeModeCount()];
    if (mode.size == 0) {
        if (logger_) logger_->line(std::string("TXRAW wake mode ") + std::to_string(wakeMode_) +
                                   " " + mode.name + " skipped");
        serialWritePause(50000);
        return true;
    }
    const bool ok = transport.write(mode.bytes, mode.size) == static_cast<int>(mode.size);
    if (logger_) {
        logger_->line(std::string("TXRAW wake mode ") + std::to_string(wakeMode_) +
                      " " + mode.name + " len " + std::to_string(static_cast<unsigned>(mode.size)) +
                      " hex " + hexPreview(std::vector<uint8_t>(mode.bytes, mode.bytes + mode.size), 72) +
                      (ok ? " ok" : " failed"));
    }
    if (ok) serialWritePause();
    return ok;
}

bool MeshtasticProtocol::writeWantConfig(Transport &transport, uint32_t nonce) {
    std::vector<uint8_t> toRadio;
    appendVarint(toRadio, 3, nonce); // ToRadio.want_config_id

    std::vector<uint8_t> frame = {0x94, 0xc3,
        static_cast<uint8_t>((toRadio.size() >> 8) & 0xff),
        static_cast<uint8_t>(toRadio.size() & 0xff)};
    frame.insert(frame.end(), toRadio.begin(), toRadio.end());
    const bool ok = transport.write(frame.data(), frame.size()) == static_cast<int>(frame.size());
    if (ok && logger_) {
        logger_->line("STREAM Wii -> RAK ToRadio want_config_id " + std::to_string(nonce));
    }
    if (logger_) {
        logger_->line("TXRAW ToRadio want_config len " + std::to_string(static_cast<unsigned>(frame.size())) +
                      " hex " + hexPreview(frame, 40));
    }
    if (ok) serialWritePause();
    return ok;
}

bool MeshtasticProtocol::sendHeartbeat(Transport &transport) {
    std::vector<uint8_t> heartbeat;
    appendVarint(heartbeat, 1, heartbeatNonce_++);
    std::vector<uint8_t> toRadio;
    appendBytes(toRadio, 7, heartbeat); // ToRadio.heartbeat

    std::vector<uint8_t> frame = {0x94, 0xc3,
        static_cast<uint8_t>((toRadio.size() >> 8) & 0xff),
        static_cast<uint8_t>(toRadio.size() & 0xff)};
    frame.insert(frame.end(), toRadio.begin(), toRadio.end());
    const bool ok = transport.write(frame.data(), frame.size()) == static_cast<int>(frame.size());
    if (ok && logger_) {
        logger_->line("STREAM Wii -> RAK ToRadio heartbeat");
    }
    if (logger_) {
        logger_->line("TXRAW ToRadio heartbeat len " + std::to_string(static_cast<unsigned>(frame.size())) +
                      " hex " + hexPreview(frame, 40));
    }
    if (ok) serialWritePause();
    return ok;
}

bool MeshtasticProtocol::startConfig(Transport &transport) {
    configNonce_ = configNonce_ * 1664525u + 1013904223u;
    if (configNonce_ == 69420u) {
        configNonce_++;
    }
    const bool wakeOk = writeWake(transport);
    const bool configOk = writeWantConfig(transport, configNonce_);
    if (logger_) {
        logger_->line(std::string("STREAM Wii -> RAK startConfig wake ") +
                      std::to_string(wakeMode_) + " " + wakeModeName() +
                      (wakeOk && configOk ? " ok" : " failed"));
    }
    return wakeOk && configOk;
}

bool MeshtasticProtocol::sendText(Transport &transport, uint32_t to, uint8_t channel,
                                  const std::string &text, bool wantAck) {
    std::vector<uint8_t> frame = buildTextToRadioFrame(to, channel, text, wantAck);
    const bool ok = transport.write(frame.data(), frame.size()) == static_cast<int>(frame.size());
    if (logger_) {
        logger_->line(std::string("STREAM Wii -> RAK ToRadio text ") +
                      nodeId(to) + " ch " + std::to_string(channel) +
                      " len " + std::to_string(static_cast<unsigned>(text.size())) +
                      (ok ? " ok" : " failed"));
        logger_->line("TXRAW ToRadio text frame len " + std::to_string(static_cast<unsigned>(frame.size())) +
                      " hex " + hexPreview(frame, 48));
    }
    return ok;
}

void MeshtasticProtocol::poll(Transport &transport, AppState &state) {
    uint8_t buffer[512];
    int n = transport.read(buffer, sizeof(buffer));
    if (n < 0) {
        state.usbConnected = false;
        state.protocolReady = false;
        state.usbStatus = "USB disconnected";
        reset();
        return;
    }
    if (n > 0) {
        state.rxBytes += static_cast<uint32_t>(n);
        std::vector<uint8_t> chunk(buffer, buffer + n);
        const bool consoleText = looksLikeConsoleText(chunk);
        DebugPacket debug;
        debug.time = static_cast<uint32_t>(std::time(nullptr));
        debug.title = consoleText ? "Serial Console" : "USB RX";
        debug.summary = "USB read chunk len=" + std::to_string(static_cast<unsigned>(n));
        debug.meshFields = consoleText ? "serial console/debug text" : "pre-frame raw bytes";
        debug.dataFields = "hex: " + hexPreview(chunk, 48);
        debug.decoded = consoleText ? "ASCII: " + asciiPreview(chunk, 64) : "Waiting for 94 c3 framed FromRadio";
        addDebugPacket(state, debug);
        if (consoleText) {
            addStreamEvent(state, logger_, "USB console " + asciiPreview(chunk, 56));
        } else {
        addStreamEvent(state, logger_, "USB RX " + std::to_string(static_cast<unsigned>(n)) +
                       " bytes hex " + hexPreview(chunk, 16));
        }
        if (consoleText) {
            consumeConsoleText(chunk, state);
        }
        if (logger_) {
            logger_->line("RXUSB chunk len " + std::to_string(static_cast<unsigned>(n)) +
                          " hex " + hexPreview(chunk, 96) +
                          " ascii " + asciiPreview(chunk, 96));
        }
    }
    for (int i = 0; i < n; ++i) {
        consume(buffer[i], state);
    }
}

void MeshtasticProtocol::consume(uint8_t byte, AppState &state) {
    switch (frameState_) {
    case NeedStart1:
        if (byte == 0x94) frameState_ = NeedStart2;
        break;
    case NeedStart2:
        if (byte == 0xc3) frameState_ = NeedLen1;
        else if (byte == 0x94) frameState_ = NeedStart2;
        else frameState_ = NeedStart1;
        break;
    case NeedLen1:
        frameLen_ = static_cast<uint16_t>(byte) << 8;
        frameState_ = NeedLen2;
        break;
    case NeedLen2:
        frameLen_ |= byte;
        if (frameLen_ > 512) {
            if (logger_) logger_->line("Dropping oversize Meshtastic frame");
            state.rxBadFrames++;
            frameState_ = NeedStart1;
        } else {
            frame_.clear();
            frame_.reserve(frameLen_);
            frameState_ = NeedPayload;
        }
        break;
    case NeedPayload:
        frame_.push_back(byte);
        if (frame_.size() >= frameLen_) {
            recordRawFrame(frame_, state);
            if (parseFromRadio(frame_, state)) {
                state.rxFrames++;
            } else {
                state.rxBadFrames++;
            }
            frameState_ = NeedStart1;
        }
        break;
    }
}

void MeshtasticProtocol::consumeConsoleText(const std::vector<uint8_t> &payload, AppState &state) {
    for (uint8_t b : payload) {
        if (b == 0x1b) {
            consoleLine_ += "<esc>";
        } else if (b == '\r' || b == '\n') {
            if (!consoleLine_.empty()) {
                parseConsoleLine(consoleLine_, state);
                consoleLine_.clear();
            }
        } else if (b == '\t' || (b >= 32 && b <= 126)) {
            consoleLine_ += static_cast<char>(b);
            if (consoleLine_.size() > 512) {
                parseConsoleLine(consoleLine_, state);
                consoleLine_.clear();
            }
        }
    }
}

void MeshtasticProtocol::parseConsoleLine(const std::string &line, AppState &state) {
    if (line.find("decoded message") == std::string::npos || line.find("Portnum=1") == std::string::npos) {
        return;
    }
    uint32_t from = 0;
    uint32_t to = 0;
    uint32_t channel = 0;
    parseHexField(line, "fr=", from);
    parseHexField(line, "to=", to);
    parseHexField(line, "Ch=", channel);

    Message msg;
    msg.from = from;
    msg.to = to;
    msg.channelIndex = static_cast<uint8_t>(channel & 0xff);
    msg.rxTime = static_cast<uint32_t>(std::time(nullptr));
    msg.direct = to != BroadcastNode;
    msg.senderId = nodeId(from);
    auto known = nodeNames_.find(from);
    msg.senderName = known == nodeNames_.end() ? msg.senderId : known->second;
    auto channelName = channels_.find(msg.channelIndex);
    msg.channelName = channelName == channels_.end() || channelName->second.empty() ? primaryFallbackName_ : channelName->second;
    DebugPacket debug;
    debug.time = msg.rxTime;
    debug.title = "Console Text";
    debug.summary = "Serial console decoded TEXT_MESSAGE_APP from " + msg.senderId;
    debug.meshFields = line.substr(0, 66);
    debug.dataFields = "fallback from firmware console log";
    debug.decoded = "Console saw a text packet, but the real payload requires a framed FromRadio.packet";
    addDebugPacket(state, debug);
    addProtocolEvent(state, logger_, "console text packet from " + msg.senderId + "; waiting for framed payload");
}

void MeshtasticProtocol::recordRawFrame(const std::vector<uint8_t> &payload, AppState &state) {
    DebugPacket debug;
    debug.time = static_cast<uint32_t>(std::time(nullptr));
    debug.title = "Raw FromRadio";
    debug.summary = "FromRadio frame len=" + std::to_string(static_cast<unsigned>(payload.size()));
    debug.meshFields = "raw hex: " + hexPreview(payload, 40);
    debug.dataFields = "all serial frames are logged before parsing";
    debug.decoded = "parser result follows in Stream/Debug";
    addDebugPacket(state, debug);
    if (logger_) {
        logger_->line("RXRAW FromRadio len " + std::to_string(static_cast<unsigned>(payload.size())) +
                      " hex " + hexPreview(payload, 64));
    }
}

bool MeshtasticProtocol::parseFromRadio(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    uint32_t fromRadioId = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 0) {
            uint64_t id = 0;
            r.readVarint(id);
            fromRadioId = static_cast<uint32_t>(id);
        } else if (wire == 2 && (field == 2 || field == 3 || field == 4 || field == 5 || field == 6 ||
                                  field == 9 || field == 10 || field == 11 || field == 13 || field == 16)) {
            std::vector<uint8_t> nested;
            if (!r.readBytes(nested)) return false;
            if (field == 2) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " packet");
                parsePacket(nested, state);
            }
            if (field == 3) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " my_info");
                parseMyInfo(nested, state);
            }
            if (field == 4) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " node_info");
                parseNodeInfo(nested, state);
            }
            if (field == 5) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " config");
                parseConfig(nested, state);
            }
            if (field == 6) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " log_record");
                parseLogRecord(nested, state);
            }
            if (field == 9) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " module_config");
            }
            if (field == 10) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " channel");
                parseChannel(nested, state);
            }
            if (field == 11) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " queue_status");
            }
            if (field == 13) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " metadata");
                parseMetadata(nested, state);
            }
            if (field == 16) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " client_notification");
            }
        } else if (wire == 0 && field == 7) {
            uint64_t id = 0;
            r.readVarint(id);
            state.protocolReady = true;
            addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) +
                           " config_complete " + std::to_string(static_cast<unsigned>(id)));
            addProtocolEvent(state, logger_, "config complete id " + std::to_string(static_cast<unsigned>(id)));
        } else {
            addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) +
                           " field " + std::to_string(field) + " wire " + std::to_string(wire));
            if (!r.skip(wire)) {
                addProtocolEvent(state, logger_, "fromRadio skip failed field " + std::to_string(field) +
                                               " wire " + std::to_string(wire));
                addStreamEvent(state, logger_, "RAK -> Wii malformed FromRadio field " +
                               std::to_string(field) + " wire " + std::to_string(wire) +
                               " len " + std::to_string(static_cast<unsigned>(payload.size())));
                addStreamEvent(state, logger_, "payload hex " + hexPreview(payload, 24));
                return false;
            }
        }
    }
    return r.ok();
}

void MeshtasticProtocol::updatePrimaryFallback(AppState &state) {
    auto current = channels_.find(0);
    if (current == channels_.end() || current->second.empty() || current->second == "Primary") {
        channels_[0] = primaryFallbackName_;
        state.channelName = primaryFallbackName_;
    }
}

void MeshtasticProtocol::parseConfig(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 6 && wire == 2) {
            std::vector<uint8_t> lora;
            if (!r.readBytes(lora)) return;
            parseLoraConfig(lora, state);
        } else {
            r.skip(wire);
        }
    }
}

void MeshtasticProtocol::parseLoraConfig(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 2 && wire == 0) {
            uint64_t preset = 0;
            r.readVarint(preset);
            primaryFallbackName_ = modemPresetName(static_cast<uint32_t>(preset));
            updatePrimaryFallback(state);
            addProtocolEvent(state, logger_, "lora preset " + primaryFallbackName_);
        } else {
            r.skip(wire);
        }
    }
}

void MeshtasticProtocol::parseLogRecord(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    std::string message;
    std::string source;
    uint32_t level = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 2) {
            r.readString(message);
        } else if (field == 2 && wire == 2) {
            r.readString(source);
        } else if (field == 3 && wire == 0) {
            uint64_t v = 0;
            r.readVarint(v);
        } else if (field == 4 && wire == 0) {
            uint64_t v = 0;
            r.readVarint(v);
            level = static_cast<uint32_t>(v);
        } else {
            r.skip(wire);
        }
    }
    std::string line = "log";
    if (level) line += " level " + std::to_string(level);
    if (!source.empty()) line += " [" + source + "]";
    if (!message.empty()) line += " " + message;
    addStreamEvent(state, logger_, line);

    DebugPacket debug;
    debug.time = static_cast<uint32_t>(std::time(nullptr));
    debug.title = "LogRecord";
    debug.summary = line;
    debug.meshFields = "FromRadio.log_record";
    debug.dataFields = "payload hex: " + hexPreview(payload, 40);
    debug.decoded = message.empty() ? "Decoded Payload: log record" : "Decoded Payload: " + message;
    addDebugPacket(state, debug);
}

void MeshtasticProtocol::parseMetadata(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    std::string firmware;
    std::string fields;
    while (r.next(field, wire)) {
        char fw[16];
        std::snprintf(fw, sizeof(fw), "%u:%u ", field, wire);
        if (fields.size() < 96) fields += fw;
        if (field == 1 && wire == 2) {
            r.readString(firmware);
        } else {
            r.skip(wire);
        }
    }

    std::string summary = firmware.empty() ? "Device metadata" : "Device metadata firmware " + firmware;
    addStreamEvent(state, logger_, summary);

    DebugPacket debug;
    debug.time = static_cast<uint32_t>(std::time(nullptr));
    debug.title = "Metadata";
    debug.summary = summary;
    debug.meshFields = "FromRadio.metadata";
    debug.dataFields = fields.empty() ? "metadata fields: none" : "metadata fields: " + fields;
    debug.decoded = firmware.empty() ? "Decoded Payload: metadata" : "Decoded Payload: firmware " + firmware;
    addDebugPacket(state, debug);
}

void MeshtasticProtocol::updateKnownNodes(AppState &state) {
    state.knownNodes.clear();
    for (const auto &entry : nodeNames_) {
        NodeSummary node;
        node.id = entry.first;
        node.nodeId = nodeId(entry.first);
        node.name = entry.second.empty() ? node.nodeId : entry.second;
        node.isMine = entry.first == myNode_;
        state.knownNodes.push_back(node);
        if (node.isMine && state.nodeName == state.myNodeId) {
            state.nodeName = node.name;
        }
    }
    state.nodeCount = static_cast<uint32_t>(state.knownNodes.size());
}

void MeshtasticProtocol::parseMyInfo(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 0) {
            uint64_t n = 0;
            r.readVarint(n);
            myNode_ = static_cast<uint32_t>(n);
            state.myNodeId = nodeId(myNode_);
            state.nodeName = state.myNodeId;
            updateKnownNodes(state);
            addProtocolEvent(state, logger_, "my node " + state.nodeName);
        } else {
            r.skip(wire);
        }
    }
}

void MeshtasticProtocol::parseNodeInfo(const std::vector<uint8_t> &payload, AppState &state) {
    uint32_t num = 0;
    std::string name;
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 0) {
            uint64_t n = 0;
            r.readVarint(n);
            num = static_cast<uint32_t>(n);
        } else if (field == 2 && wire == 2) {
            std::vector<uint8_t> user;
            r.readBytes(user);
            ProtoReader u(user.data(), user.size());
            uint32_t uf = 0, uw = 0;
            std::string longName, shortName, id;
            while (u.next(uf, uw)) {
                if (uf == 1 && uw == 2) u.readString(id);
                else if (uf == 2 && uw == 2) u.readString(longName);
                else if (uf == 3 && uw == 2) u.readString(shortName);
                else u.skip(uw);
            }
            name = !longName.empty() ? longName : (!shortName.empty() ? shortName : id);
        } else {
            r.skip(wire);
        }
    }
    if (num != 0 && !name.empty()) {
        const bool wasNew = nodeNames_.find(num) == nodeNames_.end();
        nodeNames_[num] = name;
        updateKnownNodes(state);
        if (num == myNode_) {
            state.nodeName = name;
            state.myNodeId = nodeId(num);
        }
        if (wasNew || num == myNode_) {
            addProtocolEvent(state, logger_, "node " + nodeId(num) + " " + name);
        }
    }
}

void MeshtasticProtocol::parseChannel(const std::vector<uint8_t> &payload, AppState &state) {
    int index = 0;
    std::string name;
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 0) {
            uint64_t n = 0;
            r.readVarint(n);
            index = static_cast<int>(n);
        } else if (field == 2 && wire == 2) {
            std::vector<uint8_t> settings;
            r.readBytes(settings);
            ProtoReader s(settings.data(), settings.size());
            uint32_t sf = 0, sw = 0;
            while (s.next(sf, sw)) {
                if (sf == 3 && sw == 2) s.readString(name);
                else s.skip(sw);
            }
        } else {
            r.skip(wire);
        }
    }
    if (index >= 0 && index < 256) {
        channels_[static_cast<uint8_t>(index)] = name.empty() && index == 0 ? primaryFallbackName_ : name;
        if (index == 0) state.channelName = channels_[0];
        addProtocolEvent(state, logger_, "channel " + std::to_string(index) + " " +
                         channels_[static_cast<uint8_t>(index)]);
    }
}

void MeshtasticProtocol::parsePacket(const std::vector<uint8_t> &payload, AppState &state) {
    Message msg;
    msg.to = BroadcastNode;
    msg.rxTime = static_cast<uint32_t>(std::time(nullptr));

    std::vector<uint8_t> decoded;
    bool hasDecoded = false;
    bool hasEncrypted = false;
    std::string meshFields;
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        char fw[16];
        std::snprintf(fw, sizeof(fw), "%u:%u ", field, wire);
        if (meshFields.size() < 96) meshFields += fw;
        if (field == 1 && wire == 5) r.readFixed32(msg.from);
        else if (field == 2 && wire == 5) r.readFixed32(msg.to);
        else if (field == 3 && wire == 0) {
            uint64_t ch = 0;
            r.readVarint(ch);
            msg.channelIndex = static_cast<uint8_t>(ch);
        } else if (field == 4 && wire == 2) {
            r.readBytes(decoded);
            hasDecoded = true;
        } else if (field == 5 && wire == 2) {
            std::vector<uint8_t> encrypted;
            r.readBytes(encrypted);
            hasEncrypted = true;
        }
        else if (field == 7 && wire == 5) r.readFixed32(msg.rxTime);
        else r.skip(wire);
    }

    state.packetCount++;
    uint32_t port = 0;
    std::string text;
    std::string dataFields;
    if (!decoded.empty()) {
        state.decodedPacketCount++;
        ProtoReader d(decoded.data(), decoded.size());
        while (d.next(field, wire)) {
            char fw[16];
            std::snprintf(fw, sizeof(fw), "%u:%u ", field, wire);
            if (dataFields.size() < 96) dataFields += fw;
            if (field == 1 && wire == 0) {
                uint64_t p = 0;
                d.readVarint(p);
                port = static_cast<uint32_t>(p);
            } else if (field == 2 && wire == 2) {
                d.readString(text);
            } else {
                if (!d.skip(wire)) {
                    addProtocolEvent(state, logger_, "data skip failed field " + std::to_string(field) +
                                                   " wire " + std::to_string(wire));
                    return;
                }
            }
        }
    }

    state.lastPacketFrom = msg.from;
    state.lastPacketTo = msg.to;
    state.lastPacketPort = port;
    state.lastPacketChannel = msg.channelIndex;

    std::string direction = msg.to == BroadcastNode ? "channel" : "direct";
    DebugPacket debug;
    debug.time = msg.rxTime;
    debug.title = "Packet";
    debug.summary = "MeshPacket{from=" + nodeId(msg.from) + ", to=" + nodeId(msg.to) +
                    ", channel=" + std::to_string(msg.channelIndex) +
                    ", port=" + portName(port) +
                    ", decoded=" + std::to_string(static_cast<unsigned>(decoded.size())) +
                    (hasEncrypted ? ", encrypted=true" : ", encrypted=false") + "}";
    debug.meshFields = "mesh fields: " + meshFields;
    debug.dataFields = dataFields.empty() ? "decoded: none" : "data fields: " + dataFields;
    if (port == PortTextMessage && !text.empty()) {
        debug.decoded = "Decoded Payload: " + text;
    } else if (port == PortRouting) {
        debug.decoded = "Decoded Payload: ROUTING_APP delivery/status packet";
    } else {
        debug.decoded = "Decoded Payload: " + portName(port);
    }
    addDebugPacket(state, debug);

    addStreamEvent(state, logger_, "packet " + nodeId(msg.from) + " -> " + nodeId(msg.to) +
                   " ch " + std::to_string(msg.channelIndex) +
                   " port " + std::to_string(port) + " " + portName(port) + " " + direction +
                   " decoded " + std::to_string(static_cast<unsigned>(decoded.size())) +
                   (hasEncrypted ? " encrypted" : ""));
    addStreamEvent(state, logger_, "mesh fields " + meshFields +
                   (dataFields.empty() ? "" : " data " + dataFields));

    if (!hasDecoded || decoded.empty()) {
        addProtocolEvent(state, logger_, std::string("packet without decoded payload ") +
                         (hasEncrypted ? "encrypted" : "no-data"));
        return;
    }
    if (port == PortRouting) {
        Message receipt;
        receipt.from = msg.from;
        receipt.to = msg.to;
        receipt.rxTime = msg.rxTime;
        receipt.channelIndex = msg.channelIndex;
        receipt.direct = true;
        receipt.senderId = nodeId(msg.from);
        auto known = nodeNames_.find(msg.from);
        receipt.senderName = known == nodeNames_.end() ? receipt.senderId : known->second;
        receipt.channelName = "Routing";
        receipt.text = "Routing/ACK packet received";
        state.messages.push_back(receipt);
        addProtocolEvent(state, logger_, "routing packet from " + nodeId(msg.from) +
                         " decoded " + std::to_string(static_cast<unsigned>(decoded.size())));
        while (static_cast<int>(state.messages.size()) > MaxMessages) {
            state.messages.erase(state.messages.begin());
        }
        return;
    }
    if (port != PortTextMessage || text.empty()) {
        if (port != 0) {
            addProtocolEvent(state, logger_, "packet " + portName(port) + " " + std::to_string(port) +
                             " from " + nodeId(msg.from));
        } else {
            addProtocolEvent(state, logger_, "decoded packet with no port/text from " + nodeId(msg.from));
        }
        return;
    }
    msg.text = text;
    msg.direct = msg.to != BroadcastNode;
    msg.senderId = nodeId(msg.from);
    auto known = nodeNames_.find(msg.from);
    msg.senderName = known == nodeNames_.end() ? msg.senderId : known->second;
    auto channel = channels_.find(msg.channelIndex);
    msg.channelName = channel == channels_.end() || channel->second.empty() ? primaryFallbackName_ : channel->second;
    state.messages.push_back(msg);
    state.textMessageCount++;
    addProtocolEvent(state, logger_, std::string(msg.direct ? "direct text " : "channel text ") +
                     msg.senderName + ": " + msg.text.substr(0, 40));
    while (static_cast<int>(state.messages.size()) > MaxMessages) {
        state.messages.erase(state.messages.begin());
    }
}

std::vector<uint8_t> buildMockTextFromRadio(uint32_t from, uint32_t to, uint8_t channel, const std::string &text) {
    std::vector<uint8_t> data;
    appendVarint(data, 1, PortTextMessage);
    appendString(data, 2, text);

    std::vector<uint8_t> packet;
    appendFixed32(packet, 1, from);
    appendFixed32(packet, 2, to);
    appendVarint(packet, 3, channel);
    appendBytes(packet, 4, data);
    appendFixed32(packet, 7, 1234567890);

    std::vector<uint8_t> fromRadio;
    appendBytes(fromRadio, 2, packet);
    return fromRadio;
}

std::vector<uint8_t> buildTextToRadioFrame(uint32_t to, uint8_t channel, const std::string &text, bool wantAck) {
    std::vector<uint8_t> data;
    appendVarint(data, 1, PortTextMessage);
    appendString(data, 2, text);

    std::vector<uint8_t> packet;
    appendFixed32(packet, 2, to);
    appendVarint(packet, 3, channel);
    appendBytes(packet, 4, data);
    if (wantAck) {
        appendVarint(packet, 10, 1);
    }

    std::vector<uint8_t> toRadio;
    appendBytes(toRadio, 1, packet);

    std::vector<uint8_t> frame = {0x94, 0xc3,
        static_cast<uint8_t>((toRadio.size() >> 8) & 0xff),
        static_cast<uint8_t>(toRadio.size() & 0xff)};
    frame.insert(frame.end(), toRadio.begin(), toRadio.end());
    return frame;
}

}
