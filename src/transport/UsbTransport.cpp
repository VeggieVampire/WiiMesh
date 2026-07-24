#include "wiimesh/UsbTransport.h"

#include <cstdio>
#include <cstring>
#include <map>

#if defined(WIIMESH_WII)
#include <gccore.h>
#include <ogc/usb.h>
#include <malloc.h>
#endif

namespace wiimesh {

UsbTransport::UsbTransport(Logger *logger) : logger_(logger) {
#if defined(WIIMESH_WII)
    USB_Initialize();
#endif
}

UsbTransport::~UsbTransport() {
    close();
}

static std::string classHint(const UsbDeviceInfo &info) {
    if (info.vendorId == 0x239a || info.vendorId == 0x1915 || info.vendorId == 0x2e8a) {
        return "rak/nrf52840-candidate";
    }
    for (const auto &iface : info.interfaces) {
        if (iface.klass == 0x02 || iface.klass == 0x0a) {
            return "cdc-acm";
        }
        if (iface.klass == 0xff) {
            return "vendor-specific";
        }
    }
    return "unsupported";
}

static bool knownNrfCdcCandidate(const UsbDeviceInfo &info) {
    return info.vendorId == 0x239a || // Adafruit/TinyUSB UF2 CDC family
           info.vendorId == 0x1915 || // Nordic Semiconductor
           info.vendorId == 0x2e8a;   // Raspberry Pi RP2040 family, useful for future USB serial boards
}

#if defined(WIIMESH_WII)
static int cdcControlInterface(const UsbDeviceInfo &info) {
    for (const auto &iface : info.interfaces) {
        if (iface.klass == 0x02 && iface.subclass == 0x02) {
            return iface.number;
        }
    }
    return -1;
}

static const UsbDeviceInfo *findCdcControlSibling(const std::vector<UsbDeviceInfo> &devices,
                                                  const UsbDeviceInfo &dataDev) {
    for (const auto &candidate : devices) {
        if (candidate.vendorId == dataDev.vendorId &&
            candidate.productId == dataDev.productId &&
            cdcControlInterface(candidate) >= 0) {
            return &candidate;
        }
    }
    return nullptr;
}
#endif

void UsbTransport::pollDevices(std::vector<UsbDeviceInfo> &devices) {
    devices.clear();
#if defined(WIIMESH_WII)
    const uint8_t classes[] = {0xff, 0x02, 0x0a, 0x00, 0x03};
    std::map<int32_t, usb_device_entry> unique;
    for (uint8_t klass : classes) {
        usb_device_entry entries[16];
        uint8_t count = 0;
        std::memset(entries, 0, sizeof(entries));
        if (USB_GetDeviceList(entries, 16, klass, &count) < 0) {
            if (logger_) logger_->line("USB_GetDeviceList failed for class " + hex16(klass));
            continue;
        }
        for (uint8_t i = 0; i < count; ++i) {
            unique[entries[i].device_id] = entries[i];
        }
    }

    if (logger_) {
        char line[96];
        std::snprintf(line, sizeof(line), "USB scan found %u raw device entries",
                      static_cast<unsigned>(unique.size()));
        logger_->line(line);
    }

    for (const auto &entry : unique) {
        const usb_device_entry &usb = entry.second;
        UsbDeviceInfo out;
        out.deviceId = usb.device_id;
        out.vendorId = usb.vid;
        out.productId = usb.pid;
        out.driverHint = "descriptor-pending";

        s32 fd = -1;
        if (USB_OpenDevice(usb.device_id, usb.vid, usb.pid, &fd) < 0) {
            out.driverHint = knownNrfCdcCandidate(out) ? "rak/nrf-open-failed" : "open-failed";
            out.diagnostic = "USB_OpenDevice failed";
            devices.push_back(out);
            if (logger_) logger_->line("USB_OpenDevice failed for " + hex16(out.vendorId) + ":" + hex16(out.productId));
            continue;
        }

        usb_devdesc desc;
        std::memset(&desc, 0, sizeof(desc));
        if (USB_GetDescriptors(fd, &desc) >= 0) {
            out.deviceClass = desc.bDeviceClass;
            out.deviceSubClass = desc.bDeviceSubClass;
            out.deviceProtocol = desc.bDeviceProtocol;
            for (uint8_t c = 0; c < desc.bNumConfigurations; ++c) {
                usb_configurationdesc &cfg = desc.configurations[c];
                if (out.configurationValue == 0) {
                    out.configurationValue = cfg.bConfigurationValue;
                }
                for (uint8_t j = 0; j < cfg.bNumInterfaces; ++j) {
                    usb_interfacedesc &iface = cfg.interfaces[j];
                    InterfaceInfo ii;
                    ii.number = iface.bInterfaceNumber;
                    ii.alternate = iface.bAlternateSetting;
                    ii.klass = iface.bInterfaceClass;
                    ii.subclass = iface.bInterfaceSubClass;
                    ii.protocol = iface.bInterfaceProtocol;
                    for (uint8_t e = 0; e < iface.bNumEndpoints; ++e) {
                        usb_endpointdesc &ep = iface.endpoints[e];
                        ii.endpoints.push_back({ep.bEndpointAddress, ep.bmAttributes,
                                                ep.wMaxPacketSize, ep.bInterval});
                    }
                    out.interfaces.push_back(ii);
                }
            }
            USB_FreeDescriptors(&desc);
            out.diagnostic = "descriptors ok";
        } else {
            out.driverHint = knownNrfCdcCandidate(out) ? "rak/nrf-desc-failed" : "descriptor-failed";
            out.diagnostic = "USB_GetDescriptors failed";
            usb_devdesc basic;
            std::memset(&basic, 0, sizeof(basic));
            if (USB_GetDeviceDescription(fd, &basic) >= 0) {
                out.deviceClass = basic.bDeviceClass;
                out.deviceSubClass = basic.bDeviceSubClass;
                out.deviceProtocol = basic.bDeviceProtocol;
                out.diagnostic = "device descriptor ok; full descriptors failed";
            }
        }
        USB_CloseDevice(&fd);
        if (out.driverHint == "descriptor-pending") {
            out.driverHint = classHint(out);
        }
        devices.push_back(out);
    }
#else
    (void)devices;
#endif
}

bool UsbTransport::isCdcAcm(const UsbDeviceInfo &info) const {
    bool control = false;
    bool data = false;
    bool bulkIn = false;
    bool bulkOut = false;
    for (const auto &iface : info.interfaces) {
        control = control || (iface.klass == 0x02 && iface.subclass == 0x02);
        data = data || iface.klass == 0x0a;
        for (const auto &ep : iface.endpoints) {
            if ((ep.attributes & 0x03) == 0x02) {
                bulkIn = bulkIn || ((ep.address & 0x80) != 0);
                bulkOut = bulkOut || ((ep.address & 0x80) == 0);
            }
        }
    }
    return (control && data) ||
           (knownNrfCdcCandidate(info) && data && bulkIn && bulkOut);
}

bool UsbTransport::openFirstSupported(const std::vector<UsbDeviceInfo> &devices) {
    close();
#if defined(WIIMESH_WII)
    for (const auto &dev : devices) {
        if (!isCdcAcm(dev)) {
            if (logger_) logger_->line("Skipping unsupported USB serial candidate " + hex16(dev.vendorId) + ":" + hex16(dev.productId));
            continue;
        }
        if (dev.interfaces.empty()) {
            if (logger_) logger_->line("Cannot open serial candidate without endpoint descriptors " + hex16(dev.vendorId) + ":" + hex16(dev.productId));
            continue;
        }
        s32 fd = -1;
        if (USB_OpenDevice(dev.deviceId, dev.vendorId, dev.productId, &fd) < 0) {
            lastError_ = "USB_OpenDevice serial failed";
            continue;
        }
        for (const auto &iface : dev.interfaces) {
            for (const auto &ep : iface.endpoints) {
                if ((ep.attributes & 0x03) == 0x02) {
                    if (ep.address & 0x80) bulkIn_ = ep.address;
                    else bulkOut_ = ep.address;
                    dataInterface_ = iface.number;
                    dataAlternate_ = iface.alternate;
                }
            }
        }
        if (!bulkIn_ || !bulkOut_) {
            USB_CloseDevice(&fd);
            lastError_ = "No bulk IN/OUT endpoints";
            continue;
        }

        s32 setCfg = 0;
        if (dev.configurationValue != 0) {
            setCfg = USB_SetConfiguration(fd, dev.configurationValue);
        }
        s32 setAlt = USB_SetAlternativeInterface(fd, dataInterface_, dataAlternate_);
        s32 clearIn = USB_ClearHalt(fd, bulkIn_);
        s32 clearOut = USB_ClearHalt(fd, bulkOut_);
        if (logger_) {
            char line[160];
            std::snprintf(line, sizeof(line), "CDC data setup cfg %u=>%ld if %u alt %u=>%ld clear %02x=>%ld %02x=>%ld",
                          dev.configurationValue, static_cast<long>(setCfg),
                          dataInterface_, dataAlternate_, static_cast<long>(setAlt),
                          bulkIn_, static_cast<long>(clearIn), bulkOut_, static_cast<long>(clearOut));
            logger_->line(line);
        }

        uint8_t lineCoding[7] = {0x00, 0xc2, 0x01, 0x00, 0x00, 0x00, 0x08};
        int controlIface = cdcControlInterface(dev);
        const UsbDeviceInfo *controlDev = &dev;
        if (controlIface < 0) {
            controlDev = findCdcControlSibling(devices, dev);
            controlIface = controlDev ? cdcControlInterface(*controlDev) : 0;
        }
        controlInterface_ = controlIface;

        s32 lcData = USB_WriteCtrlMsg(fd, 0x21, 0x20, 0, static_cast<u16>(controlIface),
                                      sizeof(lineCoding), lineCoding);
        s32 dtrData = USB_WriteCtrlMsg(fd, 0x21, 0x22, 0x0003, static_cast<u16>(controlIface),
                                       0, nullptr);
        if (logger_) {
            char line[128];
            std::snprintf(line, sizeof(line), "CDC single-fd setup fd %ld iface %d line %ld dtr %ld",
                          static_cast<long>(fd), controlIface,
                          static_cast<long>(lcData), static_cast<long>(dtrData));
            logger_->line(line);
        }

        fd_ = fd;
        open_ = true;
        deviceId_ = dev.deviceId;
        configurationValue_ = dev.configurationValue;
        char detail[128];
        std::snprintf(detail, sizeof(detail), "cdc fd %ld if %u ep out %02x in %02x cfg %ld alt %ld",
                      static_cast<long>(fd_), dataInterface_, bulkOut_, bulkIn_, static_cast<long>(setCfg),
                      static_cast<long>(setAlt));
        lastError_ = detail;
        if (logger_) logger_->line("Opened CDC ACM device " + hex16(dev.vendorId) + ":" + hex16(dev.productId));
        return true;
    }
#else
    (void)devices;
#endif
    return false;
}

void UsbTransport::close() {
#if defined(WIIMESH_WII)
    if (open_) {
        s32 local = fd_;
        USB_CloseDevice(&local);
    }
#endif
    fd_ = -1;
    open_ = false;
    bulkIn_ = bulkOut_ = 0;
    dataInterface_ = 0;
    dataAlternate_ = 0;
    configurationValue_ = 0;
    controlInterface_ = 0;
    deviceId_ = -1;
    readPending_ = false;
    readComplete_ = false;
    readResult_ = 0;
    writePending_ = false;
    writeComplete_ = false;
    writeResult_ = 0;
    asyncTxSize_ = 0;
}

bool UsbTransport::isOpen() const {
    return open_;
}

void UsbTransport::setWriteMatrixEnabled(bool enabled) {
    writeMatrixEnabled_ = enabled;
}

int UsbTransport::read(uint8_t *buffer, size_t size) {
#if defined(WIIMESH_WII)
    if (!open_ || !bulkIn_) return -1;

    if (readComplete_) {
        readComplete_ = false;
        readPending_ = false;
        if (readResult_ < 0) {
            char line[80];
            std::snprintf(line, sizeof(line), "async read done ret %ld; keeping fd open",
                          static_cast<long>(readResult_));
            lastError_ = line;
            if (logger_) logger_->line(lastError_);
            return -1;
        }
        const size_t n = readResult_ < static_cast<int>(size)
            ? static_cast<size_t>(readResult_)
            : size;
        std::memcpy(buffer, asyncRx_, n);
        return static_cast<int>(n);
    }

    if (!readPending_) {
        const u16 requestSize = sizeof(asyncRx_);
        s32 submitted = USB_ReadBlkMsgAsync(fd_, bulkIn_, requestSize, asyncRx_,
                                            UsbTransport::readCallback, this);
        if (submitted < 0) {
            char line[64];
            std::snprintf(line, sizeof(line), "read submit ret %ld; keeping fd open",
                          static_cast<long>(submitted));
            lastError_ = line;
            if (logger_) logger_->line(lastError_);
            return -1;
        }
        readPending_ = true;
    }
    return 0;
#else
    (void)buffer;
    (void)size;
    return -1;
#endif
}

int UsbTransport::readCallback(int result, void *userdata) {
    UsbTransport *self = static_cast<UsbTransport *>(userdata);
    if (!self) {
        return 0;
    }
    self->readResult_ = result;
    self->readComplete_ = true;
    return 0;
}

int UsbTransport::write(const uint8_t *buffer, size_t size) {
#if defined(WIIMESH_WII)
    if (writeComplete_) {
        writeComplete_ = false;
        writePending_ = false;
        char done[96];
        std::snprintf(done, sizeof(done), "async write done ep %02x len %u ret %ld",
                      bulkOut_, static_cast<unsigned>(asyncTxSize_), static_cast<long>(writeResult_));
        lastError_ = done;
        if (logger_) logger_->line(lastError_);
        if (writeResult_ < 0) {
            if (logger_) logger_->line("USB async write failed; closing serial fd");
            close();
            return -1;
        }
    }
    if (!open_) {
        lastError_ = "write blocked: fd closed";
        if (logger_) logger_->line(lastError_);
        return -1;
    }
    if (!bulkOut_) {
        lastError_ = "write blocked: no bulk OUT ep";
        if (logger_) logger_->line(lastError_);
        return -1;
    }
    if (writePending_) {
        lastError_ = "write blocked: async pending";
        if (logger_) logger_->line(lastError_);
        return -1;
    }
    if (size > sizeof(asyncTx_)) {
        lastError_ = "write blocked: tx too large";
        if (logger_) logger_->line(lastError_);
        return -1;
    }
    std::memset(asyncTx_, 0, sizeof(asyncTx_));
    std::memcpy(asyncTx_, buffer, size);
    asyncTxSize_ = size;

    if (!writeMatrixEnabled_) {
        DCFlushRange(asyncTx_, size);
        s32 ret = USB_WriteBlkMsg(fd_, bulkOut_, static_cast<u16>(size), asyncTx_);
        char line[96];
        std::snprintf(line, sizeof(line), "single sync write ep %02x len %u ret %ld",
                      bulkOut_, static_cast<unsigned>(size), static_cast<long>(ret));
        lastError_ = line;
        if (logger_) logger_->line(lastError_);
        if (ret < 0) {
            if (logger_) logger_->line("single USB write failed; closing serial fd");
            close();
            return -1;
        }
        return static_cast<int>(size);
    }

    auto logAttempt = [&](unsigned attempt, const char *name, u8 ep, u16 len, s32 ret) {
        char line[128];
        std::snprintf(line, sizeof(line), "write try %03u %-18s ep %02x len %u ret %ld",
                      attempt, name, ep, static_cast<unsigned>(len), static_cast<long>(ret));
        if (logger_) logger_->line(line);
        lastError_ = line;
    };
    auto success = [&](unsigned attempt, const char *name, u8 ep, u16 len, s32 ret, bool pending) {
        char line[128];
        std::snprintf(line, sizeof(line), "WORKING USB WRITE %03u %s ep %02x len %u ret %ld",
                      attempt, name, ep, static_cast<unsigned>(len), static_cast<long>(ret));
        lastError_ = line;
        if (logger_) logger_->line(lastError_);
        writePending_ = pending;
        return static_cast<int>(size);
    };

    const uint8_t lineCoding[7] = {0x00, 0xc2, 0x01, 0x00, 0x00, 0x00, 0x08};
    const u8 endpoints[] = {
        bulkOut_,
        static_cast<u8>(bulkOut_ & 0x0f),
        static_cast<u8>(bulkOut_ | 0x80),
        0x01,
        0x02,
        0x00,
    };
    const u16 lengths[] = {
        static_cast<u16>(size),
        7, 8, 12, 16, 32, 63, 64,
    };

    if (logger_) {
        char begin[160];
        std::snprintf(begin, sizeof(begin), "USB write matrix begin fd %ld cfg %u data-if %u ctrl-if %d ep %02x",
                      static_cast<long>(fd_), configurationValue_, dataInterface_,
                      controlInterface_, bulkOut_);
        logger_->line(begin);
    }

    auto prep = [&](unsigned variant, u8 ep) {
        switch (variant) {
        case 0:
            return;
        case 1:
            USB_SetAlternativeInterface(fd_, dataInterface_, dataAlternate_);
            return;
        case 2:
            USB_ClearHalt(fd_, ep);
            return;
        case 3:
            USB_SetAlternativeInterface(fd_, dataInterface_, dataAlternate_);
            USB_ClearHalt(fd_, ep);
            return;
        case 4:
            if (configurationValue_) USB_SetConfiguration(fd_, configurationValue_);
            return;
        case 5:
            if (configurationValue_) USB_SetConfiguration(fd_, configurationValue_);
            USB_SetAlternativeInterface(fd_, dataInterface_, dataAlternate_);
            USB_ClearHalt(fd_, ep);
            return;
        case 6:
            USB_ClearHalt(fd_, bulkIn_);
            USB_ClearHalt(fd_, ep);
            return;
        case 7:
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, static_cast<u16>(controlInterface_),
                             sizeof(lineCoding), const_cast<uint8_t *>(lineCoding));
            return;
        case 8:
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                             0, nullptr);
            return;
        case 9:
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, static_cast<u16>(controlInterface_),
                             sizeof(lineCoding), const_cast<uint8_t *>(lineCoding));
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                             0, nullptr);
            return;
        case 10:
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0000, static_cast<u16>(controlInterface_),
                             0, nullptr);
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                             0, nullptr);
            return;
        case 11:
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, static_cast<u16>(dataInterface_),
                             sizeof(lineCoding), const_cast<uint8_t *>(lineCoding));
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(dataInterface_),
                             0, nullptr);
            return;
        case 12:
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, 0, sizeof(lineCoding),
                             const_cast<uint8_t *>(lineCoding));
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, 0, 0, nullptr);
            return;
        case 13:
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, 1, sizeof(lineCoding),
                             const_cast<uint8_t *>(lineCoding));
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, 1, 0, nullptr);
            return;
        case 14:
            if (configurationValue_) USB_SetConfiguration(fd_, configurationValue_);
            USB_SetAlternativeInterface(fd_, dataInterface_, 0);
            USB_ClearHalt(fd_, ep);
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                             0, nullptr);
            return;
        default:
            if (configurationValue_) USB_SetConfiguration(fd_, configurationValue_);
            USB_SetAlternativeInterface(fd_, dataInterface_, dataAlternate_);
            USB_ClearHalt(fd_, bulkIn_);
            USB_ClearHalt(fd_, ep);
            USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, static_cast<u16>(controlInterface_),
                             sizeof(lineCoding), const_cast<uint8_t *>(lineCoding));
            USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                             0, nullptr);
            return;
        }
    };

    unsigned attempt = 0;
    for (unsigned prepVariant = 0; prepVariant < 16; ++prepVariant) {
        for (u8 ep : endpoints) {
            for (u16 len : lengths) {
                if (len > sizeof(asyncTx_)) {
                    continue;
                }

                prep(prepVariant, ep);
                DCFlushRange(asyncTx_, len);
                char name[32];
                std::snprintf(name, sizeof(name), "p%02u-bulk-sync", prepVariant);
                s32 ret = USB_WriteBlkMsg(fd_, ep, len, asyncTx_);
                logAttempt(++attempt, name, ep, len, ret);
                if (ret >= 0) return success(attempt, name, ep, len, ret, false);

                prep(prepVariant, ep);
                DCFlushRange(asyncTx_, len);
                std::snprintf(name, sizeof(name), "p%02u-intr-sync", prepVariant);
                ret = USB_WriteIntrMsg(fd_, ep, len, asyncTx_);
                logAttempt(++attempt, name, ep, len, ret);
                if (ret >= 0) return success(attempt, name, ep, len, ret, false);
            }
        }
    }

    for (unsigned prepVariant = 0; prepVariant < 16; ++prepVariant) {
        for (u8 ep : endpoints) {
            const u16 len = static_cast<u16>(size);
            prep(prepVariant, ep);
            DCFlushRange(asyncTx_, len);
            char name[32];
            std::snprintf(name, sizeof(name), "p%02u-bulk-async", prepVariant);
            s32 ret = USB_WriteBlkMsgAsync(fd_, ep, len, asyncTx_,
                                           UsbTransport::writeCallback, this);
            logAttempt(++attempt, name, ep, len, ret);
            if (ret >= 0) return success(attempt, name, ep, len, ret, true);

            prep(prepVariant, ep);
            DCFlushRange(asyncTx_, len);
            std::snprintf(name, sizeof(name), "p%02u-intr-async", prepVariant);
            ret = USB_WriteIntrMsgAsync(fd_, ep, len, asyncTx_,
                                        UsbTransport::writeCallback, this);
            logAttempt(++attempt, name, ep, len, ret);
            if (ret >= 0) return success(attempt, name, ep, len, ret, true);
        }
    }

    if (logger_) {
        char failed[96];
        std::snprintf(failed, sizeof(failed), "USB write matrix failed attempts %u; closing serial fd", attempt);
        logger_->line(failed);
        lastError_ = failed;
    }
    close();
    return -1;
