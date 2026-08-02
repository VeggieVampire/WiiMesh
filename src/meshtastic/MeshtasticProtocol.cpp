#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/ProtoReader.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#if defined(WIIMESH_WII)
#include <unistd.h>
#endif

namespace wiimesh {

constexpr uint32_t PortTextMessage = 1;
constexpr uint32_t PortPosition = 3;
constexpr uint32_t PortRouting = 5;

static bool startsWith(const std::string &text, const char *prefix) {
    const size_t n = std::strlen(prefix);
    return text.size() >= n && text.compare(0, n, prefix) == 0;
}

static std::string trimText(std::string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    if (start > 0) text.erase(0, start);
    return text;
}

static std::string lowerAscii(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool isMeshFileCommand(const std::string &text) {
    return startsWith(text, "[START] ") || startsWith(text, "[CHUNK] ") || startsWith(text, "[END] ");
}

static bool safeMeshFileName(const std::string &name) {
    if (name.empty() || name.size() > 48) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '.' || c == '_' || c == '-') continue;
        return false;
    }
    return true;
}

static MeshFileTransfer &meshFileSlot(AppState &state, uint32_t sender, const std::string &filename) {
    for (auto &transfer : state.meshFiles) {
        if (transfer.sender == sender && transfer.filename == filename && !transfer.saved) return transfer;
    }
    for (auto &transfer : state.meshFiles) {
        if (transfer.filename == filename && !transfer.saved) {
            if (transfer.sender == 0) {
                transfer.sender = sender;
                transfer.senderId = nodeId(sender);
            }
            return transfer;
        }
    }
    MeshFileTransfer transfer;
    transfer.filename = filename;
    transfer.sender = sender;
    transfer.senderId = nodeId(sender);
    transfer.started = static_cast<uint32_t>(std::time(nullptr));
    transfer.updated = transfer.started;
    state.meshFiles.push_back(transfer);
    while (state.meshFiles.size() > 8) {
        state.meshFiles.erase(state.meshFiles.begin());
    }
    return state.meshFiles.back();
}

static void queueMeshFileAck(AppState &state, uint32_t to, const std::string &text) {
    if (to == 0) return;
    state.pendingTexts.push_back({to, 0, true, false, text});
}

