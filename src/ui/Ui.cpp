#include "wiimesh/Ui.h"
#include "wiimesh/generated/EmojiIcons.h"
#include "wiimesh/generated/MenuIcons.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

#if defined(WIIMESH_WII)
#include <asndlib.h>
#include <gccore.h>
#include <gxflux/gfx.h>
#include <gxflux/gfx_con.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>
#endif

namespace wiimesh {

namespace {

constexpr int MaxLine = 54;
constexpr int PageTop = 11;
constexpr int PageBottom = 25;
constexpr int PageRows = PageBottom - PageTop + 1;
constexpr int ContentRow = 12;
constexpr int PrimaryTabCount = 6;
constexpr int ScreenCount = 13;
constexpr int ScreenHome = 0;
constexpr int ScreenNodeOptions = 1;
constexpr int ScreenChannels = 2;
constexpr int ScreenChats = 3;
constexpr int ScreenMap = 4;
constexpr int ScreenSettings = 5;
constexpr int ScreenChatDetail = 6;
constexpr int ScreenLog = 7;
constexpr int ScreenDebug = 8;
constexpr int ScreenStatus = 9;
constexpr int ScreenNodeTools = 10;
constexpr int ScreenScreensaverSettings = 11;
constexpr int ScreenFontSettings = 12;
constexpr int GuiZoneCount = 5;
constexpr int KeyboardCols = 7;
constexpr int FontStyleCount = 4;
constexpr int FontSizeMax = 6;

#if defined(WIIMESH_WII)
constexpr int BellSamples = 14400;
constexpr int ScreensaverIdleFrames = 7 * 60 * 60;
constexpr int SkinWidth = 640;
constexpr int SkinHeight = 480;
s16 gBellPcm[BellSamples] ATTRIBUTE_ALIGN(32);
bool gBellReady = false;
bool gConsoleReady = false;
bool gGraphicsReady = false;
bool gScreensaverActive = false;
int gScreensaverIdleFrames = 0;
int gScreensaverFrame = 0;
int gScreensaverDismissBlockFrames = 0;
float gScreensaverX = 96.0f;
float gScreensaverY = 112.0f;
float gScreensaverDx = 0.45f;
float gScreensaverDy = 0.32f;
struct ScreensaverBadge {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    int w = 0;
    int h = 0;
};
std::vector<ScreensaverBadge> gScreensaverBadges;
gfx_tex_t gSkinTexture;
std::vector<uint16_t> gSkinPixels;
constexpr const char *GuiConfigPath = "sd:/apps/wii-mesh/GUI.config";
#endif

constexpr int ConsoleLeftPx = 32;
constexpr int ConsoleTopPx = 24;
constexpr int CharW = 8;
constexpr int CharH = 16;

int pxCol(int col) {
    return ConsoleLeftPx + (col - 1) * CharW;
}

int pxRow(int row) {
    return ConsoleTopPx + (row - 1) * CharH;
}

struct GuiZone {
    const char *name;
    int x;
    int y;
    int w;
    int h;
    int textWidth;
};

struct ChatEntry {
    bool channel = false;
    int channelIndex = 0;
    uint32_t node = 0;
    std::string nodeId;
    std::string title;
    std::string subtitle;
    std::string last;
    uint32_t time = 0;
    bool unread = false;
};

GuiZone gGuiZones[] = {
    {"header", 72, 8, 532, 64, 54},
    {"menu", -512, 88, 108, 342, 10},
    {"content", 72, -50, 532, 326, 54},
    {"footer", 72, -4, 532, 42, 54},
    {"bell", -16, 8, 56, 48, 8},
};
int gSelectedGuiZone = 0;
int gUiFontStyle = 1;
int gUiFontSize = 1;

std::string menuLabel(int tab);

GuiZone &zone(const char *name) {
    for (auto &z : gGuiZones) {
        if (std::strcmp(z.name, name) == 0) return z;
    }
    return gGuiZones[0];
}

void noteScreensaver(AppState &state, const std::string &event) {
    state.screensaverActive = gScreensaverActive;
    state.screensaverDebug = event + " active=" + std::to_string(gScreensaverActive ? 1 : 0) +
                             " idle=" + std::to_string(gScreensaverIdleFrames) +
                             " block=" + std::to_string(gScreensaverDismissBlockFrames) +
                             " mode=" + std::to_string(state.screensaverMode) +
                             " speed=" + std::to_string(state.screensaverSpeed);
    ++state.screensaverDebugSeq;
}

bool pointInRect(int px, int py, int x, int y, int w, int h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}

bool handlePointerClick(AppState &state) {
    if (!state.pointerVisible) return false;
    const int px = state.pointerX;
    const int py = state.pointerY;
    const GuiZone &menu = zone("menu");
    const int navX = pxCol(62) - 8 + menu.x;
    const int navY = pxRow(1) - 6 + menu.y;
    const int slotH = std::max(48, menu.h / PrimaryTabCount);
    if (pointInRect(px, py, navX, navY, menu.w, menu.h)) {
        const int tab = std::max(0, std::min(PrimaryTabCount - 1, (py - navY) / slotH));
        state.uiTab = tab;
        state.dashboardFocused = false;
        state.scrollOffset = 0;
        state.usbDetail = "Pointer menu " + menuLabel(tab);
        return true;
    }

    const GuiZone &content = zone("content");
    const int contentX = pxCol(2) - 6 + content.x;
    const int contentY = pxRow(9) - 4 + content.y;
    if (!pointInRect(px, py, contentX, contentY, content.w, content.h)) return false;

    if (state.uiTab == ScreenSettings) {
        const int row = (py - (contentY + 46)) / 36;
        if (row >= 0 && row <= 5) {
            state.selectedSettingsIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer selected setting";
            return true;
        }
    } else if (state.uiTab == ScreenScreensaverSettings) {
        const int row = (py - (contentY + 48)) / 44;
        if (row >= 0 && row <= 2) {
            state.selectedScreensaverIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer selected screensaver row";
            return true;
        }
    } else if (state.uiTab == ScreenFontSettings) {
        const int row = (py - (contentY + 48)) / 48;
        if (row >= 0 && row <= 1) {
            state.selectedFontIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer selected font row";
            return true;
        }
    }
    return false;
}

int zoneCol(const char *name, int baseCol) {
    return baseCol + zone(name).x / CharW;
}

int zoneRow(const char *name, int baseRow) {
    return baseRow + zone(name).y / CharH;
}

bool saveGuiConfig(AppState &state) {
#if defined(WIIMESH_WII)
    mkdir("sd:/apps", 0777);
    mkdir("sd:/apps/wii-mesh", 0777);
    FILE *f = std::fopen(GuiConfigPath, "wb");
#else
    FILE *f = std::fopen("GUI.config", "wb");
#endif
    if (!f) {
        state.usbDetail = "GUI.config save failed";
        return false;
    }
    std::fprintf(f, "# WiiMesh GUI placement config\n");
    std::fprintf(f, "# D-pad moves selected zone. Hold 1 + D-pad resizes. -/+ changes text_width. A saves and selects next.\n");
    for (const auto &z : gGuiZones) {
        std::fprintf(f, "%s.x=%d\n", z.name, z.x);
        std::fprintf(f, "%s.y=%d\n", z.name, z.y);
        std::fprintf(f, "%s.w=%d\n", z.name, z.w);
        std::fprintf(f, "%s.h=%d\n", z.name, z.h);
        std::fprintf(f, "%s.text_width=%d\n", z.name, z.textWidth);
    }
    if (std::fclose(f) != 0) {
        state.usbDetail = "GUI.config close failed";
        return false;
    }
#if defined(WIIMESH_WII)
    f = std::fopen(GuiConfigPath, "rb");
#else
    f = std::fopen("GUI.config", "rb");
#endif
    if (!f) {
        state.usbDetail = "GUI.config verify failed";
        return false;
    }
    std::fclose(f);
    state.usbDetail = "GUI.config saved";
    return true;
}

void applyGuiConfigLine(const char *line) {
    char name[32] = {};
    char field[16] = {};
    int value = 0;
    if (std::sscanf(line, "%31[^.].%15[^=]=%d", name, field, &value) != 3) return;
    if (std::strcmp(name, "nav") == 0) {
        std::strncpy(name, "menu", sizeof(name) - 1);
    }
    GuiZone &z = zone(name);
    if (std::strcmp(field, "x") == 0) z.x = value;
    if (std::strcmp(field, "y") == 0) z.y = value;
    if (std::strcmp(field, "w") == 0) z.w = std::max(32, value);
    if (std::strcmp(field, "h") == 0) z.h = std::max(24, value);
    if (std::strcmp(field, "text_width") == 0) z.textWidth = std::max(8, std::min(70, value));
}

void loadGuiConfig() {
#if defined(WIIMESH_WII)
    FILE *f = std::fopen(GuiConfigPath, "rb");
#else
    FILE *f = std::fopen("GUI.config", "rb");
#endif
    if (!f) return;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        applyGuiConfigLine(line);
    }
    std::fclose(f);
}

void compactGuiLayout() {
    GuiZone &header = zone("header");
    GuiZone &content = zone("content");
    GuiZone &footer = zone("footer");
    header.h = std::min(header.h, 64);
    if (content.y > -50) content.y = -50;
    content.h = std::max(content.h, 326);
    if (footer.x > content.x) footer.x = content.x;
    footer.w = std::min(footer.w, content.w);
}

std::string lowerText(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool isBellAlertText(const std::string &text) {
    const std::string s = lowerText(text);
    return s.find("alert bell") != std::string::npos ||
           s.find("bell character") != std::string::npos ||
           s.find("bell") != std::string::npos ||
           s.find("\a") != std::string::npos;
}

std::string cleanText(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    auto appendToken = [&](const char *token) {
        if (!out.empty() && out.back() != ' ') out.push_back(' ');
        out += token;
        out.push_back(' ');
    };
    auto appendCodepoint = [&](uint32_t cp) {
        switch (cp) {
        case 0x2302: appendToken("[home]"); break;
        case 0x2699: appendToken("[gear]"); break;
        case 0x26a0: appendToken("[warn]"); break;
        case 0x26f0: appendToken("[hill]"); break;
        case 0x2705:
        case 0x2713:
        case 0x2714: appendToken("[ok]"); break;
        case 0x2753:
        case 0x2754: appendToken("[?]"); break;
        case 0x1f3d4:
        case 0x1f3d5:
        case 0x1f3de: appendToken("[site]"); break;
        case 0x1f3e0:
        case 0x1f3e1:
        case 0x1f3e2: appendToken("[home]"); break;
        case 0x1f4ac:
        case 0x1f5e8: appendToken("[chat]"); break;
        case 0x1f4cd:
        case 0x1f5fa: appendToken("[map]"); break;
        case 0x1f4e1:
        case 0x1f4fb:
        case 0x1f6dc: appendToken("[radio]"); break;
        case 0x1f4f6: appendToken("[signal]"); break;
        case 0x1f50b: appendToken("[battery]"); break;
        case 0x1f511: appendToken("[key]"); break;
        case 0x1f512:
        case 0x1f513: appendToken("[lock]"); break;
        case 0x1f514: appendToken("[bell]"); break;
        case 0x1f465: appendToken("[nodes]"); break;
        case 0x1f697:
        case 0x1f699:
        case 0x1f69a:
        case 0x1f6b2:
        case 0x1f6b6: appendToken("[mobile]"); break;
        default:
            if (cp >= 0x1f000 || cp == 0xfe0f) {
                if (cp != 0xfe0f) appendToken("[icon]");
            } else {
                out.push_back('?');
            }
            break;
        }
    };
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (c >= 32 && c <= 126) {
            out.push_back(static_cast<char>(c));
            ++i;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            out.push_back(' ');
            ++i;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < in.size()) {
            const uint32_t cp = ((c & 0x1f) << 6) |
                (static_cast<unsigned char>(in[i + 1]) & 0x3f);
            appendCodepoint(cp);
            i += 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < in.size()) {
            const uint32_t cp = ((c & 0x0f) << 12) |
                ((static_cast<unsigned char>(in[i + 1]) & 0x3f) << 6) |
                (static_cast<unsigned char>(in[i + 2]) & 0x3f);
            appendCodepoint(cp);
            i += 3;
        } else if ((c & 0xf8) == 0xf0 && i + 3 < in.size()) {
            const uint32_t cp = ((c & 0x07) << 18) |
                ((static_cast<unsigned char>(in[i + 1]) & 0x3f) << 12) |
                ((static_cast<unsigned char>(in[i + 2]) & 0x3f) << 6) |
                (static_cast<unsigned char>(in[i + 3]) & 0x3f);
            appendCodepoint(cp);
            i += 4;
        } else {
            out.push_back('.');
            ++i;
        }
    }
    while (out.find("  ") != std::string::npos) {
        out.erase(out.find("  "), 1);
    }
    return out;
}

std::string trimTo(std::string s, int width) {
    s = cleanText(s);
    if (width <= 0) return "";
    if (static_cast<int>(s.size()) <= width) return s;
    if (width <= 3) return s.substr(0, static_cast<size_t>(width));
    return s.substr(0, static_cast<size_t>(width - 3)) + "...";
}

std::string timeText(uint32_t t) {
    if (t == 0) return "--:--";
    const time_t tt = static_cast<time_t>(t);
    tm *info = std::localtime(&tt);
    if (!info) return "--:--";
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", info);
    return buf;
}

std::string nowTimeText() {
    return timeText(static_cast<uint32_t>(std::time(nullptr)));
}

std::string nowDateText() {
    const time_t tt = std::time(nullptr);
    tm *info = std::localtime(&tt);
    if (!info) return "date waiting";
    char buf[24];
    std::strftime(buf, sizeof(buf), "%a %d %b %Y", info);
    return buf;
}

std::string fixed1(float value, const char *suffix = "") {
    if (value < -999.0f) return "waiting";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%s", value, suffix);
    return buf;
}

std::string fixed2(float value, const char *suffix = "") {
    if (value < 0.0f) return "waiting";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%s", value, suffix);
    return buf;
}

std::string integerOrWaiting(int32_t value, const char *suffix = "") {
    if (value < 0) return "waiting";
    return std::to_string(value) + suffix;
}

std::string lastHeardText(uint32_t t) {
    if (t == 0) return "LAST UNKNOWN";
    const uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (t > now) return "JUST NOW";
    const uint32_t age = now - t;
    if (age < 60) return std::to_string(age) + " SEC AGO";
    if (age < 3600) return std::to_string(age / 60) + " MIN AGO";
    if (age < 86400) return std::to_string(age / 3600) + " HR AGO";
    return std::to_string(age / 86400) + " DAY AGO";
}

std::string nodeConnectionText(const NodeSummary &node) {
    if (node.isMine) return "YOU";
    if (node.hopsAway == 0) return "DIRECT";
    if (node.hopsAway > 0) {
        return std::to_string(node.hopsAway) + (node.hopsAway == 1 ? " HOP" : " HOPS");
    }
    return "UNKNOWN";
}

std::string senderText(const Message &m) {
    if (!m.senderName.empty()) return m.senderName;
    if (!m.senderId.empty()) return m.senderId;
    return nodeId(m.from);
}

std::string messageText(const Message &m) {
    if (m.text == "Routing/ACK packet received" ||
        m.text.find("Routing/ACK") != std::string::npos) {
        return "[OK]";
    }
    if (isBellAlertText(m.text)) {
        return "[BELL]";
    }
    if (m.text.find("TEXT_MESSAGE_APP seen") != std::string::npos) {
        return "[TEXT] hidden payload";
    }
    return cleanText(m.text);
}

std::string tickerText(const AppState &state) {
    if (state.messages.empty()) return "ACCESS: waiting for Meshtastic messages";
    const Message &m = state.messages.back();
    return "ACCESS: " + senderText(m) + " - " + messageText(m);
}

std::string menuLabel(int tab) {
    switch (tab) {
    case ScreenHome: return "HOME";
    case ScreenNodeOptions: return "NODE";
    case ScreenChannels: return "CHAN";
    case ScreenChats: return "CHAT";
    case ScreenMap: return "MAP";
    case ScreenSettings: return "SET";
    default: return "";
    }
}

int activePrimaryTab(const AppState &state) {
    if (state.uiTab == ScreenLog || state.uiTab == ScreenDebug || state.uiTab == ScreenStatus) {
        return ScreenSettings;
    }
    if (state.uiTab == ScreenChatDetail) {
        return ScreenChats;
    }
    if (state.uiTab == ScreenNodeTools) {
        return ScreenNodeOptions;
    }
    return state.uiTab < PrimaryTabCount ? state.uiTab : ScreenHome;
}

std::vector<std::string> keyboardKeys() {
    return {
        "Yes", "No", "OK", "On way", "Help",
        "A", "B", "C", "D", "E",
        "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O",
        "P", "Q", "R", "S", "T",
        "U", "V", "W", "X", "Y",
        "Z", "SPACE", "BACK", "SEND"
    };
}

int newestMessageIndex(const AppState &state, int offset) {
    const int newest = static_cast<int>(state.messages.size()) - 1;
    const int index = newest - offset;
    return index < 0 ? -1 : index;
}

void clampState(AppState &state) {
    if (state.uiTab < 0 || state.uiTab >= ScreenCount) state.uiTab = ScreenHome;
    if (state.selectedNodeIndex < 0) state.selectedNodeIndex = 0;
    if (state.selectedNodeIndex >= static_cast<int>(state.knownNodes.size())) {
        state.selectedNodeIndex = state.knownNodes.empty() ? 0 : static_cast<int>(state.knownNodes.size()) - 1;
    }
    if (state.nodeOptionTab < 0 || state.nodeOptionTab > 1) state.nodeOptionTab = 0;
    if (state.selectedSettingsIndex < 0) state.selectedSettingsIndex = 0;
    if (state.selectedSettingsIndex > 5) state.selectedSettingsIndex = 5;
    if (state.selectedScreensaverIndex < 0) state.selectedScreensaverIndex = 0;
    if (state.selectedScreensaverIndex > 2) state.selectedScreensaverIndex = 2;
    if (state.selectedFontIndex < 0) state.selectedFontIndex = 0;
    if (state.selectedFontIndex > 1) state.selectedFontIndex = 1;
    state.screensaverMode = ((state.screensaverMode % 3) + 3) % 3;
    if (state.screensaverSpeed < 0) state.screensaverSpeed = 0;
    if (state.screensaverSpeed > 5) state.screensaverSpeed = 5;
    state.fontStyle = ((state.fontStyle % FontStyleCount) + FontStyleCount) % FontStyleCount;
    if (state.fontSize < 0) state.fontSize = 0;
    if (state.fontSize > FontSizeMax) state.fontSize = FontSizeMax;
    const int channelRows = std::max(8, static_cast<int>(state.channels.size()));
    if (state.selectedChannelIndex < 0) state.selectedChannelIndex = 0;
    if (state.selectedChannelIndex >= channelRows) state.selectedChannelIndex = channelRows - 1;
    const int chatRows = std::max(1, static_cast<int>(state.channels.size() + state.messages.size() + 1));
    if (state.selectedChatIndex < 0) state.selectedChatIndex = 0;
    if (state.selectedChatIndex >= chatRows) state.selectedChatIndex = chatRows - 1;
    if (state.chatChannelIndex >= channelRows) state.chatChannelIndex = -1;
    if (state.selectedMessageIndex < 0) state.selectedMessageIndex = 0;
    if (state.selectedMessageIndex >= static_cast<int>(state.messages.size())) {
        state.selectedMessageIndex = state.messages.empty() ? 0 : static_cast<int>(state.messages.size()) - 1;
    }
    const int keyCount = static_cast<int>(keyboardKeys().size());
    if (state.keyboardCursor < 0) state.keyboardCursor = 0;
    if (state.keyboardCursor >= keyCount) state.keyboardCursor = keyCount - 1;
}

void setChatPeerFromMessage(AppState &state, const Message &m) {
    if (!m.direct) {
        state.chatChannelIndex = m.channelIndex;
        state.chatPeerNodeId.clear();
        state.chatPeerName = m.channelName.empty()
            ? (m.channelIndex == 0 ? state.channelName : ("Channel " + std::to_string(m.channelIndex)))
            : m.channelName;
    } else {
        state.chatChannelIndex = -1;
        state.chatPeerNodeId = m.senderId.empty() ? nodeId(m.from) : m.senderId;
        state.chatPeerName = senderText(m);
    }
}

void startCompose(AppState &state) {
    state.composingMessage = true;
    state.composeText.clear();
    state.keyboardCursor = 0;
}

void pressKeyboard(AppState &state) {
    const std::vector<std::string> keys = keyboardKeys();
    if (state.keyboardCursor < 0) state.keyboardCursor = 0;
    if (state.keyboardCursor >= static_cast<int>(keys.size())) state.keyboardCursor = static_cast<int>(keys.size()) - 1;
    const std::string key = keys[state.keyboardCursor];
    if (key == "SPACE") {
        state.composeText += ' ';
    } else if (key == "BACK") {
        if (!state.composeText.empty()) state.composeText.pop_back();
    } else if (key == "SEND") {
        if (!state.composeText.empty() && !state.chatPeerNodeId.empty()) {
            const char *id = state.chatPeerNodeId.c_str();
            if (*id == '!') ++id;
            state.pendingSendTo = static_cast<uint32_t>(std::strtoul(id, nullptr, 16));
            state.pendingSendText = state.composeText;
        }
        state.composingMessage = false;
        state.composeText.clear();
    } else {
        if (!state.composeText.empty()) state.composeText += ' ';
        state.composeText += key;
    }
}

void printAt(int row, int col, const std::string &text) {
    const int edgeWidth = std::max(1, 78 - col);
    const int width = std::min(MaxLine, edgeWidth);
    std::printf("\x1b[%d;%dH%s", row, col, trimTo(text, width).c_str());
}

void printAtW(int row, int col, int width, const std::string &text) {
    const int edgeWidth = std::max(1, 78 - col);
    const int drawWidth = std::min(width, edgeWidth);
    std::string out = trimTo(text, drawWidth);
    while (static_cast<int>(out.size()) < drawWidth) out.push_back(' ');
    std::printf("\x1b[%d;%dH%s", row, col, out.c_str());
}

void printRule(int row) {
    (void)row;
}

void printContent(int row, int col, const std::string &text) {
    const GuiZone &content = zone("content");
    const int boxChars = std::max(8, content.w / CharW - col - 1);
    printAtW(zoneRow("content", row), zoneCol("content", col),
             std::min(content.textWidth, boxChars), text);
}

void printWrappedContent(int &row, const std::string &text, int indent = 4) {
    std::string s = cleanText(text);
    if (s.empty()) {
        printContent(row++, indent, "");
        return;
    }
    const int width = zone("content").textWidth;
    while (!s.empty() && row <= PageBottom) {
        int take = std::min(width, static_cast<int>(s.size()));
        if (take < static_cast<int>(s.size())) {
            int split = take;
            while (split > 20 && s[static_cast<size_t>(split)] != ' ') --split;
            if (split > 20) take = split;
        }
        printContent(row++, indent, s.substr(0, static_cast<size_t>(take)));
        s.erase(0, static_cast<size_t>(take));
        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    }
}

#if defined(WIIMESH_WII)
uint16_t rgb565(int r, int g, int b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

uint16_t blend565(uint16_t dst, uint16_t src, int alpha) {
    const int dr = ((dst >> 11) & 31) << 3;
    const int dg = ((dst >> 5) & 63) << 2;
    const int db = (dst & 31) << 3;
    const int sr = ((src >> 11) & 31) << 3;
    const int sg = ((src >> 5) & 63) << 2;
    const int sb = (src & 31) << 3;
    const int inv = 255 - alpha;
    return rgb565((sr * alpha + dr * inv) / 255,
                  (sg * alpha + dg * inv) / 255,
                  (sb * alpha + db * inv) / 255);
}

void putPixel(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= SkinWidth || y >= SkinHeight) return;
    gSkinPixels[static_cast<size_t>(y) * SkinWidth + x] = c;
}

void fillRect(int x, int y, int w, int h, uint16_t c, int alpha = 255) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SkinWidth) w = SkinWidth - x;
    if (y + h > SkinHeight) h = SkinHeight - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) {
        uint16_t *row = &gSkinPixels[static_cast<size_t>(yy) * SkinWidth + x];
        for (int xx = 0; xx < w; ++xx) {
            row[xx] = alpha >= 255 ? c : blend565(row[xx], c, alpha);
        }
    }
}

