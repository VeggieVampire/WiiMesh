#include "wiimesh/Clock.h"
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
#include <cstring>
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
static const char *UsbConfigPath = "sd:/apps/wii-mesh/USB.config";
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

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parseHexBytes(const char *text, std::vector<uint8_t> &out) {
    out.clear();
    int high = -1;
    for (const char *p = text; *p; ++p) {
        const unsigned char uc = static_cast<unsigned char>(*p);
        if (std::isspace(uc) || *p == ':' || *p == '-' || *p == ',' || *p == '_') {
            continue;
        }
        if (*p == 'x' || *p == 'X') {
            continue;
        }
        const int v = hexValue(*p);
        if (v < 0) return false;
        if (high < 0) {
            high = v;
        } else {
            out.push_back(static_cast<uint8_t>((high << 4) | v));
            high = -1;
        }
    }
    return high < 0 && !out.empty();
}

static std::string hexPreviewMain(const std::vector<uint8_t> &payload, size_t maxBytes = 64) {
    static const char *digits = "0123456789abcdef";
    std::string out;
    const size_t n = std::min(payload.size(), maxBytes);
    for (size_t i = 0; i < n; ++i) {
        if (!out.empty()) out.push_back(' ');
        out.push_back(digits[(payload[i] >> 4) & 0xf]);
        out.push_back(digits[payload[i] & 0xf]);
    }
    if (payload.size() > maxBytes) out += " ...";
    return out;
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

static bool ensureUsbConfig() {
    FILE *existing = std::fopen(UsbConfigPath, "rb");
    if (existing) {
        std::fclose(existing);
        return true;
    }
    FILE *f = std::fopen(UsbConfigPath, "wb");
    if (!f) return false;
    const char *text =
        "# WiiMesh USB serial test profiles\n"
        "# Edit this file on SD/FTP to try new Meshtastic USB devices without rebuilding boot.dol.\n"
        "# Standard USB CDC ACM devices are tried automatically from their descriptors.\n"
        "# Add a VID:PID here only when diagnostics show bulk IN/OUT endpoints but WiiMesh skips the device.\n"
        "# Examples:\n"
        "# force_cdc=303a:1001    # one exact Espressif/ESP32-S3 VID:PID\n"
        "# force_cdc=303a:*       # all Espressif/ESP32-S3 products with bulk IN/OUT endpoints\n"
        "# force_cdc=239a:8029    # RAK4631 test unit, usually auto-detected\n"
        "# try_any_bulk_cdc=0     # set to 1 only for short diagnostics; can grab non-serial USB devices\n"
        "# Generic control transfer format:\n"
        "# control=VID:PID,bmRequestType,bRequest,wValue,wIndex,hexData\n"
        "# CP210x 115200 8N1 + DTR/RTS recipe for 10c4:ea60:\n"
        "# control=10c4:ea60,0x41,0x00,0x0001,0x0000,\n"
        "# control=10c4:ea60,0x41,0x07,0x0303,0x0000,\n"
        "# control=10c4:ea60,0x41,0x03,0x0800,0x0000,\n"
        "# control=10c4:ea60,0x41,0x1e,0x0000,0x0000,00c20100\n";
    const bool ok = std::fwrite(text, 1, std::strlen(text), f) == std::strlen(text);
    std::fclose(f);
    return ok;
}

static std::string readTextFileOrEmpty(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (!f) return "";
    std::string out;
    char buf[256];
    while (std::fgets(buf, sizeof(buf), f)) {
        out += buf;
    }
    std::fclose(f);
    return out;
}

static bool appendUsbConfigLine(const std::string &line) {
    ensureUsbConfig();
    FILE *f = std::fopen(UsbConfigPath, "ab");
    if (!f) return false;
    std::string out = trimLine(line);
    out += "\n";
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

static bool appendCp210xUsbConfigRecipe() {
    bool ok = appendUsbConfigLine("force_cdc=10c4:ea60");
    ok = appendUsbConfigLine("control=10c4:ea60,0x41,0x00,0x0001,0x0000,") && ok;
    ok = appendUsbConfigLine("control=10c4:ea60,0x41,0x07,0x0303,0x0000,") && ok;
    ok = appendUsbConfigLine("control=10c4:ea60,0x41,0x03,0x0800,0x0000,") && ok;
    ok = appendUsbConfigLine("control=10c4:ea60,0x41,0x1e,0x0000,0x0000,00c20100") && ok;
    return ok;
}

static bool resetUsbConfigFile() {
    std::remove(UsbConfigPath);
    return ensureUsbConfig();
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
    node.lastHeard = currentUnixTime();
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
    rx.rxTime = currentUnixTime();
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
    msg.rxTime = currentUnixTime();
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
    case 16: return "USB_TOOLS";
    case 17: return "ADMIN_TOOLS";
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

static bool looksLikeBangNodeId(const std::string &s) {
    if (s.size() != 9 || s[0] != '!') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

static std::string liveDisplayName(const std::string &preferred,
                                   const std::string &fallback) {
    const std::string one = oneLine(preferred);
    if (!one.empty() && one != "Unknown" && one != "waiting" && !looksLikeBangNodeId(one)) {
        return one;
    }
    const std::string two = oneLine(fallback);
    return two.empty() ? one : two;
}

static uint32_t parseBangNodeId(const std::string &s) {
    if (!looksLikeBangNodeId(s)) return 0;
    uint32_t value = 0;
    for (size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
    }
    return value;
}

static bool isLocalNode(const AppState &state, uint32_t id) {
    return id != 0 && !state.myNodeId.empty() && nodeId(id) == state.myNodeId;
}

static uint32_t directPeerForReport(const AppState &state, const Message &m) {
    const uint32_t sender = parseBangNodeId(m.senderId);
    if (!m.outgoing) {
        if (m.from != 0 && !isLocalNode(state, m.from)) return m.from;
        if (sender != 0 && !isLocalNode(state, sender)) return sender;
        if (m.to != 0 && m.to != BroadcastNode && !isLocalNode(state, m.to)) return m.to;
    } else {
        if (m.to != 0 && m.to != BroadcastNode && !isLocalNode(state, m.to)) return m.to;
        if (sender != 0 && !isLocalNode(state, sender)) return sender;
        if (m.from != 0 && !isLocalNode(state, m.from)) return m.from;
    }
    if (m.from != 0) return m.from;
    if (m.to != 0 && m.to != BroadcastNode) return m.to;
    return sender;
}

static std::string liveLayoutSample(const AppState &state) {
    std::string out;
    int unread = 0;
    for (const Message &m : state.messages) {
        if (!m.outgoing && m.from != 0 && !m.seen) ++unread;
    }
    std::string nodeName = liveDisplayName(state.nodeName, state.myNodeId);
    for (const auto &n : state.knownNodes) {
        if (n.isMine || (!state.myNodeId.empty() && n.nodeId == state.myNodeId)) {
            nodeName = liveDisplayName(n.name, liveDisplayName(n.shortName, nodeName));
            break;
        }
    }
    out += "topbar.text=WiiMesh v" + std::string(AppVersion) + " " +
           (state.protocolReady ? "ONLINE" : "SYNCING") + " Device: " +
           nodeName + " Me: " + nodeName + "\n";
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
        out += liveDisplayName(n.name, liveDisplayName(n.shortName, n.nodeId));
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

static std::string chatListReport(const AppState &state) {
    struct Row {
        bool unread = false;
        uint32_t peer = 0;
        uint32_t time = 0;
        std::string title;
        std::string last;
    };
    std::vector<Row> rows;
    auto betterTitle = [](const std::string &candidate, const std::string &current) {
        const std::string c = oneLine(candidate);
        if (c.empty()) return false;
        if (current.empty()) return true;
        if (looksLikeBangNodeId(current) && !looksLikeBangNodeId(c)) return true;
        if (current == "Me" && c != "Me") return true;
        return false;
    };
    auto nodeName = [&](uint32_t id) {
        for (const auto &n : state.knownNodes) {
            if (n.id == id || n.nodeId == nodeId(id)) {
                if (!n.name.empty() && n.name != n.nodeId) return n.name;
                if (!n.shortName.empty() && n.shortName != n.nodeId) return n.shortName;
                if (!n.nodeId.empty()) return n.nodeId;
            }
        }
        return nodeId(id);
    };
    for (const Message &m : state.messages) {
        if (!m.direct) continue;
        const uint32_t peer = directPeerForReport(state, m);
        auto found = std::find_if(rows.begin(), rows.end(), [&](const Row &r) {
            return r.peer == peer;
        });
        if (found == rows.end()) {
            Row row;
            row.peer = peer;
            row.title = nodeName(peer);
            if (row.title == nodeId(peer) && !m.outgoing && !m.senderName.empty() &&
                m.senderName != m.senderId && m.senderName != "Me") {
                row.title = m.senderName;
            }
            row.last = m.text;
            row.time = m.rxTime;
            row.unread = !m.outgoing && m.from != 0 && !m.seen;
            rows.push_back(row);
        } else {
            const std::string title = nodeName(peer);
            if (betterTitle(title, found->title)) {
                found->title = title;
            }
            if (found->title == nodeId(peer) && !m.outgoing && !m.senderName.empty() &&
                m.senderName != m.senderId && m.senderName != "Me") {
                found->title = m.senderName;
            }
            found->last = m.text;
            found->time = m.rxTime;
            if (!m.outgoing && m.from != 0 && !m.seen) found->unread = true;
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.unread != b.unread) return a.unread && !b.unread;
        return a.time > b.time;
    });
    std::string out = "DIRECT_CHAT_LIST " + std::to_string(rows.size()) + "\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row &r = rows[i];
        out += std::to_string(static_cast<int>(i)) + " ";
        out += r.unread ? "UNREAD " : "read ";
        out += "DM ";
        out += nodeId(r.peer);
        out += " | " + oneLine(r.title) + " | " + oneLine(r.last) + "\n";
    }
    return out;
}

static std::string recentLines(const std::vector<std::string> &lines, size_t maxLines) {
    std::string out;
    const size_t start = lines.size() > maxLines ? lines.size() - maxLines : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        out += oneLine(lines[i]) + "\n";
    }
    return out;
}

static std::string streamReport(const AppState &state) {
    std::string out = "STREAM_GET " + std::to_string(state.streamEvents.size()) + "\n";
    out += recentLines(state.streamEvents, 40);
    return out;
}

static std::string eventsReport(const AppState &state) {
    std::string out = "EVENTS_GET " + std::to_string(state.protocolEvents.size()) + "\n";
    out += recentLines(state.protocolEvents, 60);
    return out;
}

static std::string debugPacketReport(const AppState &state) {
    std::string out = "DEBUG_GET " + std::to_string(state.debugPackets.size()) + "\n";
    const size_t start = state.debugPackets.size() > 16 ? state.debugPackets.size() - 16 : 0;
    for (size_t i = start; i < state.debugPackets.size(); ++i) {
        const DebugPacket &p = state.debugPackets[i];
        out += std::to_string(static_cast<int>(i)) + "\t" +
               oneLine(p.title) + "\t" +
               oneLine(p.summary) + "\t" +
               oneLine(p.meshFields) + "\t" +
               oneLine(p.dataFields) + "\t" +
               oneLine(p.decoded) + "\n";
    }
    return out;
}

static std::string rxStatusReport(const AppState &state) {
    std::string out;
    out += "RX_STATUS\n";
    out += "version " + std::string(AppVersion) + "\n";
    out += "usb " + state.usbStatus + " | " + state.usbDetail + "\n";
    out += "node " + liveDisplayName(state.nodeName, state.myNodeId) + " id " + state.myNodeId + "\n";
    out += "channel " + state.channelName + "\n";
    out += "rx_bytes " + std::to_string(state.rxBytes) + " frames " + std::to_string(state.rxFrames) +
           " bad " + std::to_string(state.rxBadFrames) + " tx " + std::to_string(state.txBytes) + "\n";
    out += "packets " + std::to_string(state.packetCount) +
           " decoded " + std::to_string(state.decodedPacketCount) +
           " text " + std::to_string(state.textMessageCount) +
           " messages " + std::to_string(state.messages.size()) + "\n";
    out += "last from " + nodeId(state.lastPacketFrom) +
           " to " + nodeId(state.lastPacketTo) +
           " ch " + std::to_string(state.lastPacketChannel) +
           " port " + std::to_string(state.lastPacketPort) + "\n";
    out += "recent_messages:\n";
    const size_t msgStart = state.messages.size() > 6 ? state.messages.size() - 6 : 0;
    for (size_t i = msgStart; i < state.messages.size(); ++i) {
        const Message &m = state.messages[i];
        out += std::to_string(static_cast<int>(i)) + " " +
               (m.outgoing ? "out" : "in") + " " +
               (m.direct ? "DM" : "CH") + " " +
               (m.seen ? "seen" : "UNREAD") + " " +
               nodeId(m.from) + "->" + nodeId(m.to) + " " +
               oneLine(m.senderName) + " | " + oneLine(m.text) + "\n";
    }
    out += "stream:\n" + recentLines(state.streamEvents, 8);
    out += "events:\n" + recentLines(state.protocolEvents, 8);
    if (!state.debugPackets.empty()) {
        const DebugPacket &d = state.debugPackets.back();
        out += "last_debug " + d.title + " | " + oneLine(d.summary) + " | " +
               oneLine(d.decoded) + "\n";
    }
    return out;
}

static std::string textLogReport(const AppState &state) {
    std::string out = "TEXT_LOG " + std::to_string(state.messages.size()) +
                      " saved text_count " + std::to_string(state.textMessageCount) + "\n";
    for (size_t i = 0; i < state.messages.size(); ++i) {
        const Message &m = state.messages[i];
        out += std::to_string(static_cast<int>(i)) + "\t" +
               (m.outgoing ? "out" : "in") + "\t" +
               (m.direct ? "DM" : "CH") + "\t" +
               (m.seen ? "seen" : "UNREAD") + "\t" +
               nodeId(m.from) + "\t" + nodeId(m.to) + "\tch" +
               std::to_string(m.channelIndex) + "\t" +
               oneLine(m.senderName) + "\t" +
               oneLine(m.channelName) + "\t" +
               oneLine(m.text) + "\n";
    }
    return out;
}

static std::string usbDeviceReport(const AppState &state) {
    std::string out = "USB devices " + std::to_string(state.usbDevices.size()) + "\n";
    if (state.usbDevices.empty()) {
        out += "none\n";
        return out;
    }
    for (size_t i = 0; i < state.usbDevices.size(); ++i) {
        const UsbDeviceInfo &d = state.usbDevices[i];
        out += "#" + std::to_string(i) + " VID " + hex16(d.vendorId) +
               " PID " + hex16(d.productId) +
               " class " + hex16(d.deviceClass) +
               " sub " + hex16(d.deviceSubClass) +
               " proto " + hex16(d.deviceProtocol) +
               " cfg " + std::to_string(d.configurationValue) +
               " " + d.driverHint + " " + d.diagnostic + "\n";
        for (const auto &iface : d.interfaces) {
            out += "  if " + std::to_string(iface.number) +
                   " alt " + std::to_string(iface.alternate) +
                   " class " + hex16(iface.klass) +
                   " sub " + hex16(iface.subclass) +
                   " proto " + hex16(iface.protocol) + "\n";
            for (const auto &ep : iface.endpoints) {
                out += "    ep " + hex16(ep.address) +
                       " attr " + hex16(ep.attributes) +
                       " max " + std::to_string(ep.maxPacketSize) +
                       " interval " + std::to_string(ep.interval) + "\n";
            }
        }
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

static std::string encodeBase64(const std::vector<uint8_t> &bytes) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t a = bytes[i];
        const uint32_t b = (i + 1 < bytes.size()) ? bytes[i + 1] : 0;
        const uint32_t c = (i + 2 < bytes.size()) ? bytes[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;
        out.push_back(alphabet[(triple >> 18) & 0x3f]);
        out.push_back(alphabet[(triple >> 12) & 0x3f]);
        out.push_back(i + 1 < bytes.size() ? alphabet[(triple >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < bytes.size() ? alphabet[triple & 0x3f] : '=');
    }
    return out;
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

static bool queueSelectedMidiSend(AppState &state, Logger &logger) {
    scanMidiFiles(state);
    if (state.midiSendTo == 0) {
        state.midiStatus = "Open a direct chat first";
        state.usbStatus = "MIDI send needs DM";
        logger.line("MIDI send failed; no direct peer selected");
        return false;
    }
    if (state.midiFiles.empty()) {
        state.midiStatus = "No MIDI files to send";
        state.usbStatus = "MIDI send failed";
        logger.line("MIDI send failed; no files");
        state.midiSendTo = 0;
        return false;
    }
    const int index = clampInt(state.selectedMidiIndex, 0, static_cast<int>(state.midiFiles.size()) - 1);
    const std::string name = state.midiFiles[static_cast<size_t>(index)];
    const std::string path = std::string(MeshFilesDir) + "/" + name;
    std::vector<uint8_t> bytes;
    if (!readWholeFile(path, bytes) || bytes.empty()) {
        state.midiStatus = "Read failed " + name;
        state.usbStatus = "MIDI send failed";
        logger.line("MIDI send read failed " + path);
        state.midiSendTo = 0;
        return false;
    }
    if (bytes.size() > 8192) {
        state.midiStatus = "Too large " + name;
        state.usbStatus = "MIDI too large";
        logger.line("MIDI send too large " + path + " bytes " + std::to_string(bytes.size()));
        state.midiSendTo = 0;
        return false;
    }
    const std::string encoded = encodeBase64(bytes);
    constexpr size_t ChunkChars = 140;
    const int chunks = std::max(1, static_cast<int>((encoded.size() + ChunkChars - 1) / ChunkChars));
    const uint32_t to = state.midiSendTo;
    state.pendingTexts.push_back({to, 0, true, false,
                                  "[START] " + name + " " + std::to_string(chunks) + " base64"});
    for (int i = 0; i < chunks; ++i) {
        const size_t offset = static_cast<size_t>(i) * ChunkChars;
        state.pendingTexts.push_back({to, 0, true, false,
                                      "[CHUNK] " + std::to_string(i + 1) + "/" +
                                      std::to_string(chunks) + " " + name + " b64:" +
                                      encoded.substr(offset, ChunkChars)});
    }
    state.pendingTexts.push_back({to, 0, true, false, "[END] " + name});
    addLocalSentMessage(state, to, ("MIDI queued: " + name).c_str(), true);
    state.midiStatus = "Queued " + name;
    state.usbStatus = "MIDI queued";
    state.usbDetail = name + " -> " + nodeId(to) + " chunks " + std::to_string(chunks);
    logger.line("MIDI send queued " + name + " to " + nodeId(to) +
                " bytes " + std::to_string(bytes.size()) +
                " chunks " + std::to_string(chunks));
    state.midiSendTo = 0;
    return true;
}

static bool updateMidiPlayer(AppState &state, Logger &logger) {
    bool messagesChanged = false;
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
    } else if (state.midiCommand == 6) {
        messagesChanged = queueSelectedMidiSend(state, logger);
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
    return messagesChanged;
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
    char buffer[4096] = {};
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
            state.protocolReady = false;
            sentStart = true;
            sentHeartbeat = false;
            addStream(state, "Wii -> Radio wake " + std::to_string(protocol.wakeMode()) +
                            " " + protocol.wakeModeName() + " config");
        } else {
            state.usbStatus = std::string("Wake ") + std::to_string(protocol.wakeMode()) + " failed";
            addStream(state, "Wii -> Radio wake " + std::to_string(protocol.wakeMode()) +
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
            state.protocolReady = false;
            sentStart = true;
            sentHeartbeat = false;
            addStream(state, "Wii -> Radio config wake " + std::to_string(protocol.wakeMode()) +
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
    } else if (std::strcmp(buffer, "USB_GET") == 0 || std::strcmp(buffer, "USB") == 0) {
        const std::string report = usbDeviceReport(state);
        logger.line("UDP command USB_GET devices " + std::to_string(state.usbDevices.size()));
        sendCommandReply(sock, from, report.c_str());
    } else if (std::strcmp(buffer, "USB_SCAN") == 0) {
        transport.pollDevices(state.usbDevices);
        logDevices(logger, state);
        const std::string report = usbDeviceReport(state);
        logger.line("UDP command USB_SCAN devices " + std::to_string(state.usbDevices.size()));
        sendCommandReply(sock, from, report.c_str());
    } else if (std::strcmp(buffer, "USB_OPEN") == 0) {
        transport.pollDevices(state.usbDevices);
        logDevices(logger, state);
        const bool ok = transport.openFirstSupported(state.usbDevices);
        if (ok) {
            state.usbConnected = true;
            state.usbStatus = "USB serial connected";
            state.usbDetail = transport.lastError();
            sentStart = protocol.startConfig(transport);
            state.protocolReady = false;
            sentHeartbeat = false;
        } else {
            state.usbConnected = false;
            state.usbStatus = state.usbDevices.empty() ? "No USB devices detected" : "No USB serial opened";
            state.usbDetail = transport.lastError();
            sentStart = false;
            sentHeartbeat = false;
        }
        const std::string reply = std::string(ok ? "USB_OPEN OK\n" : "USB_OPEN FAILED\n") +
                                  usbDeviceReport(state);
        logger.line(std::string("UDP command USB_OPEN ") + (ok ? "ok " : "failed ") + state.usbDetail);
        sendCommandReply(sock, from, reply.c_str());
    } else if (std::strcmp(buffer, "USB_CONFIG_GET") == 0) {
        ensureUsbConfig();
        std::string text = readTextFileOrEmpty(UsbConfigPath);
        if (text.empty()) text = "USB_CONFIG empty or unavailable\n";
        logger.line("UDP command USB_CONFIG_GET");
        sendCommandReply(sock, from, text.c_str());
    } else if (std::strncmp(buffer, "USB_CONFIG_SET ", 15) == 0) {
        const std::string line = trimLine(buffer + 15);
        const bool allowed = line.rfind("force_cdc=", 0) == 0 ||
                             line.rfind("try_any_bulk_cdc=", 0) == 0 ||
                             line.rfind("try_bulk_cdc=", 0) == 0 ||
                             line.rfind("control=", 0) == 0 ||
                             line.rfind("usb_control=", 0) == 0;
        const bool ok = allowed && appendUsbConfigLine(line);
        logger.line(std::string("UDP command USB_CONFIG_SET ") + (ok ? "ok " : "failed ") + line);
        sendCommandReply(sock, from, ok ? "USB_CONFIG_SET OK\n" : "USB_CONFIG_SET FAILED\n");
    } else if (std::strcmp(buffer, "USB_CONFIG_CP210X") == 0) {
        const bool ok = appendCp210xUsbConfigRecipe();
        logger.line(std::string("UDP command USB_CONFIG_CP210X ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "USB_CONFIG_CP210X OK\n" : "USB_CONFIG_CP210X FAILED\n");
    } else if (std::strcmp(buffer, "USB_CONTROLS") == 0) {
        const bool ok = transport.applyConfiguredUsbControls();
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command USB_CONTROLS ") + (ok ? "ok " : "failed ") + state.usbDetail);
        sendCommandReply(sock, from, ok ? "USB_CONTROLS OK\n" : "USB_CONTROLS FAILED\n");
    } else if (std::strcmp(buffer, "USB_CONFIG_DEFAULT") == 0) {
        const bool ok = resetUsbConfigFile();
        logger.line(std::string("UDP command USB_CONFIG_DEFAULT ") + (ok ? "ok" : "failed") +
                    " reset");
        sendCommandReply(sock, from, ok ? "USB_CONFIG_DEFAULT OK\n" : "USB_CONFIG_DEFAULT FAILED\n");
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
    } else if (std::strcmp(buffer, "CHAT_LIST") == 0) {
        const std::string chats = chatListReport(state);
        logger.line("UDP command CHAT_LIST");
        sendCommandReply(sock, from, chats.c_str());
    } else if (std::strcmp(buffer, "TEXT_LOG") == 0 || std::strcmp(buffer, "MESSAGES_GET") == 0) {
        const std::string log = textLogReport(state);
        logger.line("UDP command TEXT_LOG");
        sendCommandReply(sock, from, log.c_str());
    } else if (std::strcmp(buffer, "STREAM_GET") == 0 || std::strcmp(buffer, "STREAM") == 0) {
        const std::string report = streamReport(state);
        logger.line("UDP command STREAM_GET");
        sendCommandReply(sock, from, report.c_str());
    } else if (std::strcmp(buffer, "EVENTS_GET") == 0 || std::strcmp(buffer, "EVENTS") == 0) {
        const std::string report = eventsReport(state);
        logger.line("UDP command EVENTS_GET");
        sendCommandReply(sock, from, report.c_str());
    } else if (std::strcmp(buffer, "DEBUG_GET") == 0 || std::strcmp(buffer, "DEBUG") == 0) {
        const std::string report = debugPacketReport(state);
        logger.line("UDP command DEBUG_GET");
        sendCommandReply(sock, from, report.c_str());
    } else if (std::strcmp(buffer, "RX_STATUS") == 0 || std::strcmp(buffer, "RX") == 0) {
        const std::string status = rxStatusReport(state);
        logger.line("UDP command RX_STATUS");
        sendCommandReply(sock, from, status.c_str());
    } else if (std::strcmp(buffer, "HEARTBEAT") == 0) {
        const bool ok = transport.isOpen() && protocol.sendHeartbeat(transport);
        if (ok) {
            state.txBytes += 8;
            addStream(state, "Wii -> Radio heartbeat");
        }
        state.usbStatus = ok ? "Heartbeat queued" : "Heartbeat failed";
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command HEARTBEAT ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "HEARTBEAT OK\n" : "HEARTBEAT FAILED\n");
    } else if (std::strncmp(buffer, "USB_RAW_HEX ", 12) == 0 ||
               std::strncmp(buffer, "RAW_HEX ", 8) == 0) {
        const char *hex = std::strncmp(buffer, "USB_RAW_HEX ", 12) == 0 ? buffer + 12 : buffer + 8;
        std::vector<uint8_t> bytes;
        const bool parsed = parseHexBytes(hex, bytes);
        const bool ok = parsed && transport.isOpen() &&
                        transport.write(bytes.data(), bytes.size()) == static_cast<int>(bytes.size());
        if (ok) {
            state.txBytes += static_cast<uint32_t>(bytes.size());
            addStream(state, "Wii -> Radio raw USB hex " + std::to_string(bytes.size()) + " bytes");
        }
        state.usbStatus = ok ? "Raw USB hex sent" : (parsed ? "Raw USB hex failed" : "Raw USB hex parse failed");
        state.usbDetail = parsed ? hexPreviewMain(bytes, 32) : "Use USB_RAW_HEX 94 c3 00 00";
        logger.line(std::string("UDP command USB_RAW_HEX ") +
                    (ok ? "ok " : "failed ") + std::to_string(parsed ? bytes.size() : 0));
        sendCommandReply(sock, from, ok ? "USB_RAW_HEX OK\n" :
                         (parsed ? "USB_RAW_HEX FAILED\n" : "USB_RAW_HEX PARSE_FAILED\n"));
    } else if (std::strncmp(buffer, "USB_ASCII ", 10) == 0 ||
               std::strncmp(buffer, "SERIAL_ASCII ", 13) == 0) {
        const char *text = std::strncmp(buffer, "USB_ASCII ", 10) == 0 ? buffer + 10 : buffer + 13;
        const size_t len = std::strlen(text);
        const bool ok = len > 0 && transport.isOpen() &&
                        transport.write(reinterpret_cast<const uint8_t *>(text), len) == static_cast<int>(len);
        if (ok) {
            state.txBytes += static_cast<uint32_t>(len);
            addStream(state, "Wii -> Radio raw USB ASCII " + std::to_string(static_cast<unsigned>(len)) + " bytes");
        }
        state.usbStatus = ok ? "Raw USB ASCII sent" : "Raw USB ASCII failed";
        state.usbDetail = text;
        logger.line(std::string("UDP command USB_ASCII ") + (ok ? "ok " : "failed ") +
                    std::to_string(static_cast<unsigned>(len)));
        sendCommandReply(sock, from, ok ? "USB_ASCII OK\n" : "USB_ASCII FAILED\n");
    } else if (std::strncmp(buffer, "TO_RADIO_HEX ", 13) == 0 ||
               std::strncmp(buffer, "TORADIO_HEX ", 12) == 0) {
        const char *hex = std::strncmp(buffer, "TO_RADIO_HEX ", 13) == 0 ? buffer + 13 : buffer + 12;
        std::vector<uint8_t> payload;
        const bool parsed = parseHexBytes(hex, payload);
        const bool ok = parsed && transport.isOpen() && protocol.sendToRadioPayload(transport, payload, "udp_hex");
        if (ok) {
            state.txBytes += static_cast<uint32_t>(payload.size() + 4);
            addStream(state, "Wii -> Radio ToRadio hex payload " + std::to_string(payload.size()) + " bytes");
        }
        state.usbStatus = ok ? "ToRadio hex queued" : (parsed ? "ToRadio hex failed" : "ToRadio hex parse failed");
        state.usbDetail = parsed ? hexPreviewMain(payload, 32) : "Use TO_RADIO_HEX protobuf-payload";
        logger.line(std::string("UDP command TO_RADIO_HEX ") +
                    (ok ? "ok " : "failed ") + std::to_string(parsed ? payload.size() : 0));
        sendCommandReply(sock, from, ok ? "TO_RADIO_HEX OK\n" :
                         (parsed ? "TO_RADIO_HEX FAILED\n" : "TO_RADIO_HEX PARSE_FAILED\n"));
    } else if (std::strncmp(buffer, "FROM_RADIO_HEX ", 15) == 0 ||
               std::strncmp(buffer, "FROMRADIO_HEX ", 14) == 0) {
        const char *hex = std::strncmp(buffer, "FROM_RADIO_HEX ", 15) == 0 ? buffer + 15 : buffer + 14;
        std::vector<uint8_t> payload;
        const bool parsed = parseHexBytes(hex, payload);
        const bool ok = parsed && protocol.parseFromRadio(payload, state);
        state.usbStatus = ok ? "FromRadio hex parsed" : (parsed ? "FromRadio hex ignored" : "FromRadio hex parse failed");
        state.usbDetail = parsed ? hexPreviewMain(payload, 32) : "Use FROM_RADIO_HEX protobuf-payload";
        logger.line(std::string("UDP command FROM_RADIO_HEX ") +
                    (ok ? "ok " : "failed ") + std::to_string(parsed ? payload.size() : 0));
        sendCommandReply(sock, from, ok ? "FROM_RADIO_HEX OK\n" :
                         (parsed ? "FROM_RADIO_HEX IGNORED\n" : "FROM_RADIO_HEX PARSE_FAILED\n"));
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
    } else if (std::strcmp(buffer, "NODE_CLEAR") == 0 || std::strcmp(buffer, "NODES_CLEAR") == 0 ||
               std::strcmp(buffer, "MAP_CLEAR") == 0) {
        protocol.clearNodeCache(state);
        const bool removed = std::remove(MapPath) == 0;
        logger.line(std::string("UDP command NODE_CLEAR cache cleared map ") + (removed ? "removed" : "not present"));
        state.usbStatus = "Node cache cleared";
        state.usbDetail = "waiting for fresh NodeInfo";
        sendCommandReply(sock, from, removed ? "NODE_CLEAR OK MAP_REMOVED\n" : "NODE_CLEAR OK\n");
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
            addStream(state, "Wii -> Radio DM " + nodeId(dest) + " TEXT_MESSAGE_APP queued");
        } else {
            state.usbStatus = "UDP direct text failed";
            addStream(state, "Wii -> Radio DM " + nodeId(dest) + " failed");
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
            addStream(state, "Wii -> Radio CH " + state.channelName + " TEXT_MESSAGE_APP queued");
        } else {
            state.usbStatus = "UDP channel text failed";
            addStream(state, "Wii -> Radio CH failed");
        }
        state.usbDetail = std::string("CH Primary ") + text;
        logger.line(std::string("UDP command CH ") + (ok ? "ok" : "failed"));
        sendCommandReply(sock, from, ok ? "CH OK\n" : "CH FAILED\n");
    } else {
        logger.line("UDP command unknown; use PING CDC WAKES WAKE CFG HEARTBEAT MATRIX SCAN USB_GET USB_SCAN USB_OPEN USB_RAW_HEX USB_ASCII TO_RADIO_HEX FROM_RADIO_HEX USB_CONFIG_GET USB_CONFIG_SET USB_CONFIG_CP210X USB_CONTROLS USB_CONFIG_DEFAULT LAYOUT LAYOUT_SET LAYOUT_SAVE LAYOUT_GET LIVE_DATA CHAT_LIST TEXT_LOG STREAM_GET EVENTS_GET DEBUG_GET RX_STATUS SETTINGS_GET SETTINGS_SET SETTINGS_RELOAD SETTINGS_SAVE MAP_GET MAP_SAVE TILE_URL TILE_SET_URL TILE_GET DM CH");
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
    const bool usbConfigReady = ensureUsbConfig();
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
    logger.line(std::string("USB.config ") + (usbConfigReady ? "ready" : "create failed"));
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
    uint32_t lastBellRequestSeq = state.bellRequestSeq;
    bool logDrawResult = false;
    while (true) {
        const uint32_t beforeUiMessages = state.messageRevision;
        ui.updateInput(state);
        if (state.messageRevision != beforeUiMessages) {
            messagesDirty = true;
            logger.line("UI message store changed");
        }
        if (updateMidiPlayer(state, logger)) {
            messagesDirty = true;
        }
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
        if (state.bellRequestSeq != lastBellRequestSeq) {
            logger.line("UI bell requested seq " + std::to_string(state.bellRequestSeq) +
                        (state.screensaverActive ? " screensaver" : " visible"));
            lastBellRequestSeq = state.bellRequestSeq;
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

        if (state.adminCommand != 0) {
            const int command = state.adminCommand;
            state.adminCommand = 0;
            if (command == 1) {
                const bool ok = transport.isOpen() && protocol.startConfig(transport);
                if (ok) {
                    state.txBytes += 6;
                    state.usbStatus = "Admin config wake sent";
                    state.protocolReady = false;
                    sentStart = true;
                    sentHeartbeat = false;
                    addStream(state, "ADMIN Wii -> Radio config wake " +
                                     std::to_string(protocol.wakeMode()) + " " +
                                     protocol.wakeModeName());
                } else {
                    state.usbStatus = "Admin config wake failed";
                    addStream(state, "ADMIN config wake failed");
                }
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI admin config wake ") + (ok ? "ok " : "failed ") + state.usbDetail);
            } else if (command == 2) {
                const bool ok = transport.isOpen() && protocol.sendHeartbeat(transport);
                if (ok) {
                    state.txBytes += 6;
                    sentHeartbeat = true;
                    addStream(state, "ADMIN Wii -> Radio heartbeat");
                }
                state.usbStatus = ok ? "Admin heartbeat sent" : "Admin heartbeat failed";
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI admin heartbeat ") + (ok ? "ok " : "failed ") + state.usbDetail);
            } else if (command == 3) {
                char line[160];
                std::snprintf(line, sizeof(line), "ADMIN RX snapshot rx=%u bytes/%u frames bad=%u text=%u tx=%u",
                              state.rxBytes, state.rxFrames, state.rxBadFrames,
                              state.textMessageCount, state.txBytes);
                state.usbStatus = "Admin RX snapshot";
                state.usbDetail = line;
                addStream(state, line);
                logger.line(line);
            } else if (command == 4) {
                state.usbStatus = "Admin text snapshot";
                state.usbDetail = std::to_string(static_cast<int>(state.messages.size())) + " saved messages";
                addStream(state, "ADMIN text snapshot " + state.usbDetail);
                const int count = static_cast<int>(state.messages.size());
                const int first = std::max(0, count - 5);
                for (int i = first; i < count; ++i) {
                    const Message &m = state.messages[static_cast<size_t>(i)];
                    addStream(state, "ADMIN MSG " + std::string(m.direct ? "DM " : "CH ") +
                                     (m.senderName.empty() ? m.senderId : m.senderName) +
                                     ": " + m.text);
                }
                logger.line("UI admin text snapshot " + std::to_string(count));
            } else if (command == 5) {
                state.usbStatus = "Admin stream marker";
                state.usbDetail = "marker written";
                addStream(state, "ADMIN stream marker tab=" + std::string(uiTabName(state.uiTab)) +
                                 " node=" + state.myNodeId);
                logger.line("UI admin stream marker");
            } else if (command == 6) {
                state.usbStatus = "Admin debug marker";
                state.usbDetail = "protocol events " + std::to_string(static_cast<int>(state.protocolEvents.size()));
                addStream(state, "ADMIN debug marker events=" +
                                 std::to_string(static_cast<int>(state.protocolEvents.size())) +
                                 " debug=" + std::to_string(static_cast<int>(state.debugPackets.size())));
                logger.line("UI admin debug marker");
            } else if (command == 7) {
                protocol.clearNodeCache(state);
                state.usbStatus = "Admin node cache cleared";
                state.usbDetail = "waiting for fresh NodeInfo";
                addStream(state, "ADMIN node cache cleared");
                logger.line("UI admin node cache cleared");
            } else if (command == 8) {
                protocol.reset();
                state.protocolReady = false;
                sentStart = false;
                sentHeartbeat = false;
                const bool ok = transport.isOpen() && protocol.startConfig(transport);
                if (ok) {
                    state.txBytes += 6;
                    sentStart = true;
                    state.usbStatus = "Admin full resync";
                    addStream(state, "ADMIN full resync config wake sent");
                } else {
                    state.usbStatus = "Admin resync waiting USB";
                    addStream(state, "ADMIN full resync waiting for USB");
                }
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI admin full resync ") + (ok ? "ok " : "waiting ") + state.usbDetail);
            }
        }

        if (state.usbCommand != 0) {
            const int command = state.usbCommand;
            state.usbCommand = 0;
            if (command == 1) {
                transport.pollDevices(state.usbDevices);
                logDevices(logger, state);
                state.usbStatus = "USB scan complete";
                state.usbDetail = std::to_string(state.usbDevices.size()) + " device(s)";
                logger.line("UI USB tool scan complete");
            } else if (command == 2) {
                transport.pollDevices(state.usbDevices);
                logDevices(logger, state);
                const bool ok = transport.openFirstSupported(state.usbDevices);
                if (ok) {
                    state.usbConnected = true;
                    state.usbStatus = "USB serial connected";
                    state.usbDetail = transport.lastError();
                    sentStart = protocol.startConfig(transport);
                    state.protocolReady = false;
                    sentHeartbeat = false;
                } else {
                    state.usbConnected = false;
                    state.usbStatus = "USB open failed";
                    state.usbDetail = transport.lastError();
                    sentStart = false;
                    sentHeartbeat = false;
                }
                logger.line(std::string("UI USB tool open ") + (ok ? "ok " : "failed ") + state.usbDetail);
            } else if (command == 3) {
                const bool ok = appendUsbConfigLine("force_cdc=303a:*");
                state.usbStatus = ok ? "USB.config updated" : "USB.config failed";
                state.usbDetail = "force_cdc=303a:*";
                logger.line(std::string("UI USB tool force esp32 ") + (ok ? "ok" : "failed"));
            } else if (command == 4) {
                const bool ok = appendUsbConfigLine("try_any_bulk_cdc=1");
                state.usbStatus = ok ? "USB.config diagnostic mode" : "USB.config failed";
                state.usbDetail = "try_any_bulk_cdc=1";
                logger.line(std::string("UI USB tool try any bulk ") + (ok ? "ok" : "failed"));
            } else if (command == 5) {
                const bool ok = resetUsbConfigFile();
                state.usbStatus = ok ? "USB.config reset" : "USB.config reset failed";
                state.usbDetail = "default USB rules";
                logger.line(std::string("UI USB tool reset config ") + (ok ? "ok" : "failed"));
            } else if (command == 6) {
                const bool ok = transport.reassertCdcControl();
                state.usbStatus = ok ? "CDC reasserted" : "CDC reassert failed";
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI USB tool CDC ") + (ok ? "ok " : "failed ") + state.usbDetail);
            } else if (command == 7) {
                const bool ok = transport.isOpen() && protocol.startConfig(transport);
                if (ok) {
                    state.txBytes += 6;
                    state.usbStatus = "Config wake sent";
                    state.protocolReady = false;
                    sentStart = true;
                    sentHeartbeat = false;
                } else {
                    state.usbStatus = "Config wake failed";
                }
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI USB tool config wake ") + (ok ? "ok " : "failed ") + state.usbDetail);
            } else if (command == 8) {
                const bool ok = appendCp210xUsbConfigRecipe();
                state.usbStatus = ok ? "CP210x recipe added" : "CP210x recipe failed";
                state.usbDetail = "USB.config 10c4:ea60 controls";
                logger.line(std::string("UI USB tool cp210x recipe ") + (ok ? "ok" : "failed"));
            } else if (command == 9) {
                const bool ok = transport.applyConfiguredUsbControls();
                state.usbStatus = ok ? "USB controls applied" : "USB controls failed";
                state.usbDetail = transport.lastError();
                logger.line(std::string("UI USB tool controls ") + (ok ? "ok " : "failed ") + state.usbDetail);
            }
        }

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
                    state.protocolReady = false;
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
            if (sentStart && !state.protocolReady && (ticks % 300) == 0) {
                const bool ok = protocol.startConfig(transport);
                if (ok) {
                    state.txBytes += 6;
                    state.usbStatus = std::string("Config retry ") +
                                      std::to_string(protocol.wakeMode()) +
                                      " " + protocol.wakeModeName();
                    addStream(state, "Wii -> Radio config retry while syncing");
                } else {
                    state.usbStatus = "Config retry failed";
                    addStream(state, "Wii -> Radio config retry failed");
                }
                state.usbDetail = transport.lastError();
                logger.line(std::string("USB config retry while syncing ") + (ok ? "ok " : "failed ") +
                            state.usbDetail);
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
                    addStream(state, "Wii -> Radio heartbeat");
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
                    addStream(state, "Wii -> Radio UI DM queued");
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
                    state.usbStatus = pending.saveToChat ? "MeshFile sent" : "MeshFile control sent";
                    state.usbDetail = pending.text;
                    addStream(state, "Wii -> Radio " + pending.text);
                    logger.line("MeshFile text sent to " + nodeId(pending.to) + " " + pending.text);
                    if (pending.saveToChat) {
                        addLocalSentMessage(state, pending.to, pending.text.c_str(), pending.direct);
                        messagesDirty = true;
                    }
                    state.pendingTexts.erase(state.pendingTexts.begin());
                } else {
                    state.usbStatus = "MeshFile send failed";
                    logger.line("MeshFile text send failed to " + nodeId(pending.to));
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