static bool processMeshFileText(AppState &state, uint32_t sender, const std::string &text, bool direct) {
    const uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (!direct) {
        state.usbStatus = "MeshFile ignored";
        state.usbDetail = "Use direct chat only";
        state.meshFileRevision++;
        return true;
    }
    if (startsWith(text, "[START] ")) {
        std::istringstream parts(text.substr(8));
        std::string filename;
        std::string mode;
        if (!(parts >> filename)) return false;
        if (!safeMeshFileName(filename)) return false;
        MeshFileTransfer &transfer = meshFileSlot(state, sender, filename);
        transfer.started = now;
        transfer.updated = now;
        transfer.complete = false;
        transfer.saved = false;
        transfer.base64 = false;
        transfer.autoplayTried = false;
        transfer.savedPath.clear();
        transfer.chunks.clear();
        transfer.receivedChunks = 0;
        transfer.totalChunks = 0;
        if (parts >> mode) {
            const std::string lower = lowerAscii(mode);
            transfer.base64 = lower == "b64" || lower == "base64";
        }
        state.usbStatus = "MeshFile started";
        state.usbDetail = "Receiving " + filename;
        queueMeshFileAck(state, sender, "[MFACK] " + filename + " START");
        state.meshFileRevision++;
        return true;
    }
    if (startsWith(text, "[CHUNK] ")) {
        std::istringstream parts(text.substr(8));
        std::string chunkInfo;
        std::string filename;
        if (!(parts >> chunkInfo >> filename)) return false;
        if (!safeMeshFileName(filename)) return false;
        const size_t slash = chunkInfo.find('/');
        if (slash == std::string::npos) return false;
        const int chunkIndex = std::atoi(chunkInfo.substr(0, slash).c_str());
        const int totalChunks = std::atoi(chunkInfo.substr(slash + 1).c_str());
        if (chunkIndex < 1 || totalChunks < 1 || chunkIndex > totalChunks || totalChunks > 256) return false;
        std::string chunkData;
        std::getline(parts, chunkData);
        if (!chunkData.empty() && chunkData.front() == ' ') chunkData.erase(chunkData.begin());

        MeshFileTransfer &transfer = meshFileSlot(state, sender, filename);
        if (transfer.totalChunks != totalChunks) {
            transfer.totalChunks = totalChunks;
            transfer.chunks.assign(static_cast<size_t>(totalChunks), std::string());
            transfer.receivedChunks = 0;
            transfer.complete = false;
            transfer.saved = false;
            transfer.savedPath.clear();
        }
        std::string &slot = transfer.chunks[static_cast<size_t>(chunkIndex - 1)];
        if (slot.empty()) {
            ++transfer.receivedChunks;
        }
        if (startsWith(chunkData, "b64:")) {
            transfer.base64 = true;
        }
        slot = chunkData;
        transfer.updated = now;
        transfer.complete = transfer.receivedChunks == transfer.totalChunks;
        state.usbStatus = transfer.complete ? "MeshFile complete" : "MeshFile receiving";
        state.usbDetail = filename + " " + std::to_string(transfer.receivedChunks) +
                          "/" + std::to_string(transfer.totalChunks);
        queueMeshFileAck(state, sender, "[MFACK] " + filename + " CHUNK " +
                         std::to_string(chunkIndex) + "/" + std::to_string(totalChunks) +
                         " RX " + std::to_string(transfer.receivedChunks) + "/" +
                         std::to_string(transfer.totalChunks));
        state.meshFileRevision++;
        return true;
    }
    if (startsWith(text, "[END] ")) {
        const std::string filename = trimText(text.substr(6));
        if (!safeMeshFileName(filename)) return false;
        MeshFileTransfer &transfer = meshFileSlot(state, sender, filename);
        transfer.updated = now;
        transfer.complete = transfer.totalChunks > 0 && transfer.receivedChunks == transfer.totalChunks;
        state.usbStatus = transfer.complete ? "MeshFile ready to save" : "MeshFile missing chunks";
        state.usbDetail = filename + " " + std::to_string(transfer.receivedChunks) +
                          "/" + std::to_string(transfer.totalChunks);
        queueMeshFileAck(state, sender, "[MFACK] " + filename + " END " +
                         std::string(transfer.complete ? "COMPLETE " : "MISSING ") +
                         std::to_string(transfer.receivedChunks) + "/" +
                         std::to_string(transfer.totalChunks));
        state.meshFileRevision++;
        return true;
    }
    return false;
}

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

static bool markLatestOutgoingDelivered(AppState &state, uint32_t peer) {
    for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0; --i) {
        Message &m = state.messages[static_cast<size_t>(i)];
        if (!m.outgoing || m.delivered) {
            continue;
        }
        if (peer == 0 || m.to == peer || m.from == peer || m.to == BroadcastNode) {
            m.delivered = true;
            state.messageRevision++;
            return true;
        }
    }
    return false;
}

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

static float fixed32ToFloat(uint32_t bits) {
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(bits), "float must be 32-bit");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static std::string formatMacAddress(const std::vector<uint8_t> &bytes) {
    if (bytes.empty()) return "";
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    const size_t n = std::min<size_t>(6, bytes.size());
    for (size_t i = 0; i < n; ++i) {
        if (i) out += ':';
        const uint8_t b = bytes[i];
        out += hex[(b >> 4) & 0x0f];
        out += hex[b & 0x0f];
    }
    return out;
}

static std::string utf8CodepointSummary(const std::string &text) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t advance = 1;
        if ((c & 0x80) == 0) {
            ++i;
            continue;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
            cp = ((c & 0x1f) << 6) |
                (static_cast<unsigned char>(text[i + 1]) & 0x3f);
            advance = 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
            cp = ((c & 0x0f) << 12) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 2]) & 0x3f);
            advance = 3;
        } else if ((c & 0xf8) == 0xf0 && i + 3 < text.size()) {
            cp = ((c & 0x07) << 18) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) |
                ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 3]) & 0x3f);
            advance = 4;
        }
        if (cp != 0) {
            if (!out.empty()) out += ' ';
            out += "U+";
            bool started = false;
            for (int shift = 28; shift >= 0; shift -= 4) {
                const unsigned digit = (cp >> shift) & 0x0f;
                if (digit || started || shift == 0) {
                    out += hex[digit];
                    started = true;
                }
            }
        }
        i += advance;
    }
    return out.empty() ? "ascii only" : out;
}

