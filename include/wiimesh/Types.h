#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wiimesh {

constexpr const char *AppVersion = "0.1.51";
constexpr uint32_t BroadcastNode = 0xffffffffu;
constexpr int MaxMessages = 100;
constexpr int MaxDebugPackets = 80;

struct EndpointInfo {
    uint8_t address = 0;
    uint8_t attributes = 0;
    uint16_t maxPacketSize = 0;
    uint8_t interval = 0;
};

struct InterfaceInfo {
    uint8_t number = 0;
    uint8_t alternate = 0;
    uint8_t klass = 0;
    uint8_t subclass = 0;
    uint8_t protocol = 0;
    std::vector<EndpointInfo> endpoints;
};

struct UsbDeviceInfo {
    int32_t deviceId = -1;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint8_t deviceClass = 0;
    uint8_t deviceSubClass = 0;
    uint8_t deviceProtocol = 0;
    uint8_t configurationValue = 0;
    std::vector<InterfaceInfo> interfaces;
    std::string driverHint;
    std::string diagnostic;
};

struct Message {
    uint32_t from = 0;
    uint32_t to = BroadcastNode;
    uint32_t rxTime = 0;
    uint8_t channelIndex = 0;
    bool direct = false;
    std::string senderName;
    std::string senderId;
    std::string channelName;
    std::string text;
};

struct NodeSummary {
    uint32_t id = 0;
    std::string nodeId;
    std::string name;
    bool isMine = false;
};

struct DebugPacket {
    uint32_t time = 0;
    std::string title;
    std::string summary;
    std::string meshFields;
    std::string dataFields;
    std::string decoded;
};

struct AppState {
    bool usbConnected = false;
    bool protocolReady = false;
    std::string usbStatus = "Scanning USB";
    std::string usbDetail;
    std::string logStatus = "log pending";
    std::string netStatus = "IP/UDP off; press +";
    std::string nodeName = "Unknown";
    std::string myNodeId = "waiting";
    std::string channelName = "LongFast";
    std::vector<std::string> protocolEvents;
    std::vector<std::string> streamEvents;
    std::vector<DebugPacket> debugPackets;
    std::vector<NodeSummary> knownNodes;
    std::vector<UsbDeviceInfo> usbDevices;
    std::vector<Message> messages;
    int scrollOffset = 0;
    int uiTab = 0;
    bool showDiagnostics = false;
    bool networkRequested = false;
    bool networkReady = false;
    uint32_t txBytes = 0;
    uint32_t rxBytes = 0;
    uint32_t rxFrames = 0;
    uint32_t rxBadFrames = 0;
    uint32_t nodeCount = 0;
    uint32_t textMessageCount = 0;
    uint32_t packetCount = 0;
    uint32_t decodedPacketCount = 0;
    uint32_t lastPacketFrom = 0;
    uint32_t lastPacketTo = 0;
    uint32_t lastPacketPort = 0;
    uint8_t lastPacketChannel = 0;
};

std::string hex16(uint16_t value);
std::string hex32(uint32_t value);
std::string nodeId(uint32_t value);

}
