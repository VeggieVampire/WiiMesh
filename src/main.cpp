#include "wiimesh/Logger.h"
#include "wiimesh/MapDownloader.h"
#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/MessageStore.h"
#include "wiimesh/Transport.h"
#include "wiimesh/Ui.h"
#include "wiimesh/UsbTransport.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <dirent.h>
#include <string>
#include <vector>

#if defined(WIIMESH_WII)
#include <asndlib.h>
#include <fat.h>
#include <network.h>
#include <ogcsys.h>
#include <unistd.h>
#include <cstring>
#endif

using namespace wiimesh;

static const char *AppDir = "sd:/apps/wii-mesh";
static const char *ThemeDir = "sd:/apps/wii-mesh/theme";
static const char *MapTilesDir = "sd:/apps/wii-mesh/maps";
static const char *MeshFilesDir = "sd:/apps/wii-mesh/received_files";
static const char *DebugLogPath = "sd:/apps/wii-mesh/debug.log";
static const char *MessagesPath = "sd:/apps/wii-mesh/messages.dat";
static const char *MapPath = "sd:/apps/wii-mesh/mesh_map.dat";
static const char *MapUrlPath = "sd:/apps/wii-mesh/maps.url";
static const char *SettingsConfigPath = "sd:/apps/wii-mesh/Settings.config";
static constexpr unsigned short UdpLogPort = 44015;
static constexpr unsigned short UdpCommandPort = 44016;
static const char *DebugTargetPath = "sd:/apps/wii-mesh/debug-target.txt";
static const char *DefaultDebugTarget = "192.168.0.233";
static constexpr int WiiSockaddrInLen = 8;

static std::string loadDebugTarget() {
    FILE *f = std::fopen(DebugTargetPath, "rb");
    if (!f) {
        return DefaultDebugTarget;
    }
    char line[64] = {};
    std::fgets(line, sizeof(line), f);
    std::fclose(f);
    std::string out = line;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '\t')) {
        out.pop_back();
    }
    return out.empty() ? DefaultDebugTarget : out;
}

static std::string trimLine(std::string out) {
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '\t')) {
        out.pop_back();
    }
    while (!out.empty() && (out.front() == ' ' || out.front() == '\t')) {
        out.erase(out.begin());
    }
    return out;
}

static int clampInt(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static bool parseBoolValue(const std::string &value) {
    return value == "1" || value == "on" || value == "ON" || value == "true" ||
           value == "TRUE" || value == "yes" || value == "YES";
}

static bool applySettingsLine(AppState &state, const std::string &rawLine, std::string *message = nullptr) {
    std::string line = trimLine(rawLine);
    if (line.empty() || line[0] == '#') {
        return true;
    }
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
        if (message) *message = "missing '=' in " + line;
        return false;
    }
    const std::string key = trimLine(line.substr(0, eq));
    const std::string value = trimLine(line.substr(eq + 1));
    const int n = std::atoi(value.c_str());
    if (key == "font.style") {
        state.fontStyle = clampInt(n, 0, 3);
    } else if (key == "font.size") {
        state.fontSize = clampInt(n, 0, 6);
    } else if (key == "screensaver.mode") {
        state.screensaverMode = clampInt(n, 0, 2);
    } else if (key == "screensaver.speed") {
        state.screensaverSpeed = clampInt(n, 0, 5);
    } else if (key == "debug.enabled") {
        state.debugEnabled = parseBoolValue(value);
    } else if (key == "pointer.enabled") {
        state.pointerEnabled = parseBoolValue(value);
    } else if (key == "midi.repeat") {
        state.midiRepeat = parseBoolValue(value);
    } else {
        if (message) *message = "unknown setting " + key;
        return false;
    }
    if (message) *message = key + "=" + value;
    return true;
}

static std::string settingsExport(const AppState &state) {
    std::string out;
    out += "# WiiMesh live settings\n";
    out += "# Edit on SD then send UDP SETTINGS_RELOAD, or use SETTINGS_SET key=value.\n";
    out += "font.style=" + std::to_string(state.fontStyle) + "\n";
    out += "font.size=" + std::to_string(state.fontSize) + "\n";
    out += "screensaver.mode=" + std::to_string(state.screensaverMode) + "\n";
    out += "screensaver.speed=" + std::to_string(state.screensaverSpeed) + "\n";
    out += "debug.enabled=" + std::to_string(state.debugEnabled ? 1 : 0) + "\n";
    out += "pointer.enabled=" + std::to_string(state.pointerEnabled ? 1 : 0) + "\n";
    out += "midi.repeat=" + std::to_string(state.midiRepeat ? 1 : 0) + "\n";
    return out;
}

static bool saveSettingsConfig(const AppState &state) {
#if defined(WIIMESH_WII)
    mkdir("sd:/apps", 0777);
    mkdir(AppDir, 0777);
#endif
    FILE *f = std::fopen(SettingsConfigPath, "wb");
    if (!f) {
        return false;
    }
    const std::string text = settingsExport(state);
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    return ok;
}

static bool loadSettingsConfig(AppState &state, std::string *message = nullptr) {
    FILE *f = std::fopen(SettingsConfigPath, "rb");
    if (!f) {
        saveSettingsConfig(state);
        if (message) *message = "Settings.config created";
        return true;
    }
    char line[256] = {};
    int applied = 0;
    while (std::fgets(line, sizeof(line), f)) {
        std::string detail;
        if (applySettingsLine(state, line, &detail)) {
            if (!trimLine(line).empty() && trimLine(line)[0] != '#') ++applied;
        } else if (message) {
            *message = detail;
        }
    }
    std::fclose(f);
    if (message && message->empty()) {
        *message = "Settings.config loaded " + std::to_string(applied) + " settings";
    }
    return true;
}

static std::string loadMapUrlTemplate() {
    FILE *f = std::fopen(MapUrlPath, "rb");
    if (!f) {
        return "";
    }
    char line[512] = {};
    std::fgets(line, sizeof(line), f);
    std::fclose(f);
    return trimLine(line);
}

static bool saveMapUrlTemplate(const std::string &url) {
    FILE *f = std::fopen(MapUrlPath, "wb");
    if (!f) {
        return false;
    }
    std::fwrite(url.data(), 1, url.size(), f);
    std::fwrite("\n", 1, 1, f);
    std::fclose(f);
    return true;
}

static std::string safeStyleName(std::string s) {
    if (s.empty()) {
        return "default";
    }
    for (char &c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) c = '_';
    }
    return s;
}

static void logDevices(Logger &logger, const AppState &state);

#if defined(WIIMESH_ICON_DEBUG)
static NodeSummary sampleNode(uint32_t id, const char *name, const char *shortName,
                              uint32_t hardwareModel = 0, const char *hardwareName = "KNOWN",
                              bool mine = false) {
    NodeSummary node;
    node.id = id;
    node.nodeId = nodeId(id);
    node.name = name;
    node.shortName = shortName;
    node.hardwareModel = hardwareModel;
    node.hardwareName = hardwareName;
    node.lastHeard = static_cast<uint32_t>(std::time(nullptr));
    node.snr = id == 0x433bf1b8u ? 0.0f : (id == 0x61916fb3u ? -1.0f : 6.0f);
    node.hopsAway = mine ? 0 : (id == 0x433bf1b8u ? -1 : (id == 0x61916fb3u ? 2 : 1));
    node.batteryLevel = mine ? 65 : -1;
    node.voltage = mine ? 3.84f : 0.0f;
    node.isMine = mine;
    return node;
}