static bool hasUtf8IconCandidate(const std::string &text) {
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t advance = 1;
        if ((c & 0x80) == 0) {
            ++i;
            continue;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
            cp = ((c & 0x1f) << 6) |
                (static_cast<unsigned char>(text[i + 1]) & 0x3f);
            advance = 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
            cp = ((c & 0x0f) << 12) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 2]) & 0x3f);
            advance = 3;
        } else if ((c & 0xf8) == 0xf0 && i + 3 < text.size()) {
            cp = ((c & 0x07) << 18) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) |
                ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 3]) & 0x3f);
            advance = 4;
        }
        if (cp >= 0x2300 && cp != 0xfe0f && cp != 0x200d && cp != 0x2b1b &&
            !(cp >= 0x1f3fb && cp <= 0x1f3ff)) {
            return true;
        }
        i += advance;
    }
    return false;
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

static bool parseFloatField(const std::string &line, const std::string &key, float &value) {
    const size_t p = line.find(key);
    if (p == std::string::npos) return false;
    char *end = nullptr;
    const double out = std::strtod(line.c_str() + p + key.size(), &end);
    if (end == line.c_str() + p + key.size()) return false;
    value = static_cast<float>(out);
    return true;
}

static bool parseIntField(const std::string &line, const std::string &key, int32_t &value) {
    const size_t p = line.find(key);
    if (p == std::string::npos) return false;
    char *end = nullptr;
    const long out = std::strtol(line.c_str() + p + key.size(), &end, 10);
    if (end == line.c_str() + p + key.size()) return false;
    value = static_cast<int32_t>(out);
    return true;
}

static bool findIpv4(const std::string &line, std::string &ip) {
    for (size_t i = 0; i < line.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(line[i]))) continue;
        size_t end = i;
        int dots = 0;
        while (end < line.size() &&
               (std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '.')) {
            if (line[end] == '.') ++dots;
            ++end;
        }
        if (dots != 3) continue;
        const std::string candidate = line.substr(i, end - i);
        int parts = 0;
        bool ok = true;
        size_t p = 0;
        while (p < candidate.size()) {
            size_t next = candidate.find('.', p);
            const std::string part = candidate.substr(p, next == std::string::npos ? std::string::npos : next - p);
            if (part.empty() || part.size() > 3) {
                ok = false;
                break;
            }
            char *stop = nullptr;
            const long value = std::strtol(part.c_str(), &stop, 10);
            if (stop == part.c_str() || *stop != '\0' || value < 0 || value > 255) {
                ok = false;
                break;
            }
            ++parts;
            if (next == std::string::npos) break;
            p = next + 1;
        }
        if (ok && parts == 4) {
            ip = candidate;
            return true;
        }
    }
    return false;
}

