#include "wiimesh/Logger.h"

#include <cstdio>
#include <cstring>

#if defined(WIIMESH_WII)
#include <network.h>
#endif

#if defined(WIIMESH_WII)
constexpr int WiiSockaddrInLen = 8;

static void sendUdpLine(int socket, unsigned short port, u32 addrValue, const char *line) {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_len = WiiSockaddrInLen;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = addrValue;
    net_sendto(socket, line, std::strlen(line), 0,
               reinterpret_cast<sockaddr *>(&addr), WiiSockaddrInLen);
}
#endif

namespace wiimesh {

bool Logger::open(const char *path) {
    close();
    file_ = std::fopen(path, "ab");
    if (file_) {
        std::setvbuf(static_cast<FILE *>(file_), nullptr, _IONBF, 0);
    }
    return file_ != nullptr;
}

void Logger::close() {
    if (file_) {
        std::fclose(static_cast<FILE *>(file_));
        file_ = nullptr;
    }
#if defined(WIIMESH_WII)
    if (udpSocket_ >= 0) {
        net_close(udpSocket_);
    }
#endif
    udpSocket_ = -1;
    udpReady_ = false;
}

bool Logger::openUdpTarget(const std::string &targetIp, unsigned short port, std::string *statusOut) {
    udpPort_ = port;
#if defined(WIIMESH_WII)
    char localIp[16] = {};
    char netmask[16] = {};
    char gateway[16] = {};
    s32 init = net_init();
    s32 cfg = if_config(localIp, netmask, gateway, true, 20);
    if (cfg < 0) {
        if (statusOut) {
            char line[64];
            std::snprintf(line, sizeof(line), "dhcp failed init %ld cfg %ld",
                          static_cast<long>(init), static_cast<long>(cfg));
            *statusOut = line;
        }
        return false;
    }
    udpTarget_ = inet_addr(targetIp.c_str());
    if (udpTarget_ == 0) {
        if (statusOut) {
            *statusOut = "bad target ip " + targetIp;
        }
        return false;
    }
    udpSocket_ = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udpSocket_ < 0) {
        if (statusOut) {
            char line[96];
            std::snprintf(line, sizeof(line), "ip %s socket failed init %ld ret %ld",
                          localIp, static_cast<long>(init), static_cast<long>(udpSocket_));
            *statusOut = line;
        }
        return false;
    }
    sockaddr_in local;
    std::memset(&local, 0, sizeof(local));
    local.sin_len = WiiSockaddrInLen;
    local.sin_family = AF_INET;
    local.sin_port = htons(port - 1);
    local.sin_addr.s_addr = INADDR_ANY;
    s32 bind = net_bind(udpSocket_, reinterpret_cast<sockaddr *>(&local), WiiSockaddrInLen);
    if (bind < 0) {
        if (statusOut) {
            char line[96];
            std::snprintf(line, sizeof(line), "ip %s bind failed sock %ld bind %ld",
                          localIp, static_cast<long>(udpSocket_), static_cast<long>(bind));
            *statusOut = line;
        }
        net_close(udpSocket_);
        udpSocket_ = -1;
        return false;
    }
    udpReady_ = true;
    if (statusOut) {
        char line[96];
        std::snprintf(line, sizeof(line), "ip %s -> %s:%u sock %ld bind %ld",
                      localIp, targetIp.c_str(), port, static_cast<long>(udpSocket_),
                      static_cast<long>(bind));
        *statusOut = line;
    }
    return true;
#else
    if (statusOut) {
        *statusOut = "udp unavailable on host";
    }
    return false;
#endif
}

void Logger::line(const std::string &text) {
    char line[512];
    std::snprintf(line, sizeof(line), "%06u %s\n", sequence_++, text.c_str());
    if (!file_) {
    } else {
        std::fputs(line, static_cast<FILE *>(file_));
        std::fflush(static_cast<FILE *>(file_));
    }
#if defined(WIIMESH_WII)
    if (udpReady_ && udpSocket_ >= 0) {
        sendUdpLine(udpSocket_, udpPort_, udpTarget_, line);
    }
#endif
}

std::string Logger::status() const {
    std::string out = file_ ? "file ok" : "file failed";
    out += udpReady_ ? " udp ok" : " udp off";
    return out;
}

}