static void seedIconDebugState(AppState &state) {
    state.protocolReady = true;
    state.usbConnected = true;
    state.usbStatus = "Icon debug mock";
    state.usbDetail = "Meshtastic short-name badge test";
    state.myNodeId = "!cb05014c";
    state.nodeName = "pumpkinPulse";
    state.channelName = "LongFast";
    state.uiTab = 2;
    state.dashboardFocused = true;
    state.selectedNodeIndex = 2;
    state.knownNodes.push_back(sampleNode(0x433bf1b8u, "Mesheteer", "\xf0\x9f\xa4\xba", 43, "HELTEC_V3"));
    state.knownNodes.push_back(sampleNode(0x61916fb3u, "TULL LIKING PRESCRIPTION LIBEL", "TULL", 9, "RAK4631"));
    state.knownNodes.push_back(sampleNode(0x02e58684u, "Masheli 8684", "1Sky", 71, "TRACKER"));
    state.knownNodes.push_back(sampleNode(0xfa9ae7bbu, "Meshtastic e7bb", "e7bb", 110, "HELTEC_V4"));
    state.knownNodes.push_back(sampleNode(0xcb05014cu, "pumpkinPulse", "pmpk", 9, "RAK4631", true));
    state.nodeCount = static_cast<uint32_t>(state.knownNodes.size());
    state.onlineNodeCount = static_cast<int32_t>(state.knownNodes.size());
    state.totalNodeCount = static_cast<int32_t>(state.knownNodes.size());
    Message rx;
    rx.from = 0x433bf1b8u;
    rx.to = 0xcb05014cu;
    rx.rxTime = static_cast<uint32_t>(std::time(nullptr));
    rx.direct = true;
    rx.senderName = "Mesheteer";
    rx.senderId = nodeId(rx.from);
    rx.channelName = state.channelName;
    rx.text = "Radio check from the trail head";
    state.messages.push_back(rx);
    Message ack = rx;
    ack.rxTime += 60;
    ack.text = "Routing ACK packet received";
    state.messages.push_back(ack);
    Message channel = rx;
    channel.to = BroadcastNode;
    channel.rxTime += 120;
    channel.direct = false;
    channel.channelIndex = 0;
    channel.channelName = state.channelName;
    channel.senderName = "MacnCheeseNoodles";
    channel.senderId = "!9ea0cc08";
    channel.text = "LongFast channel check";
    state.messages.push_back(channel);
    state.chatPeerNodeId = nodeId(rx.from);
    state.chatPeerName = "Mesheteer";
    state.textMessageCount = static_cast<uint32_t>(state.messages.size());
    ChannelSummary primary;
    primary.index = 0;
    primary.name = "LongFast";
    primary.role = 1;
    primary.hasPsk = true;
    primary.pskBytes = 1;
    primary.positionPrecision = 13;
    state.channels.push_back(primary);
    ChannelSummary test;
    test.index = 1;
    test.name = "tst";
    test.role = 2;
    test.hasPsk = true;
    test.pskBytes = 16;
    state.channels.push_back(test);
    ChannelSummary open;
    open.index = 2;
    open.name = "ClearNet";
    open.role = 2;
    open.downlinkEnabled = true;
    state.channels.push_back(open);
}
#endif

static void addStream(AppState &state, const std::string &text) {
    state.streamEvents.push_back(text);
    while (state.streamEvents.size() > 30) {
        state.streamEvents.erase(state.streamEvents.begin());
    }
}

static void addLocalSentMessage(AppState &state, uint32_t to, const char *text, bool direct) {
    Message msg;
    msg.from = 0;
    msg.to = to;
    msg.rxTime = static_cast<uint32_t>(std::time(nullptr));
    msg.channelIndex = 0;
    msg.direct = direct;
    msg.outgoing = true;
    msg.delivered = false;
    msg.seen = true;
    msg.senderName = "Me";
    msg.senderId = state.myNodeId;
    msg.channelName = state.channelName;
    msg.text = text;
    state.messages.push_back(msg);
    state.messageRevision++;
    while (static_cast<int>(state.messages.size()) > MaxMessages) {
        state.messages.erase(state.messages.begin());
    }
}

static bool parseNodeNumber(const char *text, uint32_t &node) {
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (*text == '!') {
        ++text;
    }
    if (!std::isxdigit(static_cast<unsigned char>(*text))) {
        return false;
    }
    char *end = nullptr;
    unsigned long value = std::strtoul(text, &end, 16);
    if (end == text || value > 0xfffffffful) {
        return false;
    }
    node = static_cast<uint32_t>(value);
    return true;
}

static const char *uiTabName(int tab) {
    switch (tab) {
    case 0: return "HOME";
    case 1: return "NODE";
    case 2: return "CHANNELS";
    case 3: return "CHATS";
    case 4: return "MAP";
    case 5: return "SETTINGS";
    case 6: return "CHAT_DETAIL";
    case 7: return "LOG";
    case 8: return "DEBUG";
    case 9: return "STATUS";
    case 10: return "NODE_TOOLS";
    case 11: return "SCREENSAVER_SETTINGS";
    case 12: return "FONT_SETTINGS";
    case 13: return "FONT_DEBUG";
    case 14: return "MIDI_PLAYER";
    case 15: return "GUI_OPTIONS";
    default: return "UNKNOWN";
    }
}

static const char *skipToken(const char *text) {
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    while (*text && *text != ' ' && *text != '\t') {
        ++text;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    return text;
}

static std::string wakeListText() {
    std::string out = "WAKES";
    for (uint32_t i = 0; i < MeshtasticProtocol::wakeModeCount(); ++i) {
        out += " ";
        out += std::to_string(i);
        out += "=";
        out += MeshtasticProtocol::wakeModeName(i);
    }
    out += "\n";
    return out;
}

static std::string oneLine(std::string s) {
    for (char &c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    return s;
}

static std::string liveLayoutSample(const AppState &state) {
    std::string out;
    int unread = 0;
    for (const Message &m : state.messages) {
        if (!m.outgoing && m.from != 0 && !m.seen) ++unread;
    }
    const std::string nodeName = state.nodeName == "Unknown" ? "waiting" : state.nodeName;
    out += "topbar.text=WiiMesh v" + std::string(AppVersion) + " " +
           (state.protocolReady ? "ONLINE" : "SYNCING") + " Device: " +
           nodeName + " Me: " + state.myNodeId + "\n";
    if (!state.messages.empty()) {
        const Message &m = state.messages.back();
        const std::string sender = m.senderName.empty() ? m.senderId : m.senderName;
        out += "ticker.text=ACCESS " + oneLine(sender + ": " + m.text) + "\n";
        out += "messages.text=Messages | " + oneLine(std::string(m.direct ? "DM " : "CH ") +
               sender + " | " + m.text) + " | " + std::to_string(state.messages.size()) + " saved\n";
        out += "chat.text=Chat | " + oneLine(sender) + " | " + oneLine(m.text) +
               " | Unread " + std::to_string(unread) + "\n";
    }
    out += "nodes.text=Nodes | " + std::to_string(state.nodeCount) + " known";
    int shown = 0;
    for (const auto &n : state.knownNodes) {
        if (shown++ >= 4) break;
        out += " | ";
        out += oneLine(n.name.empty() ? n.nodeId : n.name);
    }
    out += "\n";
    out += "radio_status.text=Radio Status | ";
    out += state.protocolReady ? "Healthy\n" : "Connecting\n";
    out += "telemetry.text=Telemetry | RX " + std::to_string(state.rxBytes) +
           " TX " + std::to_string(state.txBytes) +
           " Nodes " + std::to_string(state.nodeCount) +
           " Text " + std::to_string(static_cast<int>(state.messages.size())) +
           " Unread " + std::to_string(unread) + "\n";
    if (!state.meshFiles.empty()) {
        const MeshFileTransfer &transfer = state.meshFiles.back();
        out += "files.text=Files | " + oneLine(transfer.filename) + " | " +
               std::to_string(transfer.receivedChunks) + "/" +
               std::to_string(transfer.totalChunks) + " | ";
        out += transfer.saved ? "saved\n" : (transfer.complete ? "complete\n" : "receiving\n");
    }
    return out;
}

static std::string meshMapText(const AppState &state) {
    std::string out = "# WiiMesh mesh map v" + std::string(AppVersion) + "\n";
    out += "# node_id\tlong_name\tshort_name\tlat\tlon\taltitude_m\tprecision_bits\tlast_heard\thops\tsnr\tbattery\tvoltage\thardware\tmac\n";
    for (const auto &n : state.knownNodes) {
        out += n.nodeId + "\t" + oneLine(n.name) + "\t" + oneLine(n.shortName) + "\t";
        if (n.hasPosition) {
            char pos[96];
            std::snprintf(pos, sizeof(pos), "%.7f\t%.7f\t%ld\t%lu",
                          n.latitudeI / 10000000.0, n.longitudeI / 10000000.0,
                          static_cast<long>(n.altitude), static_cast<unsigned long>(n.precisionBits));
            out += pos;
        } else {
            out += "unknown\tunknown\t\t";
        }
        char meta[160];
        std::snprintf(meta, sizeof(meta), "\t%lu\t%ld\t%.1f\t%ld\t%.2f\t%s\t%s\n",
                      static_cast<unsigned long>(n.lastHeard), static_cast<long>(n.hopsAway),
                      n.snr, static_cast<long>(n.batteryLevel), n.voltage,
                      n.hardwareName.c_str(), n.macAddress.c_str());
        out += meta;
    }
    return out;
}

static bool saveMeshMap(const char *path, const AppState &state) {
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const std::string map = meshMapText(state);
    const size_t wrote = std::fwrite(map.data(), 1, map.size(), f);
    std::fclose(f);
    return wrote == map.size();
}

static bool endsWithIgnoreCase(const std::string &text, const char *suffix) {
    const size_t n = std::strlen(suffix);
    if (text.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[text.size() - n + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) return false;
    }
    return true;
}

static int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool decodeBase64(const std::string &text, std::vector<uint8_t> &out) {
    int val = 0;
    int bits = -8;
    out.clear();
    for (char c : text) {
        if (c == '=') break;
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        const int v = base64Value(c);
        if (v < 0) return false;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xff));
            bits -= 8;
        }
    }
    return true;
}

