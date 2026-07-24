#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wiimesh {

class ProtoReader {
public:
    ProtoReader(const uint8_t *data, size_t size);
    bool next(uint32_t &field, uint32_t &wire);
    bool readVarint(uint64_t &value);
    bool readFixed32(uint32_t &value);
    bool readBytes(std::vector<uint8_t> &value);
    bool readString(std::string &value);
    bool skip(uint32_t wire);
    bool ok() const;

private:
    const uint8_t *data_;
    size_t size_;
    size_t pos_ = 0;
    bool ok_ = true;
};

std::vector<uint8_t> encodeVarint(uint64_t value);
void appendKey(std::vector<uint8_t> &out, uint32_t field, uint32_t wire);
void appendVarint(std::vector<uint8_t> &out, uint32_t field, uint64_t value);
void appendFixed32(std::vector<uint8_t> &out, uint32_t field, uint32_t value);
void appendBytes(std::vector<uint8_t> &out, uint32_t field, const std::vector<uint8_t> &value);
void appendString(std::vector<uint8_t> &out, uint32_t field, const std::string &value);

}
