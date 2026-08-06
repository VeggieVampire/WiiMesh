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
    bool sendHeartbeat(Transport &transport);
    void setWakeMode(uint32_t mode);
    uint32_t wakeMode() const;
    const char *wakeModeName() const;
    static uint32_t wakeModeCount();
    static const char *wakeModeName(uint32_t mode);
    void clearNodeCache(AppState &state);
    bool sendText(Transport &transport, uint32_t to, uint8_t channel, const std::string &text, bool wantAck);
    bool sendToRadioPayload(Transport &transport, const std::vector<uint8_t> &payload, const char *label = "custom");
    void poll(Transport &transport, AppState &state);
    bool parseFromRadio(const std::vector<uint8_t> &payload, AppState &state);

private:
    enum FrameState { NeedStart1, NeedStart2, NeedLen1, NeedLen2, NeedPayload };
    Logger *logger_ = nullptr;
    FrameState frameState_ = NeedStart1;
    uint16_t frameLen_ = 0;
    std::vector<uint8_t> frame_;
    uint32_t heartbeatNonce_ = 1;
    uint32_t configNonce_ = 0x13572468;
    uint32_t wakeMode_ = 0;
    uint32_t myNode_ = 0;
    std::map<uint32_t, std::string> nodeNames_;
    std::map<uint32_t, std::string> nodeShortNames_;
    std::map<uint32_t, std::string> nodeMacAddresses_;
    std::map<uint32_t, uint32_t> nodeHardwareModels_;
    std::map<uint32_t, uint32_t> nodeLastHeard_;
    std::map<uint32_t, int32_t> nodeHopsAway_;
    std::map<uint32_t, float> nodeSnr_;
    std::map<uint32_t, int32_t> nodeBatteryLevels_;
    std::map<uint32_t, float> nodeVoltages_;
    std::map<uint32_t, int32_t> nodeLatitudes_;
    std::map<uint32_t, int32_t> nodeLongitudes_;
    std::map<uint32_t, int32_t> nodeAltitudes_;
    std::map<uint32_t, uint32_t> nodePositionTimes_;
    std::map<uint32_t, uint32_t> nodePrecisionBits_;
    std::map<uint8_t, std::string> channels_;
    std::string primaryFallbackName_ = "LongFast";
    std::string consoleLine_;

    void updateKnownNodes(AppState &state);
    void updatePrimaryFallback(AppState &state);
    bool writeWake(Transport &transport);
    bool writeWantConfig(Transport &transport, uint32_t nonce);
    void consume(uint8_t byte, AppState &state);
    void consumeConsoleText(const std::vector<uint8_t> &payload, AppState &state);
    void parseConsoleLine(const std::string &line, AppState &state);
    void recordRawFrame(const std::vector<uint8_t> &payload, AppState &state);
    void parseConfig(const std::vector<uint8_t> &payload, AppState &state);
    void parseLoraConfig(const std::vector<uint8_t> &payload, AppState &state);
    void parseLogRecord(const std::vector<uint8_t> &payload, AppState &state);
    void parseMetadata(const std::vector<uint8_t> &payload, AppState &state);
    void parseMyInfo(const std::vector<uint8_t> &payload, AppState &state);
    void parseNodeInfo(const std::vector<uint8_t> &payload, AppState &state);
    bool parsePosition(uint32_t node, const std::vector<uint8_t> &payload, AppState &state);
    void parseChannel(const std::vector<uint8_t> &payload, AppState &state);
    void parsePacket(const std::vector<uint8_t> &payload, AppState &state);
};

std::vector<uint8_t> buildMockTextFromRadio(uint32_t from, uint32_t to, uint8_t channel, const std::string &text);
std::vector<uint8_t> buildTextToRadioFrame(uint32_t to, uint8_t channel, const std::string &text, bool wantAck);

}
