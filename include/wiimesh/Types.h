#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wiimesh {

constexpr const char *AppVersion = "0.1.183";
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
    bool outgoing = false;
    bool delivered = false;
    std::string senderName;
    std::string senderId;
    std::string channelName;
    std::string text;
};

struct PendingText {
    uint32_t to = 0;
    uint8_t channelIndex = 0;
    bool direct = true;
    bool saveToChat = false;
    std::string text;
};

struct MeshFileTransfer {
    std::string filename;
    std::string senderId;
    uint32_t sender = 0;
    uint32_t started = 0;
    uint32_t updated = 0;
    int totalChunks = 0;
    int receivedChunks = 0;
    bool complete = false;
    bool saved = false;
    bool base64 = false;
    bool autoplayTried = false;
    std::string savedPath;
    std::vector<std::string> chunks;
};

enum class KeyboardMode {
    Lowercase,
    Uppercase,
    Symbols,
    UnicodeBadges
};

struct NodeSummary {
    uint32_t id = 0;
    std::string nodeId;
    std::string name;
    std::string shortName;
    std::string macAddress;
    uint32_t hardwareModel = 0;
    std::string hardwareName;
    uint32_t lastHeard = 0;
    int32_t hopsAway = -1;
    float snr = -1000.0f;
    int32_t batteryLevel = -1;
    float voltage = 0.0f;
    bool hasPosition = false;
    int32_t latitudeI = 0;
    int32_t longitudeI = 0;
    int32_t altitude = 0;
    uint32_t positionTime = 0;
    uint32_t precisionBits = 0;
    bool isMine = false;
};

struct ChannelSummary {
    int index = 0;
    std::string name;
    int role = 0;
    bool hasPsk = false;
    size_t pskBytes = 0;
    uint32_t channelId = 0;
    bool uplinkEnabled = false;
    bool downlinkEnabled = false;
    bool muted = false;
    uint32_t positionPrecision = 0;
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
    std::string netStatus = "IP/UDP off; Settings Wii IP";
    std::string wiiIp = "offline";
    std::string udpTargetIp;
    std::string meshDeviceIp = "unknown";
    std::string nodeName = "Unknown";
    std::string myNodeId = "waiting";
    std::string channelName = "LongFast";
    std::vector<std::string> protocolEvents;
    std::vector<std::string> streamEvents;
    std::vector<DebugPacket> debugPackets;
    std::vector<NodeSummary> knownNodes;
    std::vector<ChannelSummary> channels;
    std::vector<UsbDeviceInfo> usbDevices;
    std::vector<Message> messages;
    uint32_t messageRevision = 0;
    int scrollOffset = 0;
    int uiTab = 0;
    int transitionTicks = 0;
    int transitionDir = 1;
    int selectedNodeIndex = 0;
    int nodeOptionTab = 0;
    int selectedChannelIndex = 0;
    int selectedChatIndex = 0;
    int chatChannelIndex = -1;
    int selectedSettingsIndex = 0;
    int selectedScreensaverIndex = 0;
    int selectedFontIndex = 0;
    int selectedGuiOptionIndex = 0;
    int selectedMidiIndex = 0;
    int midiCommand = 0;
    bool midiRepeat = false;
    bool midiPlaying = false;
    int midiFramesRemaining = 0;
    std::string midiNowPlaying;
    std::string midiStatus = "No MIDI loaded";
    std::vector<std::string> midiFiles;
    int screensaverMode = 0;
    int screensaverSpeed = 2;
    int fontStyle = 1;
    int fontSize = 3;
    bool screensaverActive = false;
    uint32_t screensaverDebugSeq = 0;
    std::string screensaverDebug;
    bool pointerVisible = false;
    bool pointerEnabled = true;
    int pointerX = 0;
    int pointerY = 0;
    int selectedMessageIndex = 0;
    int keyboardCursor = 0;
    bool dashboardFocused = false;
    bool composingMessage = false;
    std::string chatPeerNodeId;
    std::string chatPeerName = "No contact";
    std::string composeText;
    KeyboardMode keyboardMode = KeyboardMode::Lowercase;
    bool keyboardShiftActive = false;
    bool keyboardCapsLockActive = false;
    size_t composeCursorPosition = 0;
    std::vector<std::string> recentKeyboardWords;
    std::string pendingSendText;
    uint32_t pendingSendTo = 0;
    std::vector<PendingText> pendingTexts;
    std::vector<MeshFileTransfer> meshFiles;
    uint32_t meshFileRevision = 0;
    bool showDiagnostics = false;
    bool showCalibration = false;
    bool debugEnabled = false;
    bool menuEnabled[6] = {true, true, true, true, true, true};
    bool networkRequested = false;
    bool networkReady = false;
    uint32_t txBytes = 0;
    uint32_t rxBytes = 0;
    uint32_t rxFrames = 0;
    uint32_t rxBadFrames = 0;
    uint32_t nodeCount = 0;
    uint32_t textMessageCount = 0;
    uint32_t seenMessageCount = 0;
    uint32_t packetCount = 0;
    uint32_t decodedPacketCount = 0;
    uint32_t mapRevision = 0;
    uint32_t lastPacketFrom = 0;
    uint32_t lastPacketTo = 0;
    uint32_t lastPacketPort = 0;
    uint8_t lastPacketChannel = 0;
    int32_t onlineNodeCount = -1;
    int32_t totalNodeCount = -1;
    int32_t batteryLevel = -1;
    float voltage = 0.0f;
    float channelUtilization = -1.0f;
    float airUtilTx = -1.0f;
    float lastRxSnr = -1000.0f;
    int32_t lastRxRssi = 0;
};

std::string hex16(uint16_t value);
std::string hex32(uint32_t value);
std::string nodeId(uint32_t value);

}
