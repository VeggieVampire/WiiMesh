#pragma once

#include "wiimesh/Transport.h"

#include <deque>

namespace wiimesh {

class MockTransport final : public Transport {
public:
    void pollDevices(std::vector<UsbDeviceInfo> &devices) override;
    bool openFirstSupported(const std::vector<UsbDeviceInfo> &devices) override;
    void close() override;
    bool isOpen() const override;
    int read(uint8_t *buffer, size_t size) override;
    int write(const uint8_t *buffer, size_t size) override;
    void pushFrame(const std::vector<uint8_t> &payload);

private:
    bool open_ = false;
    std::deque<uint8_t> rx_;
};

}