static bool meshFileBytes(const MeshFileTransfer &transfer, std::vector<uint8_t> &out) {
    out.clear();
    if (transfer.base64) {
        std::string joined;
        for (std::string chunk : transfer.chunks) {
            if (chunk.rfind("b64:", 0) == 0) chunk.erase(0, 4);
            joined += chunk;
        }
        return decodeBase64(joined, out);
    }
    for (const auto &chunk : transfer.chunks) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return true;
}

#if defined(WIIMESH_WII)
constexpr int MidiSampleRate = 32000;
constexpr int MidiMaxSeconds = 20;
constexpr int MidiMaxSamples = MidiSampleRate * MidiMaxSeconds;
static s16 gMidiPcm[MidiMaxSamples] ATTRIBUTE_ALIGN(32);

struct MidiNote {
    uint32_t startTick = 0;
    uint32_t endTick = 0;
    uint8_t note = 60;
    uint8_t velocity = 90;
};

static uint16_t be16(const std::vector<uint8_t> &data, size_t off) {
    return static_cast<uint16_t>((data[off] << 8) | data[off + 1]);
}

static uint32_t be32(const std::vector<uint8_t> &data, size_t off) {
    return (static_cast<uint32_t>(data[off]) << 24) |
           (static_cast<uint32_t>(data[off + 1]) << 16) |
           (static_cast<uint32_t>(data[off + 2]) << 8) |
           static_cast<uint32_t>(data[off + 3]);
}

static bool readMidiVar(const std::vector<uint8_t> &data, size_t end, size_t &pos, uint32_t &value) {
    value = 0;
    for (int i = 0; i < 4 && pos < end; ++i) {
        const uint8_t b = data[pos++];
        value = (value << 7) | (b & 0x7f);
        if ((b & 0x80) == 0) return true;
    }
    return false;
}

static bool parseMidiNotes(const std::vector<uint8_t> &data, std::vector<MidiNote> &notes,
                           uint16_t &division, uint32_t &tempoUs) {
    notes.clear();
    division = 480;
    tempoUs = 500000;
    if (data.size() < 14 || std::memcmp(data.data(), "MThd", 4) != 0) return false;
    const uint32_t headerLen = be32(data, 4);
    if (headerLen < 6 || 8 + headerLen > data.size()) return false;
    division = be16(data, 12);
    if ((division & 0x8000) != 0 || division == 0) return false;
    size_t pos = 8 + headerLen;
    while (pos + 8 <= data.size()) {
        if (std::memcmp(&data[pos], "MTrk", 4) != 0) break;
        const size_t trackEnd = std::min(data.size(), pos + 8 + be32(data, pos + 4));
        pos += 8;
        uint32_t tick = 0;
        uint8_t running = 0;
        uint32_t activeStart[128] = {};
        uint8_t activeVelocity[128] = {};
        while (pos < trackEnd && notes.size() < 256) {
            uint32_t delta = 0;
            if (!readMidiVar(data, trackEnd, pos, delta)) break;
            tick += delta;
            if (pos >= trackEnd) break;
            uint8_t status = data[pos++];
            if (status < 0x80) {
                if (running == 0) break;
                --pos;
                status = running;
            } else if (status < 0xf0) {
                running = status;
            }
            if (status == 0xff) {
                if (pos >= trackEnd) break;
                const uint8_t type = data[pos++];
                uint32_t len = 0;
                if (!readMidiVar(data, trackEnd, pos, len) || pos + len > trackEnd) break;
                if (type == 0x51 && len == 3 && tick == 0) {
                    tempoUs = (static_cast<uint32_t>(data[pos]) << 16) |
                              (static_cast<uint32_t>(data[pos + 1]) << 8) |
                              static_cast<uint32_t>(data[pos + 2]);
                }
                pos += len;
            } else if (status == 0xf0 || status == 0xf7) {
                uint32_t len = 0;
                if (!readMidiVar(data, trackEnd, pos, len) || pos + len > trackEnd) break;
                pos += len;
            } else {
                const uint8_t op = status & 0xf0;
                const int dataLen = (op == 0xc0 || op == 0xd0) ? 1 : 2;
                if (pos + dataLen > trackEnd) break;
                const uint8_t a = data[pos++];
                const uint8_t b = dataLen == 2 ? data[pos++] : 0;
                if (a < 128 && (op == 0x90 || op == 0x80)) {
                    if (op == 0x90 && b > 0) {
                        activeStart[a] = tick;
                        activeVelocity[a] = b;
                    } else if (activeVelocity[a] > 0) {
                        notes.push_back({activeStart[a], tick > activeStart[a] ? tick : activeStart[a] + division / 4, a, activeVelocity[a]});
                        activeVelocity[a] = 0;
                    }
                }
            }
        }
        for (int n = 0; n < 128 && notes.size() < 256; ++n) {
            if (activeVelocity[n] > 0) {
                notes.push_back({activeStart[n], activeStart[n] + division / 2, static_cast<uint8_t>(n), activeVelocity[n]});
            }
        }
        pos = trackEnd;
    }
    return !notes.empty();
}