void strokeRect(int x, int y, int w, int h, uint16_t c) {
    fillRect(x, y, w, 2, c);
    fillRect(x, y + h - 2, w, 2, c);
    fillRect(x, y, 2, h, c);
    fillRect(x + w - 2, y, 2, h, c);
}

void line(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        putPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void panel(int x, int y, int w, int h, bool selected = false) {
    const uint16_t fill = selected ? rgb565(5, 58, 92) : rgb565(4, 28, 43);
    const uint16_t edge = selected ? rgb565(51, 215, 238) : rgb565(27, 86, 112);
    fillRect(x, y, w, h, fill, 235);
    strokeRect(x, y, w, h, edge);
}

void drawSignalBars(int x, int y, uint16_t c) {
    for (int i = 0; i < 8; ++i) {
        const int h = 6 + i * 4;
        fillRect(x + i * 6, y - h, 4, h, c);
    }
}

void thickLine(int x0, int y0, int x1, int y1, uint16_t c, int thickness = 2) {
    const int radius = std::max(0, thickness / 2);
    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            if (ox * ox + oy * oy <= radius * radius) {
                line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, c);
            }
        }
    }
}

const uint8_t *tinyGlyph(char ch) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t A[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t B[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t C[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t D[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t E[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t F[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t G[7] = {14, 17, 16, 19, 17, 17, 14};
    static const uint8_t H[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t I[7] = {31, 4, 4, 4, 4, 4, 31};
    static const uint8_t J[7] = {1, 1, 1, 1, 17, 17, 14};
    static const uint8_t K[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t L[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t M[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t N[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t O[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t P[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t Q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t R[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t S[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t T[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t U[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t V[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t W[7] = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t X[7] = {17, 17, 10, 4, 10, 17, 17};
    static const uint8_t Y[7] = {17, 17, 10, 4, 4, 4, 4};
    static const uint8_t Z[7] = {31, 1, 2, 4, 8, 16, 31};
    static const uint8_t N0[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t N1[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t N2[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t N3[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t N4[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t N5[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t N6[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t N7[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t N8[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t N9[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t Dash[7] = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t Dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t Colon[7] = {0, 12, 12, 0, 12, 12, 0};
    static const uint8_t Slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const uint8_t Percent[7] = {24, 25, 2, 4, 8, 19, 3};
    static const uint8_t Plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t Eq[7] = {0, 0, 31, 0, 31, 0, 0};
    static const uint8_t Lb[7] = {14, 8, 8, 8, 8, 8, 14};
    static const uint8_t Rb[7] = {14, 2, 2, 2, 2, 2, 14};
    switch (ch) {
    case 'A': return A;
    case 'B': return B;
    case 'C': return C;
    case 'D': return D;
    case 'E': return E;
    case 'F': return F;
    case 'G': return G;
    case 'H': return H;
    case 'I': return I;
    case 'J': return J;
    case 'K': return K;
    case 'L': return L;
    case 'M': return M;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'Q': return Q;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'U': return U;
    case 'V': return V;
    case 'W': return W;
    case 'X': return X;
    case 'Y': return Y;
    case 'Z': return Z;
    case '0': return N0;
    case '1': return N1;
    case '2': return N2;
    case '3': return N3;
    case '4': return N4;
    case '5': return N5;
    case '6': return N6;
    case '7': return N7;
    case '8': return N8;
    case '9': return N9;
    case '-':
    case '_': return Dash;
    case '.': return Dot;
    case ':': return Colon;
    case '/': return Slash;
    case '%': return Percent;
    case '+': return Plus;
    case '=': return Eq;
    case '[': return Lb;
    case ']': return Rb;
    default: return blank;
    }
}

struct TinyFontMetrics {
    int glyphW = 5;
    int glyphH = 7;
    int advance = 6;
    int boldExtra = 0;
};

TinyFontMetrics tinyFontMetrics(int scale) {
    TinyFontMetrics m;
    if (scale == 1) {
        static const int widths[] = {5, 6, 7, 8, 9, 10, 11};
        static const int heights[] = {7, 8, 10, 11, 13, 14, 16};
        static const int advances[] = {6, 7, 8, 9, 11, 12, 13};
        const int size = std::max(0, std::min(gUiFontSize, FontSizeMax));
        m.glyphW = widths[size];
        m.glyphH = heights[size];
        m.advance = advances[size];
    } else {
        m.glyphW = 5 * scale;
        m.glyphH = 7 * scale;
        m.advance = 6 * scale;
    }
    if (gUiFontStyle == 3) {
        m.boldExtra = std::max(1, m.glyphW / 6);
        m.advance += m.boldExtra;
    }
    return m;
}

void drawScaledGlyphBlock(int x, int y, int w, int h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    const uint16_t shadow = rgb565(0, 5, 8);
    if (gUiFontStyle == 1 || gUiFontStyle == 3) fillRect(x + 1, y + 1, w, h, shadow);
    if (gUiFontStyle == 2 && w > 2 && h > 2) {
        fillRect(x + 1, y, std::max(1, w - 2), h, c);
        fillRect(x, y + 1, w, std::max(1, h - 2), c);
    } else {
        fillRect(x, y, w, h, c);
    }
    if (gUiFontStyle == 3) fillRect(x + 1, y, w, h, c);
}

int tinyTextWidth(const std::string &text, int scale) {
    if (text.empty()) return 0;
    const TinyFontMetrics m = tinyFontMetrics(scale);
    return m.glyphW + m.boldExtra + (static_cast<int>(text.size()) - 1) * m.advance;
}

std::string tinySafeText(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : cleanText(text)) {
        if (std::islower(ch)) ch = static_cast<unsigned char>(std::toupper(ch));
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ') {
            out.push_back(static_cast<char>(ch));
        } else if (ch == '-' || ch == '_' || ch == '.' || ch == ':' || ch == '/' ||
                   ch == '%' || ch == '+' || ch == '=' || ch == '[' || ch == ']') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back(' ');
        }
    }
    return out;
}

std::string fitTinyText(const std::string &text, int maxPx, int scale) {
    std::string safe = tinySafeText(text);
    if (maxPx <= 0) return "";
    while (!safe.empty() && tinyTextWidth(safe, scale) > maxPx) {
        safe.pop_back();
    }
    while (!safe.empty() && safe.back() == ' ') safe.pop_back();
    return safe;
}

void drawTinyText(const std::string &text, int x, int y, int scale, uint16_t c) {
    const std::string safe = tinySafeText(text);
    const TinyFontMetrics m = tinyFontMetrics(scale);
    for (size_t i = 0; i < safe.size(); ++i) {
        const uint8_t *glyph = tinyGlyph(safe[i]);
        const int gx = x + static_cast<int>(i) * m.advance;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1u << (4 - col))) {
                    const int x0 = col * m.glyphW / 5;
                    const int x1 = (col + 1) * m.glyphW / 5;
                    const int y0 = row * m.glyphH / 7;
                    const int y1 = (row + 1) * m.glyphH / 7;
                    drawScaledGlyphBlock(gx + x0, y + y0, std::max(1, x1 - x0),
                                         std::max(1, y1 - y0), c);
                }
            }
        }
    }
}

void drawTinyTextFit(const std::string &text, int x, int y, int maxPx, int scale, uint16_t c) {
    drawTinyText(fitTinyText(text, maxPx, scale), x, y, scale, c);
}

std::vector<std::string> wrapTinyText(const std::string &text, int maxPx, int scale, int maxLines) {
    std::vector<std::string> out;
    std::istringstream words(tinySafeText(text));
    std::string word;
    std::string line;
    while (words >> word && static_cast<int>(out.size()) < maxLines) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (tinyTextWidth(candidate, scale) <= maxPx) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            out.push_back(line);
            line.clear();
            if (static_cast<int>(out.size()) >= maxLines) break;
        }
        while (!word.empty() && tinyTextWidth(word, scale) > maxPx) {
            std::string piece = word;
            while (!piece.empty() && tinyTextWidth(piece, scale) > maxPx) piece.pop_back();
            if (piece.empty()) break;
            out.push_back(piece);
            word.erase(0, piece.size());
            if (static_cast<int>(out.size()) >= maxLines) break;
        }
        line = word;
    }
    if (!line.empty() && static_cast<int>(out.size()) < maxLines) out.push_back(line);
    return out;
}

void drawWrappedTinyText(const std::string &text, int x, int y, int maxPx, int scale,
                         int maxLines, int lineGap, uint16_t c) {
    const std::vector<std::string> lines = wrapTinyText(text, maxPx, scale, maxLines);
    for (size_t i = 0; i < lines.size(); ++i) {
        drawTinyText(lines[i], x, y + static_cast<int>(i) * lineGap, scale, c);
    }
}

void drawBitmapIcon(const uint32_t *rows, int x, int y, int w, int h, uint16_t c) {
    for (int iy = 0; iy < icons::IconSize; ++iy) {
        const int y0 = y + iy * h / icons::IconSize;
        const int y1 = y + (iy + 1) * h / icons::IconSize;
        const int ph = std::max(1, y1 - y0);
        for (int ix = 0; ix < icons::IconSize; ++ix) {
            if (!(rows[iy] & (1u << (31 - ix)))) continue;
            const int x0 = x + ix * w / icons::IconSize;
            const int x1 = x + (ix + 1) * w / icons::IconSize;
            fillRect(x0, y0, std::max(1, x1 - x0), ph, c);
        }
    }
}

const emoji::Icon *emojiIcon(uint32_t cp) {
    for (int i = 0; i < emoji::IconCount; ++i) {
        if (emoji::Icons[i].codepoint == cp) return &emoji::Icons[i];
    }
    return nullptr;
}

void drawEmojiIcon(const emoji::Icon &icon, int x, int y, int w, int h) {
    for (int iy = 0; iy < 32; ++iy) {
        const int y0 = y + iy * h / 32;
        const int y1 = y + (iy + 1) * h / 32;
        const int ph = std::max(1, y1 - y0);
        for (int ix = 0; ix < 32; ++ix) {
            const int index = iy * 32 + ix;
            const int alpha = icon.alpha[index];
            if (alpha == 0) continue;
            const int x0 = x + ix * w / 32;
            const int x1 = x + (ix + 1) * w / 32;
            fillRect(x0, y0, std::max(1, x1 - x0), ph, icon.pixels[index], alpha);
        }
    }
}

const uint32_t *menuIconRows(int kind) {
    switch (kind) {
    case ScreenHome: return icons::Home;
    case ScreenNodeOptions: return icons::Node;
    case ScreenChannels: return icons::Channels;
    case ScreenChats: return icons::Chat;
    case ScreenMap: return icons::Map;
    case ScreenSettings: return icons::Settings;
    default: return icons::Home;
    }
}

void drawBellGlyph(int x, int y, int w, int h, uint16_t fill, uint16_t edge) {
    (void)edge;
    drawBitmapIcon(icons::Bell, x, y, w, h, fill);
}

void drawPointerCursor(const AppState &state) {
    if (!state.pointerEnabled || !state.pointerVisible) return;
    const int x = state.pointerX;
    const int y = state.pointerY;
    const uint16_t cyan = rgb565(51, 215, 238);
    const uint16_t white = rgb565(245, 248, 248);
    fillRect(x - 1, y - 8, 3, 17, cyan, 220);
    fillRect(x - 8, y - 1, 17, 3, cyan, 220);
    fillRect(x, y - 5, 1, 11, white, 255);
    fillRect(x - 5, y, 11, 1, white, 255);
}

uint16_t nodeBadgeAccent(const NodeSummary &node) {
    static const uint16_t colors[] = {
        rgb565(67, 226, 207),
        rgb565(198, 82, 35),
        rgb565(176, 215, 132),
        rgb565(229, 110, 144),
        rgb565(181, 104, 179),
        rgb565(47, 193, 174),
        rgb565(161, 230, 20),
        rgb565(112, 16, 32),
        rgb565(203, 167, 218),
        rgb565(242, 207, 40),
        rgb565(80, 112, 178),
        rgb565(107, 239, 173),
        rgb565(42, 247, 76),
        rgb565(234, 58, 224),
        rgb565(24, 68, 229),
        rgb565(82, 191, 1),
    };
    uint32_t hash = node.id ? node.id : 2166136261u;
    const std::string seed = !node.shortName.empty() ? node.shortName : node.name;
    for (unsigned char c : seed) {
        hash ^= c;
        hash *= 16777619u;
    }
    return colors[hash % (sizeof(colors) / sizeof(colors[0]))];
}

std::string nodeBadgeLabel(const NodeSummary &node) {
    std::string raw = !node.shortName.empty() ? node.shortName : node.name;
    raw = cleanText(raw);
    const std::string lowered = lowerText(raw);
    const std::string marker = "meshtastic ";
    const size_t markerPos = lowered.find(marker);
    if (markerPos != std::string::npos && markerPos + marker.size() < raw.size()) {
        raw = raw.substr(markerPos + marker.size());
    }
    std::string out;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            if (out.size() >= 4) break;
        }
    }
    if (out.empty()) {
        const std::string id = node.nodeId.empty() ? nodeId(node.id) : node.nodeId;
        for (int i = static_cast<int>(id.size()) - 1; i >= 0 && out.size() < 4; --i) {
            const char c = id[static_cast<size_t>(i)];
            if (std::isxdigit(static_cast<unsigned char>(c))) {
                out.insert(out.begin(), static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }
    }
    return out.empty() ? "NODE" : out;
}

bool decodeNextUtf8Codepoint(const std::string &text, size_t &i, uint32_t &cp) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    size_t advance = 1;
    cp = 0;
    if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
        cp = ((c & 0x1f) << 6) |
            (static_cast<unsigned char>(text[i + 1]) & 0x3f);
        advance = 2;
    } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
        cp = ((c & 0x0f) << 12) |
            ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) |
            (static_cast<unsigned char>(text[i + 2]) & 0x3f);
        advance = 3;
    } else if ((c & 0xf8) == 0xf0 && i + 3 < text.size()) {
        cp = ((c & 0x07) << 18) |
            ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) |
            ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) |
            (static_cast<unsigned char>(text[i + 3]) & 0x3f);
        advance = 4;
    }
    i += advance;
    return cp != 0;
}

bool isIgnoredIconJoiner(uint32_t cp) {
    return cp == 0xfe0f || cp == 0x200d || cp == 0x2b1b ||
           (cp >= 0x1f3fb && cp <= 0x1f3ff);
}

uint32_t firstUtf8IconCodepoint(const std::string &text) {
    const std::string lowered = lowerText(text);
    auto hasAlias = [&](const char *alias) {
        return lowered.find(alias) != std::string::npos;
    };
    if (hasAlias(":house:") || hasAlias(":home:") || hasAlias(":office:")) return 0x1f3e0;
    if (hasAlias(":satellite:") || hasAlias(":radio:") || hasAlias(":pager:")) return 0x1f4e1;
    if (hasAlias(":bell:") || hasAlias(":alarm_clock:")) return 0x1f514;
    if (hasAlias(":speech_balloon:") || hasAlias(":thought_balloon:") || hasAlias(":chat:")) return 0x1f4ac;
    if (hasAlias(":round_pushpin:") || hasAlias(":pushpin:") || hasAlias(":world_map:") || hasAlias(":map:")) return 0x1f4cd;
    if (hasAlias(":lock:") || hasAlias(":unlock:") || hasAlias(":closed_lock_with_key:")) return 0x1f512;
    if (hasAlias(":key:")) return 0x1f511;
    if (hasAlias(":busts_in_silhouette:") || hasAlias(":family:") || hasAlias(":couple:")) return 0x1f465;
    if (hasAlias(":car:") || hasAlias(":truck:") || hasAlias(":bicyclist:") || hasAlias(":walking:")) return 0x1f697;
    if (hasAlias(":signal_strength:") || hasAlias(":signal:")) return 0x1f4f6;
    if (hasAlias(":computer:") || hasAlias(":tv:") || hasAlias(":iphone:") || hasAlias(":phone:")) return 0x1f4bb;
    if (hasAlias(":sunny:") || hasAlias(":cloud:") || hasAlias(":zap:") || hasAlias(":snowflake:")) return 0x2601;
    if (hasAlias(":evergreen_tree:") || hasAlias(":deciduous_tree:") || hasAlias(":seedling:") ||
        hasAlias(":herb:") || hasAlias(":four_leaf_clover:")) return 0x1f333;
    if (hasAlias(":heart:") || hasAlias(":green_heart:") || hasAlias(":blue_heart:") ||
        hasAlias(":purple_heart:")) return 0x2764;
    if (hasAlias(":star:") || hasAlias(":star2:") || hasAlias(":sparkles:")) return 0x2b50;
    if (hasAlias(":warning:") || hasAlias(":exclamation:") || hasAlias(":fire:")) return 0x26a0;
    if (hasAlias(":question:") || hasAlias(":grey_question:")) return 0x2753;
    if (hasAlias(":wrench:") || hasAlias(":hammer:") || hasAlias(":electric_plug:")) return 0x1f527;
    if (hasAlias(":email:") || hasAlias(":envelope:") || hasAlias(":memo:") || hasAlias(":clipboard:")) return 0x2709;
    if (hasAlias(":musical_note:") || hasAlias(":notes:") || hasAlias(":microphone:") ||
        hasAlias(":headphones:")) return 0x1f3b5;
    if (hasAlias(":video_game:") || hasAlias(":dart:") || hasAlias(":trophy:") ||
        hasAlias(":soccer:") || hasAlias(":football:")) return 0x1f3ae;
    if (hasAlias(":cat:") || hasAlias(":dog:") || hasAlias(":horse:") || hasAlias(":turtle:") ||
        hasAlias(":bird:") || hasAlias(":fish:")) return 0x1f43e;
    if (hasAlias(":smile:") || hasAlias(":grinning:") || hasAlias(":joy:") || hasAlias(":sunglasses:")) return 0x1f600;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t advance = 1;
        if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
            cp = ((c & 0x1f) << 6) |
                (static_cast<unsigned char>(text[i + 1]) & 0x3f);
            advance = 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
            cp = ((c & 0x0f) << 12) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 2]) & 0x3f);
            advance = 3;
        } else if ((c & 0xf8) == 0xf0 && i + 3 < text.size()) {
            cp = ((c & 0x07) << 18) |
                ((static_cast<unsigned char>(text[i + 1]) & 0x3f) << 12) |
                ((static_cast<unsigned char>(text[i + 2]) & 0x3f) << 6) |
                (static_cast<unsigned char>(text[i + 3]) & 0x3f);
            advance = 4;
        }
        if (cp == 0xfe0f || cp == 0x200d || (cp >= 0x1f3fb && cp <= 0x1f3ff)) {
            i += advance;
            continue;
        }
        if (cp >= 0x2300) return cp;
        i += advance;
    }
    return 0;
}

std::vector<uint32_t> utf8IconCodepoints(const std::string &text, int maxCount) {
    std::vector<uint32_t> icons;
    for (size_t i = 0; i < text.size() && static_cast<int>(icons.size()) < maxCount;) {
        uint32_t cp = 0;
        if (!decodeNextUtf8Codepoint(text, i, cp)) continue;
        if (isIgnoredIconJoiner(cp)) continue;
        if (cp >= 0x2300) icons.push_back(cp);
    }
    if (icons.empty()) {
        const uint32_t alias = firstUtf8IconCodepoint(text);
        if (alias != 0) icons.push_back(alias);
    }
    return icons;
}

std::vector<uint32_t> nodeIconCodepoints(const NodeSummary &node, int maxCount) {
    std::vector<uint32_t> icons = utf8IconCodepoints(node.shortName, maxCount);
    if (static_cast<int>(icons.size()) < maxCount) {
        std::vector<uint32_t> longIcons = utf8IconCodepoints(node.name, maxCount - static_cast<int>(icons.size()));
        icons.insert(icons.end(), longIcons.begin(), longIcons.end());
    }
    return icons;
}

#if defined(WIIMESH_ICON_DEBUG)
std::string iconCodepointSummary(const std::vector<uint32_t> &icons) {
    if (icons.empty()) return "";
    std::string out = "U+" + hex32(icons[0]).substr(2);
    if (icons.size() > 1) out += " +" + std::to_string(static_cast<int>(icons.size()) - 1);
    return out;
}
#endif

void drawNodeRadioGlyph(int x, int y, uint16_t c) {
    strokeRect(x + 5, y + 14, 22, 10, c);
    fillRect(x + 9, y + 18, 14, 3, c);
    fillRect(x + 15, y + 10, 3, 6, c);
    strokeRect(x + 11, y + 5, 10, 8, c);
    line(x + 16, y + 5, x + 21, y, c);
    line(x + 16, y + 5, x + 11, y, c);
}

void fillCircle(int cx, int cy, int radius, uint16_t c) {
    const int r2 = radius * radius;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= r2) fillRect(cx + x, cy + y, 1, 1, c, 255);
        }
    }
}

void strokeCircle(int cx, int cy, int radius, uint16_t c) {
    const int outer = radius * radius;
    const int inner = (radius - 2) * (radius - 2);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const int d = x * x + y * y;
            if (d <= outer && d >= inner) fillRect(cx + x, cy + y, 1, 1, c, 255);
        }
    }
}

void drawAvatarFrame(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    const int radius = std::min(w, h) / 2 - 1;
    fillCircle(cx, cy, radius + 1, rgb565(2, 9, 14));
    fillCircle(cx, cy, radius, fill);
    strokeCircle(cx, cy, radius, border);
    strokeCircle(cx, cy, radius - 3, rgb565(8, 18, 22));
}

void drawNodeBadgeLabel(const std::string &label, int x, int y, int w, int h,
                        uint16_t fill, uint16_t ink) {
    drawAvatarFrame(x, y, w, h, fill, rgb565(235, 238, 238));
    const int scale = 1;
    const std::string fitted = fitTinyText(label, std::max(8, w - 14), scale);
    const int textW = tinyTextWidth(fitted, scale);
    const int textH = 7 * scale;
    drawTinyText(fitted, x + (w - textW) / 2, y + (h - textH) / 2, scale, ink);
}

void drawUserIconGlyphs(const std::vector<uint32_t> &icons, int x, int y, int w, int h, uint16_t ink);

bool drawPixelIcon(uint32_t cp, int x, int y, int scale, uint16_t ink) {
    const uint8_t *rows = nullptr;
    static const uint8_t bird[8] = {
        0x18, 0x3c, 0x5a, 0x7e, 0x3c, 0x24, 0x42, 0x00,
    };
    static const uint8_t lobster[8] = {
        0x81, 0xc3, 0x66, 0x3c, 0x7e, 0xa5, 0x24, 0x42,
    };
    static const uint8_t tower[8] = {
        0x18, 0x3c, 0x24, 0x3c, 0x24, 0x5a, 0x66, 0xff,
    };
    static const uint8_t runner[8] = {
        0x18, 0x18, 0x3c, 0x5a, 0x18, 0x34, 0x42, 0x81,
    };
    static const uint8_t hiker[8] = {
        0x18, 0x18, 0x3c, 0x5a, 0x18, 0x34, 0x4a, 0x89,
    };
    if (cp == 0x1f426) {
        rows = bird;
    } else if (cp == 0x1f99e) {
        rows = lobster;
    } else if (cp == 0x1f5fc) {
        rows = tower;
    } else if (cp == 0x1f3c3 || cp == 0x1f6b6) {
        rows = runner;
    } else if (cp == 0x1f97e || cp == 0x1f9cd || cp == 0x1f9d7) {
        rows = hiker;
    } else {
        return false;
    }
    for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
            if (rows[py] & (0x80u >> px)) {
                fillRect(x + px * scale, y + py * scale, scale, scale, ink, 255);
            }
        }
    }
    return true;
}

