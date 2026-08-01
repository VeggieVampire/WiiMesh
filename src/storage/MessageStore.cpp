#include "wiimesh/MessageStore.h"

#include <cstdio>
#include <cstring>

namespace wiimesh {

static bool isStatusOnlyMessage(const std::string &text) {
    return text == "Routing/ACK packet received" ||
           text.find("Routing/ACK") != std::string::npos ||
           text.find("TEXT_MESSAGE_APP seen") != std::string::npos;
}

static void trimToMax(AppState &state) {
    while (static_cast<int>(state.messages.size()) > MaxMessages) {
        state.messages.erase(state.messages.begin());
    }
}

bool MessageStore::load(const char *path, AppState &state) {
    FILE *f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }

    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        Message m;
        char direct[8] = {};
        char sender[80] = {};
        char channel[32] = {};
        char outgoing[12] = {};
        char delivered[12] = {};
        char text[256] = {};
        unsigned from = 0, to = 0, rx = 0, ch = 0;
        int parsed = std::sscanf(line, "%u\t%u\t%u\t%u\t%7[^\t]\t%79[^\t]\t%31[^\t]\t%11[^\t]\t%11[^\t]\t%255[^\n]",
                                 &from, &to, &rx, &ch, direct, sender, channel, outgoing, delivered, text);
        if (parsed != 10) {
            outgoing[0] = '\0';
            delivered[0] = '\0';
            parsed = std::sscanf(line, "%u\t%u\t%u\t%u\t%7[^\t]\t%79[^\t]\t%31[^\t]\t%255[^\n]",
                                 &from, &to, &rx, &ch, direct, sender, channel, text);
        }
        if (parsed == 8 || parsed == 10) {
            m.from = from;
            m.to = to;
            m.rxTime = rx;
            m.channelIndex = static_cast<uint8_t>(ch);
            m.direct = std::strcmp(direct, "direct") == 0;
            m.outgoing = std::strcmp(outgoing, "out") == 0;
            m.delivered = std::strcmp(delivered, "delivered") == 0;
            m.senderName = sender;
            m.senderId = nodeId(m.from);
            m.channelName = channel;
            m.text = text;
            if (!m.outgoing && m.text.find("[sent] ") == 0) {
                m.outgoing = true;
            }
            if (isStatusOnlyMessage(m.text)) {
                continue;
            }
            state.messages.push_back(m);
        }
    }
    std::fclose(f);
    trimToMax(state);
    return true;
}

bool MessageStore::save(const char *path, const AppState &state) {
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const int start = state.messages.size() > MaxMessages
        ? static_cast<int>(state.messages.size()) - MaxMessages
        : 0;
    for (size_t i = start; i < state.messages.size(); ++i) {
        const Message &m = state.messages[i];
        if (isStatusOnlyMessage(m.text)) {
            continue;
        }
        std::fprintf(f, "%u\t%u\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%s\n",
                     m.from, m.to, m.rxTime, m.channelIndex,
                     m.direct ? "direct" : "channel",
                     m.senderName.c_str(), m.channelName.c_str(),
                     m.outgoing ? "out" : "in",
                     m.delivered ? "delivered" : "pending",
                     m.text.c_str());
    }
    std::fclose(f);
    return true;
}

}