static bool playMidiBytes(const std::vector<uint8_t> &bytes, Logger &logger, int *framesOut = nullptr) {
    std::vector<MidiNote> notes;
    uint16_t division = 0;
    uint32_t tempoUs = 0;
    if (!parseMidiNotes(bytes, notes, division, tempoUs)) {
        logger.line("MeshFile MIDI parse failed");
        return false;
    }
    const double secondsPerTick = (tempoUs / 1000000.0) / static_cast<double>(division);
    uint32_t lastTick = 0;
    for (const auto &note : notes) lastTick = std::max(lastTick, note.endTick);
    int sampleCount = static_cast<int>(lastTick * secondsPerTick * MidiSampleRate) + MidiSampleRate / 2;
    sampleCount = std::max(1, std::min(sampleCount, MidiMaxSamples));
    std::memset(gMidiPcm, 0, sizeof(gMidiPcm));
    for (const auto &note : notes) {
        const int start = std::max(0, std::min(sampleCount, static_cast<int>(note.startTick * secondsPerTick * MidiSampleRate)));
        const int end = std::max(start + 1, std::min(sampleCount, static_cast<int>(note.endTick * secondsPerTick * MidiSampleRate)));
        const double freq = 440.0 * std::pow(2.0, (static_cast<int>(note.note) - 69) / 12.0);
        const int period = std::max(2, static_cast<int>(MidiSampleRate / freq));
        const int amp = 800 + static_cast<int>(note.velocity) * 34;
        for (int i = start; i < end; ++i) {
            const int wave = ((i - start) % period) < (period / 2) ? amp : -amp;
            int mixed = static_cast<int>(gMidiPcm[i]) + wave;
            mixed = std::max(-30000, std::min(30000, mixed));
            gMidiPcm[i] = static_cast<s16>(mixed);
        }
    }
    DCFlushRange(gMidiPcm, sampleCount * sizeof(s16));
    ASND_SetVoice(1, VOICE_MONO_16BIT, MidiSampleRate, 0, gMidiPcm,
                  static_cast<u32>(sampleCount * sizeof(s16)), 210, 210, nullptr);
    if (framesOut) {
        *framesOut = std::max(20, static_cast<int>((sampleCount * 60 + MidiSampleRate - 1) / MidiSampleRate) + 8);
    }
    logger.line("MeshFile MIDI autoplay notes " + std::to_string(notes.size()) +
                " samples " + std::to_string(sampleCount));
    return true;
}

static void stopMidiVoice() {
    ASND_StopVoice(1);
}
#endif

static bool autoplayMeshFileIfNeeded(AppState &state, MeshFileTransfer &transfer, const std::vector<uint8_t> &bytes, Logger &logger) {
    if (transfer.autoplayTried) return false;
    if (!endsWithIgnoreCase(transfer.filename, ".mid") && !endsWithIgnoreCase(transfer.filename, ".midi")) return false;
    transfer.autoplayTried = true;
#if defined(WIIMESH_WII)
    int frames = 0;
    const bool ok = playMidiBytes(bytes, logger, &frames);
    if (ok) {
        state.midiPlaying = true;
        state.midiFramesRemaining = frames;
        state.midiNowPlaying = transfer.filename;
        state.midiStatus = "Autoplay " + transfer.filename;
    }
    logger.line(std::string("MeshFile MIDI autoplay ") + (ok ? "ok " : "failed ") + transfer.filename);
    return ok;
#else
    logger.line("MeshFile MIDI autoplay skipped on non-Wii " + transfer.filename);
    return false;
#endif
}

static bool saveCompletedMeshFiles(AppState &state, Logger &logger) {
    bool savedAny = false;
    mkdir(MeshFilesDir, 0777);
    for (auto &transfer : state.meshFiles) {
        if (!transfer.complete || transfer.saved || transfer.filename.empty() ||
            transfer.totalChunks <= 0 || transfer.receivedChunks != transfer.totalChunks) {
            continue;
        }
        const std::string path = std::string(MeshFilesDir) + "/" + transfer.filename;
        std::vector<uint8_t> bytes;
        if (!meshFileBytes(transfer, bytes)) {
            state.usbStatus = "MeshFile decode failed";
            state.usbDetail = transfer.filename;
            logger.line("MeshFile base64 decode failed " + transfer.filename);
            continue;
        }
        FILE *f = std::fopen(path.c_str(), "wb");
        if (!f) {
            state.usbStatus = "MeshFile save failed";
            state.usbDetail = transfer.filename;
            logger.line("MeshFile save open failed " + path);
            continue;
        }
        const bool ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        std::fclose(f);
        if (ok) {
            transfer.saved = true;
            transfer.savedPath = path;
            savedAny = true;
            state.usbStatus = "MeshFile saved";
            state.usbDetail = transfer.filename + " saved";
            logger.line("MeshFile saved " + path + " bytes " + std::to_string(bytes.size()) +
                        (transfer.base64 ? " base64" : " text"));
            if (autoplayMeshFileIfNeeded(state, transfer, bytes, logger)) {
                state.usbStatus = "MIDI autoplay";
                state.usbDetail = transfer.filename;
            }
        } else {
            state.usbStatus = "MeshFile save failed";
            state.usbDetail = transfer.filename;
            logger.line("MeshFile save write failed " + path);
        }
    }
    return savedAny;
}

static void scanMidiFiles(AppState &state) {
    std::vector<std::string> files;
    DIR *dir = opendir(MeshFilesDir);
    if (dir) {
        while (dirent *entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (endsWithIgnoreCase(name, ".mid") || endsWithIgnoreCase(name, ".midi")) {
                files.push_back(name);
            }
        }
        closedir(dir);
    }
    std::sort(files.begin(), files.end());
    state.midiFiles = files;
    if (state.selectedMidiIndex >= static_cast<int>(state.midiFiles.size())) {
        state.selectedMidiIndex = state.midiFiles.empty() ? 0 : static_cast<int>(state.midiFiles.size()) - 1;
    }
    if (state.midiFiles.empty() && !state.midiPlaying) {
        state.midiStatus = "No MIDI files saved";
    } else if (!state.midiPlaying && state.midiStatus == "No MIDI loaded") {
        state.midiStatus = "Ready";
    }
}

static bool readWholeFile(const std::string &path, std::vector<uint8_t> &bytes) {
    bytes.clear();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0 || size > 1024 * 1024) {
        std::fclose(f);
        return false;
    }
    bytes.resize(static_cast<size_t>(size));
    const bool ok = bytes.empty() || std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose(f);
    return ok;
}

static bool playSelectedMidi(AppState &state, Logger &logger) {
    scanMidiFiles(state);
    if (state.midiFiles.empty()) {
        state.midiPlaying = false;
        state.midiStatus = "No MIDI files saved";
        logger.line("MIDI player play failed; no files");
        return false;
    }
    const int index = clampInt(state.selectedMidiIndex, 0, static_cast<int>(state.midiFiles.size()) - 1);
    const std::string name = state.midiFiles[static_cast<size_t>(index)];
    const std::string path = std::string(MeshFilesDir) + "/" + name;
    std::vector<uint8_t> bytes;
    if (!readWholeFile(path, bytes)) {
        state.midiPlaying = false;
        state.midiStatus = "Read failed " + name;
        logger.line("MIDI player read failed " + path);
        return false;
    }
#if defined(WIIMESH_WII)
    int frames = 0;
    const bool ok = playMidiBytes(bytes, logger, &frames);
    if (ok) {
        state.midiPlaying = true;
        state.midiFramesRemaining = frames;
        state.midiNowPlaying = name;
        state.midiStatus = "Playing " + name;
        logger.line("MIDI player play " + name + " frames " + std::to_string(frames));
        return true;
    }
    state.midiPlaying = false;
    state.midiFramesRemaining = 0;
    state.midiStatus = "Parse failed " + name;
    logger.line("MIDI player parse failed " + name);
    return false;
#else
    state.midiPlaying = false;
    state.midiStatus = "MIDI playback is Wii-only";
    logger.line("MIDI player skipped on non-Wii " + name);
    return false;
#endif
}

