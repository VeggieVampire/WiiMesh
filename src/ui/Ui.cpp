#include "wiimesh/Ui.h"

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
constexpr int ScreenCount = 10;
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
constexpr int GuiZoneCount = 5;

#if defined(WIIMESH_WII)
constexpr int BellSamples = 14400;
constexpr int SkinWidth = 640;
constexpr int SkinHeight = 480;
s16 gBellPcm[BellSamples] ATTRIBUTE_ALIGN(32);
bool gBellReady = false;
bool gConsoleReady = false;
bool gGraphicsReady = false;
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

GuiZone gGuiZones[] = {
    {"header", 0, 0, 456, 118, 54},
    {"menu", 0, 0, 104, 330, 10},
    {"content", 0, 0, 456, 250, 54},
    {"footer", 0, 0, 456, 42, 54},
    {"bell", 0, 0, 28, 28, 8},
};
int gSelectedGuiZone = 0;

GuiZone &zone(const char *name) {
    for (auto &z : gGuiZones) {
        if (std::strcmp(z.name, name) == 0) return z;
    }
    return gGuiZones[0];
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
    for (unsigned char c : in) {
        if (c >= 32 && c <= 126) {
            out.push_back(static_cast<char>(c));
        } else if (c == '\n' || c == '\r' || c == '\t') {
            out.push_back(' ');
        } else {
            out.push_back('.');
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
    if (state.selectedMessageIndex < 0) state.selectedMessageIndex = 0;
    if (state.selectedMessageIndex >= static_cast<int>(state.messages.size())) {
        state.selectedMessageIndex = state.messages.empty() ? 0 : static_cast<int>(state.messages.size()) - 1;
    }
    const int keyCount = static_cast<int>(keyboardKeys().size());
    if (state.keyboardCursor < 0) state.keyboardCursor = 0;
    if (state.keyboardCursor >= keyCount) state.keyboardCursor = keyCount - 1;
}

void setChatPeerFromMessage(AppState &state, const Message &m) {
    state.chatPeerNodeId = m.senderId.empty() ? nodeId(m.from) : m.senderId;
    state.chatPeerName = senderText(m);
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
    std::printf("\x1b[%d;%dH%s", row, col, trimTo(text, std::min(width, edgeWidth)).c_str());
}

void printRule(int row) {
    (void)row;
}

void printContent(int row, int col, const std::string &text) {
    printAtW(zoneRow("content", row), zoneCol("content", col), zone("content").textWidth, text);
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

void drawCircle(int cx, int cy, int r, uint16_t c) {
    int x = r;
    int y = 0;
    int err = 0;
    while (x >= y) {
        putPixel(cx + x, cy + y, c);
        putPixel(cx + y, cy + x, c);
        putPixel(cx - y, cy + x, c);
        putPixel(cx - x, cy + y, c);
        putPixel(cx - x, cy - y, c);
        putPixel(cx - y, cy - x, c);
        putPixel(cx + y, cy - x, c);
        putPixel(cx + x, cy - y, c);
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
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

const uint8_t *tinyGlyph(char ch) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t A[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t C[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t D[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t E[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t H[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t M[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t N[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t O[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t P[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t S[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t T[7] = {31, 4, 4, 4, 4, 4, 4};
    switch (ch) {
    case 'A': return A;
    case 'C': return C;
    case 'D': return D;
    case 'E': return E;
    case 'H': return H;
    case 'M': return M;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'S': return S;
    case 'T': return T;
    default: return blank;
    }
}

int tinyTextWidth(const std::string &text, int scale) {
    if (text.empty()) return 0;
    return static_cast<int>(text.size()) * 5 * scale + (static_cast<int>(text.size()) - 1) * scale;
}

void drawTinyText(const std::string &text, int x, int y, int scale, uint16_t c) {
    for (size_t i = 0; i < text.size(); ++i) {
        const uint8_t *glyph = tinyGlyph(text[i]);
        const int gx = x + static_cast<int>(i) * 6 * scale;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1u << (4 - col))) {
                    fillRect(gx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
    }
}

void drawBellGlyph(int x, int y, int w, int h, uint16_t fill, uint16_t edge) {
    (void)edge;
    const int cx = x + w / 2;
    const int top = y + std::max(2, h / 8);
    const int bottom = y + h * 3 / 4;
    const int left = x + w / 5;
    const int right = x + w - w / 5;
    line(cx, top, cx, top + h / 7, fill);
    line(cx, top + h / 7, left + 3, y + h / 3, fill);
    line(cx, top + h / 7, right - 3, y + h / 3, fill);
    line(left + 3, y + h / 3, left, bottom, fill);
    line(right - 3, y + h / 3, right, bottom, fill);
    line(left, bottom, right, bottom, fill);
    line(left + 2, bottom + 2, right - 2, bottom + 2, fill);
    fillRect(cx - 2, bottom + 4, 5, 5, fill);
    fillRect(cx - 2, top, 5, 5, fill);
}

void drawIconGlyph(int kind, int x, int y, uint16_t c) {
    if (kind == ScreenHome) {
        line(x + 8, y + 16, x + 22, y + 4, c);
        line(x + 22, y + 4, x + 36, y + 16, c);
        strokeRect(x + 12, y + 16, 20, 20, c);
        fillRect(x + 20, y + 24, 6, 12, c);
    } else if (kind == ScreenNodeOptions) {
        strokeRect(x + 12, y + 22, 22, 12, c);
        fillRect(x + 18, y + 30, 10, 4, c);
        line(x + 23, y + 22, x + 23, y + 8, c);
        drawCircle(x + 23, y + 8, 7, c);
        drawCircle(x + 23, y + 8, 13, c);
    } else if (kind == ScreenChannels) {
        drawCircle(x + 15, y + 15, 6, c);
        drawCircle(x + 31, y + 15, 6, c);
        strokeRect(x + 7, y + 24, 18, 10, c);
        strokeRect(x + 23, y + 24, 18, 10, c);
    } else if (kind == ScreenChats) {
        strokeRect(x + 8, y + 10, 26, 20, c);
        line(x + 16, y + 30, x + 10, y + 38, c);
        drawCircle(x + 34, y + 24, 10, c);
    } else if (kind == ScreenMap) {
        strokeRect(x + 8, y + 10, 10, 28, c);
        strokeRect(x + 18, y + 7, 12, 28, c);
        strokeRect(x + 30, y + 10, 10, 28, c);
        line(x + 18, y + 10, x + 18, y + 36, c);
        line(x + 30, y + 8, x + 30, y + 35, c);
    } else {
        drawCircle(x + 24, y + 22, 14, c);
        drawCircle(x + 24, y + 22, 6, c);
        fillRect(x + 22, y + 4, 4, 8, c);
        fillRect(x + 22, y + 32, 4, 8, c);
        fillRect(x + 6, y + 20, 8, 4, c);
        fillRect(x + 34, y + 20, 8, 4, c);
    }
}

void drawBackgroundSkin(const AppState &state) {
    if (!gGraphicsReady || gSkinPixels.empty()) return;
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
    panel(pxCol(2) - 6 + header.x, pxRow(1) - 6 + header.y, header.w, header.h, false);
    fillRect(pxCol(3) + header.x, pxRow(8) - 8 + header.y, header.w - 28, 6, rgb565(20, 78, 83), 255);
    fillRect(pxCol(3) + header.x, pxRow(8) - 8 + header.y, state.protocolReady ? 210 : 90, 6,
             state.protocolReady ? rgb565(77, 232, 141) : rgb565(238, 191, 75), 255);
    drawSignalBars(pxCol(50) + header.x, pxRow(5) + 8 + header.y, rgb565(51, 215, 238));

    const int active = activePrimaryTab(state);
    const int navX = pxCol(62) - 8 + menu.x;
    const int navY = pxRow(1) - 6 + menu.y;
    const int tileW = menu.w;
    const int tileH = 44;
    const int slotH = 52;
    panel(navX, navY, menu.w, menu.h, false);
    for (int i = 0; i < PrimaryTabCount; ++i) {
        const int y = navY + 6 + i * slotH;
        panel(navX + 8, y, tileW - 16, tileH + 4, i == active);
        const uint16_t itemColor = i == active ? rgb565(255, 255, 255) : rgb565(165, 184, 195);
        drawIconGlyph(i, navX + 24, y - 10, itemColor);
        const std::string label = menuLabel(i);
        const int labelScale = 2;
        const int labelX = navX + menu.w / 2 - tinyTextWidth(label, labelScale) / 2;
        const int labelY = y + 34;
        drawTinyText(label, labelX, labelY, labelScale, itemColor);
    }

    panel(pxCol(2) - 6 + content.x, pxRow(9) - 4 + content.y, content.w, content.h, false);
    if (state.uiTab == ScreenChatDetail) {
        panel(pxCol(4) + content.x, pxRow(14) - 4 + content.y, 250, 54, false);
        panel(pxCol(38) + content.x, pxRow(16) - 4 + content.y, 220, 54, true);
        panel(pxCol(4) + content.x, pxRow(20) - 4 + content.y, 528, 34, false);
    } else if (state.uiTab == ScreenMap) {
        strokeRect(pxCol(10) + content.x, pxRow(14) + content.y, 430, 138, rgb565(27, 86, 112));
        line(pxCol(16) + content.x, pxRow(22) + content.y, pxCol(28) + content.x, pxRow(16) + content.y, rgb565(51, 215, 238));
        line(pxCol(28) + content.x, pxRow(16) + content.y, pxCol(50) + content.x, pxRow(20) + content.y, rgb565(51, 215, 238));
        line(pxCol(16) + content.x, pxRow(22) + content.y, pxCol(50) + content.x, pxRow(20) + content.y, rgb565(51, 215, 238));
        fillRect(pxCol(15) + content.x, pxRow(22) - 8 + content.y, 18, 18, rgb565(77, 232, 141));
        fillRect(pxCol(27) + content.x, pxRow(16) - 8 + content.y, 18, 18, rgb565(238, 191, 75));
        fillRect(pxCol(49) + content.x, pxRow(20) - 8 + content.y, 18, 18, rgb565(194, 95, 235));
    }
    panel(pxCol(2) - 6 + footer.x, pxRow(27) - 6 + footer.y, footer.w, footer.h, false);
    if (state.messages.size() > state.seenMessageCount) {
        const GuiZone &bell = zone("bell");
        drawBellGlyph(pxCol(2) + bell.x, pxRow(2) + bell.y, bell.w, bell.h,
                      rgb565(238, 191, 75), rgb565(5, 18, 29));
    }

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
    const int width = zone("footer").textWidth;
    if (state.composingMessage) {
        printAtW(row, col, width, "Typing: D-pad move cursor  A key  B close  SEND queues direct message");
    } else if (state.uiTab >= PrimaryTabCount) {
        printAtW(row, col, width, "A action  B back  L/R tools  - grid  1 USB  + IP  HOME exit");
    } else {
        printAtW(row, col, width, "Up/Down menu  L/R select item  A open  - grid  1 USB  + IP");
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
        for (int col = 0; col < 5 && index < static_cast<int>(keys.size()); ++col, ++index) {
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
    int row = ContentRow;
    printContent(row++, 3, "Home Dashboard");
    printContent(row++, 3, "Radio: " + std::string(state.protocolReady ? "Healthy/online" :
                      (state.usbConnected ? "USB connected, syncing" : "Waiting for USB")));
    printContent(row++, 3, "Primary Channel: " + state.channelName);
    printContent(row++, 3, "Device: " + state.nodeName + "  Me: " + state.myNodeId);
    printContent(row++, 3, "Nodes: " + std::to_string(state.nodeCount) +
                      "  Known IDs: " + std::to_string(state.knownNodes.size()) +
                      "  Text: " + std::to_string(state.textMessageCount));
    printContent(row++, 3, "Storage: last " + std::to_string(MaxMessages) +
                      " messages saved to SD:/apps/wii-mesh/messages.dat");
    row++;
    printContent(row++, 3, "Latest");
    if (state.messages.empty()) {
        printContent(row++, 5, "No text messages decoded yet.");
    } else {
        const Message &m = state.messages.back();
        printContent(row++, 5, timeText(m.rxTime) + " " + std::string(m.direct ? "DM " : "CH ") +
                          senderText(m) + " " + m.channelName);
        printWrappedContent(row, messageText(m), 5);
    }
    row++;
    printContent(row++, 3, "Use CHATS for conversations, NODE OPT for contacts, LOG/DEBUG for tools.");
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
    printContent(ContentRow, 3, "Node Options / Known Nodes " + std::to_string(state.knownNodes.size()) +
                 "  Radio says " + std::to_string(state.nodeCount));
    if (state.knownNodes.empty()) {
        printContent(ContentRow + 2, 5, "Waiting for node info from the RAK.");
        return;
    }
    clampState(state);
    int row = ContentRow + 2;
    int first = std::max(0, state.selectedNodeIndex - 4);
    for (int i = first; i < static_cast<int>(state.knownNodes.size()) && row <= PageBottom; ++i) {
        const NodeSummary &n = state.knownNodes[static_cast<size_t>(i)];
        std::string name = n.name.empty() ? "(unnamed)" : n.name;
        printContent(row++, 3, std::string(i == state.selectedNodeIndex ? ">" : " ") +
                      (n.isMine ? "[ME] " : "     ") + n.nodeId + "  " + name);
    }
    printAt(26, 3, "A: open chat with selected node");
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
    const std::vector<std::string> keys = keyboardKeys();

    if (down & WPAD_BUTTON_HOME) {
        std::exit(0);
    }
    if ((down & WPAD_BUTTON_MINUS) && !state.showCalibration) {
        state.showCalibration = true;
        state.showDiagnostics = false;
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
            if (state.keyboardCursor >= 5) state.keyboardCursor -= 5;
        }
        if (down & WPAD_BUTTON_DOWN) {
            if (state.keyboardCursor + 5 < static_cast<int>(keys.size())) state.keyboardCursor += 5;
        }
        if (down & WPAD_BUTTON_A) {
            pressKeyboard(state);
        }
        return;
    }

    if (down & WPAD_BUTTON_B) {
        if (state.uiTab == ScreenChatDetail) {
            state.uiTab = ScreenChats;
        } else if (state.uiTab == ScreenLog || state.uiTab == ScreenDebug || state.uiTab == ScreenStatus) {
            state.uiTab = ScreenSettings;
        } else {
            state.uiTab = ScreenHome;
        }
        state.scrollOffset = 0;
    }
    if (down & WPAD_BUTTON_UP) {
        if (state.uiTab < PrimaryTabCount) {
            state.uiTab = (state.uiTab + PrimaryTabCount - 1) % PrimaryTabCount;
            state.scrollOffset = 0;
        } else if (state.scrollOffset > 0) {
            state.scrollOffset--;
        }
    }
    if (down & WPAD_BUTTON_DOWN) {
        if (state.uiTab < PrimaryTabCount) {
            state.uiTab = (state.uiTab + 1) % PrimaryTabCount;
            state.scrollOffset = 0;
        } else {
            state.scrollOffset++;
        }
    }
    if (down & WPAD_BUTTON_LEFT) {
        if (state.uiTab == ScreenChats && state.selectedMessageIndex + 1 < static_cast<int>(state.messages.size())) {
            state.selectedMessageIndex++;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && state.selectedNodeIndex > 0) {
            state.selectedNodeIndex--;
        } else if (state.uiTab == ScreenDebug) {
            state.uiTab = ScreenLog;
        } else if (state.uiTab == ScreenStatus) {
            state.uiTab = ScreenDebug;
        }
    }
    if (down & WPAD_BUTTON_RIGHT || down & WPAD_BUTTON_2) {
        if (state.uiTab == ScreenChats && state.selectedMessageIndex > 0) {
            state.selectedMessageIndex--;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) &&
                   state.selectedNodeIndex + 1 < static_cast<int>(state.knownNodes.size())) {
            state.selectedNodeIndex++;
        } else if (state.uiTab == ScreenLog) {
            state.uiTab = ScreenDebug;
        } else if (state.uiTab == ScreenDebug) {
            state.uiTab = ScreenStatus;
        }
    }
    if (down & WPAD_BUTTON_A) {
        if ((state.uiTab == ScreenHome || state.uiTab == ScreenChats) && !state.messages.empty()) {
            clampState(state);
            if (state.uiTab == ScreenHome) {
                state.selectedMessageIndex = static_cast<int>(state.messages.size()) - 1;
            }
            const Message &m = state.messages[static_cast<size_t>(state.selectedMessageIndex)];
            setChatPeerFromMessage(state, m);
            state.uiTab = ScreenChatDetail;
            state.seenMessageCount = static_cast<uint32_t>(state.messages.size());
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && !state.knownNodes.empty()) {
            clampState(state);
            const NodeSummary &n = state.knownNodes[static_cast<size_t>(state.selectedNodeIndex)];
            state.chatPeerNodeId = n.nodeId;
            state.chatPeerName = n.name.empty() ? n.nodeId : n.name;
            state.uiTab = ScreenChatDetail;
        } else if (state.uiTab == ScreenChatDetail && !state.chatPeerNodeId.empty()) {
            startCompose(state);
        } else if (state.uiTab == ScreenSettings) {
            state.uiTab = ScreenLog;
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
        drawBackgroundSkin(state);
        gfx_con_clear();
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
