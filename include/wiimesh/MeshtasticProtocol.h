#pragma once

#include "wiimesh/Logger.h"
#include "wiimesh/Transport.h"

#include <map>

namespace wiimesh {

class MeshtasticProtocol {
public:
    explicit MeshtasticProtocol(Logger *logger);
    void reset();
    bool startConfig(Transport &transport);
    void poll(Transport &transport, AppState &state);
    bool parseFromRadio(const std::vector<uint8_t> &payload, AppState &state);

private:
    enum FrameState { NeedStart1, NeedStart2, NeedLen1, NeedLen2, NeedPayload };
    Logger *logger_ = nullptr;
    FrameState frameState_ = NeedStart1;
    uint16_t frameLen_ = 0;
    std::vector<uint8_t> frame_;
    uint32_t wantConfigId_ = 1;
    uint32_t myNode_ = 0;
    std::map<uint32_t, std::string> nodeNames_;
    std::map<uint8_t, std::string> channels_;

    void updateKnownNodes(AppState &state);
    void consume(uint8_t byte, AppState &state);
    void parseMyInfo(const std::vector<uint8_t> &payload, AppState &state);
    void parseNodeInfo(const std::vector<uint8_t> &payload, AppState &state);
    void parseChannel(const std::vector<uint8_t> &payload, AppState &state);
    void parsePacket(const std::vector<uint8_t> &payload, AppState &state);
};

std::vector<uint8_t> buildMockTextFromRadio(uint32_t from, uint32_t to, uint8_t channel, const std::string &text);

}
