#pragma once

#include <string>

namespace wiimesh {

class Logger {
public:
    bool open(const char *path);
    bool openUdpTarget(const std::string &targetIp, unsigned short port, std::string *statusOut = nullptr);
    void close();
    void line(const std::string &text);
    std::string status() const;

private:
    void *file_ = nullptr;
    unsigned sequence_ = 0;
    int udpSocket_ = -1;
    unsigned short udpPort_ = 0;
    unsigned int udpTarget_ = 0;
    bool udpReady_ = false;
};

}
