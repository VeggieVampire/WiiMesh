#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/ProtoReader.h"

#include <ctime>

namespace wiimesh {

constexpr uint32_t PortTextMessage = 1;

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
    while (state.streamEvents.size() > 10) {
        state.streamEvents.erase(state.streamEvents.begin());
    }
    if (logger) {
        logger->line("STREAM " + text);
    }
}

MeshtasticProtocol::MeshtasticProtocol(Logger *logger) : logger_(logger) {
    channels_[0] = "Primary";
}

void MeshtasticProtocol::reset() {
    frameState_ = NeedStart1;
    frameLen_ = 0;
    frame_.clear();
}

bool MeshtasticProtocol::startConfig(Transport &transport) {
    std::vector<uint8_t> toRadio;
    appendVarint(toRadio, 3, wantConfigId_++); // ToRadio.want_config_id

    std::vector<uint8_t> frame = {0x94, 0xc3,
        static_cast<uint8_t>((toRadio.size() >> 8) & 0xff),
        static_cast<uint8_t>(toRadio.size() & 0xff)};
    frame.insert(frame.end(), toRadio.begin(), toRadio.end());
    const bool ok = transport.write(frame.data(), frame.size()) == static_cast<int>(frame.size());
    if (ok && logger_) {
        logger_->line("STREAM Wii -> RAK ToRadio want_config_id " + std::to_string(wantConfigId_ - 1));
    }
    return ok;
}

void MeshtasticProtocol::poll(Transport &transport, AppState &state) {
    uint8_t buffer[128];
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
        frameState_ = byte == 0xc3 ? NeedLen1 : NeedStart1;
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

bool MeshtasticProtocol::parseFromRadio(const std::vector<uint8_t> &payload, AppState &state) {
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    uint32_t fromRadioId = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 0) {
            uint64_t id = 0;
            r.readVarint(id);
            fromRadioId = static_cast<uint32_t>(id);
        } else if (wire == 2 && (field == 2 || field == 3 || field == 4 || field == 10)) {
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
            if (field == 10) {
                addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) + " channel");
                parseChannel(nested, state);
            }
        } else if (wire == 0 && field == 7) {
            uint64_t id = 0;
            r.readVarint(id);
            state.protocolReady = true;
            addStreamEvent(state, logger_, "RAK -> Wii FromRadio#" + std::to_string(fromRadioId) +
                           " config_complete " + std::to_string(static_cast<unsigned>(id)));
            addProtocolEvent(state, logger_, "config complete id " + std::to_string(static_cast<unsigned>(id)));
        } else {
            if (!r.skip(wire)) {
                addProtocolEvent(state, logger_, "fromRadio skip failed field " + std::to_string(field) +
                                               " wire " + std::to_string(wire));
                return false;
            }
        }
    }
    return r.ok();
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
        channels_[static_cast<uint8_t>(index)] = name.empty() ? "Primary" : name;
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
    ProtoReader r(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (r.next(field, wire)) {
        if (field == 1 && wire == 5) r.readFixed32(msg.from);
        else if (field == 2 && wire == 5) r.readFixed32(msg.to);
        else if (field == 3 && wire == 0) {
            uint64_t ch = 0;
            r.readVarint(ch);
            msg.channelIndex = static_cast<uint8_t>(ch);
        } else if (field == 4 && wire == 2) r.readBytes(decoded);
        else if (field == 7 && wire == 5) r.readFixed32(msg.rxTime);
        else r.skip(wire);
    }

    if (decoded.empty()) return;
    uint32_t port = 0;
    std::string text;
    ProtoReader d(decoded.data(), decoded.size());
    while (d.next(field, wire)) {
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

    state.lastPacketFrom = msg.from;
    state.lastPacketTo = msg.to;
    state.lastPacketPort = port;
    state.lastPacketChannel = msg.channelIndex;

    std::string direction = msg.to == BroadcastNode ? "channel" : "direct";
    addStreamEvent(state, logger_, "packet " + nodeId(msg.from) + " -> " + nodeId(msg.to) +
                   " ch " + std::to_string(msg.channelIndex) +
                   " port " + std::to_string(port) + " " + direction);

    if (port != PortTextMessage || text.empty()) {
        if (port != 0) {
            addProtocolEvent(state, logger_, "packet port " + std::to_string(port) +
                             " from " + nodeId(msg.from));
        }
        return;
    }
    msg.text = text;
    msg.direct = msg.to != BroadcastNode;
    msg.senderId = nodeId(msg.from);
    auto known = nodeNames_.find(msg.from);
    msg.senderName = known == nodeNames_.end() ? msg.senderId : known->second;
    auto channel = channels_.find(msg.channelIndex);
    msg.channelName = channel == channels_.end() ? "Primary" : channel->second;
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

}