void drawUserIconGlyph(uint32_t cp, int x, int y, int w, int h, uint16_t ink) {
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    auto fencer = [&]() {
        fillCircle(cx - 6, y + 10, 3, ink);
        thickLine(cx - 5, y + 15, cx + 1, y + 22, ink);
        thickLine(cx - 1, y + 16, x + w - 5, y + 7, ink);
        line(x + w - 9, y + 6, x + w - 2, y + 4, ink);
        line(x + w - 9, y + 6, x + w - 2, y + 8, ink);
        thickLine(cx - 4, y + 22, x + 8, y + h - 7, ink);
        thickLine(cx, y + 22, x + w - 12, y + h - 8, ink);
        line(cx - 9, y + 17, x + 7, y + 22, ink);
    };
    auto face = [&]() {
        strokeRect(cx - 10, cy - 10, 20, 20, ink);
        fillRect(cx - 5, cy - 3, 3, 3, ink);
        fillRect(cx + 4, cy - 3, 3, 3, ink);
        line(cx - 5, cy + 6, cx, cy + 9, ink);
        line(cx, cy + 9, cx + 6, cy + 6, ink);
    };
    auto person = [&]() {
        strokeRect(cx - 6, y + 6, 12, 12, ink);
        strokeRect(cx - 12, y + 21, 24, 10, ink);
    };
    auto leaf = [&]() {
        line(cx, y + 5, cx, y + h - 5, ink);
        line(cx, y + 13, x + 9, y + 8, ink);
        line(cx, y + 18, x + w - 9, y + 10, ink);
        line(cx, y + 23, x + 10, y + 25, ink);
    };
    auto cloud = [&]() {
        strokeRect(x + 8, y + 15, w - 16, 11, ink);
        strokeRect(x + 12, y + 10, 10, 10, ink);
        strokeRect(x + 22, y + 8, 10, 12, ink);
    };
    auto tool = [&]() {
        line(x + 8, y + 7, x + w - 8, y + h - 7, ink);
        strokeRect(x + 6, y + 5, 9, 9, ink);
        fillRect(x + w - 12, y + h - 12, 8, 8, ink);
    };
    auto note = [&]() {
        fillRect(cx + 3, y + 7, 3, 18, ink);
        fillRect(cx + 5, y + 7, 11, 3, ink);
        strokeRect(cx - 8, y + 23, 11, 7, ink);
    };
    auto heart = [&]() {
        fillRect(cx - 8, y + 10, 7, 7, ink);
        fillRect(cx + 1, y + 10, 7, 7, ink);
        line(cx - 10, y + 17, cx, y + 29, ink);
        line(cx + 10, y + 17, cx, y + 29, ink);
    };
    auto star = [&]() {
        line(cx, y + 5, cx + 4, cy - 1, ink);
        line(cx + 4, cy - 1, x + w - 6, cy - 1, ink);
        line(x + w - 6, cy - 1, cx + 5, cy + 4, ink);
        line(cx + 5, cy + 4, cx + 8, y + h - 5, ink);
        line(cx + 8, y + h - 5, cx, cy + 8, ink);
        line(cx, cy + 8, cx - 8, y + h - 5, ink);
        line(cx - 8, y + h - 5, cx - 5, cy + 4, ink);
        line(cx - 5, cy + 4, x + 6, cy - 1, ink);
        line(x + 6, cy - 1, cx - 4, cy - 1, ink);
        line(cx - 4, cy - 1, cx, y + 5, ink);
    };
    if (const emoji::Icon *icon = emojiIcon(cp)) {
        (void)ink;
        drawEmojiIcon(*icon, x + 4, y + 4, w - 8, h - 8);
    } else if (cp == 0x1f93a) {
        fencer();
    } else if (drawPixelIcon(cp, cx - 12, cy - 12, 3, ink)) {
        return;
    } else if ((cp >= 0x1f600 && cp <= 0x1f64f) || (cp >= 0x1f910 && cp <= 0x1f97f)) {
        face();
    } else if ((cp >= 0x1f466 && cp <= 0x1f487) || cp == 0x1f464 || cp == 0x1f465 ||
               (cp >= 0x1f9d1 && cp <= 0x1f9dd)) {
        person();
    } else if (cp == 0x2302 || cp == 0x1f3e0 || cp == 0x1f3e1 || cp == 0x1f3e2) {
        line(x + 6, y + 16, cx, y + 6, ink);
        line(cx, y + 6, x + w - 6, y + 16, ink);
        strokeRect(x + 9, y + 16, w - 18, h - 10, ink);
        fillRect(cx - 3, y + h - 10, 6, 10, ink);
    } else if ((cp >= 0x1f300 && cp <= 0x1f32c) || cp == 0x2600 || cp == 0x2601 ||
               cp == 0x2614 || cp == 0x26a1 || cp == 0x2744) {
        cloud();
    } else if ((cp >= 0x1f330 && cp <= 0x1f343) || (cp >= 0x1f33a && cp <= 0x1f33f) ||
               cp == 0x1f344 || cp == 0x1f335 || cp == 0x1f334) {
        leaf();
    } else if ((cp >= 0x1f400 && cp <= 0x1f43e)) {
        strokeRect(cx - 9, cy - 8, 18, 16, ink);
        fillRect(cx - 4, cy - 1, 3, 3, ink);
        fillRect(cx + 3, cy - 1, 3, 3, ink);
        fillRect(cx - 8, y + 7, 6, 6, ink);
        fillRect(cx + 3, y + 7, 6, 6, ink);
    } else if (cp == 0x1f4e1 || cp == 0x1f4fb || cp == 0x1f6dc) {
        drawNodeRadioGlyph(x, y, ink);
    } else if (cp == 0x1f4bb || cp == 0x1f4fa || cp == 0x1f4f1 || cp == 0x260e ||
               cp == 0x1f4de || cp == 0x1f4df) {
        strokeRect(x + 6, y + 8, w - 12, 16, ink);
        fillRect(cx - 7, y + 27, 14, 3, ink);
        fillRect(cx - 2, y + 24, 4, 4, ink);
    } else if (cp == 0x1f514) {
        strokeRect(cx - 8, y + 14, 16, 12, ink);
        line(cx - 8, y + 14, cx - 4, y + 8, ink);
        line(cx + 8, y + 14, cx + 4, y + 8, ink);
        fillRect(cx - 2, y + 5, 4, 5, ink);
        fillRect(cx - 4, y + 27, 8, 3, ink);
    } else if (cp == 0x1f511) {
        strokeRect(x + 6, cy - 5, 12, 12, ink);
        fillRect(x + 18, cy, 16, 3, ink);
        fillRect(x + 28, cy, 3, 7, ink);
    } else if (cp == 0x1f512 || cp == 0x1f513) {
        strokeRect(x + 9, cy - 1, w - 18, 16, ink);
        strokeRect(x + 13, y + 7, w - 26, 13, ink);
        fillRect(cx - 2, cy + 4, 4, 6, ink);
    } else if (cp == 0x1f4ac || cp == 0x1f5e8) {
        strokeRect(x + 5, y + 8, w - 10, 18, ink);
        line(x + 12, y + 26, x + 8, y + 31, ink);
        line(x + 12, y + 26, x + 18, y + 26, ink);
    } else if (cp == 0x1f4cd || cp == 0x1f5fa) {
        strokeRect(cx - 7, y + 6, 14, 14, ink);
        line(cx, y + 20, cx - 7, y + 31, ink);
        line(cx, y + 20, cx + 7, y + 31, ink);
    } else if (cp == 0x1f465) {
        strokeRect(x + 7, y + 9, 8, 8, ink);
        strokeRect(x + 20, y + 9, 8, 8, ink);
        strokeRect(x + 5, y + 21, 12, 8, ink);
        strokeRect(x + 18, y + 21, 12, 8, ink);
    } else if (cp == 0x1f697 || cp == 0x1f699 || cp == 0x1f69a || cp == 0x1f6b2 || cp == 0x1f6b6) {
        strokeRect(x + 5, cy - 3, w - 10, 10, ink);
        fillRect(x + 10, cy + 8, 5, 5, ink);
        fillRect(x + w - 15, cy + 8, 5, 5, ink);
    } else if (cp == 0x1f4f6) {
        drawSignalBars(x + 7, y + 28, ink);
    } else if (cp == 0x1f3b5 || cp == 0x1f3b6 || (cp >= 0x1f3a4 && cp <= 0x1f3b8)) {
        note();
    } else if ((cp >= 0x1f3c0 && cp <= 0x1f3c6) || (cp >= 0x26bd && cp <= 0x26be) ||
               cp == 0x1f3af || cp == 0x1f3b2 || cp == 0x1f3ae) {
        strokeRect(cx - 10, cy - 10, 20, 20, ink);
        line(cx - 10, cy, cx + 10, cy, ink);
        line(cx, cy - 10, cx, cy + 10, ink);
    } else if ((cp >= 0x1f527 && cp <= 0x1f529) || cp == 0x1f528 || cp == 0x1f50c) {
        tool();
    } else if ((cp >= 0x1f4c5 && cp <= 0x1f4dd) || cp == 0x2709 || cp == 0x1f4e7 ||
               cp == 0x1f4e5 || cp == 0x1f4e4) {
        strokeRect(x + 7, y + 7, w - 14, h - 14, ink);
        line(x + 7, y + 7, cx, cy, ink);
        line(x + w - 7, y + 7, cx, cy, ink);
    } else if ((cp >= 0x1f493 && cp <= 0x1f49f) || cp == 0x2764 || cp == 0x1f90d ||
               cp == 0x1f5a4 || cp == 0x1f49a || cp == 0x1f499 || cp == 0x1f49c) {
        heart();
    } else if (cp == 0x2b50 || cp == 0x1f31f || cp == 0x2728 || cp == 0x1f4ab) {
        star();
    } else if (cp == 0x1f525 || cp == 0x1f4a5 || cp == 0x26a0 || cp == 0x2757) {
        line(cx, y + 5, x + 8, y + h - 6, ink);
        line(cx, y + 5, x + w - 8, y + h - 6, ink);
        fillRect(cx - 2, y + 14, 4, 9, ink);
        fillRect(cx - 2, y + 26, 4, 4, ink);
    } else if (cp == 0x2753 || cp == 0x2754 || cp == 0x2049) {
        strokeRect(cx - 5, y + 7, 11, 10, ink);
        fillRect(cx + 3, y + 17, 3, 7, ink);
        fillRect(cx - 1, y + 27, 4, 4, ink);
    } else {
        strokeRect(x + 8, y + 6, w - 16, h - 12, ink);
        line(x + 12, y + 12, x + w - 12, y + h - 12, ink);
        line(x + w - 12, y + 12, x + 12, y + h - 12, ink);
    }
}

void drawUserIconGlyphs(const std::vector<uint32_t> &icons, int x, int y, int w, int h, uint16_t ink) {
    if (icons.empty()) {
        const int inset = std::max(5, std::min(w, h) / 6);
        drawNodeRadioGlyph(x + inset, y + inset, ink);
        return;
    }
    const int pad = std::max(5, std::min(w, h) / 7);
    x += pad;
    y += pad;
    w = std::max(8, w - pad * 2);
    h = std::max(8, h - pad * 2);
    bool allPixelIcons = true;
    for (uint32_t cp : icons) {
        if (cp != 0x1f426 && cp != 0x1f99e && cp != 0x1f5fc &&
            cp != 0x1f3c3 && cp != 0x1f6b6 && cp != 0x1f97e &&
            cp != 0x1f9cd && cp != 0x1f9d7) {
            allPixelIcons = false;
            break;
        }
    }
    if (allPixelIcons && icons.size() > 1) {
        const int count = std::min(3, static_cast<int>(icons.size()));
        const int scale = std::max(1, std::min(2, std::min(w, h) / 18));
        const int icon = 8 * scale;
        if (count == 2) {
            drawPixelIcon(icons[0], x + 1, y + (h - icon) / 2, scale, ink);
            drawPixelIcon(icons[1], x + w - icon - 1, y + (h - icon) / 2, scale, ink);
        } else {
            drawPixelIcon(icons[0], x + 1, y + 1, scale, ink);
            drawPixelIcon(icons[1], x + w - icon - 1, y + 1, scale, ink);
            drawPixelIcon(icons[2], x + (w - icon) / 2, y + h - icon - 1, scale, ink);
        }
        return;
    }
    if (icons.size() == 1) {
        drawUserIconGlyph(icons[0], x, y, w, h, ink);
        return;
    }
    const int count = std::min(3, static_cast<int>(icons.size()));
    const int sub = std::max(12, std::min(16, std::min(w, h) / 2));
    const int px[3] = {x + 1, x + w - sub - 1, x + (w - sub) / 2};
    const int py[3] = {y + 2, y + 2, y + h - sub - 2};
    for (int i = 0; i < count; ++i) {
        drawUserIconGlyph(icons[static_cast<size_t>(i)], px[i], py[i], sub, sub, ink);
    }
}

