#include "wiimesh/Logger.h"
#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/MessageStore.h"
#include "wiimesh/Transport.h"
#include "wiimesh/Ui.h"
#include "wiimesh/UsbTransport.h"

#include <cstdio>

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

static void pollCommandSocket(s32 sock, Logger &logger, UsbTransport &transport,
                              MeshtasticProtocol &protocol, AppState &state,
                              bool &sentStart) {
    if (sock < 0) {
        return;
    }
    char buffer[96] = {};
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
    } else if (std::strcmp(buffer, "CDC") == 0) {
        bool ok = transport.reassertCdcControl();
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command CDC ") + (ok ? "ok" : "failed"));
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
        } else {
            state.usbStatus = matrix ? "Matrix TX failed" : "UDP config TX failed";
        }
        state.usbDetail = transport.lastError();
        logger.line(std::string("UDP command ") + buffer + " " + (ok ? "ok" : "failed"));
    } else if (std::strcmp(buffer, "SCAN") == 0) {
        transport.pollDevices(state.usbDevices);
        logDevices(logger, state);
        logger.line("UDP command SCAN complete");
    } else {
        logger.line("UDP command unknown; use PING CDC CFG MATRIX SCAN");
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
    if (logger.openUdpTarget(debugTarget, UdpLogPort, &udpStatus)) {
        state.netStatus = udpStatus;
        logger.line(std::string("WiiMesh starting v") + AppVersion);
    } else {
        state.netStatus = udpStatus.empty() ? "udp init failed" : udpStatus;
        logger.line(std::string("WiiMesh starting v") + AppVersion);
    }
    state.logStatus = logger.status();

    MessageStore store;
    store.load(MessagesPath, state);

    UsbTransport transport(&logger);
    MeshtasticProtocol protocol(&logger);
#if defined(WIIMESH_WII)
    s32 commandSocket = openCommandSocket(logger);
    if (commandSocket >= 0) {
        state.netStatus += " cmd 44016";
    }
#endif

    int ticks = 0;
    bool sentStart = false;
    bool messagesDirty = false;
    while (true) {
        ui.updateInput(state);
#if defined(WIIMESH_WII)
        pollCommandSocket(commandSocket, logger, transport, protocol, state, sentStart);
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
                        state.usbStatus = "Config requested; reading";
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
                    state.usbStatus = "Config requested; reading";
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
