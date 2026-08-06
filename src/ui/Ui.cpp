#include "wiimesh/Clock.h"
#include "wiimesh/Ui.h"
#include "wiimesh/generated/EmojiIcons.h"
#include "wiimesh/generated/MenuIcons.h"
#include "wiimesh/generated/RoundedFont.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#include <utility>
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
constexpr int ScreenCount = 18;
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
constexpr int ScreenFontDebug = 13;
constexpr int ScreenMidiPlayer = 14;
constexpr int ScreenGuiOptions = 15;
constexpr int ScreenUsbTools = 16;
constexpr int ScreenAdminTools = 17;
constexpr int SettingsRowCount = 13;
constexpr int UsbToolRowCount = 9;
constexpr int UsbToolRowH = 25;
constexpr int AdminToolRowCount = 8;
constexpr int AdminToolRowH = 25;
constexpr int GuiZoneCount = 5;
constexpr int KeyboardCols = 10;
constexpr int KeyboardSuggestionCount = 5;
constexpr int FontStyleCount = 4;
constexpr int FontSizeMax = 6;

enum class KeyboardKeyType {
    Empty,
    Character,
    Suggestion,
    UnicodeBadge,
    Mode,
    Shift,
    Caps,
    Space,
    Backspace,
    Enter,
    Clear,
    Left,
    Right,
    Cancel
};

struct KeyboardKey {
    KeyboardKeyType type = KeyboardKeyType::Empty;
    std::string label;
    std::string value;
    uint32_t emojiCodepoint = 0;
    std::vector<uint32_t> emojiCodepoints;
    int span = 1;
};

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
u32 gRepeatHeldButtons = 0;
int gRepeatHeldFrames = 0;
float gScreensaverX = 96.0f;
float gScreensaverY = 112.0f;
float gScreensaverDx = 0.45f;
float gScreensaverDy = 0.32f;
struct ScreensaverBadge {
    std::string text;
    std::vector<uint32_t> icons;
    uint16_t accent = 0;
    float x = 0.0f;
    float y = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    int w = 0;
    int h = 0;
};
struct ScreensaverBadgeSeed {
    std::string text;
    std::vector<uint32_t> icons;
    uint16_t accent = 0;
};
std::vector<ScreensaverBadge> gScreensaverBadges;
gfx_tex_t gSkinTexture;
std::vector<uint16_t> gSkinPixels;
constexpr const char *GuiConfigPath = "sd:/apps/wii-mesh/GUI.config";

u32 repeatedButtons(u32 held) {
    const u32 mask = WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_UP |
                     WPAD_BUTTON_DOWN | WPAD_BUTTON_PLUS | WPAD_BUTTON_MINUS;
    const u32 current = held & mask;
    if (current == 0) {
        gRepeatHeldButtons = 0;
        gRepeatHeldFrames = 0;
        return 0;
    }
    if (current != gRepeatHeldButtons) {
        gRepeatHeldButtons = current;
        gRepeatHeldFrames = 0;
        return 0;
    }
    ++gRepeatHeldFrames;
    constexpr int InitialDelayFrames = 18;
    constexpr int RepeatEveryFrames = 5;
    if (gRepeatHeldFrames < InitialDelayFrames) return 0;
    return ((gRepeatHeldFrames - InitialDelayFrames) % RepeatEveryFrames) == 0 ? current : 0;
}
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
const char *menuConfigName(int tab);
bool menuTabEnabled(const AppState &state, int tab);
struct KeyboardKey;
std::vector<KeyboardKey> keyboardKeys(const AppState *state = nullptr);
void pressKeyboard(AppState &state);
bool saveGuiConfig(AppState &state);
int firstEnabledPrimaryTab(const AppState &state);
std::vector<ChatEntry> chatEntries(const AppState &state);
int newestUnreadChatIndex(const std::vector<ChatEntry> &entries);
void openChatEntry(AppState &state, const ChatEntry &entry);
int chatDetailMessageCount(const AppState &state);
int chatDetailVisibleCount(const AppState &state);
bool looksLikeNodeIdText(const std::string &text);
ChannelSummary channelSlot(const AppState &state, int index, bool *configured = nullptr);
std::string channelDisplayName(const AppState &state, int index);

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

struct PullBarState {
    bool up = false;
    bool down = false;
};

int settingsFirstVisibleRow(int selectedIndex, int panelHeight) {
    const int rowH = 26;
    const int availableH = std::max(0, panelHeight - 42 - 28);
    const int visibleRows = std::max(1, std::min(SettingsRowCount, availableH / rowH));
    int first = selectedIndex - visibleRows + 1;
    if (first < 0) first = 0;
    const int maxFirst = std::max(0, SettingsRowCount - visibleRows);
    if (first > maxFirst) first = maxFirst;
    return first;
}

int settingsVisibleRows(int panelHeight) {
    const int rowH = 26;
    const int availableH = std::max(0, panelHeight - 42 - 28);
    return std::max(1, std::min(SettingsRowCount, availableH / rowH));
}

int chatEntryVisibleRows(int panelHeight) {
    const int rowH = 74;
    const int gap = 8;
    const int listTop = 68;
    const int listBottom = panelHeight - 34;
    return std::max(1, (listBottom - listTop + gap) / (rowH + gap));
}

int nodeVisibleRows(int panelHeight) {
    const int cardH = 86;
    const int gap = 8;
    const int listTop = 40;
    const int listBottom = panelHeight - 12;
    return std::max(1, (listBottom - listTop + gap) / (cardH + gap));
}

int midiVisibleRows(int panelHeight) {
    const int rowH = 25;
    const int listTop = 68;
    const int listBottom = panelHeight - 34;
    return std::max(1, (listBottom - listTop) / rowH);
}

int chatKeyboardHeight(int contentH) {
    return std::min(std::max(188, contentH * 2 / 3), std::max(150, contentH - 82));
}

bool handlePointerKeyboardClick(AppState &state, int px, int py, int contentX, int contentY,
                                int contentW, int contentH) {
    if (!state.composingMessage || state.uiTab != ScreenChatDetail) return false;

    const int keyboardH = chatKeyboardHeight(contentH);
    const int ky = contentY + contentH - keyboardH - 8;
    const int keyAreaX = contentX + 12;
    const int keyAreaY = ky;
    const int keyAreaW = contentW - 24;
    if (!pointInRect(px, py, keyAreaX, keyAreaY, keyAreaW, keyboardH)) return false;

    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    const int cols = KeyboardCols;
    const int keyW = std::max(30, (contentW - 44) / cols - 6);
    const int keyH = 21;
    const int visibleRows = std::max(1, (keyboardH - 50) / (keyH + 5));
    const int cursorRow = state.keyboardCursor / cols;
    const int totalRows = (static_cast<int>(keys.size()) + cols - 1) / cols;
    int firstRow = std::max(0, cursorRow - visibleRows / 2);
    firstRow = std::min(firstRow, std::max(0, totalRows - visibleRows));
    int index = firstRow * cols;
    int keyY = ky + 42;
    const int lastIndex = std::min(static_cast<int>(keys.size()), (firstRow + visibleRows) * cols);
    while (index < lastIndex && keyY + keyH < contentY + contentH - 8) {
        for (int col = 0; col < cols && index < static_cast<int>(keys.size()); ++col, ++index) {
            if (keys[static_cast<size_t>(index)].type == KeyboardKeyType::Empty) continue;
            const bool suggestion = index < KeyboardSuggestionCount;
            const int sugW = std::max(58, (contentW - 44) / KeyboardSuggestionCount - 6);
            const int span = suggestion ? 1 : std::max(1, keys[static_cast<size_t>(index)].span);
            const int keyX = suggestion
                ? contentX + 22 + index * (sugW + 6)
                : contentX + 22 + col * (keyW + 6);
            const int drawW = suggestion ? sugW : keyW * span + 6 * (span - 1);
            if (pointInRect(px, py, keyX, keyY, drawW, keyH)) {
                state.keyboardCursor = index;
                pressKeyboard(state);
                state.usbDetail = "Pointer key " + keys[static_cast<size_t>(index)].label;
                return true;
            }
        }
        keyY += keyH + 5;
    }

    state.usbDetail = "Pointer keyboard area";
    return true;
}

void activateSettingsSelection(AppState &state) {
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
        state.uiTab = ScreenAdminTools;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 4) {
        state.showCalibration = true;
        state.showDiagnostics = false;
        state.dashboardFocused = true;
        state.usbDetail = "Placement editor";
    } else if (state.selectedSettingsIndex == 5) {
        state.uiTab = ScreenScreensaverSettings;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 6) {
        state.uiTab = ScreenFontSettings;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 7) {
        state.uiTab = ScreenFontDebug;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 8) {
        state.uiTab = ScreenGuiOptions;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 9) {
        state.uiTab = ScreenMidiPlayer;
        state.dashboardFocused = true;
        state.midiCommand = 3;
    } else if (state.selectedSettingsIndex == 10) {
        state.uiTab = ScreenUsbTools;
        state.dashboardFocused = true;
    } else if (state.selectedSettingsIndex == 11) {
        if (state.clearMessagesArmed) {
            state.messages.clear();
            state.messageRevision++;
            state.textMessageCount = 0;
            state.selectedMessageIndex = 0;
            state.selectedChatIndex = 0;
            state.scrollOffset = 0;
            state.clearMessagesArmed = false;
            state.usbStatus = "Messages cleared";
            state.usbDetail = "messages.dat will save empty";
        } else {
            state.clearMessagesArmed = true;
            state.usbStatus = "Confirm clear messages";
            state.usbDetail = "Press A again on CLEAR MSGS";
        }
    } else if (state.selectedSettingsIndex == 12) {
        state.networkRequested = true;
    }
}

void activateScreensaverSettingsSelection(AppState &state) {
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
    saveGuiConfig(state);
}

void activateFontSettingsSelection(AppState &state) {
    if (state.selectedFontIndex == 0) {
        state.fontStyle = (state.fontStyle + 1) % FontStyleCount;
        saveGuiConfig(state);
    } else if (state.selectedFontIndex == 1) {
        state.fontSize = (state.fontSize + 1) % (FontSizeMax + 1);
        saveGuiConfig(state);
    } else if (state.selectedFontIndex == 2) {
        state.uiTab = ScreenFontDebug;
        state.dashboardFocused = true;
    }
}

void activateGuiOptionSelection(AppState &state) {
    if (state.selectedGuiOptionIndex >= 0 && state.selectedGuiOptionIndex < PrimaryTabCount) {
        state.menuEnabled[state.selectedGuiOptionIndex] = !state.menuEnabled[state.selectedGuiOptionIndex];
        bool any = false;
        for (int i = 0; i < PrimaryTabCount; ++i) any = any || state.menuEnabled[i];
        if (!any) state.menuEnabled[ScreenSettings] = true;
        if (state.uiTab < PrimaryTabCount && !menuTabEnabled(state, state.uiTab)) {
            state.uiTab = firstEnabledPrimaryTab(state);
        }
    } else if (state.selectedGuiOptionIndex == 6) {
        state.debugEnabled = !state.debugEnabled;
    } else if (state.selectedGuiOptionIndex == 7) {
        state.pointerEnabled = !state.pointerEnabled;
    } else if (state.selectedGuiOptionIndex == 8) {
        state.midiRepeat = !state.midiRepeat;
    }
    saveGuiConfig(state);
    state.usbDetail = "GUI option saved";
}

void activateUsbToolSelection(AppState &state) {
    state.usbCommand = state.selectedUsbToolIndex + 1;
    state.usbDetail = "USB tool queued";
}

void activateAdminToolSelection(AppState &state) {
    state.adminCommand = state.selectedAdminToolIndex + 1;
    state.usbDetail = "Admin action queued";
}

PullBarState pullBarState(const AppState &state, int panelHeight) {
    PullBarState pull;
    if (state.composingMessage && state.uiTab == ScreenChatDetail) {
        const std::vector<KeyboardKey> keys = keyboardKeys(&state);
        const int keyboardH = chatKeyboardHeight(panelHeight);
        const int keyH = 21;
        const int visibleRows = std::max(1, (keyboardH - 50) / (keyH + 5));
        const int totalRows = (static_cast<int>(keys.size()) + KeyboardCols - 1) / KeyboardCols;
        const int cursorRow = state.keyboardCursor / KeyboardCols;
        pull.up = cursorRow > 0;
        pull.down = totalRows > visibleRows && cursorRow + 1 < totalRows;
        return pull;
    }
    if (state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) {
        const int visible = nodeVisibleRows(panelHeight);
        pull.up = state.selectedNodeIndex > 0;
        pull.down = static_cast<int>(state.knownNodes.size()) > visible &&
                    state.selectedNodeIndex + 1 < static_cast<int>(state.knownNodes.size());
    } else if (state.uiTab == ScreenChats) {
        const int total = static_cast<int>(chatEntries(state).size());
        const int visible = chatEntryVisibleRows(panelHeight);
        pull.up = state.selectedChatIndex > 0;
        pull.down = total > visible && state.selectedChatIndex + 1 < total;
    } else if (state.uiTab == ScreenChannels) {
        const int total = std::max(8, static_cast<int>(state.channels.size()));
        pull.up = state.selectedChannelIndex > 0;
        pull.down = state.selectedChannelIndex + 1 < total;
    } else if (state.uiTab == ScreenSettings) {
        const int visible = settingsVisibleRows(panelHeight);
        pull.up = state.selectedSettingsIndex > 0;
        pull.down = SettingsRowCount > visible && state.selectedSettingsIndex + 1 < SettingsRowCount;
    } else if (state.uiTab == ScreenGuiOptions) {
        pull.up = state.selectedGuiOptionIndex > 0;
        pull.down = state.selectedGuiOptionIndex < 8;
    } else if (state.uiTab == ScreenMidiPlayer) {
        const int visible = midiVisibleRows(panelHeight);
        pull.up = state.selectedMidiIndex > 0;
        pull.down = static_cast<int>(state.midiFiles.size()) > visible &&
                    state.selectedMidiIndex + 1 < static_cast<int>(state.midiFiles.size());
    } else if (state.uiTab == ScreenUsbTools) {
        pull.up = state.selectedUsbToolIndex > 0;
        pull.down = state.selectedUsbToolIndex + 1 < UsbToolRowCount;
    } else if (state.uiTab == ScreenAdminTools) {
        pull.up = state.selectedAdminToolIndex > 0;
        pull.down = state.selectedAdminToolIndex + 1 < AdminToolRowCount;
    } else if (state.uiTab == ScreenChatDetail) {
        const int maxScroll = std::max(0, chatDetailMessageCount(state) - chatDetailVisibleCount(state));
        pull.up = state.scrollOffset < maxScroll;
        pull.down = state.scrollOffset > 0;
    } else if (state.uiTab == ScreenLog) {
        pull.up = state.scrollOffset + PageRows < static_cast<int>(state.streamEvents.size());
        pull.down = state.scrollOffset > 0;
    } else if (state.uiTab == ScreenDebug) {
        pull.up = state.scrollOffset + 1 < static_cast<int>(state.debugPackets.size());
        pull.down = state.scrollOffset > 0;
    }
    return pull;
}

bool applyPullBar(AppState &state, int direction, int panelHeight) {
    const PullBarState pull = pullBarState(state, panelHeight);
    if (direction < 0 && !pull.up) return false;
    if (direction > 0 && !pull.down) return false;
    if (state.composingMessage && state.uiTab == ScreenChatDetail) {
        const std::vector<KeyboardKey> keys = keyboardKeys(&state);
        if (keys.empty()) return false;
        state.keyboardCursor = direction < 0
            ? std::max(0, state.keyboardCursor - KeyboardCols)
            : std::min(static_cast<int>(keys.size()) - 1, state.keyboardCursor + KeyboardCols);
    } else if (state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) {
        state.selectedNodeIndex = std::max(0, std::min(static_cast<int>(state.knownNodes.size()) - 1,
                                                       state.selectedNodeIndex + direction));
    } else if (state.uiTab == ScreenChats) {
        const int total = static_cast<int>(chatEntries(state).size());
        state.selectedChatIndex = std::max(0, std::min(total - 1, state.selectedChatIndex + direction));
    } else if (state.uiTab == ScreenChannels) {
        const int total = std::max(8, static_cast<int>(state.channels.size()));
        state.selectedChannelIndex = std::max(0, std::min(total - 1, state.selectedChannelIndex + direction));
    } else if (state.uiTab == ScreenSettings) {
        state.selectedSettingsIndex = std::max(0, std::min(SettingsRowCount - 1, state.selectedSettingsIndex + direction));
        state.clearMessagesArmed = false;
    } else if (state.uiTab == ScreenGuiOptions) {
        state.selectedGuiOptionIndex = std::max(0, std::min(8, state.selectedGuiOptionIndex + direction));
    } else if (state.uiTab == ScreenMidiPlayer) {
        const int total = static_cast<int>(state.midiFiles.size());
        state.selectedMidiIndex = std::max(0, std::min(total - 1, state.selectedMidiIndex + direction));
    } else if (state.uiTab == ScreenUsbTools) {
        state.selectedUsbToolIndex = std::max(0, std::min(UsbToolRowCount - 1, state.selectedUsbToolIndex + direction));
    } else if (state.uiTab == ScreenAdminTools) {
        state.selectedAdminToolIndex = std::max(0, std::min(AdminToolRowCount - 1, state.selectedAdminToolIndex + direction));
    } else if (state.uiTab == ScreenChatDetail) {
        const int maxScroll = std::max(0, chatDetailMessageCount(state) - chatDetailVisibleCount(state));
        state.scrollOffset = std::max(0, std::min(maxScroll, state.scrollOffset + (direction < 0 ? 1 : -1)));
    } else if (state.uiTab == ScreenLog || state.uiTab == ScreenDebug) {
        state.scrollOffset = std::max(0, state.scrollOffset + (direction < 0 ? 1 : -1));
    } else {
        return false;
    }
    return true;
}

