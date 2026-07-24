#pragma once

#include "wiimesh/Transport.h"
#include "wiimesh/Logger.h"

namespace wiimesh {

class UsbTransport final : public Transport {
public:
    explicit UsbTransport(Logger *logger);
    ~UsbTransport() override;
    void pollDevices(std::vector<UsbDeviceInfo> &devices) override;
    bool openFirstSupported(const std::vector<UsbDeviceInfo> &devices) override;
    void close() override;
    bool isOpen() const override;
    int read(uint8_t *buffer, size_t size) override;
    int write(const uint8_t *buffer, size_t size) override;
    void setWriteMatrixEnabled(bool enabled);
    bool reassertCdcControl();
    const std::string &lastError() const;

private:
    Logger *logger_ = nullptr;
    int fd_ = -1;
    bool open_ = false;
    uint8_t bulkIn_ = 0;
    uint8_t bulkOut_ = 0;
    uint8_t dataInterface_ = 0;
    uint8_t dataAlternate_ = 0;
    uint8_t configurationValue_ = 0;
    int controlInterface_ = 0;
    int32_t deviceId_ = -1;
    bool readPending_ = false;
    bool readComplete_ = false;
    int readResult_ = 0;
    alignas(32) uint8_t asyncRx_[512] = {};
    bool writePending_ = false;
    bool writeComplete_ = false;
    int writeResult_ = 0;
    alignas(32) uint8_t asyncTx_[512] = {};
    size_t asyncTxSize_ = 0;
    bool writeMatrixEnabled_ = false;
    std::string lastError_;

    bool isCdcAcm(const UsbDeviceInfo &info) const;
    static int readCallback(int result, void *userdata);
    static int writeCallback(int result, void *userdata);
};

}