std::string screensaverName(const AppState &state) {
    if (!state.nodeName.empty() && state.nodeName != "Unknown" && state.nodeName != "waiting") {
        return cleanText(state.nodeName);
    }
    if (!state.myNodeId.empty() && state.myNodeId != "waiting") {
        return cleanText(state.myNodeId);
    }
    return "WiiMesh";
}

std::string screensaverModeText(int mode) {
    switch (mode) {
    case 1: return "LAST UNREAD";
    case 2: return "ACTIVE USERS";
    default: return "USERNAME";
    }
}

std::string fontStyleText(int style) {
    switch (((style % FontStyleCount) + FontStyleCount) % FontStyleCount) {
    case 0: return "PIXEL";
    case 2: return "SOFT PIXEL";
    case 3: return "BOLD";
    default: return "TV SHADOW";
    }
}

std::string fontSizeText(int size) {
    const int clamped = std::max(0, std::min(size, FontSizeMax));
    std::string bar;
    for (int i = 0; i <= FontSizeMax; ++i) bar += (i == clamped) ? "|" : "-";
    return bar + " " + std::to_string(clamped) + "/" + std::to_string(FontSizeMax);
}

std::string screensaverDisplayText(const AppState &state) {
    const int mode = ((state.screensaverMode % 3) + 3) % 3;
    if (mode == 1) {
        for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0; --i) {
            if (i < static_cast<int>(state.seenMessageCount)) break;
            const Message &m = state.messages[static_cast<size_t>(i)];
            return senderText(m) + ": " + messageText(m);
        }
        if (!state.messages.empty()) {
            const Message &m = state.messages.back();
            return senderText(m) + ": " + messageText(m);
        }
        return "No unread messages";
    }
    if (mode == 2) {
        const int online = state.onlineNodeCount >= 0 ? state.onlineNodeCount : static_cast<int>(state.knownNodes.size());
        const int total = state.totalNodeCount >= 0 ? state.totalNodeCount : static_cast<int>(state.knownNodes.size());
        std::string text = std::to_string(online) + "/" + std::to_string(total) + " nodes";
        int added = 0;
        for (const auto &node : state.knownNodes) {
            if (node.isMine) continue;
            const std::string name = node.shortName.empty() ? (node.name.empty() ? nodeBadgeLabel(node) : node.name) : node.shortName;
            text += added == 0 ? ": " : ", ";
            text += cleanText(name);
            if (++added >= 3) break;
        }
        return text;
    }
    return screensaverName(state);
}

#if defined(WIIMESH_WII)
float screensaverSpeedValue(const AppState &state) {
    static const float speeds[] = {0.12f, 0.22f, 0.35f, 0.52f, 0.75f, 1.05f};
    const int index = std::max(0, std::min(state.screensaverSpeed, static_cast<int>(sizeof(speeds) / sizeof(speeds[0])) - 1));
    return speeds[index];
}

std::vector<std::string> screensaverUserLabels(const AppState &state) {
    std::vector<std::string> labels;
    for (const auto &node : state.knownNodes) {
        if (labels.size() >= 8) break;
        std::string label = !node.shortName.empty() ? node.shortName : (!node.name.empty() ? node.name : nodeBadgeLabel(node));
        label = cleanText(label);
        if (label.empty()) continue;
        labels.push_back(label);
    }
    if (labels.empty()) labels.push_back(screensaverName(state));
    return labels;
}

uint32_t textHash(const std::string &text) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

