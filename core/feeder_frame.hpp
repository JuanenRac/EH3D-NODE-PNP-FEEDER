// EH3D-NODE-PNP-FEEDER - core/feeder_frame.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The frame of the chain bus, `eh3d.feeder/1`: what the node says to a feeder and what the feeder answers. One frame is
//
//   0x7E | version | address | command | sequence | length | payload (0..32 bytes) | CRC-16 (big endian)
//
// The CRC is CRC-16/CCITT-FALSE (polynomial 0x1021, start 0xFFFF) over everything between the start byte and the CRC. Address 0 is the node, 1..32 a feeder, 0xFF all of
// them (a command to all is never answered). An answer carries the command with the high bit set; an error is the command 0xFF with the code in the first byte of the
// payload. A reader that finds a bad frame drops one byte and looks for the next start byte, so a burst of noise costs one frame and nothing else.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pnp_feeder {

constexpr std::uint8_t kFrameStart = 0x7E;
constexpr std::uint8_t kFrameVersion = 1;
constexpr std::uint8_t kAddrNode = 0;
constexpr std::uint8_t kAddrAll = 0xFF;
constexpr std::size_t kMaxPayload = 32;

enum Cmd : std::uint8_t {
    kCmdPing = 0x01,       // answers with the status
    kCmdIdentify = 0x02,   // the feeder blinks its LED for a few seconds; answers with the firmware version (3 bytes)
    kCmdAdvance = 0x03,    // payload: how many pitches (1..8)
    kCmdRetract = 0x04,    // payload: how many pitches (1..8); refused unless the feeder allows it
    kCmdStatus = 0x05,     // answers with the status
    kCmdSetCount = 0x06,   // payload: the parts left in the tape, 2 bytes (0xFFFF: not counted)
    kCmdStop = 0x07,       // stops a move at once
    kCmdAck = 0x08,        // clears a fault that is not present any more; payload 1 byte: 1 also re-enables a feeder disabled by too many faults
    kCmdSetPitch = 0x09,   // payload: 2, 4, 8, 12 or 16
    kReplyFlag = 0x80,
    kCmdError = 0xFF,
};

/// The codes an error answer carries.
enum ErrCode : std::uint8_t {
    kErrBadCommand = 1,
    kErrBadArgument = 2,
    kErrBusy = 3,
    kErrFault = 4,        // a fault is held: acknowledge it first
    kErrEmpty = 5,        // no tape or no parts left
    kErrTension = 6,
    kErrDisabled = 7,
    kErrSafe = 8,         // the feeder is in its safe state after losing the node
    kErrStillActive = 9,  // an acknowledgement while the cause is still there
    kErrRetractOff = 10,
};

struct Frame {
    std::uint8_t addr = 0;
    std::uint8_t cmd = 0;
    std::uint8_t seq = 0;
    std::vector<std::uint8_t> payload;
};

inline std::uint16_t crc16(const std::uint8_t* data, std::size_t n) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < n; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(data[i]) << 8));
        for (int b = 0; b < 8; ++b) crc = static_cast<std::uint16_t>((crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1));
    }
    return crc;
}

inline std::vector<std::uint8_t> encode(const Frame& f) {
    std::vector<std::uint8_t> out;
    out.push_back(kFrameStart);
    out.push_back(kFrameVersion);
    out.push_back(f.addr);
    out.push_back(f.cmd);
    out.push_back(f.seq);
    std::size_t n = f.payload.size() > kMaxPayload ? kMaxPayload : f.payload.size();
    out.push_back(static_cast<std::uint8_t>(n));
    for (std::size_t i = 0; i < n; ++i) out.push_back(f.payload[i]);
    std::uint16_t crc = crc16(out.data() + 1, out.size() - 1);
    out.push_back(static_cast<std::uint8_t>(crc >> 8));
    out.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    return out;
}

/// Reads frames out of a stream of bytes that may have noise in it.
class Decoder {
public:
    void push(const std::uint8_t* data, std::size_t n) { buf_.insert(buf_.end(), data, data + n); }
    void push(const std::vector<std::uint8_t>& v) { push(v.data(), v.size()); }

    /// The next whole frame with a good check; false when there is none yet.
    bool next(Frame& out) {
        for (;;) {
            std::size_t s = 0;
            while (s < buf_.size() && buf_[s] != kFrameStart) { ++s; ++dropped_; }
            buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(s));
            if (buf_.size() < 6) return false;
            if (buf_[1] != kFrameVersion || buf_[5] > kMaxPayload) { drop_one(); continue; }
            std::size_t need = 6 + static_cast<std::size_t>(buf_[5]) + 2;
            if (buf_.size() < need) return false;
            std::uint16_t want = crc16(buf_.data() + 1, need - 3);
            std::uint16_t have = static_cast<std::uint16_t>((buf_[need - 2] << 8) | buf_[need - 1]);
            if (want != have) { ++bad_crc_; drop_one(); continue; }
            out.addr = buf_[2];
            out.cmd = buf_[3];
            out.seq = buf_[4];
            out.payload.assign(buf_.begin() + 6, buf_.begin() + 6 + buf_[5]);
            buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(need));
            return true;
        }
    }

    unsigned long bad_crc() const { return bad_crc_; }
    unsigned long dropped() const { return dropped_; }

private:
    void drop_one() { buf_.erase(buf_.begin()); ++dropped_; }
    std::vector<std::uint8_t> buf_;
    unsigned long bad_crc_ = 0;
    unsigned long dropped_ = 0;
};

}  // namespace pnp_feeder