static void stopMidi(AppState &state, Logger &logger) {
#if defined(WIIMESH_WII)
    stopMidiVoice();
#endif
    state.midiPlaying = false;
    state.midiFramesRemaining = 0;
    state.midiStatus = state.midiNowPlaying.empty() ? "Stopped" : "Stopped " + state.midiNowPlaying;
    logger.line("MIDI player stop");
}

static bool deleteSelectedMidi(AppState &state, Logger &logger) {
    scanMidiFiles(state);
    if (state.midiFiles.empty()) {
        state.midiStatus = "No MIDI files to delete";
        logger.line("MIDI player delete failed; no files");
        return false;
    }
    const int index = clampInt(state.selectedMidiIndex, 0, static_cast<int>(state.midiFiles.size()) - 1);
    const std::string name = state.midiFiles[static_cast<size_t>(index)];
    const std::string path = std::string(MeshFilesDir) + "/" + name;
    if (state.midiPlaying && state.midiNowPlaying == name) {
        stopMidi(state, logger);
    }
    const bool ok = std::remove(path.c_str()) == 0;
    if (ok) {
        state.midiStatus = "Deleted " + name;
        state.midiNowPlaying.clear();
        logger.line("MIDI player deleted " + path);
        scanMidiFiles(state);
        return true;
    }
    state.midiStatus = "Delete failed " + name;
    logger.line("MIDI player delete failed " + path);
    scanMidiFiles(state);
    return false;
}

static void updateMidiPlayer(AppState &state, Logger &logger) {
    if (state.midiCommand == 3) {
        scanMidiFiles(state);
        state.midiCommand = 0;
    } else if (state.midiCommand == 1) {
        playSelectedMidi(state, logger);
        state.midiCommand = 0;
    } else if (state.midiCommand == 2) {
        stopMidi(state, logger);
        state.midiCommand = 0;
    } else if (state.midiCommand == 4) {
        logger.line(std::string("MIDI player repeat ") + (state.midiRepeat ? "on" : "off"));
        state.midiCommand = 0;
    } else if (state.midiCommand == 5) {
        deleteSelectedMidi(state, logger);
        state.midiCommand = 0;
    }
    if (state.midiPlaying && state.midiFramesRemaining > 0) {
        --state.midiFramesRemaining;
        if (state.midiFramesRemaining == 0) {
            if (state.midiRepeat) {
                logger.line("MIDI player repeat restart " + state.midiNowPlaying);
                playSelectedMidi(state, logger);
            } else {
                state.midiPlaying = false;
                state.midiStatus = "Finished " + state.midiNowPlaying;
                logger.line("MIDI player finished " + state.midiNowPlaying);
            }
        }
    }
}

#if defined(WIIMESH_WII)
static s32 openCommandSocket(Logger &logger) {
    s32 sock = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        logger.line("UDP command socket open failed");
        return -1;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_len = WiiSockaddrInLen;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UdpCommandPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (net_bind(sock, reinterpret_cast<sockaddr *>(&addr), WiiSockaddrInLen) < 0) {
        logger.line("UDP command bind failed");
        net_close(sock);
        return -1;
    }
    u32 yes = 1;
    net_ioctl(sock, FIONBIO, &yes);
    logger.line("UDP command port 44016 ready");
    return sock;
}

static void sendCommandReply(s32 sock, const sockaddr_in &to, const char *text) {
    sockaddr_in replyTo = to;
    net_sendto(sock, text, std::strlen(text), 0,
               reinterpret_cast<sockaddr *>(&replyTo), WiiSockaddrInLen);
}

static bool ensureTileFolders(const std::string &style, int z, int x) {
    mkdir(MapTilesDir, 0777);
    const std::string styleDir = std::string(MapTilesDir) + "/" + safeStyleName(style);
    const std::string zDir = styleDir + "/" + std::to_string(z);
    const std::string xDir = zDir + "/" + std::to_string(x);
    mkdir(styleDir.c_str(), 0777);
    mkdir(zDir.c_str(), 0777);
    mkdir(xDir.c_str(), 0777);
    return true;
}

static std::string tilePath(const std::string &style, int z, int x, int y) {
    return std::string(MapTilesDir) + "/" + safeStyleName(style) + "/" +
           std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".png";
}