void ensureScreensaverBadges(const AppState &state) {
    const std::vector<std::string> labels = screensaverUserLabels(state);
    bool changed = labels.size() != gScreensaverBadges.size();
    if (!changed) {
        for (size_t i = 0; i < labels.size(); ++i) {
            if (gScreensaverBadges[i].text != labels[i]) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return;
    gScreensaverBadges.clear();
    const float speed = screensaverSpeedValue(state);
    for (size_t i = 0; i < labels.size(); ++i) {
        ScreensaverBadge badge;
        badge.text = labels[i];
        badge.w = std::min(240, std::max(76, static_cast<int>(badge.text.size()) * 11 + 34));
        badge.h = 42;
        const uint32_t hash = textHash(badge.text) ^ static_cast<uint32_t>(i * 0x9e3779b9u);
        badge.x = 44.0f + static_cast<float>(hash % std::max(1, SkinWidth - badge.w - 88));
        badge.y = 52.0f + static_cast<float>((hash >> 8) % std::max(1, SkinHeight - badge.h - 104));
        badge.dx = ((hash & 1u) ? speed : -speed) * (0.75f + 0.08f * static_cast<float>(i % 4));
        badge.dy = ((hash & 2u) ? speed : -speed) * (0.52f + 0.07f * static_cast<float>((i + 1) % 4));
        gScreensaverBadges.push_back(badge);
    }
}

void updateScreensaverBadgeMotion(const AppState &state) {
    ensureScreensaverBadges(state);
    const float speed = screensaverSpeedValue(state);
    for (size_t i = 0; i < gScreensaverBadges.size(); ++i) {
        ScreensaverBadge &badge = gScreensaverBadges[i];
        const float xSpeed = speed * (0.75f + 0.08f * static_cast<float>(i % 4));
        const float ySpeed = speed * (0.52f + 0.07f * static_cast<float>((i + 1) % 4));
        badge.dx = badge.dx < 0 ? -xSpeed : xSpeed;
        badge.dy = badge.dy < 0 ? -ySpeed : ySpeed;
        badge.x += badge.dx;
        badge.y += badge.dy;
        const float minX = 6.0f;
        const float maxX = static_cast<float>(SkinWidth - badge.w - 6);
        const float minY = 6.0f;
        const float maxY = static_cast<float>(SkinHeight - badge.h - 6);
        if (badge.x < minX) {
            badge.x = minX;
            badge.dx = std::abs(badge.dx);
        } else if (badge.x > maxX) {
            badge.x = maxX;
            badge.dx = -std::abs(badge.dx);
        }
        if (badge.y < minY) {
            badge.y = minY;
            badge.dy = std::abs(badge.dy);
        } else if (badge.y > maxY) {
            badge.y = maxY;
            badge.dy = -std::abs(badge.dy);
        }
    }
}

void drawScreensaverBox(int x, int y, int boxW, int boxH, uint16_t edge) {
    fillRect(x - 6, y - 6, boxW + 12, boxH + 12, rgb565(35, 55, 58), 255);
    fillRect(x, y, boxW, boxH, rgb565(8, 18, 22), 255);
    strokeRect(x, y, boxW, boxH, edge);
    fillRect(x + 12, y + boxH - 14, boxW - 24, 4, rgb565(51, 215, 238), 255);
}

void updateScreensaverMotion(const AppState &state, const std::string &name) {
    const int textChars = std::max(8, static_cast<int>(name.size()));
    int boxW = std::min(500, std::max(180, textChars * 18 + 64));
    int boxH = 92;
    const float visibleEdge = 34.0f;
    const float minX = -static_cast<float>(boxW) + visibleEdge;
    const float maxX = static_cast<float>(SkinWidth) - visibleEdge;
    const float minY = -static_cast<float>(boxH) + visibleEdge;
    const float maxY = static_cast<float>(SkinHeight) - visibleEdge;
    const float speed = screensaverSpeedValue(state);
    const float sx = gScreensaverDx < 0 ? -speed : speed;
    const float sy = gScreensaverDy < 0 ? -speed * 0.72f : speed * 0.72f;
    gScreensaverX += sx;
    gScreensaverY += sy;
    if (gScreensaverX < minX) {
        gScreensaverX = minX;
        gScreensaverDx = std::abs(speed);
    } else if (gScreensaverX > maxX) {
        gScreensaverX = maxX;
        gScreensaverDx = -std::abs(speed);
    }
    if (gScreensaverY < minY) {
        gScreensaverY = minY;
        gScreensaverDy = std::abs(speed * 0.72f);
    } else if (gScreensaverY > maxY) {
        gScreensaverY = maxY;
        gScreensaverDy = -std::abs(speed * 0.72f);
    }
}

void drawScreensaverSkin(const AppState &state) {
    if (!gGraphicsReady || gSkinPixels.empty()) return;
    ++gScreensaverFrame;
    for (int py = 0; py < SkinHeight; ++py) {
        const uint16_t c = blend565(rgb565(1, 7, 12), rgb565(4, 19, 24), py * 255 / SkinHeight);
        for (int px = 0; px < SkinWidth; ++px) {
            gSkinPixels[static_cast<size_t>(py) * SkinWidth + px] = c;
        }
    }
    if (((state.screensaverMode % 3) + 3) % 3 == 2) {
        updateScreensaverBadgeMotion(state);
        for (size_t i = 0; i < gScreensaverBadges.size(); ++i) {
            const ScreensaverBadge &badge = gScreensaverBadges[i];
            const uint16_t edge = (i % 3 == 0) ? rgb565(77, 232, 141)
                : (i % 3 == 1 ? rgb565(51, 215, 238) : rgb565(238, 191, 75));
            drawScreensaverBox(static_cast<int>(badge.x), static_cast<int>(badge.y), badge.w, badge.h, edge);
        }
    } else {
        const std::string name = screensaverDisplayText(state);
        updateScreensaverMotion(state, name);
        const int textChars = std::max(8, static_cast<int>(name.size()));
        const int boxW = std::min(500, std::max(180, textChars * 18 + 64));
        const int boxH = 92;
        const int x = static_cast<int>(gScreensaverX);
        const int y = static_cast<int>(gScreensaverY);
        drawScreensaverBox(x, y, boxW, boxH, rgb565(77, 232, 141));
        const int pulse = (gScreensaverFrame / 18) % 9;
        for (int i = 0; i < 9; ++i) {
            const int h = 6 + ((i + pulse) % 5) * 5;
            fillRect(x + boxW - 42 + i * 4, y + 18 + (26 - h), 3, h, rgb565(51, 215, 238), 240);
        }
    }
    gfx_tex_convert(&gSkinTexture, gSkinPixels.data());
    gfx_tex_flush_texture(&gSkinTexture);
    gfx_screen_coords_t coords = {0.0f, 0.0f, 640.0f, 480.0f};
    gfx_draw_tex(&gSkinTexture, &coords);
}

void drawSectionTitle(const std::string &title, int x, int y, int maxPx) {
    drawTinyTextFit(title, x, y, maxPx, 1, rgb565(238, 246, 246));
    fillRect(x, y + 10, std::min(maxPx, 130), 3, rgb565(77, 232, 141), 230);
}

void drawGraphicalHeader(const AppState &state, int x, int y, int w, int h) {
    const uint16_t bright = rgb565(238, 246, 246);
    const uint16_t cyan = rgb565(51, 215, 238);
    const uint16_t green = rgb565(77, 232, 141);
    const uint16_t amber = rgb565(238, 191, 75);
    panel(x, y, w, h, false);
    drawTinyTextFit("WIIMESH V" + std::string(AppVersion), x + 14, y + 10, std::max(80, w / 3), 1, bright);
    drawTinyTextFit(state.protocolReady ? "ONLINE" : (state.usbConnected ? "SYNCING" : "OFFLINE"),
                    x + w - 138, y + 10, 60, 1, state.protocolReady ? green : amber);
    drawTinyTextFit(state.networkReady ? ("WII " + state.wiiIp) : "WII OFF", x + w - 118, y + 10, 110, 1, cyan);
    drawTinyTextFit(state.channelName + "  NODES " + std::to_string(state.nodeCount) +
                    "  TEXT " + std::to_string(state.textMessageCount),
                    x + 14, y + 25, w - 112, 1, rgb565(180, 205, 212));
    drawTinyTextFit(tickerText(state), x + 14, y + 40, w - 112, 1, green);
    drawSignalBars(x + w - 74, y + 60, cyan);
    const int barY = std::min(y + h - 8, y + 56);
    fillRect(x + 8, barY, w - 16, 5, rgb565(20, 78, 83), 255);
    fillRect(x + 8, barY, state.protocolReady ? std::max(50, w - 120) : 90, 5,
             state.protocolReady ? green : amber, 255);
}

void drawGraphicalMenu(const AppState &state, int x, int y, int w, int h) {
    const int active = activePrimaryTab(state);
    const int slotH = std::max(48, h / PrimaryTabCount);
    panel(x, y, w, h, false);
    for (int i = 0; i < PrimaryTabCount; ++i) {
        const int itemY = y + i * slotH + 3;
        const int itemH = std::max(42, std::min(slotH - 5, 58));
        const bool selected = i == active;
        panel(x + 6, itemY, w - 12, itemH, selected);
        const uint16_t ink = selected ? rgb565(255, 255, 255) : rgb565(182, 197, 202);
        const int iconSize = std::max(20, std::min(27, itemH - 20));
        const int iconX = x + (w - iconSize) / 2;
        const int iconY = itemY + 5;
        drawBitmapIcon(menuIconRows(i), iconX, iconY, iconSize, iconSize, ink);
        const std::string label = menuLabel(i);
        const int labelW = tinyTextWidth(label, 1);
        const int labelX = x + (w - labelW) / 2;
        drawTinyText(label, labelX, itemY + itemH - 11, 1, ink);
    }
}

void drawHomePanel(const AppState &state, int x, int y, int w, int h) {
    const uint32_t unread = state.textMessageCount > state.seenMessageCount
        ? state.textMessageCount - state.seenMessageCount
        : 0;
    const std::string nodeTotal = state.totalNodeCount >= 0
        ? std::to_string(state.totalNodeCount)
        : std::to_string(state.nodeCount);
    const std::string online = state.onlineNodeCount >= 0
        ? std::to_string(state.onlineNodeCount) + " OF " + nodeTotal + " NODES ONLINE"
        : nodeTotal + " NODES KNOWN";
    drawSectionTitle("HOME", x + 16, y + 14, w - 32);
    int rowY = y + 34;
    const int rowGap = 18;
    drawTinyTextFit("MAIL " + std::to_string(unread) + " NEW  " +
                    std::to_string(state.messages.size()) + "/" + std::to_string(MaxMessages) + " SAVED",
                    x + 24, rowY, w - 48, 1, rgb565(235, 244, 244));
    rowY += rowGap;
    drawTinyTextFit("NODES " + online, x + 24, rowY, w - 48, 1, rgb565(235, 244, 244));
    rowY += rowGap;
    drawTinyTextFit("TIME " + nowTimeText() + "  " + nowDateText(), x + 24, rowY, w - 48, 1, rgb565(235, 244, 244));
    rowY += rowGap;
    drawTinyTextFit("RADIO " + state.channelName + "  " + (state.protocolReady ? "ONLINE" : "WAITING"),
                    x + 24, rowY, w - 48, 1, rgb565(235, 244, 244));
    rowY += rowGap;
    drawTinyTextFit("LORA " + state.usbDetail, x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    rowY += rowGap;
    drawTinyTextFit("SIG SNR " + fixed1(state.lastRxSnr, "DB") + " RSSI " +
                    (state.lastRxRssi == 0 ? std::string("WAITING") : std::to_string(state.lastRxRssi)),
                    x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    rowY += rowGap;
    drawTinyTextFit("PWR BAT " + integerOrWaiting(state.batteryLevel, "%") + "  VOLT " + fixed2(state.voltage, "V"),
                    x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    rowY += rowGap;
    drawTinyTextFit("AIR CHUTIL " + fixed1(state.channelUtilization, "%") + " TXAIR " + fixed1(state.airUtilTx, "%"),
                    x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    rowY += rowGap;
    drawTinyTextFit("WII IP " + state.wiiIp + "  UDP -> " +
                    (state.udpTargetIp.empty() ? "unset" : state.udpTargetIp),
                    x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    rowY += rowGap;
    drawTinyTextFit("M DEVICE IP " + state.meshDeviceIp, x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    if (!state.messages.empty()) {
        const Message &m = state.messages.back();
        fillRect(x + 14, y + h - 64, w - 28, 50, rgb565(18, 39, 47), 230);
        strokeRect(x + 14, y + h - 64, w - 28, 50, rgb565(35, 99, 118));
        drawTinyTextFit("LATEST", x + 24, y + h - 54, w - 48, 1, rgb565(77, 232, 141));
        drawTinyTextFit(timeText(m.rxTime) + " " + (m.direct ? "DM " : "CH ") + senderText(m),
                        x + 24, y + h - 38, w - 48, 1, rgb565(235, 244, 244));
        drawTinyTextFit(messageText(m), x + 24, y + h - 23, w - 48, 1, rgb565(235, 244, 244));
    }
}

void drawNodeCard(const NodeSummary &node, bool selected, int x, int y, int w, int h, const AppState &state) {
    const uint16_t edge = selected ? rgb565(77, 232, 141) : rgb565(120, 128, 130);
    const uint16_t fill = selected ? rgb565(60, 86, 88) : rgb565(50, 53, 54);
    fillRect(x, y, w, h, fill, 245);
    strokeRect(x, y, w, h, edge);
    const int avatar = std::min(38, h - 12);
    const int ax = x + 12;
    const int ay = y + (h - avatar) / 2;
    const std::vector<uint32_t> icons = nodeIconCodepoints(node, 3);
    if (icons.empty()) {
        drawNodeBadgeLabel(nodeBadgeLabel(node), ax, ay, avatar, avatar, nodeBadgeAccent(node), rgb565(7, 18, 21));
    } else {
        drawAvatarFrame(ax, ay, avatar, avatar, nodeBadgeAccent(node), rgb565(235, 238, 238));
        drawUserIconGlyphs(icons, ax, ay, avatar, avatar, rgb565(248, 250, 250));
    }
    const int tx = ax + avatar + 18;
    const int rightW = 88;
    const std::string title = node.name.empty() ? node.nodeId : node.name;
    drawTinyTextFit(title, tx, y + 9, std::max(20, w - (tx - x) - rightW), 1, rgb565(245, 248, 248));
    drawTinyTextFit(node.shortName.empty() || !icons.empty() ? node.nodeId : node.shortName + "  " + node.nodeId,
                    tx, y + 26, std::max(20, w - (tx - x) - rightW), 1, rgb565(210, 222, 224));
    std::string top = nodeConnectionText(node);
    std::string mid = lastHeardText(node.lastHeard);
    std::string bottom = node.hardwareName.empty() ? "KNOWN" : node.hardwareName;
    if (node.snr > -999.0f) {
        char snrBuf[24];
        std::snprintf(snrBuf, sizeof(snrBuf), "SNR %.1f", node.snr);
        mid = snrBuf;
    }
    if (node.batteryLevel >= 0 || node.voltage > 0.0f) {
        bottom = integerOrWaiting(node.batteryLevel, "%") + " " + fixed2(node.voltage, "V");
    } else if (!node.macAddress.empty()) {
        bottom = node.macAddress.substr(std::max<int>(0, static_cast<int>(node.macAddress.size()) - 8));
    }
    if (node.isMine) {
        bottom = integerOrWaiting(state.batteryLevel >= 0 ? state.batteryLevel : node.batteryLevel, "%") +
                 " " + fixed2(state.voltage > 0.0f ? state.voltage : node.voltage, "V");
    }
    drawTinyTextFit(top, x + w - rightW + 4, y + 7, rightW - 8, 1, rgb565(235, 244, 244));
    drawTinyTextFit(mid, x + w - rightW + 4, y + 22, rightW - 8, 1, rgb565(77, 232, 141));
    drawTinyTextFit(bottom, x + w - rightW + 4, y + h - 13, rightW - 8, 1, rgb565(235, 244, 244));
}

void drawNodesPanel(const AppState &state, int x, int y, int w, int h) {
    const std::string online = state.onlineNodeCount >= 0
        ? std::to_string(state.onlineNodeCount) + " OF " +
          std::to_string(state.totalNodeCount >= 0 ? state.totalNodeCount : static_cast<int32_t>(state.nodeCount)) +
          " NODES ONLINE"
        : std::to_string(state.nodeCount) + " NODES KNOWN";
    fillRect(x, y, w, 34, rgb565(67, 123, 119), 255);
    drawBitmapIcon(icons::Node, x + 12, y + 5, 24, 24, rgb565(235, 244, 244));
    drawTinyTextFit(online, x + 44, y + 10, w - 58, 1, rgb565(235, 244, 244));
    if (state.knownNodes.empty()) {
        drawTinyTextFit("WAITING FOR NODE INFO FROM THE RAK", x + 22, y + 44, w - 44, 1, rgb565(235, 244, 244));
        return;
    }
    int first = state.dashboardFocused ? std::max(0, state.selectedNodeIndex - 2) : 0;
    const int cardH = 43;
    int cy = y + 30;
    for (int slot = 0; slot < 5; ++slot) {
        const int index = first + slot;
        if (index >= static_cast<int>(state.knownNodes.size()) || cy + cardH > y + h - 10) break;
        drawNodeCard(state.knownNodes[static_cast<size_t>(index)],
                     state.dashboardFocused && index == state.selectedNodeIndex,
                     x + 8, cy, w - 16, cardH, state);
        cy += cardH + 5;
    }
}

ChannelSummary channelSlot(const AppState &state, int index, bool *configured = nullptr) {
    for (const auto &candidate : state.channels) {
        if (candidate.index == index) {
            if (configured) *configured = true;
            return candidate;
        }
    }
    if (configured) *configured = false;
    ChannelSummary ch;
    ch.index = index;
    ch.name = index == 0 ? state.channelName : std::to_string(index);
    ch.role = index == 0 ? 1 : 0;
    return ch;
}

std::string channelDisplayName(const AppState &state, int index) {
    bool configured = false;
    const ChannelSummary ch = channelSlot(state, index, &configured);
    if (!ch.name.empty() && ch.name != std::to_string(index)) return ch.name;
    if (index == 0 && !state.channelName.empty()) return state.channelName;
    return configured ? ("Channel " + std::to_string(index)) : std::to_string(index);
}

std::string nodeNameForId(const AppState &state, uint32_t id, const std::string &fallback) {
    for (const auto &node : state.knownNodes) {
        if (node.id == id || node.nodeId == nodeId(id)) {
            if (!node.name.empty()) return node.name;
            if (!node.shortName.empty()) return node.shortName;
            if (!node.nodeId.empty()) return node.nodeId;
        }
    }
    return fallback.empty() ? nodeId(id) : fallback;
}

std::vector<ChatEntry> chatEntries(const AppState &state) {
    std::vector<ChatEntry> entries;
    auto pushChannel = [&](int index) {
        ChatEntry e;
        e.channel = true;
        e.channelIndex = index;
        e.title = channelDisplayName(state, index);
        e.subtitle = "Group channel";
        for (int i = 0; i < static_cast<int>(state.messages.size()); ++i) {
            const Message &m = state.messages[static_cast<size_t>(i)];
            if (m.direct || m.channelIndex != index) continue;
            e.last = senderText(m) + ": " + messageText(m);
            e.time = m.rxTime;
            if (i >= static_cast<int>(state.seenMessageCount)) e.unread = true;
        }
        entries.push_back(e);
    };

    bool hasPrimary = false;
    for (const auto &ch : state.channels) {
        if (ch.role == 0) continue;
        pushChannel(ch.index);
        if (ch.index == 0) hasPrimary = true;
    }
    if (!hasPrimary) pushChannel(0);

    for (int i = 0; i < static_cast<int>(state.messages.size()); ++i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        if (!m.direct) continue;
        const uint32_t peer = (m.from == 0 || m.senderId == state.myNodeId) ? m.to : m.from;
        auto found = std::find_if(entries.begin(), entries.end(), [&](const ChatEntry &e) {
            return !e.channel && e.node == peer;
        });
        if (found == entries.end()) {
            ChatEntry e;
            e.channel = false;
            e.node = peer;
            e.nodeId = nodeId(peer);
            e.title = nodeNameForId(state, peer, senderText(m));
            e.subtitle = "Direct message";
            e.last = messageText(m);
            e.time = m.rxTime;
            e.unread = i >= static_cast<int>(state.seenMessageCount);
            entries.push_back(e);
        } else {
            found->last = messageText(m);
            found->time = m.rxTime;
            if (i >= static_cast<int>(state.seenMessageCount)) found->unread = true;
        }
    }

    return entries;
}

void openChatEntry(AppState &state, const ChatEntry &entry) {
    if (entry.channel) {
        state.chatChannelIndex = entry.channelIndex;
        state.chatPeerNodeId.clear();
        state.chatPeerName = entry.title;
    } else {
        state.chatChannelIndex = -1;
        state.chatPeerNodeId = entry.nodeId;
        state.chatPeerName = entry.title;
    }
    state.uiTab = ScreenChatDetail;
    state.dashboardFocused = true;
    state.scrollOffset = 0;
    state.seenMessageCount = static_cast<uint32_t>(state.messages.size());
}

void drawChannelKeyIcon(int x, int y, uint16_t c) {
    strokeCircle(x + 8, y + 8, 5, c);
    line(x + 12, y + 12, x + 25, y + 25, c);
    fillRect(x + 20, y + 20, 8, 3, c);
    fillRect(x + 24, y + 15, 3, 8, c);
}

void drawChannelLockIcon(int x, int y, uint16_t c) {
    strokeRect(x + 7, y + 13, 18, 14, c);
    fillRect(x + 10, y + 18, 12, 6, c);
    line(x + 11, y + 13, x + 11, y + 8, c);
    line(x + 21, y + 13, x + 21, y + 8, c);
    line(x + 11, y + 8, x + 16, y + 4, c);
    line(x + 21, y + 8, x + 16, y + 4, c);
}

void drawChatsPanel(const AppState &state, int x, int y, int w, int h) {
    const std::vector<ChatEntry> entries = chatEntries(state);
    const int selected = entries.empty() ? 0 : std::max(0, std::min(state.selectedChatIndex, static_cast<int>(entries.size()) - 1));
    drawSectionTitle("ACTIVE CHATS", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 30, w - 28, 28, rgb565(67, 123, 119), 255);
    drawBitmapIcon(icons::Chat, x + 22, y + 35, 18, 18, rgb565(235, 244, 244));
    drawTinyTextFit(std::to_string(static_cast<int>(entries.size())) + " ACTIVE CHATS",
                    x + 48, y + 40, w - 70, 1, rgb565(245, 248, 248));

    if (entries.empty()) {
        drawUserIconGlyph(0x1f4e5, x + w / 2 - 18, y + h / 2 - 24, 36, 36, rgb565(143, 177, 188));
        drawTinyTextFit("NO CHATS YET", x + 24, y + h / 2 + 18, w - 48, 1, rgb565(180, 205, 212));
    } else {
        const int first = std::max(0, selected - 2);
        const int rowH = 42;
        int rowY = y + 68;
        for (int slot = 0; slot < 6; ++slot) {
            const int index = first + slot;
            if (index >= static_cast<int>(entries.size()) || rowY + rowH > y + h - 34) break;
            const ChatEntry &e = entries[static_cast<size_t>(index)];
            const bool active = index == selected;
            const uint16_t edge = e.unread ? rgb565(238, 143, 41) : (active ? rgb565(77, 232, 141) : rgb565(145, 152, 152));
            fillRect(x + 18, rowY, w - 36, rowH, active ? rgb565(48, 70, 72) : rgb565(54, 54, 54), 245);
            strokeRect(x + 18, rowY, w - 36, rowH, edge);
            if (e.channel) {
                drawBitmapIcon(icons::Channels, x + 28, rowY + 8, 24, 24, e.unread ? rgb565(238, 143, 41) : rgb565(77, 232, 141));
            } else {
                NodeSummary pseudo;
                pseudo.id = e.node;
                pseudo.nodeId = e.nodeId;
                pseudo.name = e.title;
                drawNodeBadgeLabel(nodeBadgeLabel(pseudo), x + 26, rowY + 7, 26, 26, nodeBadgeAccent(pseudo), rgb565(7, 18, 21));
            }
            drawTinyTextFit((e.unread ? "* " : "") + e.title, x + 62, rowY + 7, w - 134, 1, rgb565(245, 248, 248));
            drawTinyTextFit(e.last.empty() ? e.subtitle : e.last, x + 62, rowY + 22, w - 134, 1, rgb565(180, 205, 212));
            drawTinyTextFit(e.time ? timeText(e.time) : "", x + w - 72, rowY + 14, 50, 1, rgb565(180, 205, 212));
            rowY += rowH + 6;
        }
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN CHAT  A OPEN  B MENU  ORANGE = UNREAD", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawChannelsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("GROUP CHANNELS", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 30, w - 28, 28, rgb565(67, 123, 119), 255);
    drawBitmapIcon(icons::Channels, x + 22, y + 35, 18, 18, rgb565(235, 244, 244));
    drawTinyTextFit(std::to_string(static_cast<int>(state.channels.size())) + " CONFIGURED CHANNELS",
                    x + 48, y + 40, w - 70, 1, rgb565(245, 248, 248));

    const int first = std::max(0, state.selectedChannelIndex - 2);
    const int rowH = 35;
    int rowY = y + 68;
    for (int slot = 0; slot < 7; ++slot) {
        const int index = first + slot;
        if (index >= 8 || rowY + rowH > y + h - 34) break;
        bool configured = false;
        const ChannelSummary ch = channelSlot(state, index, &configured);
        const bool enabled = configured && ch.role != 0;
        const bool active = index == state.selectedChannelIndex;
        const uint16_t bg = active ? rgb565(52, 73, 73) : rgb565(54, 54, 54);
        const uint16_t edge = active ? rgb565(77, 232, 141) : rgb565(145, 152, 152);
        fillRect(x + 18, rowY, w - 36, rowH, bg, 245);
        strokeRect(x + 18, rowY, w - 36, rowH, edge);

        const int iconX = x + 30;
        const int iconY = rowY + 4;
        const uint16_t iconColor = !enabled || !ch.hasPsk ? rgb565(224, 58, 58)
            : (ch.pskBytes > 1 ? rgb565(77, 232, 141) : rgb565(238, 214, 69));
        if (enabled && ch.hasPsk && ch.pskBytes <= 1) {
            drawChannelKeyIcon(iconX, iconY, iconColor);
        } else {
            drawChannelLockIcon(iconX, iconY, iconColor);
        }

        const std::string name = enabled ? channelDisplayName(state, index) : std::to_string(index);
        drawTinyTextFit((ch.role == 1 ? "*" : "") + name, x + 64, rowY + 7, w - 150, 1, rgb565(245, 248, 248));
        std::string detail = enabled ? (ch.hasPsk ? "encrypted" : "open") : "empty";
        if (ch.muted) detail += " muted";
        drawTinyTextFit(detail, x + 64, rowY + 21, w - 150, 1, enabled ? rgb565(180, 205, 212) : rgb565(145, 152, 152));
        drawTinyTextFit(enabled ? "A CHAT" : "disabled", x + w - 82, rowY + 14, 64, 1,
                        enabled ? rgb565(77, 232, 141) : rgb565(145, 152, 152));
        rowY += rowH + 6;
    }

    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN CHANNEL  A OPEN CHAT  B MENU", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawMapPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("MAP", x + 16, y + 14, w - 32);
    bool hasGeo = false;
    int32_t minLat = 0, maxLat = 0, minLon = 0, maxLon = 0;
    for (const auto &n : state.knownNodes) {
        if (!n.hasPosition) continue;
        if (!hasGeo) {
            minLat = maxLat = n.latitudeI;
            minLon = maxLon = n.longitudeI;
            hasGeo = true;
        } else {
            minLat = std::min(minLat, n.latitudeI);
            maxLat = std::max(maxLat, n.latitudeI);
            minLon = std::min(minLon, n.longitudeI);
            maxLon = std::max(maxLon, n.longitudeI);
        }
    }
    const int listW = std::min(154, std::max(112, w / 3));
    const int mapX = x + listW + 18;
    const int mapY = y + 31;
    const int mapW = w - listW - 30;
    const int mapH = h - 62;
    fillRect(x + 14, y + 31, listW, mapH, rgb565(17, 31, 39), 235);
    strokeRect(x + 14, y + 31, listW, mapH, rgb565(35, 92, 116));
    drawTinyTextFit("NODES", x + 24, y + 43, listW - 20, 1, rgb565(77, 232, 141));
    const std::string online = state.onlineNodeCount >= 0
        ? std::to_string(state.onlineNodeCount) + "/" +
          std::to_string(state.totalNodeCount >= 0 ? state.totalNodeCount : static_cast<int32_t>(state.nodeCount)) + " ONLINE"
        : std::to_string(state.nodeCount) + " KNOWN";
    drawTinyTextFit(online, x + 24, y + 58, listW - 20, 1, rgb565(180, 205, 212));

    fillRect(mapX, mapY, mapW, mapH, rgb565(6, 24, 36), 245);
    strokeRect(mapX, mapY, mapW, mapH, rgb565(35, 92, 116));
    for (int gx = mapX + 36; gx < mapX + mapW; gx += 48) {
        line(gx, mapY + 4, gx, mapY + mapH - 4, rgb565(10, 45, 58));
    }
    for (int gy = mapY + 32; gy < mapY + mapH; gy += 38) {
        line(mapX + 4, gy, mapX + mapW - 4, gy, rgb565(10, 45, 58));
    }
    drawTinyTextFit(hasGeo ? "GPS MESH MAP" : "RELATIVE MESH VIEW",
                    mapX + 12, mapY + 10, mapW - 24, 1, rgb565(143, 177, 188));

    if (state.knownNodes.empty()) {
        drawTinyTextFit("WAITING FOR POSITION OR NODE INFO", mapX + 20, mapY + 72, mapW - 40, 1, rgb565(235, 244, 244));
        drawTinyTextFit("NODE LIST WILL FILL THIS MAP", mapX + 20, mapY + 91, mapW - 40, 1, rgb565(170, 197, 204));
        return;
    }

    const int selected = std::max(0, std::min(state.selectedNodeIndex, static_cast<int>(state.knownNodes.size()) - 1));
    const int listFirst = std::max(0, selected - 3);
    int rowY = y + 78;
    for (int slot = 0; slot < 6; ++slot) {
        const int index = listFirst + slot;
        if (index >= static_cast<int>(state.knownNodes.size()) || rowY + 29 > y + h - 38) break;
        const NodeSummary &n = state.knownNodes[static_cast<size_t>(index)];
        const bool active = index == selected && state.dashboardFocused;
        fillRect(x + 22, rowY, listW - 16, 27, active ? rgb565(16, 68, 93) : rgb565(32, 42, 46), 235);
        strokeRect(x + 22, rowY, listW - 16, 27, active ? rgb565(77, 232, 141) : rgb565(69, 81, 84));
        drawTinyTextFit(n.name.empty() ? n.nodeId : n.name, x + 30, rowY + 6, listW - 32, 1, rgb565(245, 248, 248));
        drawTinyTextFit(std::string(n.hasPosition ? "GPS " : "NO GPS ") + nodeConnectionText(n),
                        x + 30, rowY + 18, listW - 32, 1, rgb565(180, 205, 212));
        rowY += 31;
    }

    const int cx = mapX + mapW / 2;
    const int cy = mapY + mapH / 2 + 8;
    int myIndex = -1;
    for (int i = 0; i < static_cast<int>(state.knownNodes.size()); ++i) {
        if (state.knownNodes[static_cast<size_t>(i)].isMine ||
            state.knownNodes[static_cast<size_t>(i)].nodeId == state.myNodeId) {
            myIndex = i;
            break;
        }
    }
    if (myIndex < 0) myIndex = selected;
    const int maxNodes = std::min(12, static_cast<int>(state.knownNodes.size()));
    if (maxNodes <= 0) return;
    if (myIndex < 0 || myIndex >= maxNodes) myIndex = 0;
    int pxs[12] = {};
    int pys[12] = {};
    if (hasGeo) {
        if (maxLat == minLat) {
            maxLat += 1000;
            minLat -= 1000;
        }
        if (maxLon == minLon) {
            maxLon += 1000;
            minLon -= 1000;
        }
        const int64_t latRange = static_cast<int64_t>(maxLat) - static_cast<int64_t>(minLat);
        const int64_t lonRange = static_cast<int64_t>(maxLon) - static_cast<int64_t>(minLon);
        const int plotW = std::max(1, mapW - 82);
        const int plotH = std::max(1, mapH - 88);
        for (int i = 0; i < maxNodes; ++i) {
            const NodeSummary &n = state.knownNodes[static_cast<size_t>(i)];
            if (n.hasPosition) {
                const int64_t lonOff = static_cast<int64_t>(n.longitudeI) - static_cast<int64_t>(minLon);
                const int64_t latOff = static_cast<int64_t>(maxLat) - static_cast<int64_t>(n.latitudeI);
                pxs[i] = mapX + 34 + static_cast<int>(lonOff * plotW / lonRange);
                pys[i] = mapY + 44 + static_cast<int>(latOff * plotH / latRange);
            } else {
                const uint32_t id = n.id;
                pxs[i] = mapX + mapW - 42;
                pys[i] = mapY + 44 + static_cast<int>((id >> 8) % std::max(20, mapH - 90));
            }
            if (i == myIndex && !n.hasPosition) {
                pxs[i] = cx;
                pys[i] = cy;
            }
            pxs[i] = std::max(mapX + 28, std::min(mapX + mapW - 48, pxs[i]));
            pys[i] = std::max(mapY + 38, std::min(mapY + mapH - 42, pys[i]));
        }
    } else {
    for (int i = 0; i < maxNodes; ++i) {
        const uint32_t id = state.knownNodes[static_cast<size_t>(i)].id;
        const int ring = 42 + static_cast<int>((id >> 5) % std::max(24, mapH / 3));
        const int sx = static_cast<int>((id & 255u) % std::max(20, mapW - 70));
        const int sy = static_cast<int>(((id >> 8) & 255u) % std::max(20, mapH - 70));
        pxs[i] = mapX + 34 + sx;
        pys[i] = mapY + 42 + sy;
        if (i == myIndex) {
            pxs[i] = cx;
            pys[i] = cy;
        } else if (std::abs(pxs[i] - cx) < 34 && std::abs(pys[i] - cy) < 34) {
            pxs[i] += (pxs[i] < cx ? -ring / 2 : ring / 2);
            pys[i] += (pys[i] < cy ? -ring / 3 : ring / 3);
        }
        pxs[i] = std::max(mapX + 28, std::min(mapX + mapW - 48, pxs[i]));
        pys[i] = std::max(mapY + 38, std::min(mapY + mapH - 42, pys[i]));
    }
    }
    for (int i = 0; i < maxNodes; ++i) {
        if (i == myIndex) continue;
        thickLine(pxs[myIndex] + 13, pys[myIndex] + 13, pxs[i] + 13, pys[i] + 13, rgb565(51, 215, 238), i == selected ? 3 : 2);
    }
    for (int i = 0; i < maxNodes; ++i) {
        const NodeSummary &n = state.knownNodes[static_cast<size_t>(i)];
        const bool active = i == selected;
        const int size = active ? 34 : 26;
        const int nx = pxs[i] - (active ? 4 : 0);
        const int ny = pys[i] - (active ? 4 : 0);
        fillRect(nx - 3, ny - 3, size + 6, size + 6, rgb565(3, 13, 18), 230);
        const std::vector<uint32_t> icons = nodeIconCodepoints(n, 2);
        if (icons.empty()) {
            drawNodeBadgeLabel(nodeBadgeLabel(n), nx, ny, size, size, nodeBadgeAccent(n), rgb565(7, 18, 21));
        } else {
            drawAvatarFrame(nx, ny, size, size, nodeBadgeAccent(n), rgb565(235, 238, 238));
            drawUserIconGlyphs(icons, nx, ny, size, size, rgb565(248, 250, 250));
        }
        if (active) strokeRect(nx - 5, ny - 5, size + 10, size + 10, rgb565(77, 232, 141));
        drawTinyTextFit(n.shortName.empty() ? nodeBadgeLabel(n) : n.shortName,
                        nx - 8, ny + size + 5, 54, 1, active ? rgb565(245, 248, 248) : rgb565(180, 205, 212));
    }
    const NodeSummary &sel = state.knownNodes[static_cast<size_t>(selected)];
    fillRect(mapX + 10, mapY + mapH - 40, mapW - 20, 28, rgb565(10, 30, 42), 235);
    strokeRect(mapX + 10, mapY + mapH - 40, mapW - 20, 28, rgb565(35, 99, 118));
    std::string selectedLine = (sel.name.empty() ? sel.nodeId : sel.name) + "  " + nodeConnectionText(sel);
    if (sel.hasPosition) {
        char pos[96];
        std::snprintf(pos, sizeof(pos), "  %.5f %.5f %ldM",
                      sel.latitudeI / 10000000.0, sel.longitudeI / 10000000.0,
                      static_cast<long>(sel.altitude));
        selectedLine += pos;
    } else {
        selectedLine += "  NO GPS";
    }
    drawTinyTextFit(selectedLine,
                    mapX + 20, mapY + mapH - 31, mapW - 40, 1, rgb565(245, 248, 248));
    drawTinyTextFit(hasGeo ? "A CHAT  UP/DOWN SELECT NODE  MAP DATA READY" : "A CHAT  UP/DOWN SELECT NODE  WAITING GPS",
                    x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawSettingsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("SETTINGS", x + 16, y + 14, w - 32);
    struct Row {
        const char *label;
        std::string value;
        std::string help;
    };
    const Row rows[] = {
        {"LOG", "PACKET STREAM", "A opens live packet log"},
        {"DEBUG", "DECODED PACKET CARDS", "A opens packet detail"},
        {"STATUS", "USB NETWORK COUNTERS", "A opens device status"},
        {"SCREENSAVER", "OPEN", "Mode, speed, and test screen"},
        {"FONT", "OPEN", "Style and size testing"},
        {"NETWORK", state.networkReady ? ("WII " + state.wiiIp) : "OFF", "+ enables IP/UDP"},
    };
    const int count = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
    int cy = y + 46;
    for (int i = 0; i < count && cy + 34 < y + h - 28; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedSettingsIndex;
        fillRect(x + 18, cy, w - 36, 31, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 31, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 8, 108, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 140, cy + 8, w - 166, 1, i >= 3 ? rgb565(77, 232, 141) : rgb565(235, 244, 244));
        drawTinyTextFit(rows[i].help, x + 140, cy + 21, w - 166, 1, rgb565(180, 205, 212));
        cy += 36;
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN SETTING  A OPEN  B MENU", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawScreensaverSettingsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("SCREENSAVER", x + 16, y + 14, w - 32);
    struct Row {
        const char *label;
        std::string value;
        std::string help;
    };
    const int speed = std::max(0, std::min(state.screensaverSpeed, 5));
    std::string speedBar;
    for (int i = 0; i < 6; ++i) speedBar += i <= speed ? "|" : ".";
    const Row rows[] = {
        {"DISPLAY", screensaverModeText(state.screensaverMode), "Username, unread, or active users"},
        {"SPEED", speedBar + " " + std::to_string(speed + 1) + "/6", "Left/Right changes movement speed"},
        {"TEST NOW", "START", "A starts the screensaver immediately"},
    };
    int cy = y + 48;
    for (int i = 0; i < 3; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedScreensaverIndex;
        fillRect(x + 18, cy, w - 36, 38, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 38, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 9, 112, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 146, cy + 9, w - 174, 1, rgb565(77, 232, 141));
        drawTinyTextFit(rows[i].help, x + 146, cy + 24, w - 174, 1, rgb565(180, 205, 212));
        cy += 44;
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("LEFT/RIGHT ADJUST  A TEST/CYCLE  B SETTINGS", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawFontSettingsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("FONT", x + 16, y + 14, w - 32);
    struct Row {
        const char *label;
        std::string value;
        std::string help;
    };
    const Row rows[] = {
        {"STYLE", fontStyleText(state.fontStyle), "Pixel, TV shadow, soft, or bold"},
        {"SIZE", fontSizeText(state.fontSize), "Left/right slider; 3 is default"},
    };
    int cy = y + 48;
    for (int i = 0; i < 2; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedFontIndex;
        fillRect(x + 18, cy, w - 36, 40, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 40, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 10, 100, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 136, cy + 10, w - 164, 1, rgb565(77, 232, 141));
        drawTinyTextFit(rows[i].help, x + 136, cy + 25, w - 164, 1, rgb565(180, 205, 212));
        cy += 48;
    }
    fillRect(x + 18, cy + 8, w - 36, 66, rgb565(18, 39, 47), 230);
    strokeRect(x + 18, cy + 8, w - 36, 66, rgb565(35, 99, 118));
    drawTinyTextFit("SAMPLE TEXT  STYLE " + fontStyleText(state.fontStyle) +
                    " SIZE " + std::to_string(std::max(0, std::min(state.fontSize, FontSizeMax))),
                    x + 30, cy + 22, w - 60, 1, rgb565(77, 232, 141));
    drawTinyTextFit("WiiMesh LongFast MacnCheeseNoodles 012345", x + 30, cy + 42, w - 60, 1, rgb565(245, 248, 248));
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("LEFT/RIGHT ADJUST  A CYCLE  B SETTINGS", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawNodeOptionsPanel(const AppState &state, int x, int y, int w, int h) {
    (void)h;
    drawSectionTitle("NODE OPTIONS", x + 16, y + 14, w - 32);
    const int tabW = (w - 32) / 2;
    const int tabY = y + 28;
    fillRect(x + 16, tabY, tabW, 24, state.nodeOptionTab == 0 ? rgb565(67, 123, 119) : rgb565(42, 47, 49), 245);
    fillRect(x + 16 + tabW, tabY, tabW, 24, state.nodeOptionTab == 1 ? rgb565(67, 123, 119) : rgb565(42, 47, 49), 245);
    strokeRect(x + 16, tabY, tabW, 24, rgb565(118, 125, 126));
    strokeRect(x + 16 + tabW, tabY, tabW, 24, rgb565(118, 125, 126));
    drawTinyTextFit("FILTER", x + 16 + tabW / 2 - 18, tabY + 8, tabW - 10, 1, rgb565(235, 244, 244));
    drawTinyTextFit("HIGHLIGHT", x + 16 + tabW + tabW / 2 - 27, tabY + 8, tabW - 10, 1, rgb565(235, 244, 244));
    const char *filterRows[] = {"UNKNOWN        OFF", "OFFLINE        OFF", "PUBLIC KEY     OFF",
                                "CHANNEL        ALL", "HOPS AWAY      <=7", "POSITION       OFF",
                                "NAME           ENTER FILTER"};
    const char *highlightRows[] = {"DIRECT CHATS   ON", "THIS DEVICE    ON", "CLOSE SIGNAL   OFF",
                                   "NO POSITION    DIM", "CHANNEL MATCH  ON", "RECENT NODES   ON"};
    int cy = y + 62;
    const int count = state.nodeOptionTab == 0 ? 7 : 6;
    for (int i = 0; i < count; ++i) {
        fillRect(x + 18, cy, w - 36, 22, rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 22, rgb565(84, 91, 92));
        drawTinyTextFit(state.nodeOptionTab == 0 ? filterRows[i] : highlightRows[i],
                        x + 28, cy + 8, w - 56, 1, i < 2 ? rgb565(122, 239, 206) : rgb565(235, 244, 244));
        cy += 25;
    }
    drawTinyTextFit("LEFT/RIGHT SWITCH TABS  B NODE LIST", x + 22, y + h - 24, w - 44, 1, rgb565(77, 232, 141));
}

void drawChatPanel(const AppState &state, int x, int y, int w, int h) {
    const int keyboardH = state.composingMessage ? std::min(126, std::max(104, h / 2)) : 0;
    const int chatBottom = y + h - keyboardH - 8;
    const bool channelChat = state.chatChannelIndex >= 0;
    drawSectionTitle(channelChat ? "GROUP CHAT" : "DIRECT CHAT", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 28, w - 28, 27, rgb565(67, 123, 119), 245);
    if (channelChat) {
        drawBitmapIcon(icons::Channels, x + 24, y + 32, 20, 20, rgb565(245, 248, 248));
    }
    drawTinyTextFit(state.chatPeerName.empty() ? "NO CONTACT" : state.chatPeerName,
                    x + (channelChat ? 52 : 24), y + 37, w - 160, 1, rgb565(245, 248, 248));
    drawTinyTextFit(channelChat ? ("CH " + std::to_string(state.chatChannelIndex)) : state.chatPeerNodeId,
                    x + w - 110, y + 37, 90, 1, rgb565(180, 205, 212));
    int cy = state.composingMessage ? y + 60 : y + 64;
    int shown = 0;
    const int bubbleH = state.composingMessage ? 34 : 58;
    for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0 && cy + bubbleH < chatBottom; --i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        const std::string sid = m.senderId.empty() ? nodeId(m.from) : m.senderId;
        if (channelChat) {
            if (m.direct || m.channelIndex != state.chatChannelIndex) continue;
        } else if (!state.chatPeerNodeId.empty() && sid != state.chatPeerNodeId && m.to != state.pendingSendTo) {
            continue;
        }
        const bool mine = m.from == 0 || m.senderId == state.myNodeId;
        const int bw = std::min(w - 56, 278);
        const int bx = mine ? x + w - bw - 24 : x + 24;
        fillRect(bx, cy, bw, bubbleH, mine ? rgb565(24, 80, 104) : rgb565(30, 48, 57), 245);
        strokeRect(bx, cy, bw, bubbleH, mine ? rgb565(51, 215, 238) : rgb565(88, 101, 105));
        drawTinyTextFit(timeText(m.rxTime) + " " + senderText(m), bx + 10, cy + 8, bw - 20, 1, rgb565(180, 205, 212));
        drawWrappedTinyText(messageText(m), bx + 10, cy + (state.composingMessage ? 22 : 25), bw - 20,
                            1, state.composingMessage ? 1 : 2, 13, rgb565(245, 248, 248));
        cy += bubbleH + 8;
        if (++shown >= (state.composingMessage ? 1 : 4)) break;
    }
    if (shown == 0) {
        drawTinyTextFit(channelChat ? "NO MESSAGES IN THIS CHANNEL YET" : "READY TO MESSAGE THIS NODE",
                        x + 24, y + 76, w - 48, 1, rgb565(235, 244, 244));
        drawTinyTextFit(channelChat ? "CHANNEL SEND COMES AFTER RECEIVE UI IS SOLID" : "PRESS A TO OPEN THE KEYBOARD",
                        x + 24, y + 96, w - 48, 1, rgb565(170, 197, 204));
    }
    if (state.composingMessage) {
        const std::vector<std::string> keys = keyboardKeys();
        const int ky = y + h - keyboardH - 8;
        fillRect(x + 12, ky, w - 24, keyboardH, rgb565(8, 18, 26), 250);
        strokeRect(x + 12, ky, w - 24, keyboardH, rgb565(51, 215, 238));
        fillRect(x + 22, ky + 10, w - 44, 23, rgb565(21, 31, 41), 255);
        strokeRect(x + 22, ky + 10, w - 44, 23, rgb565(77, 232, 141));
        drawTinyTextFit(state.composeText.empty() ? "TYPE A MESSAGE..." : state.composeText,
                        x + 30, ky + 18, w - 86, 1, state.composeText.empty() ? rgb565(143, 177, 188) : rgb565(245, 248, 248));
        drawTinyTextFit(std::to_string(state.composeText.size()) + "/200", x + w - 62, ky + 18, 40, 1, rgb565(180, 205, 212));
        const int cols = KeyboardCols;
        const int keyW = std::max(32, (w - 44) / cols - 4);
        const int keyH = 13;
        int index = 0;
        int keyY = ky + 42;
        while (index < static_cast<int>(keys.size()) && keyY + keyH < y + h - 8) {
            for (int col = 0; col < cols && index < static_cast<int>(keys.size()); ++col, ++index) {
                const int keyX = x + 22 + col * (keyW + 4);
                const bool selected = index == state.keyboardCursor;
                const bool send = keys[static_cast<size_t>(index)] == "SEND";
                const bool back = keys[static_cast<size_t>(index)] == "BACK";
                fillRect(keyX, keyY, keyW, keyH,
                         selected ? rgb565(17, 103, 145) : (send ? rgb565(18, 91, 63) : rgb565(35, 45, 51)), 245);
                strokeRect(keyX, keyY, keyW, keyH,
                           selected ? rgb565(77, 232, 141) : (send ? rgb565(77, 232, 141) : rgb565(85, 101, 107)));
                uint16_t ink = selected ? rgb565(255, 255, 255) : rgb565(215, 226, 229);
                if (back) ink = rgb565(238, 191, 75);
                drawTinyTextFit(keys[static_cast<size_t>(index)], keyX + 4, keyY + 6, keyW - 8, 1, ink);
            }
            keyY += keyH + 3;
        }
    } else {
        fillRect(x + 18, y + h - 38, w - 36, 26, rgb565(12, 31, 41), 245);
        strokeRect(x + 18, y + h - 38, w - 36, 26, rgb565(35, 99, 118));
        drawTinyTextFit(channelChat ? "B BACK TO CHATS   CHANNEL VIEW" : "A TYPE DIRECT MESSAGE   B BACK",
                        x + 28, y + h - 29, w - 56, 1, rgb565(77, 232, 141));
    }
}

void drawGraphicalContent(const AppState &state, int x, int y, int w, int h) {
    panel(x, y, w, h, state.dashboardFocused);
    if (state.dashboardFocused) {
        strokeRect(x + 4, y + 4, std::max(4, w - 8), std::max(4, h - 8), rgb565(77, 232, 141));
    }
    switch (state.uiTab) {
    case ScreenHome: drawHomePanel(state, x, y, w, h); break;
    case ScreenNodeOptions: drawNodesPanel(state, x, y, w, h); break;
    case ScreenNodeTools: drawNodeOptionsPanel(state, x, y, w, h); break;
    case ScreenChannels: drawChannelsPanel(state, x, y, w, h); break;
    case ScreenChats: drawChatsPanel(state, x, y, w, h); break;
    case ScreenMap: drawMapPanel(state, x, y, w, h); break;
    case ScreenSettings: drawSettingsPanel(state, x, y, w, h); break;
    case ScreenScreensaverSettings: drawScreensaverSettingsPanel(state, x, y, w, h); break;
    case ScreenFontSettings: drawFontSettingsPanel(state, x, y, w, h); break;
    case ScreenChatDetail: drawChatPanel(state, x, y, w, h); break;
    default: drawHomePanel(state, x, y, w, h); break;
    }
}

void drawGraphicalFooter(const AppState &state, int x, int y, int w, int h) {
    panel(x, y, w, h, false);
    const std::string help = state.composingMessage
        ? "KEYBOARD  D-PAD MOVE  A KEY/SEND  B CANCEL"
        : state.dashboardFocused
        ? "FOCUSED  UP/DOWN SCROLL  A OPEN  B MENU"
        : "UP/DOWN MENU  A FOCUS  1 USB  + IP  - GRID";
    drawTinyTextFit(help, x + 14, y + 14, w - 28, 1, rgb565(235, 244, 244));
}

bool shouldUseGraphicsOnly(const AppState &state) {
    if (!gGraphicsReady || state.showCalibration || state.showDiagnostics) return false;
    return state.uiTab == ScreenHome || state.uiTab == ScreenNodeOptions ||
           state.uiTab == ScreenChannels || state.uiTab == ScreenChats ||
           state.uiTab == ScreenMap || state.uiTab == ScreenSettings ||
           state.uiTab == ScreenChatDetail || state.uiTab == ScreenNodeTools ||
           state.uiTab == ScreenScreensaverSettings || state.uiTab == ScreenFontSettings;
}
#endif

void drawBackgroundSkin(const AppState &state) {
    if (!gGraphicsReady || gSkinPixels.empty()) return;
    gUiFontStyle = ((state.fontStyle % FontStyleCount) + FontStyleCount) % FontStyleCount;
    gUiFontSize = std::max(0, std::min(state.fontSize, FontSizeMax));
    if (state.showCalibration) {
        const uint16_t dark = rgb565(8, 22, 34);
        const uint16_t light = rgb565(20, 58, 76);
        const uint16_t cyan = rgb565(51, 215, 238);
        const uint16_t yellow = rgb565(238, 191, 75);
        const uint16_t green = rgb565(77, 232, 141);
        for (int y = 0; y < SkinHeight; y += 32) {
            for (int x = 0; x < SkinWidth; x += 32) {
                fillRect(x, y, 32, 32, (((x / 32) + (y / 32)) & 1) ? light : dark);
            }
        }
        strokeRect(0, 0, SkinWidth, SkinHeight, yellow);
        strokeRect(16, 16, SkinWidth - 32, SkinHeight - 32, green);
        strokeRect(ConsoleLeftPx, ConsoleTopPx, 576, 432, cyan);
        for (int x = 0; x < SkinWidth; x += 64) {
            fillRect(x, 0, 2, SkinHeight, cyan, 190);
        }
        for (int y = 0; y < SkinHeight; y += 64) {
            fillRect(0, y, SkinWidth, 2, cyan, 190);
        }
        auto drawCalZone = [&](int index, int x, int y, const GuiZone &z, uint16_t fill) {
            const bool selected = index == gSelectedGuiZone;
            fillRect(x + z.x, y + z.y, z.w, z.h, fill, selected ? 230 : 150);
            strokeRect(x + z.x, y + z.y, z.w, z.h, selected ? rgb565(255, 255, 255) : cyan);
            if (selected) {
                strokeRect(x + z.x + 3, y + z.y + 3, std::max(4, z.w - 6), std::max(4, z.h - 6), yellow);
                fillRect(x + z.x - 4, y + z.y - 4, 10, 10, yellow);
                fillRect(x + z.x + z.w - 6, y + z.y - 4, 10, 10, yellow);
                fillRect(x + z.x - 4, y + z.y + z.h - 6, 10, 10, yellow);
                fillRect(x + z.x + z.w - 6, y + z.y + z.h - 6, 10, 10, yellow);
            }
        };
        drawCalZone(0, pxCol(2) - 6, pxRow(1) - 6, zone("header"), rgb565(29, 75, 99));
        drawCalZone(1, pxCol(62) - 8, pxRow(1) - 6, zone("menu"), rgb565(70, 48, 110));
        drawCalZone(2, pxCol(2) - 6, pxRow(9) - 4, zone("content"), rgb565(18, 88, 70));
        drawCalZone(3, pxCol(2) - 6, pxRow(27) - 6, zone("footer"), rgb565(111, 74, 23));
        drawCalZone(4, pxCol(2), pxRow(2), zone("bell"), rgb565(238, 191, 75));
        gfx_tex_convert(&gSkinTexture, gSkinPixels.data());
        gfx_tex_flush_texture(&gSkinTexture);
        gfx_screen_coords_t coords = {0.0f, 0.0f, 640.0f, 480.0f};
        gfx_draw_tex(&gSkinTexture, &coords);
        return;
    }
    const uint16_t bgTop = rgb565(5, 18, 29);
    const uint16_t bgBot = rgb565(2, 7, 13);
    for (int y = 0; y < SkinHeight; ++y) {
        const int a = y * 255 / SkinHeight;
        const uint16_t c = blend565(bgTop, bgBot, a);
        for (int x = 0; x < SkinWidth; ++x) {
            gSkinPixels[static_cast<size_t>(y) * SkinWidth + x] = c;
        }
    }

    fillRect(0, 0, SkinWidth, SkinHeight, rgb565(0, 12, 20), 90);
    const GuiZone &header = zone("header");
    const GuiZone &menu = zone("menu");
    const GuiZone &content = zone("content");
    const GuiZone &footer = zone("footer");
    const int headerX = pxCol(2) - 6 + header.x;
    const int headerY = pxRow(1) - 6 + header.y;
    const int contentX = pxCol(2) - 6 + content.x;
    const int contentY = pxRow(9) - 4 + content.y;
    const int footerX = pxCol(2) - 6 + footer.x;
    const int footerY = pxRow(27) - 6 + footer.y;
    const int navX = pxCol(62) - 8 + menu.x;
    const int navY = pxRow(1) - 6 + menu.y;

    drawGraphicalHeader(state, headerX, headerY, header.w, header.h);

    drawGraphicalMenu(state, navX, navY, menu.w, menu.h);

    drawGraphicalContent(state, contentX, contentY, content.w, content.h);
    drawGraphicalFooter(state, footerX, footerY, footer.w, footer.h);
    if (state.messages.size() > state.seenMessageCount) {
        const GuiZone &bell = zone("bell");
        drawBellGlyph(pxCol(2) + bell.x, pxRow(2) + bell.y, bell.w, bell.h,
                      rgb565(238, 191, 75), rgb565(5, 18, 29));
    }
    drawPointerCursor(state);

    gfx_tex_convert(&gSkinTexture, gSkinPixels.data());
    gfx_tex_flush_texture(&gSkinTexture);
    gfx_screen_coords_t coords = {0.0f, 0.0f, 640.0f, 480.0f};
    gfx_draw_tex(&gSkinTexture, &coords);
}
#endif

void drawHeader(const AppState &state) {
    const std::string status = state.protocolReady ? "ONLINE" : (state.usbConnected ? "SYNCING" : "OFFLINE");
    std::printf("\x1b[2J");
    const GuiZone &header = zone("header");
    const int row = zoneRow("header", 2);
    const int col = zoneCol("header", 3);
    printAtW(row, col, header.textWidth, "WiiMesh v" + std::string(AppVersion) + "  " + status +
             "  " + (state.networkReady ? "IP/UDP ON" : "IP/UDP OFF"));
    printAtW(row + 1, col, header.textWidth, "USB: " + state.usbStatus);
    printAtW(row + 2, col, header.textWidth, "Me: " + state.myNodeId + "  " + state.nodeName);
    printAtW(row + 3, col, header.textWidth, "Ch: " + state.channelName + "  Nodes " + std::to_string(state.nodeCount) +
                  "  Text " + std::to_string(state.textMessageCount));
    printAtW(row + 4, col, header.textWidth, "RX " + std::to_string(state.rxBytes) + " bytes/" +
                  std::to_string(state.rxFrames) + " frames bad " +
                  std::to_string(state.rxBadFrames) + "  TX " + std::to_string(state.txBytes));
    printAtW(row + 5, col, header.textWidth, tickerText(state));
}

void drawTabs(const AppState &state) {
    (void)state;
}

void drawFooter(const AppState &state) {
    printRule(27);
    const int row = zoneRow("footer", 28);
    const int col = zoneCol("footer", 3);
    const GuiZone &footer = zone("footer");
    const int width = std::min(footer.textWidth, std::max(8, footer.w / CharW - 4));
    if (state.composingMessage) {
        printAtW(row, col, width, "Typing: D-pad keys  A choose  B close  SEND queues");
    } else if (state.dashboardFocused) {
        printAtW(row, col, width, "Focused: Up/Down scroll  A open  B menu  2 options");
    } else if (state.uiTab >= PrimaryTabCount) {
        printAtW(row, col, width, "A action  B back  - grid  1 USB  + IP  HOME");
    } else {
        printAtW(row, col, width, "Up/Down menu  A focus  - grid  1 USB  + IP");
    }
}

void drawKeyboard(const AppState &state) {
    const std::vector<std::string> keys = keyboardKeys();
    printRule(18);
    printAt(19, 3, "To: " + state.chatPeerName + " " + state.chatPeerNodeId);
    printAt(20, 3, "Text: " + trimTo(state.composeText, 64));
    printAt(21, 3, "Cursor: " + keys[static_cast<size_t>(state.keyboardCursor)]);
    int row = 22;
    int index = 0;
    while (index < static_cast<int>(keys.size()) && row <= 26) {
        std::string line;
        for (int col = 0; col < KeyboardCols && index < static_cast<int>(keys.size()); ++col, ++index) {
            std::string label = keys[static_cast<size_t>(index)];
            if (index == state.keyboardCursor) label = ">" + label;
            else label = " " + label;
            while (label.size() < 12) label += ' ';
            line += label;
        }
        printContent(row++, 3, line);
    }
}

void drawHome(const AppState &state) {
    const uint32_t unread = state.textMessageCount > state.seenMessageCount
        ? state.textMessageCount - state.seenMessageCount
        : 0;
    const std::string nodeTotal = state.totalNodeCount >= 0
        ? std::to_string(state.totalNodeCount)
        : std::to_string(state.nodeCount);
    const std::string online = state.onlineNodeCount >= 0
        ? std::to_string(state.onlineNodeCount) + " of " + nodeTotal + " nodes online"
        : nodeTotal + " nodes known";
    std::vector<std::string> rows;
    rows.push_back("HOME");
    rows.push_back("Messages   " + std::to_string(unread) + " new   " +
                   std::to_string(state.messages.size()) + "/" + std::to_string(MaxMessages) + " saved");
    rows.push_back("Nodes      " + online);
    rows.push_back("Channel    " + state.channelName + "   " +
                   (state.protocolReady ? "online" : (state.usbConnected ? "syncing" : "waiting")));
    rows.push_back("Device     " + screensaverName(state));
    rows.push_back("Time       " + nowTimeText() + "   " + nowDateText());
    rows.push_back("Radio      RX " + std::to_string(state.rxFrames) + " bad " +
                   std::to_string(state.rxBadFrames) + " TX " + std::to_string(state.txBytes));
    rows.push_back("Signal     SNR " + fixed1(state.lastRxSnr, "dB") +
                   " RSSI " + (state.lastRxRssi == 0 ? std::string("--") :
                   std::to_string(state.lastRxRssi)));
    rows.push_back("Power      " + integerOrWaiting(state.batteryLevel, "%") +
                   "   " + fixed2(state.voltage, "V"));
    rows.push_back("Air        Ch " + fixed1(state.channelUtilization, "%") +
                   " Tx " + fixed1(state.airUtilTx, "%"));
    rows.push_back("Network    " + std::string(state.networkReady ? "IP/UDP on" : "off"));
    rows.push_back("Wii IP     " + state.wiiIp);
    rows.push_back("M Device IP " + state.meshDeviceIp);
    rows.push_back("UDP target " + (state.udpTargetIp.empty() ? std::string("unset") : state.udpTargetIp));
    rows.push_back("Build      v" + std::string(AppVersion));
    rows.push_back("Latest");
    if (state.messages.empty()) {
        rows.push_back("  No text messages decoded yet.");
    } else {
        const Message &m = state.messages.back();
        rows.push_back("  " + timeText(m.rxTime) + " " + std::string(m.direct ? "DM " : "CH ") +
                       senderText(m) + " " + m.channelName);
        rows.push_back("  " + messageText(m));
    }

    const int visibleRows = std::min(10, PageBottom - ContentRow + 1);
    int start = state.dashboardFocused ? state.scrollOffset : 0;
    start = std::max(0, std::min(start, std::max(0, static_cast<int>(rows.size()) - visibleRows)));
    for (int i = 0; i < visibleRows; ++i) {
        const int idx = start + i;
        if (idx >= static_cast<int>(rows.size())) break;
        printContent(ContentRow + i, 3, rows[static_cast<size_t>(idx)]);
    }
}

void drawMessages(AppState &state) {
    printContent(ContentRow, 3, "Chats / Messages " + std::to_string(state.messages.size()) + "/" + std::to_string(MaxMessages));
    if (state.messages.empty()) {
        printContent(ContentRow + 2, 5, "Waiting for channel or direct text messages.");
        printContent(ContentRow + 3, 5, "Use LOG/DEBUG if RX increases but messages do not decode.");
        return;
    }
    clampState(state);
    int row = ContentRow + 2;
    const int selectedOffset = static_cast<int>(state.messages.size()) - 1 - state.selectedMessageIndex;
    const int startOffset = std::max(0, selectedOffset - 1);
    for (int offset = startOffset; row <= 17; ++offset) {
        const int index = newestMessageIndex(state, offset);
        if (index < 0) break;
        const Message &m = state.messages[static_cast<size_t>(index)];
        const bool selected = index == state.selectedMessageIndex;
        const std::string type = m.direct ? "DM" : "CH";
        printContent(row++, 3, std::string(selected ? ">" : " ") + timeText(m.rxTime) + " " + type +
                      " " + senderText(m) + " " + m.channelName);
        printContent(row++, 5, messageText(m));
    }
    printAt(19, 3, "A: open selected chat/reply");
}

void drawNodes(AppState &state) {
    const std::string online = state.onlineNodeCount >= 0
        ? std::to_string(state.onlineNodeCount) + " of " +
          std::to_string(state.totalNodeCount >= 0 ? state.totalNodeCount : static_cast<int32_t>(state.nodeCount)) +
          " nodes online"
        : std::to_string(state.nodeCount) + " nodes known";
    printContent(ContentRow, 3, "NODE LIST   " + online);
    if (state.knownNodes.empty()) {
        printContent(ContentRow + 2, 5, "Waiting for node info from the RAK.");
        printContent(ContentRow + 4, 5, "2: Node Options filters/highlights");
        return;
    }
    clampState(state);
    int row = ContentRow + 3;
    int first = std::max(0, state.selectedNodeIndex - 2);
    for (int slot = 0; slot < 5 && row <= PageBottom - 1; ++slot) {
        const int i = first + slot;
        if (i >= static_cast<int>(state.knownNodes.size())) break;
        const NodeSummary &n = state.knownNodes[static_cast<size_t>(i)];
        const std::vector<uint32_t> icons = nodeIconCodepoints(n, 3);
        const bool selected = i == state.selectedNodeIndex;
        const std::string name = n.name.empty() ? "(unnamed)" : n.name;
        std::string rightTop;
#if defined(WIIMESH_ICON_DEBUG)
        if (!icons.empty()) {
            rightTop = iconCodepointSummary(icons);
        } else
#endif
        if (n.isMine) {
            rightTop = "100% " + fixed2(state.voltage, "V");
        } else {
            rightTop = (i % 3 == 0 ? "hops: 3" : (i % 3 == 1 ? "hops: 1" : "known"));
        }
        const std::string rightBottom = n.isMine
            ? "now"
            : (state.lastRxRssi == 0 ? "now" : "rssi " + std::to_string(state.lastRxRssi));
        printContent(row, 9, std::string(selected && state.dashboardFocused ? ">" : " ") +
                     trimTo(name, 24));
        printContent(row, 38, rightTop);
        ++row;
        printContent(row, 9, trimTo(n.nodeId, 12));
        printContent(row, 42, rightBottom);
        row += 2;
    }
    printContent(PageBottom, 3, "A chat   Up/Down node   2 options");
}

void drawNodeOptions(const AppState &state) {
    int row = ContentRow;
    printContent(row++, 3, "Node Options");
    printContent(row++, 3, std::string(state.nodeOptionTab == 0 ? "[Filter]" : " Filter ") +
                 "    " + std::string(state.nodeOptionTab == 1 ? "[Highlight]" : " Highlight "));
    row++;
    if (state.nodeOptionTab == 0) {
        printContent(row++, 3, "Unknown        [off]");
        printContent(row++, 3, "Offline        [off]");
        printContent(row++, 3, "Public Key     [off]");
        printContent(row++, 3, "Channel        [ALL]");
        printContent(row++, 3, "Hops away      [<= 7]");
        printContent(row++, 3, "Position       [off]");
        printContent(row++, 3, "Name           [enter filter...]");
        row++;
        printContent(row++, 3, "Read-only placeholders for now.");
        printContent(row++, 3, "Filters will hide nodes once controls are wired.");
    } else {
        printContent(row++, 3, "Closest signal [off]");
        printContent(row++, 3, "Direct chats   [on]");
        printContent(row++, 3, "This device    [on]");
        printContent(row++, 3, "No position    [dim]");
        printContent(row++, 3, "Channel match  [on]");
        row++;
        printContent(row++, 3, "Highlights will tint Node List rows later.");
    }
    printContent(PageBottom, 3, "L/R: Filter/Highlight   B: Node List");
}

void drawChannels(const AppState &state) {
    int row = ContentRow;
    printContent(row++, 3, "Channels");
    printContent(row++, 3, "> [LOCK] " + state.channelName + "  primary");
    printContent(row++, 5, "Channel index 0. Text packets shown in CHATS.");
    row++;
    printContent(row++, 3, "More channel metadata needs Channel protobuf decoding.");
    printContent(row++, 3, "For now this confirms the active channel name WiiMesh knows.");
}

void drawMap(const AppState &state) {
    int row = ContentRow;
    printContent(row++, 3, "Map");
    printContent(row++, 3, "Map view placeholder.");
    printContent(row++, 3, "Known nodes:");
    int shown = 0;
    for (const auto &n : state.knownNodes) {
        if (shown++ >= 8 || row > PageBottom) break;
        printContent(row++, 5, "* " + (n.name.empty() ? n.nodeId : n.name) + " " + n.nodeId);
    }
    if (shown == 0) {
        printContent(row++, 5, "No nodes yet.");
    }
}

void drawSettings(const AppState &state) {
    int row = ContentRow;
    printContent(row++, 3, "Settings / Tools");
    printContent(row++, 3, "> Log       packet stream");
    printContent(row++, 3, "  Debug     decoded packet cards");
    printContent(row++, 3, "  Status    USB, network, packet counters");
    row++;
    printContent(row++, 3, "A: open Log. Use Right from LOG for DEBUG/STATUS.");
    printContent(row++, 3, "+: enable IP/UDP.  1: USB diagnostics.");
    row++;
    printContent(row++, 3, "Network: " + state.netStatus);
    printContent(row++, 3, "Wii IP:  " + state.wiiIp);
    printContent(row++, 3, "M Device IP: " + state.meshDeviceIp);
    printContent(row++, 3, "UDP ->   " + (state.udpTargetIp.empty() ? std::string("unset") : state.udpTargetIp));
}

void drawChat(const AppState &state) {
    printContent(ContentRow, 3, "Chat");
    printContent(ContentRow + 1, 3, "Peer: " + state.chatPeerName + " " + state.chatPeerNodeId);
    int row = ContentRow + 3;
    int shown = 0;
    for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0 && row <= 17; --i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        const std::string sid = m.senderId.empty() ? nodeId(m.from) : m.senderId;
        if (!state.chatPeerNodeId.empty() && sid != state.chatPeerNodeId && m.to != state.pendingSendTo) {
            continue;
        }
        printContent(row++, 3, timeText(m.rxTime) + " " + senderText(m));
        printWrappedContent(row, messageText(m), 5);
        if (++shown >= 2) break;
    }
    if (shown == 0) {
        printContent(ContentRow + 4, 5, state.chatPeerNodeId.empty()
                      ? "Pick a node or message first."
                      : "No saved messages with this node yet.");
    }
    printAt(19, 3, "A: type direct message");
}

void drawStream(const AppState &state) {
    printContent(ContentRow, 3, "Streaming Log");
    int row = ContentRow + 2;
    const int newest = static_cast<int>(state.streamEvents.size()) - 1;
    const int start = newest - state.scrollOffset;
    for (int i = start; i >= 0 && row <= PageBottom; --i) {
        printContent(row++, 3, cleanText(state.streamEvents[static_cast<size_t>(i)]));
    }
    if (row == ContentRow + 2) printContent(row, 5, "No stream events yet.");
}

void drawDebug(const AppState &state) {
    printContent(ContentRow, 3, "Debug Packets");
    if (state.debugPackets.empty()) {
        printContent(ContentRow + 2, 5, "No debug packets yet.");
        return;
    }
    int index = static_cast<int>(state.debugPackets.size()) - 1 - state.scrollOffset;
    if (index < 0) index = 0;
    const DebugPacket &p = state.debugPackets[static_cast<size_t>(index)];
    int row = ContentRow + 2;
    printContent(row++, 3, "Packet " + std::to_string(index + 1) + "/" +
                      std::to_string(state.debugPackets.size()) + " " + timeText(p.time));
    printWrappedContent(row, p.title, 3);
    printWrappedContent(row, p.summary, 3);
    printWrappedContent(row, p.meshFields, 3);
    printWrappedContent(row, p.dataFields, 3);
    printWrappedContent(row, p.decoded, 3);
}

void drawStatus(const AppState &state) {
    int row = ContentRow;
    printContent(row++, 3, "Status");
    printContent(row++, 3, "USB detail: " + state.usbDetail);
    printContent(row++, 3, "Log: " + state.logStatus);
    printContent(row++, 3, "Net: " + state.netStatus);
    printContent(row++, 3, "Wii IP: " + state.wiiIp);
    printContent(row++, 3, "M Device IP: " + state.meshDeviceIp);
    printContent(row++, 3, "UDP target: " + (state.udpTargetIp.empty() ? std::string("unset") : state.udpTargetIp));
    printContent(row++, 3, "Last packet from " + nodeId(state.lastPacketFrom) +
                      " to " + nodeId(state.lastPacketTo));
    printContent(row++, 3, "Port " + std::to_string(state.lastPacketPort) +
                      " channel " + std::to_string(state.lastPacketChannel));
    printContent(row++, 3, "Decoded packets " + std::to_string(state.decodedPacketCount));
}

void drawDiagnostics(const AppState &state) {
    int row = 10;
    printContent(row++, 3, "USB Diagnostics");
    printContent(row++, 3, state.usbDetail.empty() ? "No detail yet." : state.usbDetail);
    for (const auto &d : state.usbDevices) {
        if (row >= 27) break;
        char line[128];
        std::snprintf(line, sizeof(line), "VID %s PID %s class %02x sub %02x proto %02x %s",
                      hex16(d.vendorId).c_str(), hex16(d.productId).c_str(),
                      d.deviceClass, d.deviceSubClass, d.deviceProtocol, d.driverHint.c_str());
        printContent(row++, 3, line);
        for (const auto &iface : d.interfaces) {
            if (row >= 27) break;
            std::snprintf(line, sizeof(line), "  if %u class %02x sub %02x proto %02x",
                          iface.number, iface.klass, iface.subclass, iface.protocol);
            printContent(row++, 3, line);
            for (const auto &ep : iface.endpoints) {
                if (row >= 27) break;
                std::snprintf(line, sizeof(line), "    ep %02x attr %02x max %u interval %u",
                              ep.address, ep.attributes, ep.maxPacketSize, ep.interval);
                printContent(row++, 3, line);
            }
        }
    }
    printAt(28, 3, "1: close diagnostics  HOME: exit");
}

void drawCalibrationText(const AppState &state) {
    std::printf("\x1b[2J");
    printAt(2, 3, "WiiMesh Placement Calibration");
    printAt(3, 3, "A saves+next. B exits. Hold 1+D-pad resizes. -/+ text width.");
    printAt(5, 3, "Outer yellow = full 640x480. Green = 16px safe border.");
    printAt(6, 3, "Cyan = console text area. Grid blocks are 64px.");
    printAt(8, 3, "X: 0     64    128   192   256   320   384   448   512   576");
    const GuiZone &selected = gGuiZones[gSelectedGuiZone];
    printAt(9, 3, "SELECTED [" + std::to_string(gSelectedGuiZone + 1) + "/" +
                  std::to_string(GuiZoneCount) + "]: " + std::string(selected.name) +
                  "  x=" + std::to_string(selected.x) +
                  " y=" + std::to_string(selected.y) +
                  " w=" + std::to_string(selected.w) +
                  " h=" + std::to_string(selected.h));
    printAt(10, 3, "D-pad move  Hold 1+D-pad resize  -/+ text width=" +
                   std::to_string(selected.textWidth));
    printAt(12, 3, "Zones: bright white/yellow box is active");
    for (int i = 0; i < GuiZoneCount; ++i) {
        const GuiZone &z = gGuiZones[i];
        printAt(13 + i, 3, std::string(gSelectedGuiZone == i ? ">>> " : "    ") +
                         std::to_string(i + 1) + " " + z.name +
                         " x=" + std::to_string(z.x) +
                         " y=" + std::to_string(z.y) +
                         " w=" + std::to_string(z.w) +
                         " h=" + std::to_string(z.h) +
                         " text=" + std::to_string(z.textWidth));
    }
    printAt(24, 3, "Last save: " + state.usbDetail);
    printAt(10, 68, "Y064");
    printAt(14, 68, "Y128");
    printAt(18, 68, "Y192");
    printAt(22, 68, "Y256");
    printAt(26, 68, "Y320");
    printAt(28, 68, "Y384");
}

#if defined(WIIMESH_WII)
void buildBellTone() {
    if (gBellReady) return;
    for (int i = 0; i < BellSamples; ++i) {
        const int period = (i < BellSamples / 2) ? 36 : 27;
        const int wave = ((i / period) & 1) ? 1 : -1;
        const int env = BellSamples - i;
        gBellPcm[i] = static_cast<s16>(wave * env * 8500 / BellSamples);
    }
    DCFlushRange(gBellPcm, sizeof(gBellPcm));
    gBellReady = true;
}

void maybePlayBell(const AppState &state) {
    static size_t lastSeen = 0;
    static bool armed = false;
    if (!armed) {
        lastSeen = state.messages.size();
        armed = true;
        return;
    }
    if (state.messages.size() <= lastSeen) return;
    for (size_t i = lastSeen; i < state.messages.size(); ++i) {
        const Message &m = state.messages[i];
        if (m.direct && isBellAlertText(m.text)) {
            buildBellTone();
            ASND_SetVoice(0, VOICE_MONO_16BIT, 32000, 0, gBellPcm, sizeof(gBellPcm), 220, 220, nullptr);
            break;
        }
    }
    lastSeen = state.messages.size();
}
#endif

std::string basicLayoutExport() {
    return "# WiiMesh basic UI\n"
           "# This build ignores MeshLayout.config so the real Wii screen stays readable.\n"
           "ui=basic\n";
}

}

void Ui::init() {
#if defined(WIIMESH_WII)
    VIDEO_Init();
    WPAD_Init();
    WPAD_SetVRes(0, SkinWidth, SkinHeight);
    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    ASND_Init();
    ASND_Pause(0);

    GXRModeObj mode;
    gfx_video_get_modeobj(&mode, GFX_STANDARD_AUTO, GFX_MODE_DEFAULT);
    gfx_video_set_overscan(0);
    gfx_video_init(&mode);
    gfx_init();
    gfx_set_underscan(18, 18);
    gSkinPixels.resize(static_cast<size_t>(SkinWidth) * SkinHeight);
    if (gfx_tex_init(&gSkinTexture, GFX_TF_RGB565, 0, SkinWidth, SkinHeight)) {
        gfx_tex_set_bilinear_filter(&gSkinTexture, false);
        gGraphicsReady = true;
    }
    loadGuiConfig();
    compactGuiLayout();
    gfx_screen_coords_t conCoords = {32.0f, 24.0f, 576.0f, 432.0f};
    gConsoleReady = gfx_con_init(&conCoords);
    if (gConsoleReady) {
        gfx_con_set_alpha(255, 0);
    }
#endif
}

bool Ui::reloadLayout(AppState &state) {
    state.usbDetail = "Basic UI ignores MeshLayout.config";
    return true;
}

bool Ui::applyLayoutLine(AppState &state, const std::string &line) {
    (void)line;
    state.usbDetail = "Basic UI ignores live layout lines";
    return true;
}

bool Ui::saveLayout(AppState &state) const {
    state.usbDetail = "Basic UI has no layout file to save";
    return true;
}

std::string Ui::exportLayout() const {
    return basicLayoutExport();
}

void Ui::updateInput(AppState &state) {
#if defined(WIIMESH_WII)
    WPAD_ScanPads();
    const u32 down = WPAD_ButtonsDown(0);
    const u32 held = WPAD_ButtonsHeld(0);
    ir_t ir;
    std::memset(&ir, 0, sizeof(ir));
    WPAD_IR(0, &ir);
    state.pointerVisible = state.pointerEnabled && ir.valid;
    if (state.pointerVisible) {
        state.pointerX = std::max(0, std::min(SkinWidth - 1, static_cast<int>(ir.x)));
        state.pointerY = std::max(0, std::min(SkinHeight - 1, static_cast<int>(ir.y)));
    }
    const std::vector<std::string> keys = keyboardKeys();
    if (gScreensaverDismissBlockFrames > 0) --gScreensaverDismissBlockFrames;

    if ((down & WPAD_BUTTON_HOME) && !gScreensaverActive) {
        std::exit(0);
    }
    if ((down & WPAD_BUTTON_MINUS) && !state.showCalibration) {
        state.showCalibration = true;
        state.showDiagnostics = false;
        return;
    }
    if (gScreensaverActive) {
        if (down && gScreensaverDismissBlockFrames <= 0) {
            gScreensaverActive = false;
            state.screensaverActive = false;
            state.usbDetail = "Screensaver closed";
            noteScreensaver(state, "dismiss button");
            return;
        }
        state.screensaverActive = true;
        return;
    }
    if (down || held) {
        gScreensaverIdleFrames = 0;
    } else {
        if (gScreensaverIdleFrames < ScreensaverIdleFrames) {
            ++gScreensaverIdleFrames;
        }
        if (gScreensaverIdleFrames >= ScreensaverIdleFrames) {
            gScreensaverActive = true;
            state.screensaverActive = true;
            noteScreensaver(state, "start idle");
        }
    }
    if ((down & WPAD_BUTTON_A) && handlePointerClick(state)) {
        return;
    }
    if (state.showCalibration) {
        GuiZone &z = gGuiZones[gSelectedGuiZone];
        constexpr int Step = 4;
        if (down & WPAD_BUTTON_A) {
            if (saveGuiConfig(state)) {
                gSelectedGuiZone = (gSelectedGuiZone + 1) % GuiZoneCount;
                state.usbDetail = "Saved; selected " + std::string(gGuiZones[gSelectedGuiZone].name);
            }
            return;
        }
        if (down & WPAD_BUTTON_B) {
            state.showCalibration = false;
            return;
        }
        const bool resizing = (held & WPAD_BUTTON_1) &&
            (down & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_UP | WPAD_BUTTON_DOWN));
        if (resizing) {
            if (down & WPAD_BUTTON_LEFT) z.w = std::max(32, z.w - Step);
            if (down & WPAD_BUTTON_RIGHT) z.w += Step;
            if (down & WPAD_BUTTON_UP) z.h = std::max(24, z.h - Step);
            if (down & WPAD_BUTTON_DOWN) z.h += Step;
        } else {
            if (down & WPAD_BUTTON_LEFT) z.x -= Step;
            if (down & WPAD_BUTTON_RIGHT) z.x += Step;
            if (down & WPAD_BUTTON_UP) z.y -= Step;
            if (down & WPAD_BUTTON_DOWN) z.y += Step;
        }
        if (down & WPAD_BUTTON_MINUS) z.textWidth = std::max(4, z.textWidth - 2);
        if (down & WPAD_BUTTON_PLUS) z.textWidth = std::min(70, z.textWidth + 2);
        return;
    }
    if (down & WPAD_BUTTON_1) {
        state.showDiagnostics = !state.showDiagnostics;
        return;
    }
    if (down & WPAD_BUTTON_PLUS) {
        state.networkRequested = true;
    }
    if (state.showDiagnostics) return;

    clampState(state);

    if (state.composingMessage) {
        if (down & WPAD_BUTTON_B) {
            state.composingMessage = false;
            return;
        }
        if (down & WPAD_BUTTON_LEFT) {
            if (state.keyboardCursor > 0) state.keyboardCursor--;
        }
        if (down & WPAD_BUTTON_RIGHT) {
            if (state.keyboardCursor + 1 < static_cast<int>(keys.size())) state.keyboardCursor++;
        }
        if (down & WPAD_BUTTON_UP) {
            if (state.keyboardCursor >= KeyboardCols) state.keyboardCursor -= KeyboardCols;
        }
        if (down & WPAD_BUTTON_DOWN) {
            if (state.keyboardCursor + KeyboardCols < static_cast<int>(keys.size())) state.keyboardCursor += KeyboardCols;
        }
        if (down & WPAD_BUTTON_A) {
            pressKeyboard(state);
        }
        return;
    }

    if (down & WPAD_BUTTON_B) {
        if (state.dashboardFocused && state.uiTab < PrimaryTabCount) {
            state.dashboardFocused = false;
        } else if (state.uiTab == ScreenChatDetail) {
            state.uiTab = ScreenChats;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenNodeTools) {
            state.uiTab = ScreenNodeOptions;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenScreensaverSettings || state.uiTab == ScreenFontSettings) {
            state.uiTab = ScreenSettings;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenLog || state.uiTab == ScreenDebug || state.uiTab == ScreenStatus) {
            state.uiTab = ScreenSettings;
            state.dashboardFocused = true;
        } else {
            state.uiTab = ScreenHome;
            state.dashboardFocused = false;
        }
        state.scrollOffset = 0;
    }
    if (down & WPAD_BUTTON_UP) {
        if (state.uiTab < PrimaryTabCount && !state.dashboardFocused) {
            state.uiTab = (state.uiTab + PrimaryTabCount - 1) % PrimaryTabCount;
            state.scrollOffset = 0;
        } else if (state.uiTab == ScreenChats && state.selectedChatIndex > 0) {
            state.selectedChatIndex--;
        } else if (state.uiTab == ScreenChannels && state.selectedChannelIndex > 0) {
            state.selectedChannelIndex--;
        } else if (state.uiTab == ScreenSettings && state.selectedSettingsIndex > 0) {
            state.selectedSettingsIndex--;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex > 0) {
            state.selectedScreensaverIndex--;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex > 0) {
            state.selectedFontIndex--;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && state.selectedNodeIndex > 0) {
            state.selectedNodeIndex--;
        } else if (state.scrollOffset > 0) {
            state.scrollOffset--;
        }
    }
    if (down & WPAD_BUTTON_DOWN) {
        if (state.uiTab < PrimaryTabCount && !state.dashboardFocused) {
            state.uiTab = (state.uiTab + 1) % PrimaryTabCount;
            state.scrollOffset = 0;
        } else if (state.uiTab == ScreenChats &&
                   state.selectedChatIndex + 1 < static_cast<int>(chatEntries(state).size())) {
            state.selectedChatIndex++;
        } else if (state.uiTab == ScreenChannels &&
                   state.selectedChannelIndex + 1 < std::max(8, static_cast<int>(state.channels.size()))) {
            state.selectedChannelIndex++;
        } else if (state.uiTab == ScreenSettings && state.selectedSettingsIndex < 5) {
            state.selectedSettingsIndex++;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex < 2) {
            state.selectedScreensaverIndex++;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex < 1) {
            state.selectedFontIndex++;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) &&
                   state.selectedNodeIndex + 1 < static_cast<int>(state.knownNodes.size())) {
            state.selectedNodeIndex++;
        } else {
            state.scrollOffset++;
        }
    }
    if (down & WPAD_BUTTON_LEFT) {
        if (state.uiTab == ScreenChats && state.selectedChatIndex > 0) {
            state.selectedChatIndex--;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && state.selectedNodeIndex > 0) {
            state.selectedNodeIndex--;
        } else if (state.uiTab == ScreenNodeTools) {
            state.nodeOptionTab = 0;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 0) {
            state.screensaverMode = (state.screensaverMode + 2) % 3;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 1) {
            state.screensaverSpeed = std::max(0, state.screensaverSpeed - 1);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 0) {
            state.fontStyle = (state.fontStyle + FontStyleCount - 1) % FontStyleCount;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 1) {
            state.fontSize = std::max(0, state.fontSize - 1);
        } else if (state.uiTab == ScreenDebug) {
            state.uiTab = ScreenLog;
        } else if (state.uiTab == ScreenStatus) {
            state.uiTab = ScreenDebug;
        }
    }
    if ((down & WPAD_BUTTON_2) && state.uiTab == ScreenNodeOptions) {
        state.uiTab = ScreenNodeTools;
        state.scrollOffset = 0;
        return;
    }
    if (down & WPAD_BUTTON_RIGHT || down & WPAD_BUTTON_2) {
        if (state.uiTab == ScreenChats &&
            state.selectedChatIndex + 1 < static_cast<int>(chatEntries(state).size())) {
            state.selectedChatIndex++;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) &&
                   state.selectedNodeIndex + 1 < static_cast<int>(state.knownNodes.size())) {
            state.selectedNodeIndex++;
        } else if (state.uiTab == ScreenNodeTools) {
            state.nodeOptionTab = 1;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 0) {
            state.screensaverMode = (state.screensaverMode + 1) % 3;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 1) {
            state.screensaverSpeed = std::min(5, state.screensaverSpeed + 1);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 0) {
            state.fontStyle = (state.fontStyle + 1) % FontStyleCount;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 1) {
            state.fontSize = std::min(FontSizeMax, state.fontSize + 1);
        } else if (state.uiTab == ScreenLog) {
            state.uiTab = ScreenDebug;
        } else if (state.uiTab == ScreenDebug) {
            state.uiTab = ScreenStatus;
        }
    }
    if (down & WPAD_BUTTON_A) {
        if (state.uiTab < PrimaryTabCount && !state.dashboardFocused) {
            state.dashboardFocused = true;
            state.scrollOffset = 0;
            return;
        }
        if (state.uiTab == ScreenChats) {
            const std::vector<ChatEntry> entries = chatEntries(state);
            if (!entries.empty()) {
                const int index = std::max(0, std::min(state.selectedChatIndex, static_cast<int>(entries.size()) - 1));
                openChatEntry(state, entries[static_cast<size_t>(index)]);
            }
        } else if (state.uiTab == ScreenHome && !state.messages.empty()) {
            clampState(state);
            state.selectedMessageIndex = static_cast<int>(state.messages.size()) - 1;
            const Message &m = state.messages[static_cast<size_t>(state.selectedMessageIndex)];
            setChatPeerFromMessage(state, m);
            state.uiTab = ScreenChatDetail;
            state.seenMessageCount = static_cast<uint32_t>(state.messages.size());
        } else if (state.uiTab == ScreenChannels) {
            ChatEntry entry;
            entry.channel = true;
            entry.channelIndex = state.selectedChannelIndex;
            entry.title = channelDisplayName(state, state.selectedChannelIndex);
            openChatEntry(state, entry);
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && !state.knownNodes.empty()) {
            clampState(state);
            const NodeSummary &n = state.knownNodes[static_cast<size_t>(state.selectedNodeIndex)];
            state.chatPeerNodeId = n.nodeId;
            state.chatPeerName = n.name.empty() ? n.nodeId : n.name;
            state.uiTab = ScreenChatDetail;
        } else if (state.uiTab == ScreenChatDetail && state.chatChannelIndex < 0 && !state.chatPeerNodeId.empty()) {
            startCompose(state);
        } else if (state.uiTab == ScreenSettings) {
            if (state.selectedSettingsIndex == 0) {
                state.uiTab = ScreenLog;
                state.dashboardFocused = true;
            } else if (state.selectedSettingsIndex == 1) {
                state.uiTab = ScreenDebug;
                state.dashboardFocused = true;
            } else if (state.selectedSettingsIndex == 2) {
                state.uiTab = ScreenStatus;
                state.dashboardFocused = true;
            } else if (state.selectedSettingsIndex == 3) {
                state.uiTab = ScreenScreensaverSettings;
                state.dashboardFocused = true;
            } else if (state.selectedSettingsIndex == 4) {
                state.uiTab = ScreenFontSettings;
                state.dashboardFocused = true;
            } else if (state.selectedSettingsIndex == 5) {
                state.networkRequested = true;
            }
        } else if (state.uiTab == ScreenScreensaverSettings) {
            if (state.selectedScreensaverIndex == 0) {
                state.screensaverMode = (state.screensaverMode + 1) % 3;
            } else if (state.selectedScreensaverIndex == 1) {
                state.screensaverSpeed = (state.screensaverSpeed + 1) % 6;
            } else if (state.selectedScreensaverIndex == 2) {
                gScreensaverIdleFrames = ScreensaverIdleFrames;
                gScreensaverActive = true;
                gScreensaverDismissBlockFrames = 30;
                state.screensaverActive = true;
                state.usbDetail = "Screensaver test";
                noteScreensaver(state, "start test");
            }
        } else if (state.uiTab == ScreenFontSettings) {
            if (state.selectedFontIndex == 0) {
                state.fontStyle = (state.fontStyle + 1) % FontStyleCount;
            } else if (state.selectedFontIndex == 1) {
                state.fontSize = (state.fontSize + 1) % (FontSizeMax + 1);
            }
        }
    }
#else
    (void)state;
#endif
}

void Ui::draw(const AppState &state) {
#if defined(WIIMESH_WII)
    maybePlayBell(state);
    if (gConsoleReady) {
        gfx_frame_start();
        if (gScreensaverActive) {
            drawScreensaverSkin(state);
            gfx_con_clear();
            if (((state.screensaverMode % 3) + 3) % 3 == 2) {
                for (const auto &badge : gScreensaverBadges) {
                    const int row = std::max(2, std::min(27, 1 + (static_cast<int>(badge.y) - ConsoleTopPx + 13) / CharH));
                    const int col = std::max(2, std::min(70, 1 + (static_cast<int>(badge.x) - ConsoleLeftPx + 18) / CharW));
                    printAt(row, col, badge.text);
                }
            } else {
                const std::string name = screensaverDisplayText(state);
                const int row = std::max(2, std::min(27, 1 + (static_cast<int>(gScreensaverY) - ConsoleTopPx + 25) / CharH));
                const int col = std::max(2, std::min(70, 1 + (static_cast<int>(gScreensaverX) - ConsoleLeftPx + 28) / CharW));
                printAt(row, col, name);
                printAt(row + 1, col, state.screensaverMode == 1 ? "Latest message" : "Meshtastic node");
            }
            gfx_con_draw();
            gfx_frame_end();
            return;
        }
        drawBackgroundSkin(state);
        gfx_con_clear();
        if (shouldUseGraphicsOnly(state)) {
            gfx_frame_end();
            return;
        }
    } else {
        std::printf("\x1b[2J");
    }
#else
    std::printf("\x1b[2J");
#endif

    AppState &mutableState = const_cast<AppState &>(state);
    clampState(mutableState);
    if (state.showCalibration) {
        drawCalibrationText(state);
        #if defined(WIIMESH_WII)
        if (gConsoleReady) {
            gfx_con_draw();
            gfx_frame_end();
        } else {
            VIDEO_WaitVSync();
        }
        #endif
        return;
    }
    drawHeader(state);
    if (!state.showDiagnostics) {
        drawTabs(state);
    }

    if (state.showDiagnostics) {
        drawDiagnostics(state);
    } else if (state.uiTab == ScreenHome) {
        drawHome(state);
    } else if (state.uiTab == ScreenChats) {
        drawMessages(mutableState);
    } else if (state.uiTab == ScreenNodeOptions) {
        drawNodes(mutableState);
    } else if (state.uiTab == ScreenNodeTools) {
        drawNodeOptions(mutableState);
    } else if (state.uiTab == ScreenChannels) {
        drawChannels(state);
    } else if (state.uiTab == ScreenMap) {
        drawMap(mutableState);
    } else if (state.uiTab == ScreenSettings) {
        drawSettings(state);
    } else if (state.uiTab == ScreenChatDetail) {
        drawChat(state);
    } else if (state.uiTab == ScreenLog) {
        drawStream(state);
    } else if (state.uiTab == ScreenDebug) {
        drawDebug(state);
    } else if (state.uiTab == ScreenStatus) {
        drawStatus(state);
    } else {
        drawHome(state);
    }
    if (!state.showDiagnostics && state.composingMessage) {
        drawKeyboard(state);
    }
    drawFooter(state);

#if defined(WIIMESH_WII)
    if (gConsoleReady) {
        gfx_con_draw();
        gfx_frame_end();
    } else {
        VIDEO_WaitVSync();
    }
#endif
}

}
