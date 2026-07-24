#pragma once

#include "wiimesh/Types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wiimesh {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void pollDevices(std::vector<UsbDeviceInfo> &devices) = 0;
    virtual bool openFirstSupported(const std::vector<UsbDeviceInfo> &devices) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual int read(uint8_t *buffer, size_t size) = 0;
    virtual int write(const uint8_t *buffer, size_t size) = 0;
};

}
