#include "wiimesh/Clock.h"

#include <ctime>

#if defined(WIIMESH_WII)
#include <ogc/lwp_watchdog.h>
#endif

namespace wiimesh {

uint32_t currentUnixTime() {
    const time_t standard = std::time(nullptr);
    if (standard > 1577836800) {
        return static_cast<uint32_t>(standard);
    }

#if defined(WIIMESH_WII)
    const uint64_t seconds = ticks_to_secs(gettime());
    constexpr uint64_t WiiEpochToUnix = 946684800ull;
    if (seconds > 1577836800ull) {
        return static_cast<uint32_t>(seconds);
    }
    if (seconds > 0) {
        return static_cast<uint32_t>(seconds + WiiEpochToUnix);
    }
#endif

    return standard > 0 ? static_cast<uint32_t>(standard) : 0;
}

} // namespace wiimesh
