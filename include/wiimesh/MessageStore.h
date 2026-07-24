#pragma once

#include "wiimesh/Types.h"

namespace wiimesh {

class MessageStore {
public:
    bool load(const char *path, AppState &state);
    bool save(const char *path, const AppState &state);
};

}