#else
    (void)buffer;
    (void)size;
    return -1;
#endif
}

int UsbTransport::writeCallback(int result, void *userdata) {
    UsbTransport *self = static_cast<UsbTransport *>(userdata);
    if (!self) {
        return 0;
    }
    self->writeResult_ = result;
    self->writeComplete_ = true;
    return 0;
}

bool UsbTransport::reassertCdcControl() {
#if defined(WIIMESH_WII)
    if (!open_) {
        lastError_ = "CDC reassert failed: fd closed";
        return false;
    }
    uint8_t lineCoding[7] = {0x00, 0xc2, 0x01, 0x00, 0x00, 0x00, 0x08};
    s32 clearIn = bulkIn_ ? USB_ClearHalt(fd_, bulkIn_) : -999;
    s32 clearOut = bulkOut_ ? USB_ClearHalt(fd_, bulkOut_) : -999;
    s32 alt = USB_SetAlternativeInterface(fd_, dataInterface_, dataAlternate_);
    s32 lc = USB_WriteCtrlMsg(fd_, 0x21, 0x20, 0, static_cast<u16>(controlInterface_),
                              sizeof(lineCoding), lineCoding);
    s32 dtr = USB_WriteCtrlMsg(fd_, 0x21, 0x22, 0x0003, static_cast<u16>(controlInterface_),
                               0, nullptr);
    char line[160];
    std::snprintf(line, sizeof(line), "cdc reassert if %u ctrl %d alt %ld clear %02x/%ld %02x/%ld line %ld dtr %ld",
                  dataInterface_, controlInterface_, static_cast<long>(alt),
                  bulkIn_, static_cast<long>(clearIn), bulkOut_, static_cast<long>(clearOut),
                  static_cast<long>(lc), static_cast<long>(dtr));
    lastError_ = line;
    if (logger_) logger_->line(lastError_);
    return lc >= 0 && dtr >= 0;
#else
    lastError_ = "CDC reassert unavailable on host";
    return false;
#endif
}

const std::string &UsbTransport::lastError() const {
    return lastError_;
}

}
