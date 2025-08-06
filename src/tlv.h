#pragma once
#include <Arduino.h>
#include <vector>

enum : uint8_t {
    kTLVType_UInt     = 0x04,
    kTLVType_MaskLen  = 0x1F,
    kTLVLen_1Byte     = 0x00,
    kTLVLen_2Bytes    = 0x01,
};

enum PaseOpcode : uint8_t {
    kPBKDFParamRequest  = 0x01,
    kPBKDFParamResponse = 0x02,
    kPASEPake1          = 0x03,
    kPASEPake2          = 0x04,
    kPASEPake3          = 0x05
};

// For PASE
struct PairingState {
    bool     active  = false;
    uint8_t  step    = 0;
    uint16_t sessionID = 0;
    uint8_t  peerAddr[16];
} pairing;

inline void tlvEncodeUInt8(std::vector<uint8_t>& buf, uint8_t tag, uint8_t value) {
    buf.push_back(kTLVType_UInt | (tag << 5) | kTLVLen_1Byte);
    buf.push_back(1);
    buf.push_back(value);
}

inline void tlvEncodeUInt16(std::vector<uint8_t>& buf, uint8_t tag, uint16_t value) {
    buf.push_back(kTLVType_UInt | (tag << 5) | kTLVLen_2Bytes);
    buf.push_back(value & 0xFF);
    buf.push_back(value >> 8);
}

inline bool tlvDecodeNext(const uint8_t* p, size_t len,
                          uint8_t& tag, const uint8_t*& val,
                          size_t& vlen, size_t& consumed)
{
    if (len < 2) return false;

    uint8_t ctl = p[0];
    tag = (ctl >> 5) & 0x07;

    uint8_t baseType = ctl & 0x1E;
    if (baseType != kTLVType_UInt)
        return false;

    uint8_t lenCode = ctl & 0x01;

    if (lenCode == 0) {
        if (len < 3 || p[1] != 1) return false;
        vlen     = 1;
        val      = p + 2;
        consumed = 3;
    } else {
        vlen     = 2;
        if (len < 3) return false;
        val      = p + 1;
        consumed = 3;
    }
    return true;
}

inline void tlvEncodeBytes(std::vector<uint8_t>& buf, uint8_t tag,
                           const uint8_t* data, size_t len)
{
    buf.push_back(0x10 | (tag << 5));
    buf.push_back(len);
    buf.insert(buf.end(), data, data + len);
}