#include "wiimesh/Types.h"

#include <cstdio>

namespace wiimesh {

std::string hex16(uint16_t value) {
    char out[7];
    std::snprintf(out, sizeof(out), "0x%04x", value);
    return out;
}

std::string hex32(uint32_t value) {
    char out[11];
    std::snprintf(out, sizeof(out), "0x%08x", value);
    return out;
}

std::string nodeId(uint32_t value) {
    char out[11];
    std::snprintf(out, sizeof(out), "!%08x", value);
    return out;
}

}
