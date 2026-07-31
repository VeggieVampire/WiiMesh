#include "wiimesh/Logger.h"
#include "wiimesh/MapDownloader.h"
#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/MessageStore.h"
#include "wiimesh/Transport.h"
#include "wiimesh/Ui.h"
#include "wiimesh/UsbTransport.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#if defined(WIIMESH_WII)
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
    msg.senderName = "Me";
    msg.senderId = state.myNodeId;
    msg.channelName = state.channelName;
    msg.text = std::string("[sent] ") + text;
    state.messages.push_back(msg);
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
        out += "chat.text=Chat | " + oneLine(sender) + " | " + oneLine(m.text) + " | Read-only client\n";
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
           " Text " + std::to_string(state.textMessageCount) + "\n";
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
    state.netStatus = std::string("IP/UDP off; + enables ") + debugTarget;
    logger.line(std::string("WiiMesh starting v") + AppVersion);
    logger.line("Settings " + settingsLoadStatus);
    state.logStatus = logger.status();

    MessageStore store;
    store.load(MessagesPath, state);

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
                logger.line("Network enabled by + button");
            } else {
                state.networkRequested = false;
                state.netStatus = udpStatus.empty() ? "udp init failed; press + retry" : udpStatus + "; press + retry";
                logger.line("Network enable failed: " + state.netStatus);
            }
            networkAttempting = false;
            state.logStatus = logger.status();
        }
        pollCommandSocket(commandSocket, logger, transport, protocol, ui, state, sentStart, sentHeartbeat);
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
                const size_t beforeMessages = state.messages.size();
                protocol.poll(transport, state);
                if (state.messages.size() != beforeMessages) {
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
                const bool ok = protocol.sendText(transport, state.pendingSendTo, 0,
                                                  state.pendingSendText.c_str(), true);
                if (ok) {
                    state.txBytes += static_cast<uint32_t>(state.pendingSendText.size());
                    state.usbStatus = "Reply queued";
                    addStream(state, "Wii -> RAK UI DM queued");
                    logger.line("UI reply queued to " + nodeId(state.pendingSendTo));
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