static void parseConsoleStatusLine(const std::string &line, AppState &state) {
    int32_t intValue = 0;
    float floatValue = 0.0f;
    if (parseFloatField(line, "air_util_tx=", floatValue)) state.airUtilTx = floatValue;
    if (parseFloatField(line, "channel_utilization=", floatValue)) state.channelUtilization = floatValue;
    if (parseIntField(line, "battery_level=", intValue)) state.batteryLevel = intValue;
    if (parseFloatField(line, "voltage=", floatValue)) state.voltage = floatValue;
    if (parseFloatField(line, "rxSNR=", floatValue)) state.lastRxSnr = floatValue;
    if (parseIntField(line, "rxRSSI=", intValue)) state.lastRxRssi = intValue;
    if (parseIntField(line, "rxRSSI ", intValue)) state.lastRxRssi = intValue;

    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if ((lower.find("wifi") != std::string::npos || lower.find("ip") != std::string::npos ||
         lower.find("http") != std::string::npos || lower.find("network") != std::string::npos) &&
        lower.find("rxrssi") == std::string::npos) {
        std::string ip;
        if (findIpv4(line, ip)) {
            state.meshDeviceIp = ip;
        }
    }

    const size_t online = line.find("Node status update:");
    if (online != std::string::npos) {
        char *end = nullptr;
        const char *start = line.c_str() + online + std::strlen("Node status update:");
        const long onlineCount = std::strtol(start, &end, 10);
        if (end != start) state.onlineNodeCount = static_cast<int32_t>(onlineCount);
        const size_t total = line.find("online,", online);
        if (total != std::string::npos) {
            start = line.c_str() + total + std::strlen("online,");
            const long totalCount = std::strtol(start, &end, 10);
            if (end != start) {
                state.totalNodeCount = static_cast<int32_t>(totalCount);
                state.nodeCount = static_cast<uint32_t>(totalCount);
            }
        }
    }
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
    parseConsoleStatusLine(line, state);
    if (line.find("decoded message") == std::string::npos || line.find("Portnum=1") == std::string::npos) {
        return;
    }
    uint32_t from = 0;
    uint32_t to = 0;
    uint32_t channel = 0;
    parseHexField(line, "fr=", from);
    parseHexField(line, "to=", to);
    parseHexField(line, "Ch=", channel);

    const std::string fromId = nodeId(from);
    const std::string toId = nodeId(to);
    const bool fromThisWiiClient = from == 0 || from == myNode_ || fromId == state.myNodeId;
    const bool toThisWiiClient = to != BroadcastNode && to != 0 && toId == state.myNodeId;
    const bool consolePhoneSide =
        line.find("PACKET FROM PHON") != std::string::npos ||
        line.find("PACKET FROM PHONE") != std::string::npos ||
        line.find("phone downloaded packet") != std::string::npos;
    const bool consoleLooksInbound =
        !consolePhoneSide &&
        !fromThisWiiClient &&
        (to == BroadcastNode || toThisWiiClient || state.myNodeId == "waiting");

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
    addProtocolEvent(state, logger_, std::string(consoleLooksInbound ? "console inbound" : "console outbound/local") +
                     " TEXT_MESSAGE_APP notice from " + msg.senderId +
                     " to " + toId + "; waiting for framed payload");
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

std::string hardwareModelName(uint32_t model) {
    switch (model) {
    case 0: return "UNSET";
    case 9: return "RAK4631";
    case 16: return "TLORA_T3_S3";
    case 10: return "HELTEC_V2";
    case 43: return "HELTEC_V3";
    case 49: return "HELTEC_PAPER";
    case 50: return "T_DECK";
    case 71: return "TRACKER_T1000_E";
    case 84: return "WISMESH_TAP";
    case 95: return "SEEED_SOLAR";
    case 105: return "WISMESH_TAG";
    case 110: return "HELTEC_V4";
    default: break;
    }
    return "MODEL " + std::to_string(model);
}

void MeshtasticProtocol::updateKnownNodes(AppState &state) {
    state.knownNodes.clear();
    for (const auto &entry : nodeNames_) {
        NodeSummary node;
        node.id = entry.first;
        node.nodeId = nodeId(entry.first);
        node.name = entry.second.empty() ? node.nodeId : entry.second;
        auto shortEntry = nodeShortNames_.find(entry.first);
        node.shortName = shortEntry == nodeShortNames_.end() ? "" : shortEntry->second;
        auto macEntry = nodeMacAddresses_.find(entry.first);
        node.macAddress = macEntry == nodeMacAddresses_.end() ? "" : macEntry->second;
        auto hwEntry = nodeHardwareModels_.find(entry.first);
        node.hardwareModel = hwEntry == nodeHardwareModels_.end() ? 0 : hwEntry->second;
        node.hardwareName = hardwareModelName(node.hardwareModel);
        auto lastHeardEntry = nodeLastHeard_.find(entry.first);
        node.lastHeard = lastHeardEntry == nodeLastHeard_.end() ? 0 : lastHeardEntry->second;
        auto hopsEntry = nodeHopsAway_.find(entry.first);
        node.hopsAway = hopsEntry == nodeHopsAway_.end() ? -1 : hopsEntry->second;
        auto snrEntry = nodeSnr_.find(entry.first);
        node.snr = snrEntry == nodeSnr_.end() ? -1000.0f : snrEntry->second;
        auto batteryEntry = nodeBatteryLevels_.find(entry.first);
        node.batteryLevel = batteryEntry == nodeBatteryLevels_.end() ? -1 : batteryEntry->second;
        auto voltageEntry = nodeVoltages_.find(entry.first);
        node.voltage = voltageEntry == nodeVoltages_.end() ? 0.0f : voltageEntry->second;
        auto latEntry = nodeLatitudes_.find(entry.first);
        auto lonEntry = nodeLongitudes_.find(entry.first);
        node.hasPosition = latEntry != nodeLatitudes_.end() && lonEntry != nodeLongitudes_.end() &&
                           latEntry->second != 0 && lonEntry->second != 0;
        if (node.hasPosition) {
            node.latitudeI = latEntry->second;
            node.longitudeI = lonEntry->second;
            auto altEntry = nodeAltitudes_.find(entry.first);
            node.altitude = altEntry == nodeAltitudes_.end() ? 0 : altEntry->second;
            auto posTimeEntry = nodePositionTimes_.find(entry.first);
            node.positionTime = posTimeEntry == nodePositionTimes_.end() ? 0 : posTimeEntry->second;
            auto precisionEntry = nodePrecisionBits_.find(entry.first);
            node.precisionBits = precisionEntry == nodePrecisionBits_.end() ? 0 : precisionEntry->second;
        }
        node.isMine = entry.first == myNode_;
        state.knownNodes.push_back(node);
        if (node.isMine && state.nodeName == state.myNodeId) {
            state.nodeName = node.name;
        }
    }
    std::stable_sort(state.knownNodes.begin(), state.knownNodes.end(),
        [](const NodeSummary &a, const NodeSummary &b) {
            const bool ai = hasUtf8IconCandidate(a.shortName) || hasUtf8IconCandidate(a.name);
            const bool bi = hasUtf8IconCandidate(b.shortName) || hasUtf8IconCandidate(b.name);
            if (ai != bi) return ai;
            return a.id < b.id;
        });
    state.nodeCount = static_cast<uint32_t>(state.knownNodes.size());
}

bool MeshtasticProtocol::parsePosition(uint32_t node, const std::vector<uint8_t> &payload, AppState &state) {
    if (node == 0 || payload.empty()) return false;
    int32_t lat = 0;
    int32_t lon = 0;
    int32_t altitude = 0;
    uint32_t time = 0;
    uint32_t precisionBits = 0;
    bool sawLat = false;
    bool sawLon = false;
    ProtoReader p(payload.data(), payload.size());
    uint32_t field = 0, wire = 0;
    while (p.next(field, wire)) {
        if (field == 1 && wire == 5) {
            uint32_t raw = 0;
            p.readFixed32(raw);
            lat = static_cast<int32_t>(raw);
            sawLat = true;
        } else if (field == 2 && wire == 5) {
            uint32_t raw = 0;
            p.readFixed32(raw);
            lon = static_cast<int32_t>(raw);
            sawLon = true;
        } else if (field == 3 && wire == 0) {
            uint64_t alt = 0;
            p.readVarint(alt);
            altitude = static_cast<int32_t>(alt);
        } else if ((field == 4 || field == 7) && wire == 5) {
            p.readFixed32(time);
        } else if (field == 23 && wire == 0) {
            uint64_t bits = 0;
            p.readVarint(bits);
            precisionBits = static_cast<uint32_t>(bits);
        } else {
            p.skip(wire);
        }
    }
    if (!sawLat || !sawLon || lat == 0 || lon == 0) return false;
    const bool changed = nodeLatitudes_[node] != lat || nodeLongitudes_[node] != lon ||
                         nodeAltitudes_[node] != altitude || nodePrecisionBits_[node] != precisionBits;
    nodeLatitudes_[node] = lat;
    nodeLongitudes_[node] = lon;
    nodeAltitudes_[node] = altitude;
    if (time != 0) nodePositionTimes_[node] = time;
    if (precisionBits != 0) nodePrecisionBits_[node] = precisionBits;
    if (nodeNames_.find(node) == nodeNames_.end()) {
        nodeNames_[node] = nodeId(node);
    }
    updateKnownNodes(state);
    if (changed) {
        state.mapRevision++;
        char line[160];
        std::snprintf(line, sizeof(line), "position %s lat %.7f lon %.7f alt %ld precision %lu",
                      nodeId(node).c_str(), lat / 10000000.0, lon / 10000000.0,
                      static_cast<long>(altitude), static_cast<unsigned long>(precisionBits));
        addProtocolEvent(state, logger_, line);
    }
    return true;
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
    std::string shortName;
    std::string macAddress;
    uint32_t hardwareModel = 0;
    uint32_t lastHeard = 0;
    int32_t hopsAway = -1;
    float snr = -1000.0f;
    int32_t batteryLevel = -1;
    float voltage = 0.0f;
    std::vector<uint8_t> position;
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
            std::string longName, id;
            while (u.next(uf, uw)) {
                if (uf == 1 && uw == 2) u.readString(id);
                else if (uf == 2 && uw == 2) u.readString(longName);
                else if (uf == 3 && uw == 2) u.readString(shortName);
                else if (uf == 4 && uw == 2) {
                    std::vector<uint8_t> mac;
                    u.readBytes(mac);
                    macAddress = formatMacAddress(mac);
                }
                else if (uf == 5 && uw == 0) {
                    uint64_t hw = 0;
                    u.readVarint(hw);
                    hardwareModel = static_cast<uint32_t>(hw);
                }
                else u.skip(uw);
            }
            name = !longName.empty() ? longName : (!shortName.empty() ? shortName : id);
        } else if (field == 3 && wire == 2) {
            r.readBytes(position);
        } else if (field == 4 && wire == 5) {
            uint32_t bits = 0;
            r.readFixed32(bits);
            snr = fixed32ToFloat(bits);
        } else if (field == 5 && wire == 5) {
            r.readFixed32(lastHeard);
        } else if (field == 6 && wire == 2) {
            std::vector<uint8_t> metrics;
            r.readBytes(metrics);
            ProtoReader m(metrics.data(), metrics.size());
            uint32_t mf = 0, mw = 0;
            while (m.next(mf, mw)) {
                if (mf == 1 && mw == 0) {
                    uint64_t b = 0;
                    m.readVarint(b);
                    batteryLevel = static_cast<int32_t>(b);
                } else if (mf == 2 && mw == 5) {
                    uint32_t bits = 0;
                    m.readFixed32(bits);
                    voltage = fixed32ToFloat(bits);
                } else {
                    m.skip(mw);
                }
            }
        } else if (field == 9 && wire == 0) {
            uint64_t hops = 0;
            r.readVarint(hops);
            hopsAway = static_cast<int32_t>(hops);
        } else {
            r.skip(wire);
        }
    }
    if (num != 0 && !name.empty()) {
        const bool wasNew = nodeNames_.find(num) == nodeNames_.end();
        nodeNames_[num] = name;
        nodeShortNames_[num] = shortName;
        if (!macAddress.empty()) nodeMacAddresses_[num] = macAddress;
        if (hardwareModel != 0) nodeHardwareModels_[num] = hardwareModel;
        if (lastHeard != 0) nodeLastHeard_[num] = lastHeard;
        if (hopsAway >= 0) nodeHopsAway_[num] = hopsAway;
        if (snr > -999.0f) nodeSnr_[num] = snr;
        if (batteryLevel >= 0) nodeBatteryLevels_[num] = batteryLevel;
        if (voltage > 0.0f) nodeVoltages_[num] = voltage;
        if (!position.empty() && parsePosition(num, position, state)) {
            addStreamEvent(state, logger_, "RAK -> Wii node position " + nodeId(num));
        }
        updateKnownNodes(state);
        if (num == myNode_) {
            state.nodeName = name;
            state.myNodeId = nodeId(num);
        }
        if (wasNew || num == myNode_) {
            addProtocolEvent(state, logger_, "node " + nodeId(num) + " " + name +
                             " short " + shortName + " utf8 long " + utf8CodepointSummary(name) +
                             " short " + utf8CodepointSummary(shortName) +
                             " hw " + hardwareModelName(hardwareModel) +
                             (macAddress.empty() ? "" : " mac " + macAddress));
        }
    }
}

