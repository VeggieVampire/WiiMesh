#include "wiimesh/Logger.h"
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
static const char *DebugLogPath = "sd:/apps/wii-mesh/debug.log";
static const char *MessagesPath = "sd:/apps/wii-mesh/messages.dat";
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

static void logDevices(Logger &logger, const AppState &state);

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

static void pollCommandSocket(s32 sock, Logger &logger, UsbTransport &transport,
                              MeshtasticProtocol &protocol, AppState &state,
                              bool &sentStart, bool &sentHeartbeat) {
    if (sock < 0) {
        return;
    }
    char buffer[224] = {};
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
        logger.line("UDP command unknown; use PING CDC WAKES WAKE CFG MATRIX SCAN DM CH");
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
    Ui ui;
    ui.init();

    AppState state;
    state.usbStatus = "Booting WiiMesh";
    state.usbDetail = "Build " + std::string(AppVersion);
    ui.draw(state);

#if defined(WIIMESH_WII)
    fatInitDefault();
    mkdir("sd:/apps", 0777);
    mkdir(AppDir, 0777);
#endif

    Logger logger;
    if (logger.open(DebugLogPath)) {
        state.logStatus = "debug.log open";
    } else {
        state.logStatus = "debug.log open failed";
    }
    std::string udpStatus;
    std::string debugTarget = loadDebugTarget();
    state.netStatus = std::string("IP/UDP off; + enables ") + debugTarget;
    logger.line(std::string("WiiMesh starting v") + AppVersion);
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
    while (true) {
        ui.updateInput(state);
#if defined(WIIMESH_WII)
        if (state.networkRequested && !state.networkReady && !networkAttempting) {
            networkAttempting = true;
            state.netStatus = std::string("IP/UDP starting ") + debugTarget;
            ui.draw(state);
            if (logger.openUdpTarget(debugTarget, UdpLogPort, &udpStatus)) {
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
        pollCommandSocket(commandSocket, logger, transport, protocol, state, sentStart, sentHeartbeat);
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

        if (messagesDirty && (ticks % 60) == 0) {
            if (store.save(MessagesPath, state)) {
                messagesDirty = false;
                logger.line("messages.dat saved");
            } else {
                logger.line("messages.dat save failed");
            }
        }
        if ((ticks % 180) == 0) {
            char line[256];
            std::snprintf(line, sizeof(line),
                          "heartbeat v%s usb='%s' detail='%s' rx=%u frames=%u bad=%u tx=%u",
                          AppVersion, state.usbStatus.c_str(), state.usbDetail.c_str(),
                          state.rxBytes, state.rxFrames, state.rxBadFrames, state.txBytes);
            logger.line(line);
        }
        ui.draw(state);
        ticks++;
    }
    return 0;
}
