#include "wiimesh/Ui.h"

#include <cstdio>
#include <ctime>

#if defined(WIIMESH_WII)
#include <gccore.h>
#include <wiiuse/wpad.h>
#endif

namespace wiimesh {

void Ui::init() {
#if defined(WIIMESH_WII)
    VIDEO_Init();
    WPAD_Init();
    GXRModeObj *rmode = VIDEO_GetPreferredMode(nullptr);
    void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
#endif
}

void Ui::updateInput(AppState &state) {
#if defined(WIIMESH_WII)
    WPAD_ScanPads();
    const u32 down = WPAD_ButtonsDown(0);
    if (down & WPAD_BUTTON_HOME) {
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
    }
    if (down & WPAD_BUTTON_1) {
        state.showDiagnostics = !state.showDiagnostics;
    }
    if (down & WPAD_BUTTON_2) {
        state.showDiagnostics = false;
        state.uiTab = (state.uiTab + 1) % 4;
        state.scrollOffset = 0;
    }
    if (down & WPAD_BUTTON_PLUS) {
        state.networkRequested = true;
        if (!state.networkReady) {
            state.netStatus = "IP/UDP requested";
        }
    }
    if (down & WPAD_BUTTON_LEFT) {
        state.showDiagnostics = false;
        state.uiTab = (state.uiTab + 3) % 4;
        state.scrollOffset = 0;
    }
    if (down & WPAD_BUTTON_RIGHT) {
        state.showDiagnostics = false;
        state.uiTab = (state.uiTab + 1) % 4;
        state.scrollOffset = 0;
    }
    if (down & WPAD_BUTTON_UP) {
        if (state.scrollOffset > 0) state.scrollOffset--;
    }
    if (down & WPAD_BUTTON_DOWN) {
        int itemCount = static_cast<int>(state.messages.size());
        if (state.uiTab == 1) itemCount = static_cast<int>(state.knownNodes.size());
        if (state.uiTab == 2) itemCount = static_cast<int>(state.streamEvents.size());
        if (state.uiTab == 3) itemCount = static_cast<int>(state.debugPackets.size());
        const int pageItems = state.uiTab == 3 ? 1 : 8;
        const int maxOffset = itemCount > pageItems ? itemCount - pageItems : 0;
        if (state.scrollOffset < maxOffset) state.scrollOffset++;
    }
#else
    (void)state;
#endif
}

static void printTime(uint32_t t) {
    if (t == 0) {
        std::printf("unknown");
        return;
    }
    const time_t tt = static_cast<time_t>(t);
    tm *info = std::localtime(&tt);
    if (!info) {
        std::printf("%u", t);
        return;
    }
    char buf[24];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", info);
    std::printf("%s", buf);
}

void Ui::draw(const AppState &state) {
    const char *status = state.protocolReady ? "ONLINE" : "SYNCING";
    std::printf("\x1b[2J");
    std::printf("\x1b[4;6H============================================================");
    std::printf("\x1b[5;6H WiiMesh v%s                         %s", AppVersion, status);
    std::printf("\x1b[6;6H USB  %-48.48s", state.usbStatus.c_str());
    const char *nodeName = state.nodeName == "Unknown" ? "waiting for MyNodeInfo" : state.nodeName.c_str();
    std::printf("\x1b[7;6H Me   %-12.12s  %-34.34s", state.myNodeId.c_str(), nodeName);
    std::printf("\x1b[8;6H Ch   %-18.18s Nodes %-3u Packets %-3u Text %-3u", state.channelName.c_str(),
                state.nodeCount, state.packetCount, state.textMessageCount);
    std::printf("\x1b[9;6H RX   %u bytes / %u frames / bad %u     TX %u bytes",
                state.rxBytes, state.rxFrames, state.rxBadFrames, state.txBytes);
    std::printf("\x1b[10;6H NET  %-55.55s", state.netStatus.c_str());
    std::printf("\x1b[11;6H============================================================");
    std::printf("\x1b[12;6H %s MSGS  %s NODES  %s STREAM  %s DEBUG",
                state.uiTab == 0 ? "[*]" : "[ ]",
                state.uiTab == 1 ? "[*]" : "[ ]",
                state.uiTab == 2 ? "[*]" : "[ ]",
                state.uiTab == 3 ? "[*]" : "[ ]");
    std::printf("\x1b[13;6H 2/Left/Right: tabs  Up/Down: scroll  1: USB  +: IP  HOME: exit");

    if (state.showDiagnostics) {
        std::printf("\x1b[15;6HUSB Diagnostics");
        std::printf("\x1b[16;6H%.64s", state.usbDetail.c_str());
        std::printf("\x1b[17;6H%.64s", state.netStatus.c_str());
        int row = 19;
        for (const auto &d : state.usbDevices) {
            std::printf("\x1b[%d;6HVID %s PID %s class %02x sub %02x proto %02x %s",
                        row++, hex16(d.vendorId).c_str(), hex16(d.productId).c_str(),
                        d.deviceClass, d.deviceSubClass, d.deviceProtocol, d.driverHint.c_str());
            if (!d.diagnostic.empty() && row < 28) {
                std::printf("\x1b[%d;8H%.58s", row++, d.diagnostic.c_str());
            }
            for (const auto &iface : d.interfaces) {
                std::printf("\x1b[%d;8Hif %u alt %u class %02x sub %02x proto %02x",
                            row++, iface.number, iface.alternate, iface.klass, iface.subclass, iface.protocol);
                for (const auto &ep : iface.endpoints) {
                    std::printf("\x1b[%d;10Hep %02x attr %02x max %u interval %u",
                                row++, ep.address, ep.attributes, ep.maxPacketSize, ep.interval);
                }
            }
            if (row > 26) break;
        }
    } else {
        int row = 15;
        if (state.uiTab == 0) {
            std::printf("\x1b[%d;6HMessages %u/%d", row++, static_cast<unsigned>(state.messages.size()), MaxMessages);
            if (state.messages.empty()) {
                std::printf("\x1b[%d;8HWaiting for channel or direct text messages.", row++);
                std::printf("\x1b[%d;8HUse Stream for live protocol traffic.", row++);
            }
            const int newest = static_cast<int>(state.messages.size()) - 1;
            const int start = newest - state.scrollOffset;
            for (int i = start; i >= 0 && row < 28; --i) {
                const Message &m = state.messages[i];
                std::printf("\x1b[%d;6H", row++);
                printTime(m.rxTime);
                std::printf("  %-2s %-14.14s  %.28s",
                            m.direct ? "DM" : "CH", m.channelName.c_str(), m.senderName.c_str());
                std::printf("\x1b[%d;8H%.62s", row++, m.text.c_str());
            }
        } else if (state.uiTab == 1) {
            std::printf("\x1b[%d;6HNodes %u", row++, state.nodeCount);
            const int start = state.scrollOffset;
            for (int i = start; i < static_cast<int>(state.knownNodes.size()) && row < 28; ++i) {
                const NodeSummary &n = state.knownNodes[i];
                std::printf("\x1b[%d;6H%s %-12.12s  %.40s",
                            row++, n.isMine ? "*" : " ", n.nodeId.c_str(), n.name.c_str());
                std::printf("\x1b[%d;9HRole Client    Source NodeDB", row++);
            }
        } else {
            if (state.uiTab == 2) {
            std::printf("\x1b[%d;6HStreaming Log", row++);
            std::printf("\x1b[%d;6HCompact event feed. Debug tab shows one detailed card.", row++);
            const int start = state.scrollOffset;
            for (int i = start; i < static_cast<int>(state.streamEvents.size()) && row < 28; ++i) {
                std::string clean = state.streamEvents[i];
                for (char &c : clean) {
                    const unsigned char b = static_cast<unsigned char>(c);
                    if (b < 32 || b > 126) {
                        c = '.';
                    }
                }
                std::printf("\x1b[%d;6H%.66s", row++, clean.c_str());
            }
            } else {
                std::printf("\x1b[%d;6HDebug Panel  cards %u/%d", row++, static_cast<unsigned>(state.debugPackets.size()), MaxDebugPackets);
                if (state.debugPackets.empty()) {
                    std::printf("\x1b[%d;8HWaiting for packets.", row++);
                    std::printf("\x1b[%d;8HSend or receive traffic to populate debug cards.", row++);
                } else {
                    int index = state.scrollOffset;
                    if (index >= static_cast<int>(state.debugPackets.size())) {
                        index = static_cast<int>(state.debugPackets.size()) - 1;
                    }
                    const DebugPacket &p = state.debugPackets[index];
                    std::printf("\x1b[%d;6HCard %d/%u  %.20s", row, index + 1,
                                static_cast<unsigned>(state.debugPackets.size()), p.title.c_str());
                    std::printf("\x1b[%d;42H", row++);
                    printTime(p.time);
                    std::printf("\x1b[%d;6H%.66s", row++, p.summary.c_str());
                    std::printf("\x1b[%d;6H%.66s", row++, p.meshFields.c_str());
                    std::printf("\x1b[%d;6H%.66s", row++, p.dataFields.c_str());
                    std::printf("\x1b[%d;6HDecoded Payload:", row++);
                    std::printf("\x1b[%d;8H%.64s", row++, p.decoded.c_str());
                    std::printf("\x1b[%d;6HSingle-card detail view. Up/Down changes card.", row++);
                }
            }
        }
    }
#if defined(WIIMESH_WII)
    VIDEO_WaitVSync();
#endif
}

}
