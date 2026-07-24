#include "wiimesh/MockTransport.h"

#include <algorithm>

namespace wiimesh {

void MockTransport::pollDevices(std::vector<UsbDeviceInfo> &devices) {
    UsbDeviceInfo d;
    d.vendorId = 0x1209;
    d.productId = 0x0001;
    d.deviceClass = 0x02;
    d.driverHint = "mock-cdc-acm";
    InterfaceInfo iface;
    iface.number = 0;
    iface.klass = 0x0a;
    iface.endpoints.push_back({0x81, 0x02, 64, 0});
    iface.endpoints.push_back({0x01, 0x02, 64, 0});
    d.interfaces.push_back(iface);
    devices.push_back(d);
}

bool MockTransport::openFirstSupported(const std::vector<UsbDeviceInfo> &) {
    open_ = true;
    return true;
}

void MockTransport::close() {
    open_ = false;
}

bool MockTransport::isOpen() const {
    return open_;
}

int MockTransport::read(uint8_t *buffer, size_t size) {
    if (!open_ || rx_.empty()) {
        return 0;
    }
    const size_t n = std::min(size, rx_.size());
    for (size_t i = 0; i < n; ++i) {
        buffer[i] = rx_.front();
        rx_.pop_front();
    }
    return static_cast<int>(n);
}

int MockTransport::write(const uint8_t *, size_t size) {
    return open_ ? static_cast<int>(size) : -1;
}

void MockTransport::pushFrame(const std::vector<uint8_t> &payload) {
    rx_.push_back(0x94);
    rx_.push_back(0xc3);
    rx_.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xff));
    rx_.push_back(static_cast<uint8_t>(payload.size() & 0xff));
    for (uint8_t b : payload) {
        rx_.push_back(b);
    }
}

}