void MeshtasticProtocol::parseChannel(const std::vector<uint8_t> &payload, AppState &state) {
    int index = 0;
    std::string name;
    int role = 0;
    bool hasPsk = false;
    size_t pskBytes = 0;
    uint32_t channelId = 0;
    bool uplinkEnabled = false;
    bool downlinkEnabled = false;
    bool muted = false;
    uint32_t positionPrecision = 0;
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
                if (sf == 2 && sw == 2) {
                    std::vector<uint8_t> psk;
                    s.readBytes(psk);
                    pskBytes = psk.size();
                    hasPsk = !psk.empty() && !(psk.size() == 1 && psk[0] == 0);
                } else if (sf == 3 && sw == 2) {
                    s.readString(name);
                } else if (sf == 4 && sw == 5) {
                    s.readFixed32(channelId);
                } else if (sf == 5 && sw == 0) {
                    uint64_t v = 0;
                    s.readVarint(v);
                    uplinkEnabled = v != 0;
                } else if (sf == 6 && sw == 0) {
                    uint64_t v = 0;
                    s.readVarint(v);
                    downlinkEnabled = v != 0;
                } else if (sf == 7 && sw == 2) {
                    std::vector<uint8_t> module;
                    s.readBytes(module);
                    ProtoReader m(module.data(), module.size());
                    uint32_t mf = 0, mw = 0;
                    while (m.next(mf, mw)) {
                        if (mf == 1 && mw == 0) {
                            uint64_t v = 0;
                            m.readVarint(v);
                            positionPrecision = static_cast<uint32_t>(v);
                        } else if (mf == 2 && mw == 0) {
                            uint64_t v = 0;
                            m.readVarint(v);
                            muted = v != 0;
                        } else {
                            m.skip(mw);
                        }
                    }
                } else {
                    s.skip(sw);
                }
            }
        } else if (field == 3 && wire == 0) {
            uint64_t rValue = 0;
            r.readVarint(rValue);
            role = static_cast<int>(rValue);
        } else {
            r.skip(wire);
        }
    }
    if (index >= 0 && index < 256) {
        channels_[static_cast<uint8_t>(index)] = name.empty() && index == 0 ? primaryFallbackName_ : name;
        if (index == 0) state.channelName = channels_[0];
        ChannelSummary summary;
        summary.index = index;
        summary.name = channels_[static_cast<uint8_t>(index)];
        summary.role = role;
        summary.hasPsk = hasPsk;
        summary.pskBytes = pskBytes;
        summary.channelId = channelId;
        summary.uplinkEnabled = uplinkEnabled;
        summary.downlinkEnabled = downlinkEnabled;
        summary.muted = muted;
        summary.positionPrecision = positionPrecision;
        bool replaced = false;
        for (auto &existing : state.channels) {
            if (existing.index == index) {
                existing = summary;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            state.channels.push_back(summary);
        }
        std::stable_sort(state.channels.begin(), state.channels.end(),
                         [](const ChannelSummary &a, const ChannelSummary &b) {
                             return a.index < b.index;
                         });
        addProtocolEvent(state, logger_, "channel " + std::to_string(index) + " " +
                         channels_[static_cast<uint8_t>(index)] + " role " + std::to_string(role) +
                         " psk " + std::to_string(static_cast<unsigned>(pskBytes)));
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
    std::vector<uint8_t> dataPayload;
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
                d.readBytes(dataPayload);
            } else {
                if (!d.skip(wire)) {
                    addProtocolEvent(state, logger_, "data skip failed field " + std::to_string(field) +
                                                   " wire " + std::to_string(wire));
                    return;
                }
            }
        }
    }
    if (port == PortTextMessage && !dataPayload.empty()) {
        text.assign(dataPayload.begin(), dataPayload.end());
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
    } else if (port == PortPosition) {
        debug.decoded = "Decoded Payload: POSITION_APP";
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
    if (port == PortPosition) {
        if (parsePosition(msg.from, dataPayload, state)) {
            addProtocolEvent(state, logger_, "position packet from " + nodeId(msg.from));
            addStreamEvent(state, logger_, "RAK -> Wii POSITION_APP map point " + nodeId(msg.from));
        } else {
            addProtocolEvent(state, logger_, "position packet without usable lat/lon from " + nodeId(msg.from));
        }
        return;
    }
    if (port == PortRouting) {
        const bool delivered = markLatestOutgoingDelivered(state, msg.from);
        addProtocolEvent(state, logger_, "routing packet from " + nodeId(msg.from) +
                         " decoded " + std::to_string(static_cast<unsigned>(decoded.size())) +
                         (delivered ? " delivery marked" : ""));
        state.usbStatus = delivered ? "Message delivered" : "Routing/ACK received";
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
    const bool meshFileCommand = isMeshFileCommand(msg.text);
    if (meshFileCommand && !msg.direct) {
        processMeshFileText(state, msg.from, msg.text, false);
        addProtocolEvent(state, logger_, "meshfile ignored channel command from " + msg.senderName);
        addStreamEvent(state, logger_, "MeshFile ignored channel command; send direct to Wii user");
        return;
    }
    const bool meshFileText = meshFileCommand && processMeshFileText(state, msg.from, msg.text, msg.direct);
    state.messages.push_back(msg);
    state.messageRevision++;
    state.textMessageCount++;
    addProtocolEvent(state, logger_, std::string(meshFileText ? "meshfile " : (msg.direct ? "direct text " : "channel text ")) +
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