static void pollCommandSocket(s32 sock, Logger &logger, UsbTransport &transport,
                              MeshtasticProtocol &protocol, Ui &ui, AppState &state,
                              bool &sentStart, bool &sentHeartbeat) {
    if (sock < 0) {
        return;
    }
    char buffer[1024] = {};
    sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    s32 n = net_recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                         reinterpret_cast<sockaddr *>(&from), &fromLen);
    if (n <= 0) {
        return;
    }
    buffer[n] = 0;
    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r' || buffer[n - 1] == ' ')) {
        buffer[--n] = 0;
    }
    logger.line(std::string("UDP command ") + buffer);

    if (std::strcmp(buffer, "PING") == 0) {
        logger.line("UDP command reply PONG");
        state.usbDetail = "UDP PING received";
        sendCommandReply(sock, from, "PONG\n");
    } else if (std::strcmp(buffer, "CDC") == 0) {
        bool ok = transport.reassertCdcControl();
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command CDC ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "CDC OK\n" : "CDC FAILED\n");
    } else if (std::strcmp(buffer, "WAKES") == 0) {
        const std::string reply = wakeListText();
        logger.line("UDP command WAKES " + reply.substr(0, reply.size() - 1));
        addStream(state, reply.substr(0, reply.size() > 1 ? reply.size() - 1 : 0));
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strncmp(buffer, "WAKE ", 5) == 0) {
        const uint32_t mode = static_cast<uint32_t>(std::strtoul(buffer + 5, nullptr, 10));
        protocol.setWakeMode(mode);
        const bool ok = transport.isOpen() && protocol.startConfig(transport);
        if (ok) {
            state.txBytes += 6;
            state.usbStatus = std::string("Wake ") + std::to_string(protocol.wakeMode()) +
                              " " + protocol.wakeModeName();
            state.protocolReady = true;
            sentStart = true;
            sentHeartbeat = false;
            addStream(state, "Wii -> RAK wake " + std::to_string(protocol.wakeMode()) +
                            " " + protocol.wakeModeName() + " config");
        } else {
            state.usbStatus = std::string("Wake ") + std::to_string(protocol.wakeMode()) + " failed";
            addStream(state, "Wii -> RAK wake " + std::to_string(protocol.wakeMode()) +
                            " " + protocol.wakeModeName() + " failed");
        }
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command WAKE ") + std::to_string(protocol.wakeMode()) +
                    " " + protocol.wakeModeName() + (ok ? " ok" : " failed"));
        const std::string reply = std::string(ok ? "WAKE OK " : "WAKE FAILED ") +
                                  std::to_string(protocol.wakeMode()) + " " +
                                  protocol.wakeModeName() + "\n";
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strcmp(buffer, "CFG") == 0 || std::strcmp(buffer, "MATRIX") == 0) {
        const bool matrix = std::strcmp(buffer, "MATRIX") == 0;
        transport.setWriteMatrixEnabled(matrix);
        bool ok = transport.isOpen() && protocol.startConfig(transport);
        transport.setWriteMatrixEnabled(false);
        if (ok) {
            state.txBytes += 6;
            state.usbStatus = matrix ? "Matrix config sent" : "Config requested by UDP";
            state.protocolReady = true;
            sentStart = true;
            sentHeartbeat = false;
            addStream(state, "Wii -> RAK config wake " + std::to_string(protocol.wakeMode()) +
                            " " + protocol.wakeModeName());
        } else {
            state.usbStatus = matrix ? "Matrix TX failed" : "UDP config TX failed";
        }
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command ") + buffer + " " + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "CONFIG OK\n" : "CONFIG FAILED\n");
    } else if (std::strcmp(buffer, "SCAN") == 0) {
        transport.pollDevices(state.usbDevices);
        logDevices(logger, state);
        logger.line("UDP command SCAN complete");
        sendCommandReply(sock, from, "SCAN OK\n");
    } else if (std::strcmp(buffer, "LAYOUT") == 0 || std::strcmp(buffer, "RELOAD_LAYOUT") == 0) {
        const bool ok = ui.reloadLayout(state);
        logger.line(std::string("UDP command LAYOUT ") + (ok ? "ok" : "failed"));
        addStream(state, ok ? "Wii layout reloaded" : "Wii layout reload failed");
        sendCommandReply(sock, from, ok ? "LAYOUT OK\n" : "LAYOUT FAILED\n");
    } else if (std::strncmp(buffer, "LAYOUT_SET ", 11) == 0) {
        const bool ok = ui.applyLayoutLine(state, buffer + 11);
        logger.line(std::string("UDP command LAYOUT_SET ") + (ok ? "ok" : "ignored"));
        sendCommandReply(sock, from, ok ? "LAYOUT_SET OK\n" : "LAYOUT_SET IGNORED\n");
    } else if (std::strcmp(buffer, "LAYOUT_SAVE") == 0) {
        const bool ok = ui.saveLayout(state);
        logger.line(std::string("UDP command LAYOUT_SAVE ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "LAYOUT_SAVE OK\n" : "LAYOUT_SAVE FAILED\n");
    } else if (std::strcmp(buffer, "LAYOUT_GET") == 0) {
        const std::string layout = ui.exportLayout();
        logger.line("UDP command LAYOUT_GET");
        sendCommandReply(sock, from, layout.c_str());
    } else if (std::strcmp(buffer, "LIVE_DATA") == 0) {
        const std::string live = liveLayoutSample(state);
        logger.line("UDP command LIVE_DATA");
        sendCommandReply(sock, from, live.c_str());
    } else if (std::strcmp(buffer, "SETTINGS_GET") == 0 || std::strcmp(buffer, "CONFIG_GET") == 0) {
        const std::string settings = settingsExport(state);
        logger.line("UDP command SETTINGS_GET");
        sendCommandReply(sock, from, settings.c_str());
    } else if (std::strncmp(buffer, "SETTINGS_SET ", 13) == 0 ||
               std::strncmp(buffer, "CONFIG_SET ", 11) == 0) {
        const char *line = std::strncmp(buffer, "SETTINGS_SET ", 13) == 0 ? buffer + 13 : buffer + 11;
        std::string detail;
        const bool ok = applySettingsLine(state, line, &detail);
        const bool saved = ok && saveSettingsConfig(state);
        logger.line(std::string("UDP command SETTINGS_SET ") + detail +
                    (ok ? (saved ? " saved" : " save failed") : " failed"));
        state.usbDetail = ok ? ("Setting " + detail) : ("Setting failed " + detail);
        const std::string reply = ok ? (std::string("SETTINGS_SET OK ") + detail + (saved ? " SAVED\n" : " LIVE_ONLY\n"))
                                     : (std::string("SETTINGS_SET FAILED ") + detail + "\n");
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strcmp(buffer, "SETTINGS_RELOAD") == 0 || std::strcmp(buffer, "CONFIG_RELOAD") == 0) {
        std::string detail;
        const bool ok = loadSettingsConfig(state, &detail);
        logger.line(std::string("UDP command SETTINGS_RELOAD ") + detail);
        state.usbDetail = detail;
        const std::string reply = std::string(ok ? "SETTINGS_RELOAD OK " : "SETTINGS_RELOAD FAILED ") + detail + "\n";
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strcmp(buffer, "SETTINGS_SAVE") == 0 || std::strcmp(buffer, "CONFIG_SAVE") == 0) {
        const bool ok = saveSettingsConfig(state);
        logger.line(std::string("UDP command SETTINGS_SAVE ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "SETTINGS_SAVE OK\n" : "SETTINGS_SAVE FAILED\n");
    } else if (std::strcmp(buffer, "MAP") == 0 || std::strcmp(buffer, "MAP_GET") == 0) {
        const std::string map = meshMapText(state);
        logger.line("UDP command MAP_GET nodes " + std::to_string(state.knownNodes.size()));
        sendCommandReply(sock, from, map.c_str());
    } else if (std::strcmp(buffer, "MAP_SAVE") == 0) {
        const bool ok = saveMeshMap(MapPath, state);
        logger.line(std::string("UDP command MAP_SAVE ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "MAP_SAVE OK\n" : "MAP_SAVE FAILED\n");
    } else if (std::strcmp(buffer, "TILE_URL") == 0) {
        const std::string url = loadMapUrlTemplate();
        logger.line("UDP command TILE_URL");
        const std::string reply = url.empty() ? "TILE_URL unset\n" : ("TILE_URL " + url + "\n");
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strncmp(buffer, "TILE_SET_URL ", 13) == 0) {
        const std::string url = trimLine(buffer + 13);
        const bool ok = url.rfind("http://", 0) == 0 && url.find("{z}") != std::string::npos &&
                        url.find("{x}") != std::string::npos && url.find("{y}") != std::string::npos &&
                        saveMapUrlTemplate(url);
        logger.line(std::string("UDP command TILE_SET_URL ") + (ok ? "ok" : "failed"));
        state.usbStatus = ok ? "Map tile URL saved" : "Map tile URL failed";
        sendCommandReply(sock, from, ok ? "TILE_SET_URL OK\n" : "TILE_SET_URL FAILED use http://.../{z}/{x}/{y}.png\n");
    } else if (std::strncmp(buffer, "TILE_GET ", 9) == 0) {
        int z = -1;
        int tx = -1;
        int ty = -1;
        char style[48] = "default";
        const int parsed = std::sscanf(buffer + 9, "%d %d %d %47s", &z, &tx, &ty, style);
        const std::string templ = loadMapUrlTemplate();
        if (parsed < 3 || z < 0 || z > 20 || tx < 0 || ty < 0) {
            sendCommandReply(sock, from, "TILE_GET USAGE: TILE_GET z x y [style]\n");
            return;
        }
        if (templ.empty()) {
            sendCommandReply(sock, from, "TILE_GET FAILED: set maps.url or use TILE_SET_URL http://host/{z}/{x}/{y}.png\n");
            return;
        }
        ensureTileFolders(style, z, tx);
        const std::string outPath = tilePath(style, z, tx, ty);
        const std::string url = buildTileUrl(templ, z, tx, ty);
        state.usbStatus = "Downloading map tile";
        state.usbDetail = std::to_string(z) + "/" + std::to_string(tx) + "/" + std::to_string(ty);
        logger.line("UDP command TILE_GET " + url);
        const TileDownloadResult result = downloadHttpTile(url, outPath);
        logger.line("TILE_GET " + std::string(result.ok ? "ok " : "failed ") +
                    std::to_string(result.statusCode) + " bytes " + std::to_string(result.bytes) +
                    " " + result.message + " " + result.path);
        const std::string reply = std::string(result.ok ? "TILE_GET OK " : "TILE_GET FAILED ") +
                                  std::to_string(result.statusCode) + " " +
                                  std::to_string(result.bytes) + " " + result.message +
                                  " " + result.path + "\n";
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strncmp(buffer, "DM ", 3) == 0 || std::strncmp(buffer, "SEND ", 5) == 0) {
        const char *args = buffer + (buffer[0] == 'D' ? 3 : 5);
        uint32_t dest = 0;
        const char *text = skipToken(args);
        if (!parseNodeNumber(args, dest) || *text == 0) {
            logger.line("UDP DM usage: DM !nodeid message text");
            state.usbStatus = "UDP DM usage error";
            sendCommandReply(sock, from, "DM USAGE: DM !nodeid message text\n");
            return;
        }
        const bool ok = transport.isOpen() && protocol.sendText(transport, dest, 0, text, true);
        if (ok) {
            state.txBytes += static_cast<uint32_t>(buildTextToRadioFrame(dest, 0, text, true).size());
            state.usbStatus = "UDP direct text queued";
            addLocalSentMessage(state, dest, text, true);
            addStream(state, "Wii -> RAK DM " + nodeId(dest) + " TEXT_MESSAGE_APP queued");
        } else {
            state.usbStatus = "UDP direct text failed";
            addStream(state, "Wii -> RAK DM " + nodeId(dest) + " failed");
        }
        state.usbDetail = std::string("DM ") + nodeId(dest) + " " + text;
        logger.line(std::string("UDP command DM ") + nodeId(dest) + (ok ? " ok" : " failed"));
        sendCommandReply(sock, from, ok ? "DM OK\n" : "DM FAILED\n");
    } else if (std::strncmp(buffer, "CH ", 3) == 0) {
        const char *text = buffer + 3;
        while (*text == ' ' || *text == '\t') {
            ++text;
        }
        if (*text == 0) {
            logger.line("UDP CH usage: CH message text");
            state.usbStatus = "UDP CH usage error";
            sendCommandReply(sock, from, "CH USAGE: CH message text\n");
            return;
        }
        const bool ok = transport.isOpen() && protocol.sendText(transport, BroadcastNode, 0, text, false);
        if (ok) {
            state.txBytes += static_cast<uint32_t>(buildTextToRadioFrame(BroadcastNode, 0, text, false).size());
            state.usbStatus = "UDP channel text queued";
            addLocalSentMessage(state, BroadcastNode, text, false);
            addStream(state, "Wii -> RAK CH " + state.channelName + " TEXT_MESSAGE_APP queued");
        } else {
            state.usbStatus = "UDP channel text failed";
            addStream(state, "Wii -> RAK CH failed");
        }
        state.usbDetail = std::string("CH Primary ") + text;
        logger.line(std::string("UDP command CH ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "CH OK\n" : "CH FAILED\n");
    } else {
        logger.line("UDP command unknown; use PING CDC WAKES WAKE CFG MATRIX SCAN LAYOUT LAYOUT_SET LAYOUT_SAVE LAYOUT_GET LIVE_DATA SETTINGS_GET SETTINGS_SET SETTINGS_RELOAD SETTINGS_SAVE MAP_GET MAP_SAVE TILE_URL TILE_SET_URL TILE_GET DM CH");
        sendCommandReply(sock, from, "UNKNOWN COMMAND\n");
    }
}
#endif

static void logDevices(Logger &logger, const AppState &state) {
    for (const auto &d : state.usbDevices) {
        logger.line("USB device VID " + hex16(d.vendorId) + " PID " + hex16(d.productId) +
                    " class " + hex16(d.deviceClass) + " hint " + d.driverHint);
        for (const auto &iface : d.interfaces) {
            char line[128];
            std::snprintf(line, sizeof(line), "  interface %u class %02x subclass %02x protocol %02x",
                          iface.number, iface.klass, iface.subclass, iface.protocol);
            logger.line(line);
            for (const auto &ep : iface.endpoints) {
                std::snprintf(line, sizeof(line), "    endpoint 0x%02x attr 0x%02x max %u interval %u",
                              ep.address, ep.attributes, ep.maxPacketSize, ep.interval);
                logger.line(line);
            }
        }
    }
}

int main() {
#if defined(WIIMESH_WII)
    fatInitDefault();
    mkdir("sd:/apps", 0777);
    mkdir(AppDir, 0777);
    mkdir(ThemeDir, 0777);
    mkdir(MapTilesDir, 0777);
    mkdir(MeshFilesDir, 0777);
#endif

    Ui ui;
    ui.init();

    AppState state;
    state.usbStatus = "Booting WiiMesh";
    state.usbDetail = "Build " + std::string(AppVersion);
    std::string settingsLoadStatus;
    loadSettingsConfig(state, &settingsLoadStatus);
#if defined(WIIMESH_ICON_DEBUG)
    seedIconDebugState(state);
#endif
    ui.draw(state);

    Logger logger;
    if (logger.open(DebugLogPath)) {
        state.logStatus = "debug.log open";
    } else {
        state.logStatus = "debug.log open failed";
    }
    std::string udpStatus;
    std::string debugTarget = loadDebugTarget();
    state.udpTargetIp = debugTarget;
    state.netStatus = std::string("IP/UDP off; enable Wii IP in Settings -> ") + debugTarget;
    logger.line(std::string("WiiMesh starting v") + AppVersion);
    logger.line("Settings " + settingsLoadStatus);
    state.logStatus = logger.status();
    scanMidiFiles(state);

    MessageStore store;
    store.load(MessagesPath, state);
    state.textMessageCount = static_cast<uint32_t>(state.messages.size());

    UsbTransport transport(&logger);
    MeshtasticProtocol protocol(&logger);
#if defined(WIIMESH_WII)
    s32 commandSocket = -1;
    bool networkAttempting = false;
#endif

    int ticks = 0;
    bool sentStart = false;
    bool sentHeartbeat = false;
    bool messagesDirty = false;
    uint32_t savedMapRevision = state.mapRevision;
    int lastLoggedTab = -1;
    int lastLoggedFontStyle = -1;
    int lastLoggedFontSize = -1;
    uint32_t lastScreensaverDebugSeq = 0;
    bool logDrawResult = false;
    while (true) {
        ui.updateInput(state);
        updateMidiPlayer(state, logger);
        if (state.fontStyle != lastLoggedFontStyle || state.fontSize != lastLoggedFontSize) {
            char line[96];
            std::snprintf(line, sizeof(line), "UI font style=%d size=%d", state.fontStyle, state.fontSize);
            logger.line(line);
            lastLoggedFontStyle = state.fontStyle;
            lastLoggedFontSize = state.fontSize;
        }
        if (state.screensaverDebugSeq != lastScreensaverDebugSeq) {
            logger.line(std::string("UI screensaver ") + state.screensaverDebug);
            lastScreensaverDebugSeq = state.screensaverDebugSeq;
        }
        if (state.uiTab != lastLoggedTab) {
            char line[192];
            std::snprintf(line, sizeof(line),
                          "UI enter %s tab=%d selectedNode=%d known=%u mapRev=%u focused=%d",
                          uiTabName(state.uiTab), state.uiTab, state.selectedNodeIndex,
                          static_cast<unsigned>(state.knownNodes.size()),
                          state.mapRevision, state.dashboardFocused ? 1 : 0);
            logger.line(line);
            lastLoggedTab = state.uiTab;
            logDrawResult = true;
        }
#if defined(WIIMESH_WII)
        if (state.networkRequested && !state.networkReady && !networkAttempting) {
            networkAttempting = true;
            state.netStatus = std::string("IP/UDP starting ") + debugTarget;
            ui.draw(state);
            if (logger.openUdpTarget(debugTarget, UdpLogPort, &udpStatus)) {
                state.wiiIp = logger.localIp().empty() ? "unknown" : logger.localIp();
                state.udpTargetIp = logger.targetIp().empty() ? debugTarget : logger.targetIp();
                state.netStatus = udpStatus;
                commandSocket = openCommandSocket(logger);
                if (commandSocket >= 0) {
                    state.netStatus += " cmd 44016";
                }
                state.networkReady = true;
                logger.line("Network enabled from Settings Wii IP");
            } else {
                state.networkRequested = false;
                state.netStatus = udpStatus.empty() ? "udp init failed; retry in Settings" : udpStatus + "; retry in Settings";
                logger.line("Network enable failed: " + state.netStatus);
            }
            networkAttempting = false;
            state.logStatus = logger.status();
        }
        const uint32_t beforeCommandMessages = state.messageRevision;
        pollCommandSocket(commandSocket, logger, transport, protocol, ui, state, sentStart, sentHeartbeat);
        if (state.messageRevision != beforeCommandMessages) {
            messagesDirty = true;
        }
#endif

        if (!transport.isOpen()) {
            if ((ticks % 120) == 0) {
                transport.pollDevices(state.usbDevices);
                logDevices(logger, state);
                if (transport.openFirstSupported(state.usbDevices)) {
                    state.usbConnected = true;
                    state.usbStatus = "USB serial connected";
                    state.usbDetail = transport.lastError();
                    sentStart = protocol.startConfig(transport);
                    if (sentStart) {
                        state.txBytes += 6;
                        state.usbStatus = std::string("Config wake ") + std::to_string(protocol.wakeMode()) +
                                          " " + protocol.wakeModeName();
                        sentHeartbeat = false;
                    } else {
                        state.usbStatus = "Config TX failed; retrying";
                    }
                    state.usbDetail = transport.lastError();
                    logger.line("USB detail " + state.usbDetail);
                    state.protocolReady = sentStart;
                } else {
                    state.usbConnected = false;
                    if (state.usbDevices.empty()) {
                        state.usbStatus = "No USB devices detected";
                    } else {
                        state.usbStatus = "USB device listed; press 1 for diagnostics";
                    }
                    sentStart = false;
                    sentHeartbeat = false;
                }
            }
        } else {
            if (sentStart) {
                const uint32_t beforeMessages = state.messageRevision;
                protocol.poll(transport, state);
                if (state.messageRevision != beforeMessages) {
                    messagesDirty = true;
                }
                state.usbDetail = transport.lastError();
            }
            if (!sentStart && (ticks % 120) == 0) {
                sentStart = protocol.startConfig(transport);
                if (sentStart) {
                    state.txBytes += 6;
                    state.usbStatus = std::string("Config wake ") + std::to_string(protocol.wakeMode()) +
                                      " " + protocol.wakeModeName();
                    sentHeartbeat = false;
                } else {
                    state.usbStatus = "Config TX failed; retrying";
                }
                state.usbDetail = transport.lastError();
                logger.line("USB detail " + state.usbDetail);
            }
            if (!transport.isOpen()) {
                logger.line("USB transport closed after read error");
                state.usbConnected = false;
                state.protocolReady = false;
                state.usbStatus = "Disconnected; rescanning";
                state.usbDetail = transport.lastError();
                sentHeartbeat = false;
            }
        }

        if (transport.isOpen() && sentStart && (state.protocolReady || state.rxFrames > 0)) {
            if (!sentHeartbeat || (ticks % 18000) == 0) {
                if (protocol.sendHeartbeat(transport)) {
                    sentHeartbeat = true;
                    state.txBytes += 6;
                    addStream(state, "Wii -> RAK heartbeat");
                }
                state.usbDetail = transport.lastError();
            }
        }

        if (!state.pendingSendText.empty()) {
            if (transport.isOpen() && sentStart && state.pendingSendTo != 0) {
                const uint32_t sendTo = state.pendingSendTo;
                const std::string sentText = state.pendingSendText;
                const bool ok = protocol.sendText(transport, state.pendingSendTo, 0,
                                                  state.pendingSendText.c_str(), true);
                if (ok) {
                    state.txBytes += static_cast<uint32_t>(sentText.size());
                    state.usbStatus = "Reply queued";
                    addLocalSentMessage(state, sendTo, sentText.c_str(), true);
                    messagesDirty = true;
                    addStream(state, "Wii -> RAK UI DM queued");
                    logger.line("UI reply queued to " + nodeId(sendTo));
                    state.pendingSendText.clear();
                    state.pendingSendTo = 0;
                } else {
                    state.usbStatus = "Reply send failed";
                    logger.line("UI reply send failed");
                    state.pendingSendText.clear();
                    state.pendingSendTo = 0;
                }
            } else {
                state.usbStatus = "Reply needs node/USB";
                logger.line("UI reply discarded; missing node or USB");
                state.pendingSendText.clear();
                state.pendingSendTo = 0;
            }
        }

        if (!state.pendingTexts.empty()) {
            PendingText pending = state.pendingTexts.front();
            if (transport.isOpen() && sentStart && pending.to != 0) {
                const bool ok = protocol.sendText(transport, pending.to, pending.channelIndex,
                                                  pending.text.c_str(), pending.direct);
                if (ok) {
                    state.txBytes += static_cast<uint32_t>(pending.text.size());
                    state.usbStatus = "MeshFile receipt sent";
                    state.usbDetail = pending.text;
                    addStream(state, "Wii -> RAK " + pending.text);
                    logger.line("MeshFile receipt sent to " + nodeId(pending.to) + " " + pending.text);
                    if (pending.saveToChat) {
                        addLocalSentMessage(state, pending.to, pending.text.c_str(), pending.direct);
                        messagesDirty = true;
                    }
                    state.pendingTexts.erase(state.pendingTexts.begin());
                } else {
                    state.usbStatus = "MeshFile ACK failed";
                    logger.line("MeshFile ACK send failed to " + nodeId(pending.to));
                    state.pendingTexts.erase(state.pendingTexts.begin());
                }
            } else if (pending.to == 0) {
                state.pendingTexts.erase(state.pendingTexts.begin());
            }
        }

        if (saveCompletedMeshFiles(state, logger)) {
            addStream(state, "Wii saved MeshFile transfer");
            scanMidiFiles(state);
        }

        if (messagesDirty && (ticks % 60) == 0) {
            if (store.save(MessagesPath, state)) {
                messagesDirty = false;
                logger.line("messages.dat saved");
            } else {
                logger.line("messages.dat save failed");
            }
        }
        if (state.mapRevision != savedMapRevision && (ticks % 180) == 0) {
            if (saveMeshMap(MapPath, state)) {
                savedMapRevision = state.mapRevision;
                logger.line("mesh_map.dat saved");
            } else {
                logger.line("mesh_map.dat save failed");
            }
        }
        if ((ticks % 180) == 0) {
            char line[256];
            std::snprintf(line, sizeof(line),
                          "heartbeat v%s usb='%s' detail='%s' rx=%u frames=%u bad=%u tx=%u font=%d/%d saver=%d ptr=%d,%d,%d",
                          AppVersion, state.usbStatus.c_str(), state.usbDetail.c_str(),
                          state.rxBytes, state.rxFrames, state.rxBadFrames, state.txBytes,
                          state.fontStyle, state.fontSize, state.screensaverActive ? 1 : 0,
                          state.pointerVisible ? 1 : 0, state.pointerX, state.pointerY);
            logger.line(line);
        }
        ui.draw(state);
        if (logDrawResult) {
            char line[128];
            std::snprintf(line, sizeof(line), "UI draw ok %s tab=%d", uiTabName(state.uiTab), state.uiTab);
            logger.line(line);
            logDrawResult = false;
        }
        ticks++;
    }
    return 0;
}