bool handlePointerClick(AppState &state) {
    if (!state.pointerVisible) return false;
    const int px = state.pointerX;
    const int py = state.pointerY;
    const GuiZone &content = zone("content");
    const int contentX = pxCol(2) - 6 + content.x;
    const int contentY = pxRow(9) - 4 + content.y;
    if (pointInRect(px, py, contentX, contentY, content.w, content.h)) {
        const PullBarState pull = pullBarState(state, content.h);
        const int pullX = contentX + content.w - 23;
        const int pullY = contentY + 36;
        const int pullH = std::max(48, content.h - 70);
        if ((pull.up || pull.down) && pointInRect(px, py, pullX - 6, pullY, 30, pullH)) {
            if (py < pullY + 30 && applyPullBar(state, -1, content.h)) {
                state.dashboardFocused = true;
                state.usbDetail = "Pointer pull up";
                return true;
            }
            if (py >= pullY + pullH - 30 && applyPullBar(state, 1, content.h)) {
                state.dashboardFocused = true;
                state.usbDetail = "Pointer pull down";
                return true;
            }
        }
    }
    if (handlePointerKeyboardClick(state, px, py, contentX, contentY, content.w, content.h)) {
        return true;
    }

    const GuiZone &menu = zone("menu");
    const int navX = pxCol(62) - 8 + menu.x;
    const int navY = pxRow(1) - 6 + menu.y;
    if (pointInRect(px, py, navX, navY, menu.w, menu.h)) {
        int visibleCount = 0;
        for (int i = 0; i < PrimaryTabCount; ++i) {
            if (menuTabEnabled(state, i)) ++visibleCount;
        }
        visibleCount = std::max(1, visibleCount);
        const int slotH = std::max(48, menu.h / visibleCount);
        const int slot = std::max(0, std::min(visibleCount - 1, (py - navY) / slotH));
        int visibleIndex = 0;
        for (int tab = 0; tab < PrimaryTabCount; ++tab) {
            if (!menuTabEnabled(state, tab)) continue;
            if (visibleIndex == slot) {
                state.uiTab = tab;
                state.dashboardFocused = false;
                state.scrollOffset = 0;
                state.usbDetail = "Pointer menu " + menuLabel(tab);
                return true;
            }
            ++visibleIndex;
        }
    }

    if (!pointInRect(px, py, contentX, contentY, content.w, content.h)) return false;

    if (state.uiTab == ScreenSettings) {
        const int first = settingsFirstVisibleRow(state.selectedSettingsIndex, content.h);
        const int row = (py - (contentY + 42)) / 26;
        const int selected = first + row;
        if (row >= 0 && selected >= 0 && selected < SettingsRowCount) {
            state.selectedSettingsIndex = selected;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer setting";
            activateSettingsSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenScreensaverSettings) {
        const int row = (py - (contentY + 48)) / 44;
        if (row >= 0 && row <= 2) {
            state.selectedScreensaverIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer screensaver row";
            activateScreensaverSettingsSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenFontSettings) {
        const int row = (py - (contentY + 48)) / 48;
        if (row >= 0 && row <= 2) {
            state.selectedFontIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer font row";
            activateFontSettingsSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenGuiOptions) {
        const int row = (py - (contentY + 42)) / 31;
        if (row >= 0 && row < PrimaryTabCount + 3) {
            state.selectedGuiOptionIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer GUI option";
            activateGuiOptionSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenUsbTools) {
        const int row = (py - (contentY + 62)) / UsbToolRowH;
        if (row >= 0 && row < UsbToolRowCount) {
            state.selectedUsbToolIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer USB tool";
            activateUsbToolSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenAdminTools) {
        const int row = (py - (contentY + 62)) / AdminToolRowH;
        if (row >= 0 && row < AdminToolRowCount) {
            state.selectedAdminToolIndex = row;
            state.dashboardFocused = true;
            state.usbDetail = "Pointer admin action";
            activateAdminToolSelection(state);
            return true;
        }
    } else if (state.uiTab == ScreenChannels) {
        const int first = std::max(0, state.selectedChannelIndex - 2);
        const int rowH = 35;
        const int gap = 6;
        const int listTop = contentY + 68;
        const int row = (py - listTop) / (rowH + gap);
        const int index = first + row;
        if (row >= 0 && index >= 0 && index < 8) {
            const int rowY = listTop + row * (rowH + gap);
            if (py >= rowY && py < rowY + rowH) {
                bool configured = false;
                const ChannelSummary ch = channelSlot(state, index, &configured);
                const bool fallbackPrimary = index == 0 && !state.channelName.empty();
                const bool enabled = ch.role != 0 || fallbackPrimary;
                state.selectedChannelIndex = index;
                state.dashboardFocused = true;
                state.usbDetail = enabled ? "Pointer channel chat" : "Pointer disabled channel";
                if (enabled) {
                    ChatEntry entry;
                    entry.channel = true;
                    entry.channelIndex = index;
                    entry.title = channelDisplayName(state, index);
                    openChatEntry(state, entry);
                }
                return true;
            }
        }
    } else if (state.uiTab == ScreenChats) {
        const std::vector<ChatEntry> entries = chatEntries(state);
        if (!entries.empty()) {
            const int rowH = 74;
            const int gap = 8;
            const int listTop = contentY + 68;
            const int listBottom = contentY + content.h - 34;
            const int visibleRows = std::max(1, (listBottom - listTop + gap) / (rowH + gap));
            const int unreadIndex = newestUnreadChatIndex(entries);
            const int selected = unreadIndex >= 0 ? unreadIndex :
                std::max(0, std::min(state.selectedChatIndex, static_cast<int>(entries.size()) - 1));
            int first = selected - visibleRows / 2;
            first = std::max(0, std::min(first, std::max(0, static_cast<int>(entries.size()) - visibleRows)));
            const int row = (py - listTop) / (rowH + gap);
            const int index = first + row;
            if (row >= 0 && index >= 0 && index < static_cast<int>(entries.size())) {
                const int rowY = listTop + row * (rowH + gap);
                if (py >= rowY && py < rowY + rowH) {
                    state.selectedChatIndex = index;
                    state.dashboardFocused = true;
                    state.usbDetail = "Pointer chat";
                    openChatEntry(state, entries[static_cast<size_t>(index)]);
                    return true;
                }
            }
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
    std::fprintf(f, "\n# WiiMesh GUI options preserved across DOL updates\n");
    for (int i = 0; i < PrimaryTabCount; ++i) {
        std::fprintf(f, "menu.%s.enabled=%d\n", menuConfigName(i), state.menuEnabled[i] ? 1 : 0);
    }
    std::fprintf(f, "ui.font.style=%d\n", state.fontStyle);
    std::fprintf(f, "ui.font.size=%d\n", state.fontSize);
    std::fprintf(f, "ui.screensaver.mode=%d\n", state.screensaverMode);
    std::fprintf(f, "ui.screensaver.speed=%d\n", state.screensaverSpeed);
    std::fprintf(f, "ui.debug.enabled=%d\n", state.debugEnabled ? 1 : 0);
    std::fprintf(f, "ui.pointer.enabled=%d\n", state.pointerEnabled ? 1 : 0);
    std::fprintf(f, "ui.midi.repeat=%d\n", state.midiRepeat ? 1 : 0);
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

bool parseBoolInt(int value) {
    return value != 0;
}

int menuIndexFromConfigName(const char *name) {
    for (int i = 0; i < PrimaryTabCount; ++i) {
        if (std::strcmp(name, menuConfigName(i)) == 0) return i;
    }
    return -1;
}

void applyGuiConfigLine(AppState *state, const char *line) {
    char name[32] = {};
    char field[32] = {};
    int value = 0;
    char section[32] = {};
    char sub[32] = {};
    if (std::sscanf(line, "menu.%31[^.].enabled=%d", name, &value) == 2) {
        if (state) {
            const int index = menuIndexFromConfigName(name);
            if (index >= 0) state->menuEnabled[index] = parseBoolInt(value);
        }
        return;
    }
    if (std::sscanf(line, "ui.%31[^.].%31[^=]=%d", section, sub, &value) == 3) {
        if (!state) return;
        if (std::strcmp(section, "font") == 0 && std::strcmp(sub, "style") == 0) {
            state->fontStyle = std::max(0, std::min(FontStyleCount - 1, value));
        } else if (std::strcmp(section, "font") == 0 && std::strcmp(sub, "size") == 0) {
            state->fontSize = std::max(0, std::min(FontSizeMax, value));
        } else if (std::strcmp(section, "screensaver") == 0 && std::strcmp(sub, "mode") == 0) {
            state->screensaverMode = std::max(0, std::min(2, value));
        } else if (std::strcmp(section, "screensaver") == 0 && std::strcmp(sub, "speed") == 0) {
            state->screensaverSpeed = std::max(0, std::min(5, value));
        } else if (std::strcmp(section, "debug") == 0 && std::strcmp(sub, "enabled") == 0) {
            state->debugEnabled = parseBoolInt(value);
        } else if (std::strcmp(section, "pointer") == 0 && std::strcmp(sub, "enabled") == 0) {
            state->pointerEnabled = parseBoolInt(value);
        } else if (std::strcmp(section, "midi") == 0 && std::strcmp(sub, "repeat") == 0) {
            state->midiRepeat = parseBoolInt(value);
        }
        return;
    }
    if (std::sscanf(line, "%31[^.].%31[^=]=%d", name, field, &value) != 3) return;
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

void loadGuiConfig(AppState *state = nullptr) {
#if defined(WIIMESH_WII)
    FILE *f = std::fopen(GuiConfigPath, "rb");
#else
    FILE *f = std::fopen("GUI.config", "rb");
#endif
    if (!f) return;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        applyGuiConfigLine(state, line);
    }
    std::fclose(f);
    if (state) {
        bool any = false;
        for (int i = 0; i < PrimaryTabCount; ++i) any = any || state->menuEnabled[i];
        if (!any) state->menuEnabled[ScreenSettings] = true;
    }
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

std::string stripIconWords(std::string s) {
    const char *tokens[] = {
        "[icon]", "[home]", "[gear]", "[warn]", "[hill]", "[site]", "[chat]",
        "[map]", "[radio]", "[signal]", "[battery]", "[key]", "[lock]",
        "[nodes]", "[mobile]"
    };
    for (const char *token : tokens) {
        size_t pos = 0;
        while ((pos = s.find(token, pos)) != std::string::npos) {
            s.erase(pos, std::strlen(token));
            if (pos < s.size() && s[pos] == ' ') s.erase(pos, 1);
            if (pos > 0 && s[pos - 1] == ' ') --pos;
        }
    }
    std::string compact;
    bool lastSpace = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!compact.empty() && !lastSpace) compact.push_back(' ');
            lastSpace = true;
        } else {
            compact.push_back(c);
            lastSpace = false;
        }
    }
    while (!compact.empty() && compact.back() == ' ') compact.pop_back();
    return compact;
}

std::string displayTextWithoutIconWords(const std::string &in) {
    return stripIconWords(cleanText(in));
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
    return timeText(currentUnixTime());
}

std::string nowDateText() {
    const time_t tt = static_cast<time_t>(currentUnixTime());
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
    const uint32_t now = currentUnixTime();
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

std::string nodeBestName(const NodeSummary &node);
std::string nodeNameForId(const AppState &state, uint32_t id, const std::string &fallback);
bool looksLikeNodeIdText(const std::string &text);
uint32_t parseNodeIdText(const std::string &text);

std::string senderText(const Message &m) {
    if (!m.senderName.empty()) {
        const std::string name = displayTextWithoutIconWords(m.senderName);
        if (!name.empty()) return name;
    }
    if (!m.senderId.empty()) return m.senderId;
    return nodeId(m.from);
}

std::string senderText(const AppState &state, const Message &m) {
    const uint32_t senderIdValue = parseNodeIdText(m.senderId);
    const uint32_t senderNameValue = parseNodeIdText(m.senderName);
    const uint32_t candidates[] = {
        m.from,
        senderIdValue,
        senderNameValue,
        (m.to != BroadcastNode ? m.to : 0)
    };
    for (uint32_t id : candidates) {
        if (id == 0) continue;
        if (!state.myNodeId.empty() && nodeId(id) == state.myNodeId && m.outgoing) continue;
        const std::string known = nodeNameForId(state, id, "");
        if (!known.empty() && !looksLikeNodeIdText(known)) return known;
    }
    if (m.outgoing) return "Me";
    if (!m.senderName.empty()) {
        const std::string name = displayTextWithoutIconWords(m.senderName);
        if (!name.empty() && !looksLikeNodeIdText(name)) return name;
    }
    if (m.from != 0) return nodeId(m.from);
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
    if (m.text.rfind("[START] ", 0) == 0) {
        return "[FILE] start " + cleanText(m.text.substr(8));
    }
    if (m.text.rfind("[CHUNK] ", 0) == 0) {
        std::istringstream parts(m.text.substr(8));
        std::string chunkInfo;
        std::string filename;
        if (parts >> chunkInfo >> filename) {
            return "[FILE] " + cleanText(filename) + " chunk " + cleanText(chunkInfo);
        }
        return "[FILE] chunk";
    }
    if (m.text.rfind("[END] ", 0) == 0) {
        return "[FILE] end " + cleanText(m.text.substr(6));
    }
    std::string text = m.text;
    if (m.outgoing && text.rfind("[sent] ", 0) == 0) {
        text = text.substr(7);
    }
    const std::string display = displayTextWithoutIconWords(text);
    return display.empty() ? cleanText(text) : display;
}

bool isIncomingMessage(const Message &m) {
    return !m.outgoing && m.from != 0;
}

uint32_t unreadIncomingCount(const AppState &state) {
    uint32_t count = 0;
    for (const Message &m : state.messages) {
        if (isIncomingMessage(m) && !m.seen) ++count;
    }
    return count;
}

bool messageIsUnreadIncoming(const Message &m) {
    return isIncomingMessage(m) && !m.seen;
}

std::string tickerText(const AppState &state) {
    if (state.messages.empty()) return "ACCESS: waiting for Meshtastic messages";
    const Message &m = state.messages.back();
    return "ACCESS: " + senderText(state, m) + " - " + messageText(m);
}

std::string meshFileStatusText(const AppState &state) {
    if (state.meshFiles.empty()) return "FILES none";
    const MeshFileTransfer &transfer = state.meshFiles.back();
    std::string status = transfer.saved ? "saved" : (transfer.complete ? "complete" : "receiving");
    if (transfer.totalChunks > 0) {
        status += " " + std::to_string(transfer.receivedChunks) + "/" +
                  std::to_string(transfer.totalChunks);
    }
    return "FILES " + transfer.filename + " " + status;
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

const char *menuConfigName(int tab) {
    switch (tab) {
    case ScreenHome: return "home";
    case ScreenNodeOptions: return "node";
    case ScreenChannels: return "channels";
    case ScreenChats: return "chats";
    case ScreenMap: return "map";
    case ScreenSettings: return "settings";
    default: return "unknown";
    }
}

bool menuTabEnabled(const AppState &state, int tab) {
    return tab >= 0 && tab < PrimaryTabCount && state.menuEnabled[tab];
}

int firstEnabledPrimaryTab(const AppState &state) {
    for (int i = 0; i < PrimaryTabCount; ++i) {
        if (menuTabEnabled(state, i)) return i;
    }
    return ScreenSettings;
}

int nextEnabledPrimaryTab(const AppState &state, int tab, int dir) {
    for (int step = 1; step <= PrimaryTabCount; ++step) {
        const int next = (tab + dir * step + PrimaryTabCount * 4) % PrimaryTabCount;
        if (menuTabEnabled(state, next)) return next;
    }
    return firstEnabledPrimaryTab(state);
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
    if (state.uiTab == ScreenScreensaverSettings || state.uiTab == ScreenFontSettings ||
        state.uiTab == ScreenFontDebug || state.uiTab == ScreenMidiPlayer ||
        state.uiTab == ScreenUsbTools ||
        state.uiTab == ScreenAdminTools ||
        state.uiTab == ScreenGuiOptions) {
        return ScreenSettings;
    }
    if (state.uiTab < PrimaryTabCount && menuTabEnabled(state, state.uiTab)) return state.uiTab;
    return firstEnabledPrimaryTab(state);
}

std::string normalizeKeyboardWord(const std::string &word) {
    std::string out;
    for (char c : cleanText(word)) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '\'' || c == '-' || c == '?') out.push_back(c);
    }
    while (!out.empty() && !std::isalnum(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
    while (!out.empty() && !std::isalnum(static_cast<unsigned char>(out.back())) && out.back() != '?') out.pop_back();
    return out;
}

bool shouldSkipKeyboardWord(const std::string &word) {
    const std::string lowered = lowerText(word);
    static const char *skip[] = {
        "text", "hidden", "payload", "routing", "packet", "received", "sent",
        "message", "direct", "channel", "fromradio", "app", "seen"
    };
    for (const char *s : skip) {
        if (lowered == s) return true;
    }
    return false;
}

void pushKeyboardWord(std::vector<std::string> &out, const std::string &word, int maxWords) {
    const std::string cleaned = normalizeKeyboardWord(word);
    if (cleaned.size() < 2 || cleaned.size() > 16 || shouldSkipKeyboardWord(cleaned)) return;
    out.push_back(cleaned);
    while (static_cast<int>(out.size()) > maxWords) out.erase(out.begin());
}

void addWordCount(std::vector<std::pair<std::string, int>> &counts, const std::string &word) {
    const std::string cleaned = normalizeKeyboardWord(word);
    if (cleaned.size() < 2 || cleaned.size() > 16 || shouldSkipKeyboardWord(cleaned)) return;
    const std::string lowered = lowerText(cleaned);
    for (auto &entry : counts) {
        if (lowerText(entry.first) == lowered) {
            ++entry.second;
            return;
        }
    }
    counts.push_back({cleaned, 1});
}

void addWordCountsFromText(std::vector<std::pair<std::string, int>> &counts, const std::string &text) {
    std::istringstream words(text);
    std::string word;
    while (words >> word) addWordCount(counts, word);
}

std::vector<std::string> topKeyboardWords(const std::vector<std::pair<std::string, int>> &counts, int maxWords) {
    std::vector<std::pair<std::string, int>> ranked = counts;
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
        if (a.second != b.second) return a.second > b.second;
        return lowerText(a.first) < lowerText(b.first);
    });
    std::vector<std::string> out;
    for (const auto &entry : ranked) {
        if (entry.second < 2 && static_cast<int>(out.size()) >= 3) continue;
        out.push_back(entry.first);
        if (static_cast<int>(out.size()) >= maxWords) break;
    }
    if (static_cast<int>(out.size()) > maxWords) out.resize(static_cast<size_t>(maxWords));
    return out;
}

void addRecentKeyboardWords(AppState &state, const std::string &text) {
    std::istringstream words(text);
    std::string word;
    while (words >> word) pushKeyboardWord(state.recentKeyboardWords, word, 200);
}

std::vector<std::string> recentKeyboardWordsFromMessages(const AppState &state) {
    std::vector<std::pair<std::string, int>> counts;
    for (const std::string &word : state.recentKeyboardWords) {
        addWordCount(counts, word);
    }
    const int first = std::max(0, static_cast<int>(state.messages.size()) - 100);
    for (int i = first; i < static_cast<int>(state.messages.size()); ++i) {
        addWordCountsFromText(counts, messageText(state.messages[static_cast<size_t>(i)]));
    }
    return topKeyboardWords(counts, KeyboardSuggestionCount);
}

std::string keyboardModeName(KeyboardMode mode) {
    switch (mode) {
    case KeyboardMode::Lowercase: return "abc";
    case KeyboardMode::Uppercase: return "ABC";
    case KeyboardMode::Symbols: return "123";
    case KeyboardMode::UnicodeBadges: return "Emoji";
    }
    return "abc";
}

std::string keyboardNextModeName(KeyboardMode mode) {
    switch (mode) {
    case KeyboardMode::Lowercase: return "ABC";
    case KeyboardMode::Uppercase: return "123";
    case KeyboardMode::Symbols: return "Emoji";
    case KeyboardMode::UnicodeBadges: return "abc";
    }
    return "ABC";
}

KeyboardMode nextKeyboardMode(KeyboardMode mode) {
    switch (mode) {
    case KeyboardMode::Lowercase: return KeyboardMode::Uppercase;
    case KeyboardMode::Uppercase: return KeyboardMode::Symbols;
    case KeyboardMode::Symbols: return KeyboardMode::UnicodeBadges;
    case KeyboardMode::UnicodeBadges: return KeyboardMode::Lowercase;
    }
    return KeyboardMode::Lowercase;
}

std::string utf8FromCodepoint(uint32_t cp) {
    std::string out;
    if (cp <= 0x7f) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | ((cp >> 6) & 0x1f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | ((cp >> 12) & 0x0f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
    return out;
}

std::string utf8FromCodepoints(const uint32_t *cps, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) {
        if (cps[i] == 0) break;
        out += utf8FromCodepoint(cps[i]);
    }
    return out;
}

KeyboardKey makeKey(KeyboardKeyType type, const std::string &label, const std::string &value = "",
                    uint32_t cp = 0, int span = 1) {
    KeyboardKey key;
    key.type = type;
    key.label = label;
    key.value = value.empty() ? label : value;
    key.emojiCodepoint = cp;
    key.span = std::max(1, span);
    return key;
}

KeyboardKey unicodeBadgeKey(const std::string &fallbackLabel, uint32_t cp) {
    KeyboardKey key = makeKey(KeyboardKeyType::UnicodeBadge, fallbackLabel, utf8FromCodepoint(cp), cp);
    key.emojiCodepoints.push_back(cp);
    return key;
}

KeyboardKey unicodeEmojiKey(const emoji::Icon &icon) {
    KeyboardKey key = makeKey(KeyboardKeyType::UnicodeBadge, "emoji",
                              utf8FromCodepoints(icon.codepoints, icon.length),
                              icon.length == 1 ? icon.codepoints[0] : 0);
    for (int i = 0; i < icon.length && i < emoji::MaxCodepoints; ++i) {
        key.emojiCodepoints.push_back(icon.codepoints[i]);
    }
    return key;
}

KeyboardKey emptyKey() {
    return KeyboardKey{};
}

void appendKeyRow(std::vector<KeyboardKey> &keys, const std::vector<KeyboardKey> &row) {
    int cols = 0;
    for (KeyboardKey key : row) {
        key.span = std::max(1, std::min(KeyboardCols, key.span));
        if (cols + key.span > KeyboardCols) break;
        keys.push_back(key);
        ++cols;
        for (int i = 1; i < key.span; ++i) {
            keys.push_back(emptyKey());
            ++cols;
        }
    }
    while (cols++ < KeyboardCols) keys.push_back(emptyKey());
}

std::vector<KeyboardKey> keyboardKeys(const AppState *state) {
    std::vector<KeyboardKey> keys;
    const KeyboardMode mode = state ? state->keyboardMode : KeyboardMode::Lowercase;
    if (state) {
        for (const std::string &word : recentKeyboardWordsFromMessages(*state)) {
            keys.push_back(makeKey(KeyboardKeyType::Suggestion, word, word));
            if (static_cast<int>(keys.size()) >= KeyboardSuggestionCount) break;
        }
    }
    while (static_cast<int>(keys.size()) < KeyboardSuggestionCount) keys.push_back(emptyKey());
    while ((static_cast<int>(keys.size()) % KeyboardCols) != 0) keys.push_back(emptyKey());

    if (mode == KeyboardMode::UnicodeBadges) {
        std::vector<KeyboardKey> badgeKeys;
        badgeKeys.push_back(unicodeBadgeKey("radio", 0x1f4fb));
        for (int i = 0; i < emoji::IconCount; ++i) {
            if (emoji::Icons[i].length == 1 && emoji::Icons[i].codepoints[0] == 0x1f4fb) continue;
            badgeKeys.push_back(unicodeEmojiKey(emoji::Icons[i]));
        }
        for (size_t i = 0; i < badgeKeys.size(); i += KeyboardCols) {
            std::vector<KeyboardKey> row;
            for (size_t j = i; j < badgeKeys.size() && j < i + KeyboardCols; ++j) {
                row.push_back(badgeKeys[j]);
            }
            appendKeyRow(keys, row);
        }
    } else if (mode == KeyboardMode::Symbols) {
        const char *rows[] = {
            "1234567890",
            "!@#$%^&*()",
            "-_+=/\\|[]{}",
            ".,?:;'\"<>~`",
        };
        for (const char *row : rows) {
            std::vector<KeyboardKey> out;
            for (const char *p = row; *p; ++p) out.push_back(makeKey(KeyboardKeyType::Character, std::string(1, *p)));
            appendKeyRow(keys, out);
        }
    } else {
        const bool upper = mode == KeyboardMode::Uppercase ||
                           (state && (state->keyboardShiftActive || state->keyboardCapsLockActive));
        const char *rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
        for (int r = 0; r < 3; ++r) {
            std::vector<KeyboardKey> row;
            if (r > 0) row.push_back(emptyKey());
            for (const char *p = rows[r]; *p; ++p) {
                char c = upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(*p))) : *p;
                row.push_back(makeKey(KeyboardKeyType::Character, std::string(1, c)));
            }
            appendKeyRow(keys, row);
        }
    }
    appendKeyRow(keys, {
        makeKey(KeyboardKeyType::Shift, "Shift"),
        makeKey(KeyboardKeyType::Caps, "Caps"),
        makeKey(KeyboardKeyType::Mode, keyboardNextModeName(mode)),
        makeKey(KeyboardKeyType::Left, "<"),
        makeKey(KeyboardKeyType::Space, "Space", " ", 0, 3),
        makeKey(KeyboardKeyType::Right, ">"),
        makeKey(KeyboardKeyType::Backspace, "Back"),
        makeKey(KeyboardKeyType::Enter, "Enter"),
    });
    return keys;
}

bool keyboardKeyUsable(const std::vector<KeyboardKey> &keys, int index) {
    return index >= 0 && index < static_cast<int>(keys.size()) &&
           keys[static_cast<size_t>(index)].type != KeyboardKeyType::Empty;
}

void normalizeKeyboardCursor(AppState &state) {
    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    if (keys.empty()) {
        state.keyboardCursor = 0;
        return;
    }
    state.keyboardCursor = std::max(0, std::min(state.keyboardCursor, static_cast<int>(keys.size()) - 1));
    if (keyboardKeyUsable(keys, state.keyboardCursor)) return;
    for (int radius = 1; radius < static_cast<int>(keys.size()); ++radius) {
        if (keyboardKeyUsable(keys, state.keyboardCursor + radius)) {
            state.keyboardCursor += radius;
            return;
        }
        if (keyboardKeyUsable(keys, state.keyboardCursor - radius)) {
            state.keyboardCursor -= radius;
            return;
        }
    }
}

void moveKeyboardCursor(AppState &state, int delta) {
    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    int next = state.keyboardCursor + delta;
    while (next >= 0 && next < static_cast<int>(keys.size())) {
        if (keyboardKeyUsable(keys, next)) {
            state.keyboardCursor = next;
            return;
        }
        next += delta > 0 ? 1 : -1;
    }
}

int previousUtf8CharacterStart(const std::string &text, size_t cursorPosition) {
    if (cursorPosition == 0 || text.empty()) return 0;
    size_t pos = std::min(cursorPosition, text.size()) - 1;
    while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) --pos;
    return static_cast<int>(pos);
}

size_t nextUtf8CharacterEnd(const std::string &text, size_t cursorPosition) {
    if (cursorPosition >= text.size()) return text.size();
    const unsigned char c = static_cast<unsigned char>(text[cursorPosition]);
    size_t advance = 1;
    if ((c & 0xe0) == 0xc0) advance = 2;
    else if ((c & 0xf0) == 0xe0) advance = 3;
    else if ((c & 0xf8) == 0xf0) advance = 4;
    return std::min(text.size(), cursorPosition + advance);
}

void clampState(AppState &state) {
    if (state.uiTab < 0 || state.uiTab >= ScreenCount) state.uiTab = ScreenHome;
    bool anyMenu = false;
    for (int i = 0; i < PrimaryTabCount; ++i) anyMenu = anyMenu || state.menuEnabled[i];
    if (!anyMenu) state.menuEnabled[ScreenSettings] = true;
    if (state.uiTab < PrimaryTabCount && !menuTabEnabled(state, state.uiTab)) {
        state.uiTab = firstEnabledPrimaryTab(state);
    }
    if (state.selectedNodeIndex < 0) state.selectedNodeIndex = 0;
    if (state.selectedNodeIndex >= static_cast<int>(state.knownNodes.size())) {
        state.selectedNodeIndex = state.knownNodes.empty() ? 0 : static_cast<int>(state.knownNodes.size()) - 1;
    }
    if (state.nodeOptionTab < 0 || state.nodeOptionTab > 1) state.nodeOptionTab = 0;
    if (state.selectedSettingsIndex < 0) state.selectedSettingsIndex = 0;
    if (state.selectedSettingsIndex >= SettingsRowCount) state.selectedSettingsIndex = SettingsRowCount - 1;
    if (state.selectedGuiOptionIndex < 0) state.selectedGuiOptionIndex = 0;
    if (state.selectedGuiOptionIndex > 8) state.selectedGuiOptionIndex = 8;
    if (state.selectedScreensaverIndex < 0) state.selectedScreensaverIndex = 0;
    if (state.selectedScreensaverIndex > 2) state.selectedScreensaverIndex = 2;
    if (state.selectedFontIndex < 0) state.selectedFontIndex = 0;
    if (state.selectedFontIndex > 2) state.selectedFontIndex = 2;
    if (state.selectedMidiIndex < 0) state.selectedMidiIndex = 0;
    if (state.selectedMidiIndex >= static_cast<int>(state.midiFiles.size())) {
        state.selectedMidiIndex = state.midiFiles.empty() ? 0 : static_cast<int>(state.midiFiles.size()) - 1;
    }
    state.screensaverMode = ((state.screensaverMode % 3) + 3) % 3;
    if (state.screensaverSpeed < 0) state.screensaverSpeed = 0;
    if (state.screensaverSpeed > 5) state.screensaverSpeed = 5;
    state.fontStyle = ((state.fontStyle % FontStyleCount) + FontStyleCount) % FontStyleCount;
    if (state.fontSize < 0) state.fontSize = 0;
    if (state.fontSize > FontSizeMax) state.fontSize = FontSizeMax;
    if (state.selectedUsbToolIndex < 0) state.selectedUsbToolIndex = 0;
    if (state.selectedUsbToolIndex >= UsbToolRowCount) state.selectedUsbToolIndex = UsbToolRowCount - 1;
    if (state.selectedAdminToolIndex < 0) state.selectedAdminToolIndex = 0;
    if (state.selectedAdminToolIndex >= AdminToolRowCount) state.selectedAdminToolIndex = AdminToolRowCount - 1;
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
    const int keyCount = static_cast<int>(keyboardKeys(&state).size());
    if (state.keyboardCursor < 0) state.keyboardCursor = 0;
    if (state.keyboardCursor >= keyCount) state.keyboardCursor = keyCount - 1;
    normalizeKeyboardCursor(state);
}

std::string nodeNameForId(const AppState &state, uint32_t id, const std::string &fallback);
std::string nodeBestName(const NodeSummary &node);
std::string selfDisplayName(const AppState &state);
std::string channelDisplayName(const AppState &state, int index);
std::string directChatTitle(const AppState &state, uint32_t peer, const Message &m);
uint32_t directPeerForMessage(const AppState &state, const Message &m);

void setChatPeerFromMessage(AppState &state, const Message &m) {
    if (!m.direct) {
        state.chatChannelIndex = m.channelIndex;
        state.chatPeerNodeId.clear();
        state.chatPeerName = m.channelName.empty()
            ? (m.channelIndex == 0 ? state.channelName : ("Channel " + std::to_string(m.channelIndex)))
            : m.channelName;
    } else {
        state.chatChannelIndex = -1;
        const uint32_t peer = directPeerForMessage(state, m);
        state.chatPeerNodeId = nodeId(peer);
        state.chatPeerName = directChatTitle(state, peer, m);
    }
}

void startCompose(AppState &state) {
    state.composingMessage = true;
    state.composeText.clear();
    state.composeCursorPosition = 0;
    state.keyboardMode = KeyboardMode::Lowercase;
    state.keyboardShiftActive = false;
    state.keyboardCursor = 0;
}

void clampComposeCursor(AppState &state) {
    if (state.composeCursorPosition > state.composeText.size()) {
        state.composeCursorPosition = state.composeText.size();
    }
}

void insertAtCursor(AppState &state, const std::string &value) {
    clampComposeCursor(state);
    state.composeText.insert(state.composeCursorPosition, value);
    state.composeCursorPosition += value.size();
}

bool isBadgeTokenAt(const std::string &text, size_t start, size_t *endOut = nullptr) {
    if (start >= text.size() || text[start] != ':') return false;
    const size_t end = text.find(':', start + 1);
    if (end == std::string::npos || end == start + 1 || end - start > 32) return false;
    for (size_t i = start + 1; i < end; ++i) {
        const char c = text[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') continue;
        return false;
    }
    if (endOut) *endOut = end + 1;
    return true;
}

void insertCharacter(AppState &state, const std::string &value) {
    insertAtCursor(state, value);
    if (state.keyboardShiftActive && !state.keyboardCapsLockActive && value.size() == 1 &&
        std::isalpha(static_cast<unsigned char>(value[0]))) {
        state.keyboardShiftActive = false;
        if (state.keyboardMode == KeyboardMode::Uppercase) state.keyboardMode = KeyboardMode::Lowercase;
    }
}

void insertSpace(AppState &state) {
    insertAtCursor(state, " ");
}

void insertUnicodeBadge(AppState &state, const KeyboardKey &badge) {
    insertAtCursor(state, badge.value);
}

void applySuggestion(AppState &state, const std::string &suggestion) {
    clampComposeCursor(state);
    size_t begin = state.composeCursorPosition;
    while (begin > 0) {
        const char c = state.composeText[begin - 1];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '\'' && c != '-') break;
        --begin;
    }
    size_t end = state.composeCursorPosition;
    while (end < state.composeText.size()) {
        const char c = state.composeText[end];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '\'' && c != '-') break;
        ++end;
    }
    state.composeText.replace(begin, end - begin, suggestion + " ");
    state.composeCursorPosition = begin + suggestion.size() + 1;
    pushKeyboardWord(state.recentKeyboardWords, suggestion, 200);
}

void backspaceCompose(AppState &state) {
    clampComposeCursor(state);
    if (state.composeCursorPosition == 0 || state.composeText.empty()) return;
    size_t eraseBegin = static_cast<size_t>(previousUtf8CharacterStart(state.composeText, state.composeCursorPosition));
    size_t tokenStart = state.composeText.rfind(':', state.composeCursorPosition - 1);
    if (tokenStart != std::string::npos) {
        size_t tokenEnd = 0;
        if (isBadgeTokenAt(state.composeText, tokenStart, &tokenEnd) && tokenEnd == state.composeCursorPosition) {
            eraseBegin = tokenStart;
        }
    }
    state.composeText.erase(eraseBegin, state.composeCursorPosition - eraseBegin);
    state.composeCursorPosition = eraseBegin;
}

void moveComposeCursorLeft(AppState &state) {
    clampComposeCursor(state);
    if (state.composeCursorPosition == 0) return;
    size_t tokenStart = state.composeText.rfind(':', state.composeCursorPosition - 1);
    if (tokenStart != std::string::npos) {
        size_t tokenEnd = 0;
        if (isBadgeTokenAt(state.composeText, tokenStart, &tokenEnd) && tokenEnd == state.composeCursorPosition) {
            state.composeCursorPosition = tokenStart;
            return;
        }
    }
    state.composeCursorPosition = static_cast<size_t>(previousUtf8CharacterStart(state.composeText, state.composeCursorPosition));
}

void moveComposeCursorRight(AppState &state) {
    clampComposeCursor(state);
    if (state.composeCursorPosition >= state.composeText.size()) return;
    size_t tokenEnd = 0;
    if (isBadgeTokenAt(state.composeText, state.composeCursorPosition, &tokenEnd)) {
        state.composeCursorPosition = tokenEnd;
        return;
    }
    state.composeCursorPosition = nextUtf8CharacterEnd(state.composeText, state.composeCursorPosition);
}

void pressKeyboard(AppState &state) {
    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    if (state.keyboardCursor < 0) state.keyboardCursor = 0;
    if (state.keyboardCursor >= static_cast<int>(keys.size())) state.keyboardCursor = static_cast<int>(keys.size()) - 1;
    const KeyboardKey &key = keys[static_cast<size_t>(state.keyboardCursor)];
    switch (key.type) {
    case KeyboardKeyType::Empty: return;
    case KeyboardKeyType::Character: insertCharacter(state, key.value); break;
    case KeyboardKeyType::Suggestion: applySuggestion(state, key.value); break;
    case KeyboardKeyType::UnicodeBadge: insertUnicodeBadge(state, key); break;
    case KeyboardKeyType::Mode:
        state.keyboardMode = nextKeyboardMode(state.keyboardMode);
        state.keyboardShiftActive = false;
        normalizeKeyboardCursor(state);
        break;
    case KeyboardKeyType::Shift:
        state.keyboardShiftActive = !state.keyboardShiftActive;
        state.keyboardMode = state.keyboardShiftActive ? KeyboardMode::Uppercase : KeyboardMode::Lowercase;
        break;
    case KeyboardKeyType::Caps:
        state.keyboardCapsLockActive = !state.keyboardCapsLockActive;
        state.keyboardMode = state.keyboardCapsLockActive ? KeyboardMode::Uppercase : KeyboardMode::Lowercase;
        state.keyboardShiftActive = false;
        break;
    case KeyboardKeyType::Space: insertSpace(state); break;
    case KeyboardKeyType::Backspace: backspaceCompose(state); break;
    case KeyboardKeyType::Left: moveComposeCursorLeft(state); break;
    case KeyboardKeyType::Right: moveComposeCursorRight(state); break;
    case KeyboardKeyType::Clear:
        state.composeText.clear();
        state.composeCursorPosition = 0;
        break;
    case KeyboardKeyType::Cancel:
        state.composingMessage = false;
        break;
    case KeyboardKeyType::Enter:
        if (!state.composeText.empty() && !state.chatPeerNodeId.empty()) {
            const char *id = state.chatPeerNodeId.c_str();
            if (*id == '!') ++id;
            state.pendingSendTo = static_cast<uint32_t>(std::strtoul(id, nullptr, 16));
            state.pendingSendText = state.composeText;
            addRecentKeyboardWords(state, state.composeText);
        }
        state.composingMessage = false;
        state.composeText.clear();
        state.composeCursorPosition = 0;
        break;
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

void drawTriangleUp(int cx, int cy, int r, uint16_t c) {
    for (int dy = 0; dy < r; ++dy) {
        line(cx - dy, cy + dy, cx + dy, cy + dy, c);
    }
}

void drawTriangleDown(int cx, int cy, int r, uint16_t c) {
    for (int dy = 0; dy < r; ++dy) {
        line(cx - dy, cy - dy, cx + dy, cy - dy, c);
    }
}

void drawPullBar(int x, int y, int w, int h, const PullBarState &pull) {
    if (!pull.up && !pull.down) return;
    const int barW = 18;
    const int bx = x + w - barW - 5;
    const int by = y + 36;
    const int bh = std::max(48, h - 70);
    const uint16_t cyan = rgb565(51, 215, 238);
    const uint16_t green = rgb565(77, 232, 141);
    const uint16_t muted = rgb565(45, 70, 78);
    fillRect(bx, by, barW, bh, rgb565(8, 20, 28), 210);
    strokeRect(bx, by, barW, bh, cyan);
    fillRect(bx + 4, by + 30, barW - 8, std::max(1, bh - 60), rgb565(17, 68, 81), 230);
    drawTriangleUp(bx + barW / 2, by + 9, 6, pull.up ? green : muted);
    drawTriangleDown(bx + barW / 2, by + bh - 9, 6, pull.down ? green : muted);
}

void drawDeliveryCheck(int x, int y, uint16_t c) {
    line(x, y + 6, x + 4, y + 10, c);
    line(x + 1, y + 6, x + 5, y + 10, c);
    line(x + 4, y + 10, x + 14, y, c);
    line(x + 5, y + 10, x + 15, y, c);
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

[[maybe_unused]] const uint8_t *tinyGlyph(char ch) {
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

int roundedFontSizeIndex(int scale) {
    const int bump = std::max(0, scale - 1) * 2;
    return std::max(0, std::min(gUiFontSize + bump, rounded_font::SizeCount - 1));
}

int roundedGlyphIndex(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch < rounded_font::FirstChar || uch > rounded_font::LastChar) return '?' - rounded_font::FirstChar;
    return static_cast<int>(uch) - rounded_font::FirstChar;
}

std::string tinySafeText(const std::string &text);

void blendFontPixel(int x, int y, uint16_t c, int alpha) {
    if (alpha <= 0 || x < 0 || y < 0 || x >= SkinWidth || y >= SkinHeight) return;
    uint16_t &dst = gSkinPixels[static_cast<size_t>(y) * SkinWidth + x];
    dst = alpha >= 255 ? c : blend565(dst, c, alpha);
}

int tinyTextWidth(const std::string &text, int scale) {
    if (text.empty()) return 0;
    const int size = roundedFontSizeIndex(scale);
    int w = 0;
    for (char ch : tinySafeText(text)) {
        const int idx = size * rounded_font::GlyphCount + roundedGlyphIndex(ch);
        w += rounded_font::Advance[idx];
    }
    return w;
}

std::string tinySafeText(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : cleanText(text)) {
        if (ch >= 32 && ch <= 126) {
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
    const int size = roundedFontSizeIndex(scale);
    const int cellW = rounded_font::CellW[size];
    const int cellH = rounded_font::CellH[size];
    const uint16_t shadow = rgb565(0, 5, 8);
    const int drawY = y + 2;
    int gx = x;
    for (size_t i = 0; i < safe.size(); ++i) {
        const int idx = size * rounded_font::GlyphCount + roundedGlyphIndex(safe[i]);
        const uint32_t off = rounded_font::Offset[idx];
        if (gUiFontStyle == 1 || gUiFontStyle == 3) {
            for (int yy = 0; yy < cellH; ++yy) {
                for (int xx = 0; xx < cellW; ++xx) {
                    const int a = rounded_font::Alpha[off + yy * cellW + xx];
                    if (a) blendFontPixel(gx + xx + 1, drawY + yy + 1, shadow, std::min(160, a));
                }
            }
        }
        for (int yy = 0; yy < cellH; ++yy) {
            for (int xx = 0; xx < cellW; ++xx) {
                const int a = rounded_font::Alpha[off + yy * cellW + xx];
                if (a) {
                    blendFontPixel(gx + xx, drawY + yy, c, a);
                    if (gUiFontStyle == 2 && a > 180) blendFontPixel(gx + xx, drawY + yy, rgb565(255, 255, 255), 38);
                    if (gUiFontStyle == 3 && a > 96) blendFontPixel(gx + xx + 1, drawY + yy, c, a / 2);
                }
            }
        }
        gx += rounded_font::Advance[idx];
    }
}

void drawTinyTextFit(const std::string &text, int x, int y, int maxPx, int scale, uint16_t c) {
    drawTinyText(fitTinyText(text, maxPx, scale), x, y, scale, c);
}

void drawTinyTextFitCentered(const std::string &text, int x, int y, int maxPx, int scale, uint16_t c) {
    const std::string fitted = fitTinyText(text, maxPx, scale);
    const int textW = tinyTextWidth(fitted, scale);
    drawTinyText(fitted, x + std::max(0, (maxPx - textW) / 2), y, scale, c);
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
        if (emoji::Icons[i].length == 1 && emoji::Icons[i].codepoints[0] == cp) return &emoji::Icons[i];
    }
    return nullptr;
}

const emoji::Icon *emojiIconSequence(const std::vector<uint32_t> &cps) {
    if (cps.empty() || cps.size() > static_cast<size_t>(emoji::MaxCodepoints)) return nullptr;
    for (int i = 0; i < emoji::IconCount; ++i) {
        const emoji::Icon &icon = emoji::Icons[i];
        if (icon.length != static_cast<uint8_t>(cps.size())) continue;
        bool match = true;
        for (size_t j = 0; j < cps.size(); ++j) {
            if (icon.codepoints[j] != cps[j]) {
                match = false;
                break;
            }
        }
        if (match) return &icon;
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
    if (hasAlias(":radio:") || hasAlias(":pager:")) return 0x1f4fb;
    if (hasAlias(":satellite:")) return 0x1f4e1;
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
    const int textH = rounded_font::CellH[roundedFontSizeIndex(scale)];
    drawTinyText(fitted, x + (w - textW) / 2, y + (h - textH) / 2, scale, ink);
}

void drawUserIconGlyphs(const std::vector<uint32_t> &icons, int x, int y, int w, int h, uint16_t ink);

void drawNodeAvatar(const NodeSummary &node, int x, int y, int w, int h, bool dim = false) {
    const uint16_t accent = dim ? blend565(nodeBadgeAccent(node), rgb565(30, 35, 36), 115) : nodeBadgeAccent(node);
    const uint16_t border = dim ? rgb565(145, 152, 152) : rgb565(235, 238, 238);
    const uint16_t ink = dim ? rgb565(196, 205, 205) : rgb565(248, 250, 250);
    const std::vector<uint32_t> icons = nodeIconCodepoints(node, 3);
    if (icons.empty()) {
        drawNodeBadgeLabel(nodeBadgeLabel(node), x, y, w, h, accent, rgb565(7, 18, 21));
    } else {
        drawAvatarFrame(x, y, w, h, accent, border);
        drawUserIconGlyphs(icons, x, y, w, h, ink);
    }
}

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
        const int gap = std::max(1, scale);
        const int centerX = x + w / 2;
        const int centerY = y + h / 2;
        if (count == 2) {
            const int totalW = icon * 2;
            const int startX = centerX - totalW / 2;
            drawPixelIcon(icons[0], startX, centerY - icon / 2, scale, ink);
            drawPixelIcon(icons[1], startX + icon, centerY - icon / 2, scale, ink);
        } else {
            const int overlap = std::max(3, icon / 3);
            const int step = std::max(1, icon - overlap);
            const int startX = centerX - icon / 2 - step;
            const int rowY = centerY - icon / 2;
            drawPixelIcon(icons[0], startX, rowY - scale, scale, ink);
            drawPixelIcon(icons[1], startX + step, rowY + scale, scale, ink);
            drawPixelIcon(icons[2], startX + step * 2, rowY - scale, scale, ink);
        }
        return;
    }
    if (icons.size() == 1) {
        drawUserIconGlyph(icons[0], x, y, w, h, ink);
        return;
    }
    const int count = std::min(3, static_cast<int>(icons.size()));
    const int sub = std::max(15, std::min(22, std::min(w, h) / 2 + 2));
    const int gap = std::max(1, sub / 8);
    const int centerX = x + w / 2;
    const int centerY = y + h / 2;
    if (count == 2) {
        const int totalW = sub * 2;
        const int startX = centerX - totalW / 2;
        drawUserIconGlyph(icons[0], startX, centerY - sub / 2, sub, sub, ink);
        drawUserIconGlyph(icons[1], startX + sub, centerY - sub / 2, sub, sub, ink);
    } else {
        const int overlap = std::max(4, sub / 3);
        const int step = std::max(1, sub - overlap);
        const int startX = centerX - sub / 2 - step;
        const int rowY = centerY - sub / 2;
        drawUserIconGlyph(icons[0], startX, rowY - gap, sub, sub, ink);
        drawUserIconGlyph(icons[1], startX + step, rowY + gap, sub, sub, ink);
        drawUserIconGlyph(icons[2], startX + step * 2, rowY - gap, sub, sub, ink);
    }
}

void drawUnicodeAwareTinyTextFit(const std::string &text, int x, int y, int maxPx, int scale, uint16_t ink) {
    const int iconSize = std::max(14, rounded_font::CellH[roundedFontSizeIndex(scale)] + 2);
    int cx = x;
    std::string ascii;
    auto flushAscii = [&]() {
        if (ascii.empty()) return;
        const std::string fitted = fitTinyText(ascii, std::max(0, x + maxPx - cx), scale);
        drawTinyText(fitted, cx, y, scale, ink);
        cx += tinyTextWidth(fitted, scale);
        ascii.clear();
    };
    for (size_t i = 0; i < text.size();) {
        const size_t start = i;
        uint32_t cp = 0;
        if (!decodeNextUtf8Codepoint(text, i, cp)) {
            if (cx + tinyTextWidth(ascii + text[start], scale) >= x + maxPx) break;
            ascii.push_back(text[start]);
            continue;
        }
        if (cp >= 0x2300 && !isIgnoredIconJoiner(cp)) {
            flushAscii();
            if (cx + iconSize > x + maxPx) break;
            drawUserIconGlyphs(std::vector<uint32_t>{cp}, cx, y - 2, iconSize, iconSize, ink);
            cx += iconSize + 2;
        } else if (cp < 0x80 && std::isprint(static_cast<unsigned char>(cp))) {
            const char c = static_cast<char>(cp);
            if (cx + tinyTextWidth(ascii + c, scale) >= x + maxPx) break;
            ascii.push_back(c);
        } else if (cp == '\n' || cp == '\r' || cp == '\t') {
            if (cx + tinyTextWidth(ascii + " ", scale) >= x + maxPx) break;
            ascii.push_back(' ');
        }
    }
    flushAscii();
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
            const Message &m = state.messages[static_cast<size_t>(i)];
            if (messageIsUnreadIncoming(m)) return senderText(state, m) + ": " + messageText(m);
        }
        for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0; --i) {
            const Message &m = state.messages[static_cast<size_t>(i)];
            if (isIncomingMessage(m)) return senderText(state, m) + ": " + messageText(m);
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

std::vector<ScreensaverBadgeSeed> screensaverUserSeeds(const AppState &state) {
    std::vector<ScreensaverBadgeSeed> seeds;
    for (const auto &node : state.knownNodes) {
        if (seeds.size() >= 8) break;
        ScreensaverBadgeSeed seed;
        seed.icons = nodeIconCodepoints(node, 3);
        seed.accent = nodeBadgeAccent(node);
        seed.text = displayTextWithoutIconWords(!node.name.empty() ? node.name : node.shortName);
        if (seed.text.empty()) seed.text = nodeBadgeLabel(node);
        seed.text = cleanText(seed.text);
        if (seed.text.empty() && seed.icons.empty()) continue;
        seeds.push_back(seed);
    }
    if (seeds.empty()) {
        ScreensaverBadgeSeed seed;
        seed.text = screensaverName(state);
        seed.accent = rgb565(77, 232, 141);
        seed.icons = utf8IconCodepoints(state.nodeName + " " + state.myNodeId, 3);
        seeds.push_back(seed);
    }
    return seeds;
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
    const std::vector<ScreensaverBadgeSeed> seeds = screensaverUserSeeds(state);
    bool changed = seeds.size() != gScreensaverBadges.size();
    if (!changed) {
        for (size_t i = 0; i < seeds.size(); ++i) {
            if (gScreensaverBadges[i].text != seeds[i].text ||
                gScreensaverBadges[i].icons != seeds[i].icons ||
                gScreensaverBadges[i].accent != seeds[i].accent) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return;
    gScreensaverBadges.clear();
    const float speed = screensaverSpeedValue(state);
    for (size_t i = 0; i < seeds.size(); ++i) {
        ScreensaverBadge badge;
        badge.text = seeds[i].text;
        badge.icons = seeds[i].icons;
        badge.accent = seeds[i].accent;
        badge.w = std::min(290, std::max(128, tinyTextWidth(badge.text, 1) + 78));
        badge.h = 50;
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

void drawScreensaverUserBadge(const ScreensaverBadge &badge, size_t index) {
    const int x = static_cast<int>(badge.x);
    const int y = static_cast<int>(badge.y);
    const uint16_t edge = (index % 3 == 0) ? rgb565(77, 232, 141)
        : (index % 3 == 1 ? rgb565(51, 215, 238) : rgb565(238, 191, 75));
    drawScreensaverBox(x, y, badge.w, badge.h, edge);
    const int avatar = std::max(34, std::min(42, badge.h - 8));
    const int ax = x + 8;
    const int ay = y + (badge.h - avatar) / 2;
    if (badge.icons.empty()) {
        drawNodeBadgeLabel(badge.text.substr(0, std::min<size_t>(4, badge.text.size())),
                           ax, ay, avatar, avatar, badge.accent, rgb565(7, 18, 21));
    } else {
        drawAvatarFrame(ax, ay, avatar, avatar, badge.accent, rgb565(235, 238, 238));
        drawUserIconGlyphs(badge.icons, ax, ay, avatar, avatar, rgb565(248, 250, 250));
    }
    drawTinyTextFit(badge.text, x + avatar + 18, y + 15,
                    badge.w - avatar - 26, 1, rgb565(245, 248, 248));
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
            drawScreensaverUserBadge(gScreensaverBadges[i], i);
        }
    } else {
        const std::string name = screensaverDisplayText(state);
        updateScreensaverMotion(state, name);
        const int textChars = std::max(8, static_cast<int>(name.size()));
        const int boxW = std::min(500, std::max(180, textChars * 18 + 64));
        const int x = static_cast<int>(gScreensaverX);
        const int y = static_cast<int>(gScreensaverY);
        const int pulse = (gScreensaverFrame / 18) % 9;
        for (int i = 0; i < 9; ++i) {
            const int h = 6 + ((i + pulse) % 5) * 5;
            fillRect(x + boxW - 42 + i * 4, y + 18 + (26 - h), 3, h, rgb565(51, 215, 238), 210);
        }
    }
    gfx_tex_convert(&gSkinTexture, gSkinPixels.data());
    gfx_tex_flush_texture(&gSkinTexture);
    gfx_screen_coords_t coords = {0.0f, 0.0f, 640.0f, 480.0f};
    gfx_draw_tex(&gSkinTexture, &coords);
}

void drawSectionTitle(const std::string &title, int x, int y, int maxPx) {
    drawTinyTextFit(title, x, y, maxPx, 1, rgb565(238, 246, 246));
    fillRect(x, y + 20, std::min(maxPx, 130), 3, rgb565(77, 232, 141), 230);
}

void drawGraphicalHeader(const AppState &state, int x, int y, int w, int h) {
    const uint16_t bright = rgb565(238, 246, 246);
    const uint16_t cyan = rgb565(51, 215, 238);
    const uint16_t green = rgb565(77, 232, 141);
    const uint16_t amber = rgb565(238, 191, 75);
    panel(x, y, w, h, false);
    const int pad = 14;
    const int titleX = x + pad;
    const int statusW = 82;
    const int ipW = 142;
    const int gap = 10;
    const int ipX = x + w - pad - ipW;
    const int statusX = ipX - gap - statusW;
    const int titleW = std::max(40, statusX - titleX - gap);
    const std::string radioStatus = state.protocolReady ? "ONLINE" : (state.usbConnected ? "SYNCING" : "OFFLINE");
    const std::string wiiIpText = state.networkReady ? ("WII " + state.wiiIp) : "WII OFF";
    drawTinyTextFit("WIIMESH V" + std::string(AppVersion), titleX, y + 10, titleW, 1, bright);
    drawTinyTextFitCentered(radioStatus, statusX, y + 10, statusW, 1, state.protocolReady ? green : amber);
    drawTinyTextFit(wiiIpText, ipX, y + 10, ipW, 1, cyan);
    drawTinyTextFit(state.channelName + "  NODES " + std::to_string(state.nodeCount) +
                    "  TEXT " + std::to_string(static_cast<int>(state.messages.size())),
                    x + pad, y + 25, w - pad * 2, 1, rgb565(180, 205, 212));
    const int signalW = 72;
    drawTinyTextFit(tickerText(state) + "  ME " + selfDisplayName(state),
                    x + pad, y + 40, std::max(50, w - pad * 2 - signalW), 1, green);
    drawSignalBars(x + w - pad - 52, y + 60, cyan);
    const int barY = std::min(y + h - 8, y + 56);
    fillRect(x + 8, barY, w - 16, 5, rgb565(20, 78, 83), 255);
    fillRect(x + 8, barY, state.protocolReady ? std::max(50, w - 120) : 90, 5,
             state.protocolReady ? green : amber, 255);
}

void drawGraphicalMenu(const AppState &state, int x, int y, int w, int h) {
    const int active = activePrimaryTab(state);
    int visibleCount = 0;
    for (int i = 0; i < PrimaryTabCount; ++i) {
        if (menuTabEnabled(state, i)) ++visibleCount;
    }
    visibleCount = std::max(1, visibleCount);
    const int slotH = std::max(48, h / visibleCount);
    panel(x, y, w, h, false);
    int visibleIndex = 0;
    for (int i = 0; i < PrimaryTabCount; ++i) {
        if (!menuTabEnabled(state, i)) continue;
        const int itemY = y + visibleIndex * slotH + 3;
        const int itemH = std::max(42, std::min(slotH - 5, 58));
        const bool selected = i == active;
        panel(x + 6, itemY, w - 12, itemH, selected);
        const uint16_t ink = selected ? rgb565(255, 255, 255) : rgb565(182, 197, 202);
        const int iconSize = std::max(18, std::min(24, itemH - 26));
        const int iconX = x + (w - iconSize) / 2;
        const int iconY = itemY + 5;
        drawBitmapIcon(menuIconRows(i), iconX, iconY, iconSize, iconSize, ink);
        const std::string label = menuLabel(i);
        const int labelW = tinyTextWidth(label, 1);
        const int labelX = x + (w - labelW) / 2;
        drawTinyText(label, labelX, itemY + itemH - 22, 1, ink);
        ++visibleIndex;
    }
}

void drawHomePanel(const AppState &state, int x, int y, int w, int h) {
    const uint32_t unread = unreadIncomingCount(state);
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
    rowY += rowGap;
    drawTinyTextFit(meshFileStatusText(state) + "  MIDI " +
                    (state.midiPlaying ? ("PLAYING " + state.midiNowPlaying) : state.midiStatus),
                    x + 24, rowY, w - 48, 1, rgb565(77, 232, 141));
    rowY += rowGap;
    drawTinyTextFit("MIDI / FILES: Settings opens player and saved transfers",
                    x + 24, rowY, w - 48, 1, rgb565(180, 205, 212));
    const int midiCardY = y + h - (state.messages.empty() ? 78 : 118);
    if (midiCardY > rowY + 18) {
        fillRect(x + 14, midiCardY, w - 28, 42, rgb565(18, 39, 47), 230);
        strokeRect(x + 14, midiCardY, w - 28, 42, rgb565(35, 99, 118));
        drawTinyTextFit("MIDI PLAYER", x + 24, midiCardY + 8, 118, 1, rgb565(77, 232, 141));
        drawTinyTextFit(state.midiPlaying ? ("PLAYING " + state.midiNowPlaying) : state.midiStatus,
                        x + 148, midiCardY + 8, w - 172, 1, rgb565(235, 244, 244));
        drawTinyTextFit("2 OPEN MIDI PLAYER", x + 24, midiCardY + 25, w - 48, 1, rgb565(180, 205, 212));
    }
    if (!state.messages.empty()) {
        const Message &m = state.messages.back();
        fillRect(x + 14, y + h - 64, w - 28, 50, rgb565(18, 39, 47), 230);
        strokeRect(x + 14, y + h - 64, w - 28, 50, rgb565(35, 99, 118));
        drawTinyTextFit("LATEST", x + 24, y + h - 54, w - 48, 1, rgb565(77, 232, 141));
        drawTinyTextFit(timeText(m.rxTime) + " " + (m.direct ? "DM " : "CH ") + senderText(state, m),
                        x + 24, y + h - 38, w - 48, 1, rgb565(235, 244, 244));
        drawTinyTextFit(messageText(m), x + 24, y + h - 23, w - 48, 1, rgb565(235, 244, 244));
    }
}

void drawNodeCard(const NodeSummary &node, bool selected, int x, int y, int w, int h, const AppState &state) {
    const uint16_t edge = selected ? rgb565(77, 232, 141) : rgb565(120, 128, 130);
    const uint16_t fill = selected ? rgb565(60, 86, 88) : rgb565(50, 53, 54);
    fillRect(x, y, w, h, fill, 245);
    strokeRect(x, y, w, h, edge);
    const int avatar = std::min(76, h - 10);
    const int ax = x + 12;
    const int ay = y + (h - avatar) / 2;
    drawNodeAvatar(node, ax, ay, avatar, avatar, !selected);
    const int tx = ax + avatar + 18;
    const int rightW = 104;
    const std::string friendly = nodeBestName(node);
    const std::string idText = node.nodeId.empty() ? nodeId(node.id) : node.nodeId;
    const std::string title = friendly.empty() ? idText : friendly;
    drawTinyTextFit(title, tx, y + 12, std::max(20, w - (tx - x) - rightW), 1, rgb565(245, 248, 248));
    const int detailY = friendly.empty() ? y + 32 : y + 38;
    if (friendly.empty() && !idText.empty() && idText != title) {
        drawTinyTextFit(idText, tx, y + 32, std::max(20, w - (tx - x) - rightW), 1, rgb565(210, 222, 224));
    }
    if (!node.hardwareName.empty() && node.hardwareName != title) {
        drawTinyTextFit(node.hardwareName, tx, detailY, std::max(20, w - (tx - x) - rightW), 1, rgb565(180, 205, 212));
    }
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
    drawTinyTextFit(top, x + w - rightW + 4, y + 10, rightW - 8, 1, rgb565(235, 244, 244));
    drawTinyTextFit(mid, x + w - rightW + 4, y + 32, rightW - 8, 1, rgb565(77, 232, 141));
    drawTinyTextFit(bottom, x + w - rightW + 4, y + h - 23, rightW - 8, 1, rgb565(235, 244, 244));
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
        drawTinyTextFit("WAITING FOR NODE INFO FROM the radio", x + 22, y + 44, w - 44, 1, rgb565(235, 244, 244));
        return;
    }
    const int cardH = 86;
    const int gap = 8;
    const int listTop = y + 40;
    const int listBottom = y + h - 12;
    const int visibleRows = std::max(1, (listBottom - listTop + gap) / (cardH + gap));
    int first = state.dashboardFocused ? state.selectedNodeIndex - visibleRows / 2 : 0;
    first = std::max(0, std::min(first, std::max(0, static_cast<int>(state.knownNodes.size()) - visibleRows)));
    int cy = listTop;
    for (int slot = 0; slot < visibleRows; ++slot) {
        const int index = first + slot;
        if (index >= static_cast<int>(state.knownNodes.size()) || cy + cardH > listBottom) break;
        drawNodeCard(state.knownNodes[static_cast<size_t>(index)],
                     state.dashboardFocused && index == state.selectedNodeIndex,
                     x + 8, cy, w - 16, cardH, state);
        cy += cardH + gap;
    }
}

ChannelSummary channelSlot(const AppState &state, int index, bool *configured) {
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

std::string cleanDisplayNameCandidate(const std::string &text) {
    const std::string cleaned = displayTextWithoutIconWords(text);
    if (cleaned.empty() || cleaned == "Me" || looksLikeNodeIdText(cleaned)) return "";
    int alnum = 0;
    int noisy = 0;
    for (unsigned char c : cleaned) {
        if (std::isalnum(c)) {
            ++alnum;
        } else if (c != ' ' && c != '-' && c != '_' && c != '.' &&
                   c != '\'' && c != '&' && c != '[' && c != ']') {
            ++noisy;
        }
    }
    if (alnum < 2) return "";
    if (noisy > std::max(3, alnum / 2)) return "";
    return cleaned;
}

std::string nodeBestName(const NodeSummary &node) {
    std::string name = cleanDisplayNameCandidate(node.name);
    if (!name.empty()) return name;
    return "";
}

std::string nodeNameForId(const AppState &state, uint32_t id, const std::string &fallback) {
    for (const auto &node : state.knownNodes) {
        if (node.id == id || node.nodeId == nodeId(id)) {
            const std::string best = nodeBestName(node);
            if (!best.empty()) return best;
            if (!node.nodeId.empty()) return node.nodeId;
        }
    }
    const std::string cleanedFallback = cleanDisplayNameCandidate(fallback);
    return cleanedFallback.empty() ? nodeId(id) : cleanedFallback;
}

std::string selfDisplayName(const AppState &state) {
    for (const auto &node : state.knownNodes) {
        if (node.isMine || (!state.myNodeId.empty() && node.nodeId == state.myNodeId)) {
            const std::string best = nodeBestName(node);
            if (!best.empty()) return best;
        }
    }
    const std::string local = cleanDisplayNameCandidate(state.nodeName);
    if (!local.empty() && local != "Unknown" && local != "waiting") return local;
    return state.myNodeId.empty() ? "waiting" : state.myNodeId;
}

bool looksLikeNodeIdText(const std::string &text) {
    const size_t start = (!text.empty() && text[0] == '!') ? 1 : 0;
    const size_t digits = text.size() - start;
    if (digits != 8) return false;
    for (size_t i = start; i < text.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    return true;
}

uint32_t parseNodeIdText(const std::string &text) {
    if (!looksLikeNodeIdText(text)) return 0;
    uint32_t value = 0;
    const size_t start = (!text.empty() && text[0] == '!') ? 1 : 0;
    for (size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
    }
    return value;
}

bool isLocalNodeId(const AppState &state, uint32_t id) {
    return id != 0 && !state.myNodeId.empty() && nodeId(id) == state.myNodeId;
}

uint32_t directPeerForMessage(const AppState &state, const Message &m) {
    const uint32_t senderIdValue = parseNodeIdText(m.senderId);
    if (!m.outgoing) {
        if (m.from != 0 && !isLocalNodeId(state, m.from)) return m.from;
        if (senderIdValue != 0 && !isLocalNodeId(state, senderIdValue)) return senderIdValue;
        if (m.to != 0 && m.to != BroadcastNode && !isLocalNodeId(state, m.to)) return m.to;
    } else {
        if (m.to != 0 && m.to != BroadcastNode && !isLocalNodeId(state, m.to)) return m.to;
        if (senderIdValue != 0 && !isLocalNodeId(state, senderIdValue)) return senderIdValue;
        if (m.from != 0 && !isLocalNodeId(state, m.from)) return m.from;
    }
    if (m.from != 0) return m.from;
    if (m.to != 0 && m.to != BroadcastNode) return m.to;
    return senderIdValue;
}

std::string directChatTitle(const AppState &state, uint32_t peer, const Message &m) {
    for (const auto &node : state.knownNodes) {
        if (node.id == peer || node.nodeId == nodeId(peer) ||
            (!m.senderId.empty() && node.nodeId == m.senderId) ||
            (m.from != 0 && (node.id == m.from || node.nodeId == nodeId(m.from))) ||
            (m.to != 0 && m.to != BroadcastNode && (node.id == m.to || node.nodeId == nodeId(m.to)))) {
            const std::string best = nodeBestName(node);
            if (!best.empty()) return best;
        }
    }
    const std::string fromKnown = nodeNameForId(state, peer, "");
    if (!fromKnown.empty() && !looksLikeNodeIdText(fromKnown)) return fromKnown;
    if (!m.outgoing && m.from != 0 && !m.senderName.empty() && m.senderName != "Me" &&
        !looksLikeNodeIdText(m.senderName)) {
        const std::string sender = cleanDisplayNameCandidate(m.senderName);
        if (!sender.empty()) return sender;
    }
    return nodeId(peer);
}

bool betterChatTitle(const std::string &candidate, const std::string &current) {
    const std::string c = cleanDisplayNameCandidate(candidate);
    if (c.empty()) return false;
    if (current.empty()) return true;
    if (looksLikeNodeIdText(current) && !looksLikeNodeIdText(c)) return true;
    if (current == "Me" && c != "Me") return true;
    return false;
}

bool directMessageMatchesPeer(const AppState &state, const Message &m, const std::string &peerId) {
    if (peerId.empty()) return true;
    const std::string fromId = m.senderId.empty() ? nodeId(m.from) : m.senderId;
    const std::string rawFromId = nodeId(m.from);
    const std::string toId = nodeId(m.to);
    if (fromId == peerId || rawFromId == peerId || toId == peerId) return true;
    if (m.from == 0 && m.to != BroadcastNode && toId == peerId) return true;
    if (m.senderId == state.myNodeId && m.to != BroadcastNode && toId == peerId) return true;
    if (state.pendingSendTo != 0 && m.to == state.pendingSendTo && toId == peerId) return true;
    return false;
}

int chatDetailMessageCount(const AppState &state) {
    const bool channelChat = state.chatChannelIndex >= 0;
    int count = 0;
    for (const Message &m : state.messages) {
        if (channelChat) {
            if (!m.direct && m.channelIndex == state.chatChannelIndex) ++count;
        } else if (directMessageMatchesPeer(state, m, state.chatPeerNodeId)) {
            ++count;
        }
    }
    return count;
}

int chatDetailVisibleCount(const AppState &state) {
    return state.composingMessage ? 1 : 4;
}

std::vector<ChatEntry> chatEntries(const AppState &state) {
    std::vector<ChatEntry> entries;
    for (int i = 0; i < static_cast<int>(state.messages.size()); ++i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        if (!m.direct) continue;
        const uint32_t peer = directPeerForMessage(state, m);
        auto found = std::find_if(entries.begin(), entries.end(), [&](const ChatEntry &e) {
            return !e.channel && e.node == peer;
        });
        if (found == entries.end()) {
            ChatEntry e;
            e.channel = false;
            e.node = peer;
            e.nodeId = nodeId(peer);
            e.title = directChatTitle(state, peer, m);
            e.subtitle = "Direct message";
            e.last = messageText(m);
            e.time = m.rxTime;
            e.unread = messageIsUnreadIncoming(m);
            entries.push_back(e);
        } else {
            const std::string title = directChatTitle(state, peer, m);
            if (betterChatTitle(title, found->title)) {
                found->title = title;
            }
            found->last = messageText(m);
            found->time = m.rxTime;
            if (messageIsUnreadIncoming(m)) found->unread = true;
        }
    }

    std::sort(entries.begin(), entries.end(), [](const ChatEntry &a, const ChatEntry &b) {
        if (a.unread != b.unread) return a.unread && !b.unread;
        return a.time > b.time;
    });
    return entries;
}

int newestUnreadChatIndex(const std::vector<ChatEntry> &entries) {
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (entries[static_cast<size_t>(i)].unread) return i;
    }
    return -1;
}

void markCurrentChatSeen(AppState &state) {
    const bool channelChat = state.chatChannelIndex >= 0;
    for (Message &m : state.messages) {
        if (!isIncomingMessage(m)) continue;
        if (channelChat) {
            if (!m.direct && m.channelIndex == state.chatChannelIndex) m.seen = true;
        } else if (directMessageMatchesPeer(state, m, state.chatPeerNodeId)) {
            m.seen = true;
        }
    }
}

void openChatEntry(AppState &state, const ChatEntry &entry) {
    if (entry.channel) {
        state.chatChannelIndex = entry.channelIndex;
        state.chatPeerNodeId.clear();
        state.chatPeerName = entry.title;
    } else {
        state.chatChannelIndex = -1;
        state.chatPeerNodeId = entry.nodeId;
        state.chatPeerName = nodeNameForId(state, entry.node, entry.title);
    }
    state.uiTab = ScreenChatDetail;
    state.dashboardFocused = true;
    state.scrollOffset = 0;
    markCurrentChatSeen(state);
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
    const int unreadIndex = newestUnreadChatIndex(entries);
    const int selected = entries.empty() ? 0 :
        (unreadIndex >= 0 ? unreadIndex : std::max(0, std::min(state.selectedChatIndex, static_cast<int>(entries.size()) - 1)));
    auto findNode = [&](uint32_t id) -> const NodeSummary * {
        for (const auto &node : state.knownNodes) {
            if (node.id == id || node.nodeId == nodeId(id)) return &node;
        }
        return nullptr;
    };
    drawSectionTitle("DIRECT MESSAGES", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 30, w - 28, 28, rgb565(67, 123, 119), 255);
    drawBitmapIcon(icons::Chat, x + 22, y + 35, 18, 18, rgb565(235, 244, 244));
    drawTinyTextFit(std::to_string(static_cast<int>(entries.size())) + " DIRECT CHATS",
                    x + 48, y + 40, w - 70, 1, rgb565(245, 248, 248));

    if (entries.empty()) {
        drawUserIconGlyph(0x1f4e5, x + w / 2 - 18, y + h / 2 - 24, 36, 36, rgb565(143, 177, 188));
        drawTinyTextFit("NO DIRECT MESSAGES YET", x + 24, y + h / 2 + 18, w - 48, 1, rgb565(180, 205, 212));
        drawTinyTextFit("GROUP CHATS LIVE UNDER CHANNELS", x + 24, y + h / 2 + 38, w - 48, 1, rgb565(180, 205, 212));
    } else {
        const int rowH = 74;
        const int gap = 8;
        const int listTop = y + 68;
        const int listBottom = y + h - 34;
        const int visibleRows = std::max(1, (listBottom - listTop + gap) / (rowH + gap));
        int first = selected - visibleRows / 2;
        first = std::max(0, std::min(first, std::max(0, static_cast<int>(entries.size()) - visibleRows)));
        int rowY = y + 68;
        for (int slot = 0; slot < visibleRows; ++slot) {
            const int index = first + slot;
            if (index >= static_cast<int>(entries.size()) || rowY + rowH > y + h - 34) break;
            const ChatEntry &e = entries[static_cast<size_t>(index)];
            const bool active = index == selected;
            const uint16_t edge = e.unread ? rgb565(238, 143, 41) : (active ? rgb565(77, 232, 141) : rgb565(145, 152, 152));
            fillRect(x + 18, rowY, w - 36, rowH, active ? rgb565(48, 70, 72) : rgb565(54, 54, 54), 245);
            strokeRect(x + 18, rowY, w - 36, rowH, edge);
            NodeSummary pseudo;
            if (const NodeSummary *known = findNode(e.node)) {
                pseudo = *known;
            } else {
                pseudo.id = e.node;
                pseudo.nodeId = e.nodeId;
                pseudo.name = e.title;
            }
            const int badge = 52;
            drawNodeAvatar(pseudo, x + 28, rowY + (rowH - badge) / 2,
                           badge, badge, !active && !e.unread);
            const int textX = x + 94;
            drawTinyTextFit((e.unread ? "* " : "") + e.title, textX, rowY + 12, w - (textX - x) - 84, 1, rgb565(245, 248, 248));
            drawTinyTextFit(e.last.empty() ? e.subtitle : e.last, textX, rowY + 36, w - (textX - x) - 84, 1, rgb565(180, 205, 212));
            drawTinyTextFit(e.time ? timeText(e.time) : "", x + w - 72, rowY + 24, 50, 1, rgb565(180, 205, 212));
            rowY += rowH + gap;
        }
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN CHAT  A OPEN  B MENU  ORANGE = UNREAD", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawChannelsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("GROUP CHANNELS", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 30, w - 28, 28, rgb565(67, 123, 119), 255);
    drawBitmapIcon(icons::Channels, x + 22, y + 35, 18, 18, rgb565(235, 244, 244));
    const int configuredCount = std::max(1, static_cast<int>(state.channels.size()));
    drawTinyTextFit(std::to_string(configuredCount) + " CONFIGURED CHANNELS",
                    x + 48, y + 40, w - 70, 1, rgb565(245, 248, 248));

    const int first = std::max(0, state.selectedChannelIndex - 2);
    const int rowH = 35;
    int rowY = y + 68;
    for (int slot = 0; slot < 7; ++slot) {
        const int index = first + slot;
        if (index >= 8 || rowY + rowH > y + h - 34) break;
        bool configured = false;
        const ChannelSummary ch = channelSlot(state, index, &configured);
        const bool fallbackPrimary = index == 0 && !state.channelName.empty();
        const bool enabled = ch.role != 0 || fallbackPrimary;
        const bool active = index == state.selectedChannelIndex;
        const uint16_t bg = active ? rgb565(52, 73, 73) : rgb565(54, 54, 54);
        const uint16_t edge = active ? rgb565(77, 232, 141) : rgb565(145, 152, 152);
        fillRect(x + 18, rowY, w - 36, rowH, bg, 245);
        strokeRect(x + 18, rowY, w - 36, rowH, edge);

        const int iconX = x + 30;
        const int iconY = rowY + 4;
        const bool hasKey = ch.hasPsk || fallbackPrimary;
        const uint16_t iconColor = !enabled ? rgb565(224, 58, 58)
            : !hasKey ? rgb565(224, 58, 58)
            : (ch.pskBytes > 1 ? rgb565(77, 232, 141) : rgb565(238, 214, 69));
        if (enabled && hasKey && ch.pskBytes <= 1) {
            drawChannelKeyIcon(iconX, iconY, iconColor);
        } else {
            drawChannelLockIcon(iconX, iconY, iconColor);
        }

        const std::string name = enabled ? channelDisplayName(state, index) : std::to_string(index);
        drawTinyTextFit(((ch.role == 1 || fallbackPrimary) ? "*" : "") + name, x + 64, rowY + 7, w - 150, 1, rgb565(245, 248, 248));
        std::string detail = enabled ? (hasKey ? "encrypted" : "open") : "empty";
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
        drawNodeAvatar(n, x + 27, rowY + 4, 19, 19, !active);
        const std::string mapListName = nodeBestName(n).empty() ? nodeBadgeLabel(n) : nodeBestName(n);
        drawTinyTextFit(mapListName, x + 51, rowY + 5, listW - 55, 1, rgb565(245, 248, 248));
        drawTinyTextFit(std::string(n.hasPosition ? "GPS " : "NO GPS ") + nodeConnectionText(n),
                        x + 51, rowY + 17, listW - 55, 1, rgb565(180, 205, 212));
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
        const std::string label = nodeBestName(n).empty() ? nodeBadgeLabel(n) : nodeBestName(n);
        drawTinyTextFit(label, nx - 8, ny + size + 5, 64, 1, active ? rgb565(245, 248, 248) : rgb565(180, 205, 212));
    }
    const NodeSummary &sel = state.knownNodes[static_cast<size_t>(selected)];
    fillRect(mapX + 10, mapY + mapH - 40, mapW - 20, 28, rgb565(10, 30, 42), 235);
    strokeRect(mapX + 10, mapY + mapH - 40, mapW - 20, 28, rgb565(35, 99, 118));
    const std::string selectedName = nodeBestName(sel).empty() ? nodeBadgeLabel(sel) : nodeBestName(sel);
    std::string selectedLine = selectedName + "  " + nodeConnectionText(sel);
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
    drawTinyTextFit("UP/DOWN SELECT NODE  A CHAT  SCROLL BAR SHOWS MORE",
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
        {"ADMIN", "OPEN", "Config, sync, and cache actions"},
        {"PLACEMENT", "OPEN", "Move and resize GUI boxes"},
        {"SCREENSAVER", "OPEN", "Mode, speed, and test screen"},
        {"FONT", "OPEN", "Style and size testing"},
        {"FONT DEBUG", "OPEN", "Printable character test sheet"},
        {"GUI OPTIONS", "OPEN", "Menu buttons and UI toggles"},
        {"MIDI / FILES", state.midiFiles.empty() ? "NO MIDI" : (std::to_string(static_cast<int>(state.midiFiles.size())) + " MIDI"),
         "A opens player and transfer folder"},
        {"USB", state.usbDevices.empty() ? "SCAN / CONFIG" : (std::to_string(static_cast<int>(state.usbDevices.size())) + " DEVICE(S)"),
         "A opens USB setup tools"},
        {"CLEAR MSGS", state.clearMessagesArmed ? "PRESS A AGAIN" : (std::to_string(static_cast<int>(state.messages.size())) + " SAVED"),
         "Clears messages.dat and current chats"},
        {"WII IP", state.networkReady ? state.wiiIp : "OFF", "A starts Wii IP/UDP"},
    };
    const int count = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
    const int first = settingsFirstVisibleRow(state.selectedSettingsIndex, h);
    const int rowH = 26;
    const int availableH = std::max(0, h - 42 - 28);
    const int visibleRows = std::max(1, std::min(count, availableH / rowH));
    const int last = std::min(count, first + visibleRows);
    int cy = y + 42;
    for (int i = first; i < last; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedSettingsIndex;
        fillRect(x + 18, cy, w - 36, 24, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 24, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 4, 122, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 156, cy + 4, w - 182, 1, i >= 3 ? rgb565(77, 232, 141) : rgb565(235, 244, 244));
        cy += 26;
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    const std::string footer = "UP/DOWN SETTING  A OPEN  " +
        std::to_string(first + 1) + "-" + std::to_string(last) + "/" + std::to_string(count);
    drawTinyTextFit(footer, x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawUsbToolsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("USB", x + 16, y + 14, w - 32);
    drawTinyTextFit(state.usbStatus + "  " + state.usbDetail,
                    x + 24, y + 38, w - 48, 1, rgb565(180, 205, 212));

    struct Row {
        const char *label;
        const char *value;
        const char *help;
    };
    const Row rows[] = {
        {"SCAN", "REFRESH", "List connected USB devices"},
        {"OPEN", "TRY SERIAL", "Open first supported or forced CDC device"},
        {"FORCE ESP32", "303A:*", "Add Espressif ESP32-S3 CDC override"},
        {"TRY ANY BULK", "DIAG", "Try any device with bulk IN/OUT as CDC"},
        {"RESET CONFIG", "DEFAULT", "Restore USB.config examples"},
        {"CDC", "REASSERT", "Send CDC line coding and DTR again"},
        {"CONFIG WAKE", "SEND", "Send Meshtastic client config request"},
        {"CP210X", "ADD 115200", "Append CP210x USB.config control recipe"},
        {"USB CONTROL", "APPLY", "Run configured vendor control transfers"},
    };

    const int leftX = x + 18;
    const int topY = y + 62;
    const int listW = std::max(170, w / 2 - 26);
    const int detailX = leftX + listW + 12;
    const int detailW = w - (detailX - x) - 18;
    int cy = topY;
    for (int i = 0; i < UsbToolRowCount; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedUsbToolIndex;
        fillRect(leftX, cy, listW, 22, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(leftX, cy, listW, 22, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, leftX + 10, cy + 5, listW - 80, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, leftX + listW - 70, cy + 5, 62, 1,
                        i >= 2 ? rgb565(238, 191, 75) : rgb565(77, 232, 141));
        cy += UsbToolRowH;
    }

    fillRect(detailX, topY, detailW, h - 104, rgb565(18, 39, 47), 225);
    strokeRect(detailX, topY, detailW, h - 104, rgb565(35, 99, 118));
    const int selected = std::max(0, std::min(state.selectedUsbToolIndex, UsbToolRowCount - 1));
    drawTinyTextFit(rows[selected].help, detailX + 10, topY + 10, detailW - 20, 1, rgb565(77, 232, 141));
    drawTinyTextFit("USB.config lives on SD:/apps/wii-mesh",
                    detailX + 10, topY + 30, detailW - 20, 1, rgb565(180, 205, 212));
    drawTinyTextFit("Devices: " + std::to_string(static_cast<int>(state.usbDevices.size())),
                    detailX + 10, topY + 54, detailW - 20, 1, rgb565(245, 248, 248));
    int dy = topY + 76;
    if (state.usbDevices.empty()) {
        drawTinyTextFit("No devices from last scan.", detailX + 10, dy, detailW - 20, 1, rgb565(180, 205, 212));
    } else {
        for (size_t i = 0; i < state.usbDevices.size() && dy + 38 < y + h - 32; ++i) {
            const UsbDeviceInfo &d = state.usbDevices[i];
            drawTinyTextFit(hex16(d.vendorId) + ":" + hex16(d.productId) + " " + d.driverHint,
                            detailX + 10, dy, detailW - 20, 1, rgb565(245, 248, 248));
            dy += 18;
            if (!d.interfaces.empty()) {
                const InterfaceInfo &iface = d.interfaces.front();
                drawTinyTextFit("IF " + std::to_string(iface.number) + " C " + hex16(iface.klass) +
                                " EP " + (iface.endpoints.empty() ? "none" : hex16(iface.endpoints.front().address)),
                                detailX + 18, dy, detailW - 28, 1, rgb565(180, 205, 212));
                dy += 20;
            }
        }
    }

    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN USB TOOL  A RUN  B SETTINGS",
                    x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawAdminToolsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("ADMIN", x + 16, y + 14, w - 32);
    drawTinyTextFit(state.usbStatus + "  " + state.usbDetail,
                    x + 24, y + 38, w - 48, 1, rgb565(180, 205, 212));

    struct Row {
        const char *label;
        const char *value;
        const char *help;
    };
    const Row rows[] = {
        {"CONFIG", "WAKE", "Retry Meshtastic client config/session"},
        {"HEART", "BEAT", "Send PhoneAPI heartbeat to the mesh device"},
        {"RX", "SNAPSHOT", "Copy counters into the stream log"},
        {"TEXT", "SNAPSHOT", "Copy recent messages into the stream log"},
        {"STREAM", "MARK", "Add an admin marker to the packet stream"},
        {"DEBUG", "MARK", "Add an admin marker to status/debug"},
        {"NODES", "CLEAR", "Clear node cache and request fresh NodeInfo"},
        {"RESYNC", "FULL", "Reset protocol parser and request config"},
    };

    const int leftX = x + 18;
    const int topY = y + 62;
    const int listW = std::max(170, w / 2 - 26);
    const int detailX = leftX + listW + 12;
    const int detailW = w - (detailX - x) - 18;
    int cy = topY;
    for (int i = 0; i < AdminToolRowCount; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedAdminToolIndex;
        fillRect(leftX, cy, listW, 22, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(leftX, cy, listW, 22, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, leftX + 10, cy + 5, listW - 80, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, leftX + listW - 70, cy + 5, 62, 1,
                        i >= 6 ? rgb565(238, 191, 75) : rgb565(77, 232, 141));
        cy += AdminToolRowH;
    }

    fillRect(detailX, topY, detailW, h - 104, rgb565(18, 39, 47), 225);
    strokeRect(detailX, topY, detailW, h - 104, rgb565(35, 99, 118));
    const int selected = std::max(0, std::min(state.selectedAdminToolIndex, AdminToolRowCount - 1));
    drawTinyTextFit(rows[selected].help, detailX + 10, topY + 10, detailW - 20, 1, rgb565(77, 232, 141));
    drawTinyTextFit("Node: " + state.nodeName + "  Me: " + state.myNodeId,
                    detailX + 10, topY + 32, detailW - 20, 1, rgb565(245, 248, 248));
    drawTinyTextFit("Channel: " + state.channelName + "  Protocol " +
                    std::string(state.protocolReady ? "ready" : "syncing"),
                    detailX + 10, topY + 54, detailW - 20, 1, rgb565(180, 205, 212));
    drawTinyTextFit("RX " + std::to_string(state.rxBytes) + " bytes/" +
                    std::to_string(state.rxFrames) + " frames bad " +
                    std::to_string(state.rxBadFrames),
                    detailX + 10, topY + 76, detailW - 20, 1, rgb565(245, 248, 248));
    drawTinyTextFit("Messages " + std::to_string(static_cast<int>(state.messages.size())) +
                    "  Nodes " + std::to_string(static_cast<int>(state.knownNodes.size())) +
                    "  TX " + std::to_string(state.txBytes),
                    detailX + 10, topY + 98, detailW - 20, 1, rgb565(245, 248, 248));
    drawTinyTextFit("Wii IP " + state.wiiIp + "  M Device IP " + state.meshDeviceIp,
                    detailX + 10, topY + 120, detailW - 20, 1, rgb565(180, 205, 212));
    if (!state.protocolEvents.empty()) {
        drawTinyTextFit("Event: " + cleanText(state.protocolEvents.back()),
                        detailX + 10, topY + 144, detailW - 20, 1, rgb565(238, 191, 75));
    }

    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN ADMIN  A RUN  B SETTINGS",
                    x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawMidiPlayerPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("MIDI PLAYER", x + 16, y + 14, w - 32);
    const std::string status = state.midiPlaying
        ? "PLAYING " + state.midiNowPlaying
        : state.midiStatus;
    drawTinyTextFit(status, x + 24, y + 38, w - 48, 1,
                    state.midiPlaying ? rgb565(77, 232, 141) : rgb565(180, 205, 212));

    const int listX = x + 18;
    const int listY = y + 62;
    const int listW = w - 36;
    const int listH = h - 142;
    fillRect(listX, listY, listW, listH, rgb565(18, 39, 47), 230);
    strokeRect(listX, listY, listW, listH, rgb565(35, 99, 118));
    if (state.midiFiles.empty()) {
        drawTinyTextFit("No .mid/.midi files saved yet.", listX + 18, listY + 20, listW - 36, 1, rgb565(235, 244, 244));
        drawTinyTextFit("Receive a MeshFile MIDI direct message first.", listX + 18, listY + 42, listW - 36, 1, rgb565(180, 205, 212));
    } else {
        const int rowH = 30;
        const int visible = std::max(1, listH / rowH);
        int start = std::max(0, state.selectedMidiIndex - visible / 2);
        if (start + visible > static_cast<int>(state.midiFiles.size())) {
            start = std::max(0, static_cast<int>(state.midiFiles.size()) - visible);
        }
        for (int i = 0; i < visible && start + i < static_cast<int>(state.midiFiles.size()); ++i) {
            const int index = start + i;
            const int rowY = listY + i * rowH;
            const bool active = index == state.selectedMidiIndex;
            fillRect(listX + 8, rowY + 4, listW - 16, rowH - 6,
                     active ? rgb565(52, 73, 73) : rgb565(44, 48, 49), 235);
            strokeRect(listX + 8, rowY + 4, listW - 16, rowH - 6,
                       active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
            drawTinyTextFit((active ? "> " : "  ") + state.midiFiles[static_cast<size_t>(index)],
                            listX + 18, rowY + 10, listW - 36, 1,
                            active ? rgb565(245, 248, 248) : rgb565(210, 222, 224));
        }
    }

    const int by = y + h - 70;
    const int bw = (w - 69) / 4;
    struct Button { const char *label; bool active; };
    const Button buttons[] = {
        {"A PLAY", state.midiPlaying},
        {"1 STOP", false},
        {state.midiRepeat ? "2 REPEAT ON" : "2 REPEAT OFF", state.midiRepeat},
        {"- DELETE", false},
    };
    for (int i = 0; i < 4; ++i) {
        const int bx = x + 18 + i * (bw + 11);
        fillRect(bx, by, bw, 34, buttons[i].active ? rgb565(38, 96, 65) : rgb565(54, 58, 59), 235);
        strokeRect(bx, by, bw, 34, buttons[i].active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(buttons[i].label, bx + 10, by + 11, bw - 20, 1, rgb565(245, 248, 248));
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN FILE  A PLAY  1 STOP  2 REPEAT  - DELETE  B SETTINGS",
                    x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawGuiOptionsPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("GUI OPTIONS", x + 16, y + 14, w - 32);
    struct Row {
        std::string label;
        bool enabled;
        std::string help;
    };
    std::vector<Row> rows;
    for (int i = 0; i < PrimaryTabCount; ++i) {
        rows.push_back({std::string("MENU ") + menuLabel(i), state.menuEnabled[i], "A toggles this side-menu button"});
    }
    rows.push_back({"DEBUG PANEL", state.debugEnabled, "Shows packet/debug tools"});
    rows.push_back({"POINTER", state.pointerEnabled, "Wii Remote pointer cursor"});
    rows.push_back({"MIDI REPEAT", state.midiRepeat, "Repeats selected MIDI file"});
    const int count = static_cast<int>(rows.size());
    int cy = y + 42;
    for (int i = 0; i < count && cy + 31 < y + h - 28; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedGuiOptionIndex;
        fillRect(x + 18, cy, w - 36, 29, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 29, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 4, 122, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].enabled ? "ON" : "OFF", x + 156, cy + 4, 44, 1,
                        rows[i].enabled ? rgb565(77, 232, 141) : rgb565(238, 191, 75));
        drawTinyTextFit(rows[i].help, x + 210, cy + 4, w - 236, 1, rgb565(180, 205, 212));
        cy += 31;
    }
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN OPTION  A TOGGLE/SAVE  B SETTINGS", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
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
        fillRect(x + 18, cy, w - 36, 42, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 42, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 6, 112, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 146, cy + 6, w - 174, 1, rgb565(77, 232, 141));
        drawTinyTextFit(rows[i].help, x + 146, cy + 24, w - 174, 1, rgb565(180, 205, 212));
        cy += 46;
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
        {"DEBUG SHEET", "OPEN", "Shows every printable character"},
    };
    int cy = y + 48;
    for (int i = 0; i < 3; ++i) {
        const bool active = state.dashboardFocused && i == state.selectedFontIndex;
        fillRect(x + 18, cy, w - 36, 42, active ? rgb565(52, 73, 73) : rgb565(54, 58, 59), 235);
        strokeRect(x + 18, cy, w - 36, 42, active ? rgb565(77, 232, 141) : rgb565(84, 91, 92));
        drawTinyTextFit(rows[i].label, x + 28, cy + 6, 100, 1, rgb565(245, 248, 248));
        drawTinyTextFit(rows[i].value, x + 136, cy + 6, w - 164, 1, rgb565(77, 232, 141));
        drawTinyTextFit(rows[i].help, x + 136, cy + 24, w - 164, 1, rgb565(180, 205, 212));
        cy += 46;
    }
    fillRect(x + 18, cy + 6, w - 36, 52, rgb565(18, 39, 47), 230);
    strokeRect(x + 18, cy + 6, w - 36, 52, rgb565(35, 99, 118));
    drawTinyTextFit("SAMPLE TEXT  STYLE " + fontStyleText(state.fontStyle) +
                    " SIZE " + std::to_string(std::max(0, std::min(state.fontSize, FontSizeMax))),
                    x + 30, cy + 18, w - 60, 1, rgb565(77, 232, 141));
    drawTinyTextFit("WiiMesh LongFast MacnCheeseNoodles 012345", x + 30, cy + 36, w - 60, 1, rgb565(245, 248, 248));
    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("LEFT/RIGHT ADJUST  A CYCLE  B SETTINGS", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
}

void drawFontDebugPanel(const AppState &state, int x, int y, int w, int h) {
    drawSectionTitle("FONT DEBUG", x + 16, y + 14, w - 32);
    drawTinyTextFit("STYLE " + fontStyleText(state.fontStyle) + "  SIZE " + fontSizeText(state.fontSize),
                    x + 24, y + 38, w - 48, 1, rgb565(77, 232, 141));

    const int boxX = x + 18;
    const int boxY = y + 60;
    const int boxW = w - 36;
    const int boxH = h - 96;
    fillRect(boxX, boxY, boxW, boxH, rgb565(18, 39, 47), 230);
    strokeRect(boxX, boxY, boxW, boxH, rgb565(77, 232, 141));

    const char *rows[] = {
        " !\"#$%&'()*+,-./",
        "0123456789:;<=>?",
        "@ABCDEFGHIJKLMNO",
        "PQRSTUVWXYZ[\\]^_",
        "`abcdefghijklmno",
        "pqrstuvwxyz{|}~",
    };
    int cy = boxY + 16;
    for (const char *row : rows) {
        drawTinyTextFit(row, boxX + 16, cy, boxW - 32, 1, rgb565(245, 248, 248));
        cy += 24;
    }
    cy += 6;
    drawTinyTextFit("WiiMesh v" + std::string(AppVersion) + " LongFast", boxX + 16, cy, boxW - 32, 1, rgb565(77, 232, 141));
    cy += 22;
    drawTinyTextFit("MacnCheeseNoodles pumpkinPulse", boxX + 16, cy, boxW - 32, 1, rgb565(245, 248, 248));
    cy += 22;
    drawTinyTextFit("[]{}()<> /\\| !? @#$% &* += _-.,:;", boxX + 16, cy, boxW - 32, 1, rgb565(180, 205, 212));

    fillRect(x + 14, y + h - 27, w - 28, 20, rgb565(12, 31, 41), 235);
    drawTinyTextFit("UP/DOWN SIZE  LEFT/RIGHT STYLE  B FONT", x + 24, y + h - 20, w - 48, 1, rgb565(77, 232, 141));
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
                        x + 28, cy + 3, w - 56, 1, i < 2 ? rgb565(122, 239, 206) : rgb565(235, 244, 244));
        cy += 25;
    }
    drawTinyTextFit("LEFT/RIGHT SWITCH TABS  B NODE LIST", x + 22, y + h - 24, w - 44, 1, rgb565(77, 232, 141));
}

void drawChatPanel(const AppState &state, int x, int y, int w, int h) {
    const int keyboardH = state.composingMessage ? chatKeyboardHeight(h) : 0;
    const bool channelChat = state.chatChannelIndex >= 0;
    drawSectionTitle(channelChat ? "GROUP CHAT" : "DIRECT CHAT", x + 16, y + 14, w - 32);
    fillRect(x + 14, y + 28, w - 28, 27, rgb565(67, 123, 119), 245);
    if (channelChat) {
        drawBitmapIcon(icons::Channels, x + 24, y + 32, 20, 20, rgb565(245, 248, 248));
    } else {
        for (const auto &node : state.knownNodes) {
            if (node.nodeId == state.chatPeerNodeId || nodeId(node.id) == state.chatPeerNodeId) {
                drawNodeAvatar(node, x + 21, y + 30, 23, 23, false);
                break;
            }
        }
    }
    drawTinyTextFit(state.chatPeerName.empty() ? "NO CONTACT" : state.chatPeerName,
                    x + (channelChat ? 52 : 52), y + 37, w - 188, 1, rgb565(245, 248, 248));
    drawTinyTextFit(channelChat ? ("CH " + std::to_string(state.chatChannelIndex)) : "DIRECT",
                    x + w - 110, y + 37, 90, 1, rgb565(180, 205, 212));
    const int bubbleH = state.composingMessage ? 34 : 58;
    const int maxShown = chatDetailVisibleCount(state);
    std::vector<const Message *> matchingMessages;
    for (int i = 0; i < static_cast<int>(state.messages.size()); ++i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        if (channelChat) {
            if (m.direct || m.channelIndex != state.chatChannelIndex) continue;
        } else if (!directMessageMatchesPeer(state, m, state.chatPeerNodeId)) {
            continue;
        }
        matchingMessages.push_back(&m);
    }
    const int totalMessages = static_cast<int>(matchingMessages.size());
    const int maxScroll = std::max(0, totalMessages - maxShown);
    const int scroll = std::max(0, std::min(state.scrollOffset, maxScroll));
    const int start = maxScroll - scroll;
    const int end = std::min(static_cast<int>(matchingMessages.size()), start + maxShown);
    std::vector<const Message *> visibleMessages;
    for (int i = start; i < end; ++i) {
        visibleMessages.push_back(matchingMessages[static_cast<size_t>(i)]);
    }
    if (!matchingMessages.empty()) {
        drawTinyTextFit("MSG " + std::to_string(start + 1) + "-" + std::to_string(end) + " OF " +
                            std::to_string(static_cast<int>(matchingMessages.size())),
                        x + w - 136, y + 58, 114, 1, rgb565(180, 205, 212));
    }

    int cy = state.composingMessage ? y + 60 : y + 64;
    for (const Message *message : visibleMessages) {
        const Message &m = *message;
        const bool mine = m.from == 0 || m.senderId == state.myNodeId;
        const int bw = std::min(w - 56, 278);
        const int bx = mine ? x + w - bw - 24 : x + 24;
        fillRect(bx, cy, bw, bubbleH, mine ? rgb565(24, 80, 104) : rgb565(30, 48, 57), 245);
        strokeRect(bx, cy, bw, bubbleH, mine ? rgb565(51, 215, 238) : rgb565(88, 101, 105));
        drawTinyTextFit(timeText(m.rxTime) + " " + senderText(state, m), bx + 10, cy + 8, bw - 20, 1, rgb565(180, 205, 212));
        drawWrappedTinyText(messageText(m), bx + 10, cy + (state.composingMessage ? 22 : 25),
                            bw - ((mine || m.outgoing) ? 42 : 20),
                            1, state.composingMessage ? 1 : 2, 13, rgb565(245, 248, 248));
        if (mine || m.outgoing) {
            drawDeliveryCheck(bx + bw - 24, cy + bubbleH - 19,
                              m.delivered ? rgb565(77, 232, 141) : rgb565(140, 150, 154));
        }
        cy += bubbleH + 8;
    }
    if (visibleMessages.empty()) {
        drawTinyTextFit(channelChat ? "NO MESSAGES IN THIS CHANNEL YET" : "READY TO MESSAGE THIS NODE",
                        x + 24, y + 76, w - 48, 1, rgb565(235, 244, 244));
        drawTinyTextFit(channelChat ? "CHANNEL SEND COMES AFTER RECEIVE UI IS SOLID" : "A OPENS KEYBOARD, 2 SENDS SELECTED MIDI",
                        x + 24, y + 96, w - 48, 1, rgb565(170, 197, 204));
    }
    if (state.composingMessage) {
        const std::vector<KeyboardKey> keys = keyboardKeys(&state);
        const int ky = y + h - keyboardH - 8;
        fillRect(x + 12, ky, w - 24, keyboardH, rgb565(8, 18, 26), 250);
        strokeRect(x + 12, ky, w - 24, keyboardH, rgb565(51, 215, 238));
        fillRect(x + 22, ky + 10, w - 44, 23, rgb565(21, 31, 41), 255);
        strokeRect(x + 22, ky + 10, w - 44, 23, rgb565(77, 232, 141));
        std::string shownText = state.composeText;
        const size_t cursor = std::min(state.composeCursorPosition, shownText.size());
        shownText.insert(cursor, "|");
        drawUnicodeAwareTinyTextFit(shownText.empty() || shownText == "|" ? "TYPE A MESSAGE..." : shownText,
                                    x + 30, ky + 18, w - 86, 1,
                                    state.composeText.empty() ? rgb565(143, 177, 188) : rgb565(245, 248, 248));
        drawTinyTextFit(std::to_string(state.composeText.size()) + "/200", x + w - 62, ky + 18, 40, 1, rgb565(180, 205, 212));
        drawTinyTextFit(keyboardModeName(state.keyboardMode) +
                        (state.keyboardShiftActive ? " Shift" : "") +
                        (state.keyboardCapsLockActive ? " Caps" : ""),
                        x + 24, ky + 36, 180, 1, rgb565(77, 232, 141));
        const int cols = KeyboardCols;
        const int keyW = std::max(30, (w - 44) / cols - 6);
        const int keyH = 21;
        const int visibleRows = std::max(1, (keyboardH - 50) / (keyH + 5));
        const int totalRows = (static_cast<int>(keys.size()) + cols - 1) / cols;
        const int cursorRow = state.keyboardCursor / cols;
        int firstRow = std::max(0, cursorRow - visibleRows / 2);
        firstRow = std::min(firstRow, std::max(0, totalRows - visibleRows));
        int index = firstRow * cols;
        const int lastIndex = std::min(static_cast<int>(keys.size()), (firstRow + visibleRows) * cols);
        int keyY = ky + 42;
        while (index < lastIndex && keyY + keyH < y + h - 8) {
            for (int col = 0; col < cols && index < static_cast<int>(keys.size()); ++col, ++index) {
                const KeyboardKey &key = keys[static_cast<size_t>(index)];
                if (key.type == KeyboardKeyType::Empty) continue;
                const bool suggestion = index < KeyboardSuggestionCount;
                const int sugW = std::max(58, (w - 44) / KeyboardSuggestionCount - 6);
                const int span = suggestion ? 1 : std::max(1, key.span);
                const int keyX = suggestion ? x + 22 + index * (sugW + 6) : x + 22 + col * (keyW + 6);
                const int drawW = suggestion ? sugW : keyW * span + 6 * (span - 1);
                const bool selected = index == state.keyboardCursor;
                const bool send = key.type == KeyboardKeyType::Enter;
                const bool back = key.type == KeyboardKeyType::Backspace;
                const bool activeToggle = (key.type == KeyboardKeyType::Shift && state.keyboardShiftActive) ||
                                          (key.type == KeyboardKeyType::Caps && state.keyboardCapsLockActive);
                fillRect(keyX, keyY, drawW, keyH,
                         selected ? rgb565(17, 103, 145) :
                         (activeToggle ? rgb565(48, 92, 60) : (send ? rgb565(18, 91, 63) : rgb565(35, 45, 51))), 245);
                strokeRect(keyX, keyY, drawW, keyH,
                           selected ? rgb565(77, 232, 141) : (send ? rgb565(77, 232, 141) : rgb565(85, 101, 107)));
                uint16_t ink = selected ? rgb565(255, 255, 255) : rgb565(215, 226, 229);
                if (back) ink = rgb565(238, 191, 75);
                if (key.type == KeyboardKeyType::UnicodeBadge && !key.emojiCodepoints.empty()) {
                    if (const emoji::Icon *icon = emojiIconSequence(key.emojiCodepoints)) {
                        drawEmojiIcon(*icon, keyX + 5, keyY + 2, drawW - 10, keyH - 4);
                    } else {
                        drawUserIconGlyphs(key.emojiCodepoints, keyX + 5, keyY + 2, drawW - 10, keyH - 4, ink);
                    }
                } else if (key.type == KeyboardKeyType::UnicodeBadge && key.emojiCodepoint != 0) {
                    drawUserIconGlyphs(std::vector<uint32_t>{key.emojiCodepoint}, keyX + 5, keyY + 2, drawW - 10, keyH - 4, ink);
                } else {
                    drawTinyTextFit(key.label, keyX + 6, keyY + 4, drawW - 12, 1, ink);
                }
            }
            keyY += keyH + 5;
        }
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
    case ScreenFontDebug: drawFontDebugPanel(state, x, y, w, h); break;
    case ScreenMidiPlayer: drawMidiPlayerPanel(state, x, y, w, h); break;
    case ScreenUsbTools: drawUsbToolsPanel(state, x, y, w, h); break;
    case ScreenAdminTools: drawAdminToolsPanel(state, x, y, w, h); break;
    case ScreenGuiOptions: drawGuiOptionsPanel(state, x, y, w, h); break;
    case ScreenChatDetail: drawChatPanel(state, x, y, w, h); break;
    default: drawHomePanel(state, x, y, w, h); break;
    }
    drawPullBar(x, y, w, h, pullBarState(state, h));
}

void drawGraphicalFooter(const AppState &state, int x, int y, int w, int h) {
    panel(x, y, w, h, false);
    const std::string help = state.composingMessage
        ? "KEYBOARD  D-PAD MOVE  A KEY/SEND  B CANCEL"
        : state.uiTab == ScreenChatDetail && state.chatChannelIndex < 0
        ? "FOCUSED  UP/DOWN SCROLL  A TYPE  2 SEND MIDI  B BACK"
        : state.uiTab == ScreenChatDetail
        ? "FOCUSED  UP/DOWN SCROLL  B BACK TO CHATS"
        : state.dashboardFocused
        ? "FOCUSED  UP/DOWN SCROLL  A OPEN  B MENU"
        : "UP/DOWN MENU  A FOCUS  1 USB  - GRID";
    drawTinyTextFit(help, x + 14, y + 14, w - 28, 1, rgb565(235, 244, 244));
}

bool shouldUseGraphicsOnly(const AppState &state) {
    if (!gGraphicsReady || state.showCalibration || state.showDiagnostics) return false;
    return state.uiTab == ScreenHome || state.uiTab == ScreenNodeOptions ||
           state.uiTab == ScreenChannels || state.uiTab == ScreenChats ||
           state.uiTab == ScreenMap || state.uiTab == ScreenSettings ||
           state.uiTab == ScreenChatDetail || state.uiTab == ScreenNodeTools ||
           state.uiTab == ScreenScreensaverSettings || state.uiTab == ScreenFontSettings ||
           state.uiTab == ScreenFontDebug || state.uiTab == ScreenMidiPlayer ||
           state.uiTab == ScreenUsbTools ||
           state.uiTab == ScreenAdminTools ||
           state.uiTab == ScreenGuiOptions;
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
    if (unreadIncomingCount(state) > 0) {
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
    printAtW(row, col, header.textWidth, "WiiMesh v" + std::string(AppVersion) + "  " + status);
    printAtW(row, std::max(col, col + header.textWidth - 18), 18,
             state.networkReady ? "IP/UDP ON" : "IP/UDP OFF");
    printAtW(row + 1, col, header.textWidth, "USB: " + state.usbStatus);
    printAtW(row + 2, col, header.textWidth, "Me: " + selfDisplayName(state));
    printAtW(row + 3, col, header.textWidth, "Ch: " + state.channelName + "  Nodes " + std::to_string(state.nodeCount) +
                  "  Text " + std::to_string(static_cast<int>(state.messages.size())));
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
        printAtW(row, col, width, "A action  B back  - grid  1 USB  HOME");
    } else {
        printAtW(row, col, width, "Up/Down menu  A focus  - grid  1 USB");
    }
}

void drawKeyboard(const AppState &state) {
    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    printRule(18);
    printAt(19, 3, "To: " + state.chatPeerName + " " + state.chatPeerNodeId);
    std::string shownText = state.composeText;
    shownText.insert(std::min(state.composeCursorPosition, shownText.size()), "|");
    printAt(20, 3, "Text: " + trimTo(shownText, 64));
    printAt(21, 3, "Mode: " + keyboardModeName(state.keyboardMode) +
                  (state.keyboardShiftActive ? " Shift" : "") +
                  (state.keyboardCapsLockActive ? " Caps" : "") +
                  " Cursor: " + keys[static_cast<size_t>(state.keyboardCursor)].label);
    int row = 22;
    const int visibleRows = 5;
    const int totalRows = (static_cast<int>(keys.size()) + KeyboardCols - 1) / KeyboardCols;
    const int cursorRow = state.keyboardCursor / KeyboardCols;
    int firstRow = std::max(0, cursorRow - visibleRows / 2);
    firstRow = std::min(firstRow, std::max(0, totalRows - visibleRows));
    int index = firstRow * KeyboardCols;
    const int lastIndex = std::min(static_cast<int>(keys.size()), (firstRow + visibleRows) * KeyboardCols);
    while (index < lastIndex && row <= 26) {
        std::string line;
        for (int col = 0; col < KeyboardCols && index < static_cast<int>(keys.size()); ++col, ++index) {
            std::string label = keys[static_cast<size_t>(index)].label;
            if (keys[static_cast<size_t>(index)].type == KeyboardKeyType::Empty) label = " ";
            if (index == state.keyboardCursor) label = ">" + label;
            else label = " " + label;
            while (label.size() < 7) label += ' ';
            line += label;
        }
        printContent(row++, 3, line);
    }
}

void drawHome(const AppState &state) {
    const uint32_t unread = unreadIncomingCount(state);
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
                       senderText(state, m) + " " + m.channelName);
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
    const int visibleMessages = std::max(1, (17 - row + 1) / 2);
    int start = std::max(0, state.selectedMessageIndex - visibleMessages + 1);
    start = std::min(start, std::max(0, static_cast<int>(state.messages.size()) - visibleMessages));
    for (int index = start; index < static_cast<int>(state.messages.size()) && row <= 17; ++index) {
        const Message &m = state.messages[static_cast<size_t>(index)];
        const bool selected = index == state.selectedMessageIndex;
        const std::string type = m.direct ? "DM" : "CH";
        printContent(row++, 3, std::string(selected ? ">" : " ") + timeText(m.rxTime) + " " + type +
                      " " + senderText(state, m) + " " + m.channelName);
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
        printContent(ContentRow + 2, 5, "Waiting for node info from the radio.");
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
    printContent(row++, 3, "  Admin     config, sync, cache actions");
    printContent(row++, 3, "  MIDI      player and MeshFile transfers");
    row++;
    printContent(row++, 3, "A: open Log. Use Right from LOG for DEBUG/STATUS.");
    printContent(row++, 3, "Enable Wii IP/UDP from Settings.  1: USB diagnostics.");
    row++;
    printContent(row++, 3, "Network: " + state.netStatus);
    printContent(row++, 3, "Files:   " + meshFileStatusText(state));
    printContent(row++, 3, "MIDI:    " + (state.midiFiles.empty() ? std::string("no saved MIDI") :
                                                std::to_string(static_cast<int>(state.midiFiles.size())) + " file(s)"));
    printContent(row++, 3, "Wii IP:  " + state.wiiIp);
    printContent(row++, 3, "M Device IP: " + state.meshDeviceIp);
    printContent(row++, 3, "UDP ->   " + (state.udpTargetIp.empty() ? std::string("unset") : state.udpTargetIp));
}

void drawChat(const AppState &state) {
    printContent(ContentRow, 3, "Chat");
    printContent(ContentRow + 1, 3, "Peer: " + state.chatPeerName + " " + state.chatPeerNodeId);
    int row = ContentRow + 3;
    std::vector<const Message *> matchingMessages;
    for (int i = 0; i < static_cast<int>(state.messages.size()); ++i) {
        const Message &m = state.messages[static_cast<size_t>(i)];
        if (!directMessageMatchesPeer(state, m, state.chatPeerNodeId)) {
            continue;
        }
        matchingMessages.push_back(&m);
    }
    const int visible = 2;
    const int maxScroll = std::max(0, static_cast<int>(matchingMessages.size()) - visible);
    const int scroll = std::max(0, std::min(state.scrollOffset, maxScroll));
    const int start = maxScroll - scroll;
    int shown = 0;
    for (int i = start; i < static_cast<int>(matchingMessages.size()) && row <= 17; ++i) {
        const Message &m = *matchingMessages[static_cast<size_t>(i)];
        printContent(row++, 3, timeText(m.rxTime) + " " + senderText(state, m));
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
    static uint32_t lastBellRequest = 0;
    static bool armed = false;
    if (!armed) {
        lastBellRequest = state.bellRequestSeq;
        armed = true;
        return;
    }
    if (state.bellRequestSeq != lastBellRequest) {
        buildBellTone();
        ASND_SetVoice(0, VOICE_MONO_16BIT, 32000, 0, gBellPcm, sizeof(gBellPcm), 220, 220, nullptr);
    }
    lastBellRequest = state.bellRequestSeq;
}
#endif

std::string basicLayoutExport() {
    std::string out = "# WiiMesh GUI.config\n";
    for (const auto &z : gGuiZones) {
        out += std::string(z.name) + ".x=" + std::to_string(z.x) + "\n";
        out += std::string(z.name) + ".y=" + std::to_string(z.y) + "\n";
        out += std::string(z.name) + ".w=" + std::to_string(z.w) + "\n";
        out += std::string(z.name) + ".h=" + std::to_string(z.h) + "\n";
        out += std::string(z.name) + ".text_width=" + std::to_string(z.textWidth) + "\n";
    }
    return out;
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
    loadGuiConfig(&state);
    compactGuiLayout();
    state.usbDetail = "GUI.config reloaded";
    return true;
}

bool Ui::applyLayoutLine(AppState &state, const std::string &line) {
    applyGuiConfigLine(&state, line.c_str());
    state.usbDetail = "GUI line applied";
    return true;
}

bool Ui::saveLayout(AppState &state) const {
    return saveGuiConfig(state);
}

std::string Ui::exportLayout() const {
    return basicLayoutExport();
}

void Ui::updateInput(AppState &state) {
#if defined(WIIMESH_WII)
    static bool loadedGuiState = false;
    if (!loadedGuiState) {
        loadGuiConfig(&state);
        compactGuiLayout();
        loadedGuiState = true;
    }
    WPAD_ScanPads();
    const u32 down = WPAD_ButtonsDown(0);
    const u32 held = WPAD_ButtonsHeld(0);
    const u32 nav = down | repeatedButtons(held);
    ir_t ir;
    std::memset(&ir, 0, sizeof(ir));
    WPAD_IR(0, &ir);
    state.pointerVisible = state.pointerEnabled && ir.valid;
    if (state.pointerVisible) {
        state.pointerX = std::max(0, std::min(SkinWidth - 1, static_cast<int>(ir.x)));
        state.pointerY = std::max(0, std::min(SkinHeight - 1, static_cast<int>(ir.y)));
    }
    const std::vector<KeyboardKey> keys = keyboardKeys(&state);
    if (gScreensaverDismissBlockFrames > 0) --gScreensaverDismissBlockFrames;

    if ((down & WPAD_BUTTON_HOME) && !gScreensaverActive) {
        std::exit(0);
    }
    if ((down & WPAD_BUTTON_MINUS) && state.composingMessage) {
        state.keyboardMode = nextKeyboardMode(state.keyboardMode);
        state.keyboardShiftActive = false;
        normalizeKeyboardCursor(state);
        return;
    }
    if ((down & WPAD_BUTTON_MINUS) && state.uiTab == ScreenMidiPlayer) {
        state.midiCommand = 5;
        state.usbDetail = "MIDI delete requested";
        return;
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
            (nav & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_UP | WPAD_BUTTON_DOWN));
        if (resizing) {
            if (nav & WPAD_BUTTON_LEFT) z.w = std::max(32, z.w - Step);
            if (nav & WPAD_BUTTON_RIGHT) z.w += Step;
            if (nav & WPAD_BUTTON_UP) z.h = std::max(24, z.h - Step);
            if (nav & WPAD_BUTTON_DOWN) z.h += Step;
        } else {
            if (nav & WPAD_BUTTON_LEFT) z.x -= Step;
            if (nav & WPAD_BUTTON_RIGHT) z.x += Step;
            if (nav & WPAD_BUTTON_UP) z.y -= Step;
            if (nav & WPAD_BUTTON_DOWN) z.y += Step;
        }
        if (nav & WPAD_BUTTON_MINUS) z.textWidth = std::max(4, z.textWidth - 2);
        if (nav & WPAD_BUTTON_PLUS) z.textWidth = std::min(70, z.textWidth + 2);
        return;
    }
    if (state.uiTab == ScreenMidiPlayer && (down & WPAD_BUTTON_1)) {
        state.midiCommand = 2;
        state.usbDetail = "MIDI stop requested";
        return;
    }
    if (down & WPAD_BUTTON_1) {
        state.showDiagnostics = !state.showDiagnostics;
        return;
    }
    if (state.showDiagnostics) return;

    clampState(state);

    if (state.composingMessage) {
        if (down & WPAD_BUTTON_B) {
            state.composingMessage = false;
            return;
        }
        if (nav & WPAD_BUTTON_LEFT) moveKeyboardCursor(state, -1);
        if (nav & WPAD_BUTTON_RIGHT) moveKeyboardCursor(state, 1);
        if (nav & WPAD_BUTTON_UP) moveKeyboardCursor(state, -KeyboardCols);
        if (nav & WPAD_BUTTON_DOWN) moveKeyboardCursor(state, KeyboardCols);
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
        } else if (state.uiTab == ScreenFontDebug) {
            state.uiTab = ScreenFontSettings;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenMidiPlayer) {
            state.uiTab = ScreenHome;
            state.dashboardFocused = false;
        } else if (state.uiTab == ScreenUsbTools) {
            state.uiTab = ScreenSettings;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenAdminTools) {
            state.uiTab = ScreenSettings;
            state.dashboardFocused = true;
        } else if (state.uiTab == ScreenGuiOptions) {
            state.uiTab = ScreenSettings;
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
    if (nav & WPAD_BUTTON_UP) {
        if (state.uiTab < PrimaryTabCount && !state.dashboardFocused) {
            state.uiTab = nextEnabledPrimaryTab(state, state.uiTab, -1);
            state.scrollOffset = 0;
        } else if (state.uiTab == ScreenChatDetail) {
            const int maxScroll = std::max(0, chatDetailMessageCount(state) - chatDetailVisibleCount(state));
            if (state.scrollOffset < maxScroll) state.scrollOffset++;
        } else if (state.uiTab == ScreenChats && state.selectedChatIndex > 0) {
            state.selectedChatIndex--;
        } else if (state.uiTab == ScreenChannels && state.selectedChannelIndex > 0) {
            state.selectedChannelIndex--;
        } else if (state.uiTab == ScreenSettings && state.selectedSettingsIndex > 0) {
            state.selectedSettingsIndex--;
            state.clearMessagesArmed = false;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex > 0) {
            state.selectedScreensaverIndex--;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex > 0) {
            state.selectedFontIndex--;
        } else if (state.uiTab == ScreenFontDebug && state.fontSize > 0) {
            state.fontSize--;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenMidiPlayer && state.selectedMidiIndex > 0) {
            state.selectedMidiIndex--;
        } else if (state.uiTab == ScreenUsbTools && state.selectedUsbToolIndex > 0) {
            state.selectedUsbToolIndex--;
        } else if (state.uiTab == ScreenAdminTools && state.selectedAdminToolIndex > 0) {
            state.selectedAdminToolIndex--;
        } else if (state.uiTab == ScreenGuiOptions && state.selectedGuiOptionIndex > 0) {
            state.selectedGuiOptionIndex--;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && state.selectedNodeIndex > 0) {
            state.selectedNodeIndex--;
        } else if (state.scrollOffset > 0) {
            state.scrollOffset--;
        }
    }
    if (nav & WPAD_BUTTON_DOWN) {
        if (state.uiTab < PrimaryTabCount && !state.dashboardFocused) {
            state.uiTab = nextEnabledPrimaryTab(state, state.uiTab, 1);
            state.scrollOffset = 0;
        } else if (state.uiTab == ScreenChatDetail && state.scrollOffset > 0) {
            state.scrollOffset--;
        } else if (state.uiTab == ScreenChats &&
                   state.selectedChatIndex + 1 < static_cast<int>(chatEntries(state).size())) {
            state.selectedChatIndex++;
        } else if (state.uiTab == ScreenChannels &&
                   state.selectedChannelIndex + 1 < std::max(8, static_cast<int>(state.channels.size()))) {
            state.selectedChannelIndex++;
        } else if (state.uiTab == ScreenFontDebug && state.fontSize < FontSizeMax) {
            state.fontSize++;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenSettings && state.selectedSettingsIndex + 1 < SettingsRowCount) {
            state.selectedSettingsIndex++;
            state.clearMessagesArmed = false;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex < 2) {
            state.selectedScreensaverIndex++;
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex < 2) {
            state.selectedFontIndex++;
        } else if (state.uiTab == ScreenMidiPlayer &&
                   state.selectedMidiIndex + 1 < static_cast<int>(state.midiFiles.size())) {
            state.selectedMidiIndex++;
        } else if (state.uiTab == ScreenUsbTools && state.selectedUsbToolIndex + 1 < UsbToolRowCount) {
            state.selectedUsbToolIndex++;
        } else if (state.uiTab == ScreenAdminTools && state.selectedAdminToolIndex + 1 < AdminToolRowCount) {
            state.selectedAdminToolIndex++;
        } else if (state.uiTab == ScreenGuiOptions && state.selectedGuiOptionIndex < 8) {
            state.selectedGuiOptionIndex++;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) &&
                   state.selectedNodeIndex + 1 < static_cast<int>(state.knownNodes.size())) {
            state.selectedNodeIndex++;
        } else {
            state.scrollOffset++;
        }
    }
    if (nav & WPAD_BUTTON_LEFT) {
        if (state.uiTab == ScreenChats && state.selectedChatIndex > 0) {
            state.selectedChatIndex--;
        } else if ((state.uiTab == ScreenNodeOptions || state.uiTab == ScreenMap) && state.selectedNodeIndex > 0) {
            state.selectedNodeIndex--;
        } else if (state.uiTab == ScreenNodeTools) {
            state.nodeOptionTab = 0;
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 0) {
            state.screensaverMode = (state.screensaverMode + 2) % 3;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 1) {
            state.screensaverSpeed = std::max(0, state.screensaverSpeed - 1);
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontDebug) {
            state.fontStyle = (state.fontStyle + FontStyleCount - 1) % FontStyleCount;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 0) {
            state.fontStyle = (state.fontStyle + FontStyleCount - 1) % FontStyleCount;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 1) {
            state.fontSize = std::max(0, state.fontSize - 1);
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenDebug) {
            state.uiTab = ScreenLog;
        } else if (state.uiTab == ScreenStatus) {
            state.uiTab = ScreenDebug;
        }
    }
    if ((down & WPAD_BUTTON_2) && state.uiTab == ScreenHome) {
        state.uiTab = ScreenMidiPlayer;
        state.dashboardFocused = true;
        state.midiCommand = 3;
        state.scrollOffset = 0;
        return;
    }
    if ((down & WPAD_BUTTON_2) && state.uiTab == ScreenMidiPlayer) {
        state.midiRepeat = !state.midiRepeat;
        state.midiCommand = 4;
        state.usbDetail = state.midiRepeat ? "MIDI repeat on" : "MIDI repeat off";
        saveGuiConfig(state);
        return;
    }
    if ((down & WPAD_BUTTON_2) && state.uiTab == ScreenChatDetail) {
        if (state.chatChannelIndex < 0 && !state.chatPeerNodeId.empty()) {
            const uint32_t peer = parseNodeIdText(state.chatPeerNodeId);
            if (peer != 0) {
                state.midiSendTo = peer;
                state.midiCommand = 6;
                state.usbDetail = "MIDI send requested";
            } else {
                state.usbDetail = "MIDI send needs direct node";
            }
        } else {
            state.usbDetail = "MIDI send is direct-only";
        }
        return;
    }
    if ((down & WPAD_BUTTON_2) && state.uiTab == ScreenNodeOptions) {
        state.uiTab = ScreenNodeTools;
        state.scrollOffset = 0;
        return;
    }
    if (nav & WPAD_BUTTON_RIGHT || down & WPAD_BUTTON_2) {
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
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenScreensaverSettings && state.selectedScreensaverIndex == 1) {
            state.screensaverSpeed = std::min(5, state.screensaverSpeed + 1);
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontDebug) {
            state.fontStyle = (state.fontStyle + 1) % FontStyleCount;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 0) {
            state.fontStyle = (state.fontStyle + 1) % FontStyleCount;
            saveGuiConfig(state);
        } else if (state.uiTab == ScreenFontSettings && state.selectedFontIndex == 1) {
            state.fontSize = std::min(FontSizeMax, state.fontSize + 1);
            saveGuiConfig(state);
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
                const int unreadIndex = newestUnreadChatIndex(entries);
                const int index = unreadIndex >= 0 ? unreadIndex :
                    std::max(0, std::min(state.selectedChatIndex, static_cast<int>(entries.size()) - 1));
                openChatEntry(state, entries[static_cast<size_t>(index)]);
            }
        } else if (state.uiTab == ScreenHome && !state.messages.empty()) {
            clampState(state);
            state.selectedMessageIndex = static_cast<int>(state.messages.size()) - 1;
            const Message &m = state.messages[static_cast<size_t>(state.selectedMessageIndex)];
            setChatPeerFromMessage(state, m);
            state.uiTab = ScreenChatDetail;
            markCurrentChatSeen(state);
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
            state.chatPeerName = nodeNameForId(state, n.id, n.nodeId);
            state.uiTab = ScreenChatDetail;
            state.chatChannelIndex = -1;
            markCurrentChatSeen(state);
        } else if (state.uiTab == ScreenChatDetail && state.chatChannelIndex < 0 && !state.chatPeerNodeId.empty()) {
            startCompose(state);
        } else if (state.uiTab == ScreenSettings) {
            activateSettingsSelection(state);
        } else if (state.uiTab == ScreenScreensaverSettings) {
            activateScreensaverSettingsSelection(state);
        } else if (state.uiTab == ScreenFontSettings) {
            activateFontSettingsSelection(state);
        } else if (state.uiTab == ScreenGuiOptions) {
            activateGuiOptionSelection(state);
        } else if (state.uiTab == ScreenUsbTools) {
            activateUsbToolSelection(state);
        } else if (state.uiTab == ScreenAdminTools) {
            activateAdminToolSelection(state);
        } else if (state.uiTab == ScreenMidiPlayer) {
            state.midiCommand = 1;
            state.usbDetail = "MIDI play requested";
        }
    }
#else
    (void)state;
#endif
}

void drawMidiPlayerText(const AppState &state) {
    printContent(0, 3, "MIDI Player");
    printContent(1, 3, state.midiPlaying ? ("Playing: " + state.midiNowPlaying) : state.midiStatus);
    printContent(2, 3, std::string("Repeat: ") + (state.midiRepeat ? "on" : "off"));
    if (state.midiFiles.empty()) {
        printContent(4, 3, "No .mid/.midi files in received_files.");
    } else {
        const int visible = PageRows - 6;
        int start = std::max(0, state.selectedMidiIndex - visible / 2);
        if (start + visible > static_cast<int>(state.midiFiles.size())) {
            start = std::max(0, static_cast<int>(state.midiFiles.size()) - visible);
        }
        for (int i = 0; i < visible && start + i < static_cast<int>(state.midiFiles.size()); ++i) {
            const int index = start + i;
            printContent(4 + i, 3, std::string(index == state.selectedMidiIndex ? "> " : "  ") +
                                     state.midiFiles[static_cast<size_t>(index)]);
        }
    }
    printContent(PageRows - 1, 3, "A play  1 stop  2 repeat  - delete  B home");
}

void drawUsbToolsText(const AppState &state) {
    printContent(0, 3, "USB Tools");
    printContent(1, 3, state.usbStatus);
    printContent(2, 3, state.usbDetail);
    const char *rows[] = {
        "Scan USB devices",
        "Open serial device",
        "Add force_cdc=303a:*",
        "Add try_any_bulk_cdc=1",
        "Reset USB.config",
        "CDC reassert",
        "Send config wake",
        "Add CP210x recipe",
        "Apply USB controls",
    };
    for (int i = 0; i < UsbToolRowCount; ++i) {
        printContent(4 + i, 3, std::string(i == state.selectedUsbToolIndex ? "> " : "  ") + rows[i]);
    }
    printContent(13, 3, "Devices: " + std::to_string(static_cast<int>(state.usbDevices.size())));
    int row = 14;
    for (const auto &d : state.usbDevices) {
        if (row >= PageRows - 1) break;
        printContent(row++, 3, hex16(d.vendorId) + ":" + hex16(d.productId) + " " + d.driverHint);
    }
    printContent(PageRows - 1, 3, "A run  B settings");
}

void drawAdminToolsText(const AppState &state) {
    printContent(0, 3, "Admin");
    printContent(1, 3, state.usbStatus);
    printContent(2, 3, state.usbDetail);
    const char *rows[] = {
        "Config wake",
        "Heartbeat",
        "RX snapshot",
        "Text snapshot",
        "Stream marker",
        "Debug marker",
        "Clear node cache",
        "Full resync",
    };
    for (int i = 0; i < AdminToolRowCount; ++i) {
        printContent(4 + i, 3, std::string(i == state.selectedAdminToolIndex ? "> " : "  ") + rows[i]);
    }
    printContent(13, 3, "Node: " + state.nodeName + " " + state.myNodeId);
    printContent(14, 3, "RX " + std::to_string(state.rxFrames) + " bad " +
                          std::to_string(state.rxBadFrames) + " TX " +
                          std::to_string(state.txBytes));
    printContent(PageRows - 1, 3, "A run  B settings");
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
                // Active-user badges are drawn graphically so emoji short names render as icons.
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
    } else if (state.uiTab == ScreenMidiPlayer) {
        drawMidiPlayerText(state);
    } else if (state.uiTab == ScreenUsbTools) {
        drawUsbToolsText(state);
    } else if (state.uiTab == ScreenAdminTools) {
        drawAdminToolsText(state);
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
