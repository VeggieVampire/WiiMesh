#include "wiimesh/ProtoReader.h"

namespace wiimesh {

ProtoReader::ProtoReader(const uint8_t *data, size_t size) : data_(data), size_(size) {}

bool ProtoReader::next(uint32_t &field, uint32_t &wire) {
    if (pos_ >= size_) {
        return false;
    }
    uint64_t key = 0;
    if (!readVarint(key)) {
        return false;
    }
    field = static_cast<uint32_t>(key >> 3);
    wire = static_cast<uint32_t>(key & 0x07);
    return field != 0;
}

bool ProtoReader::readVarint(uint64_t &value) {
    value = 0;
    uint32_t shift = 0;
    while (pos_ < size_ && shift < 64) {
        const uint8_t b = data_[pos_++];
        value |= static_cast<uint64_t>(b & 0x7f) << shift;
        if ((b & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    ok_ = false;
    return false;
}

bool ProtoReader::readFixed32(uint32_t &value) {
    if (pos_ + 4 > size_) {
        ok_ = false;
        return false;
    }
    value = static_cast<uint32_t>(data_[pos_]) |
            (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
            (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
            (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return true;
}

bool ProtoReader::readBytes(std::vector<uint8_t> &value) {
    uint64_t len = 0;
    if (!readVarint(len) || pos_ + len > size_) {
        ok_ = false;
        return false;
    }
    value.assign(data_ + pos_, data_ + pos_ + len);
    pos_ += static_cast<size_t>(len);
    return true;
}

bool ProtoReader::readString(std::string &value) {
    std::vector<uint8_t> bytes;
    if (!readBytes(bytes)) {
        return false;
    }
    value.assign(bytes.begin(), bytes.end());
    return true;
}

bool ProtoReader::skip(uint32_t wire) {
    uint64_t v = 0;
    std::vector<uint8_t> b;
    uint32_t f32 = 0;
    switch (wire) {
    case 0: return readVarint(v);
    case 1:
        if (pos_ + 8 > size_) { ok_ = false; return false; }
        pos_ += 8;
        return true;
    case 2: return readBytes(b);
    case 5: return readFixed32(f32);
    default:
        ok_ = false;
        return false;
    }
}

bool ProtoReader::ok() const {
    return ok_;
}

std::vector<uint8_t> encodeVarint(uint64_t value) {
    std::vector<uint8_t> out;
    do {
        uint8_t b = static_cast<uint8_t>(value & 0x7f);
        value >>= 7;
        if (value) b |= 0x80;
        out.push_back(b);
    } while (value);
    return out;
}

void appendKey(std::vector<uint8_t> &out, uint32_t field, uint32_t wire) {
    auto key = encodeVarint((field << 3) | wire);
    out.insert(out.end(), key.begin(), key.end());
}

void appendVarint(std::vector<uint8_t> &out, uint32_t field, uint64_t value) {
    appendKey(out, field, 0);
    auto encoded = encodeVarint(value);
    out.insert(out.end(), encoded.begin(), encoded.end());
}

void appendFixed32(std::vector<uint8_t> &out, uint32_t field, uint32_t value) {
    appendKey(out, field, 5);
    out.push_back(value & 0xff);
    out.push_back((value >> 8) & 0xff);
    out.push_back((value >> 16) & 0xff);
    out.push_back((value >> 24) & 0xff);
}

void appendBytes(std::vector<uint8_t> &out, uint32_t field, const std::vector<uint8_t> &value) {
    appendKey(out, field, 2);
    auto len = encodeVarint(value.size());
    out.insert(out.end(), len.begin(), len.end());
    out.insert(out.end(), value.begin(), value.end());
}

void appendString(std::vector<uint8_t> &out, uint32_t field, const std::string &value) {
    appendBytes(out, field, std::vector<uint8_t>(value.begin(), value.end()));
}

}
