// EH3D-NODE-PNP-FEEDER - core/feeder_transport.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The layer between the frame `eh3d.feeder/1` and the wire. The frame is the same on every bus; what changes is how it travels:
//
//   RS-485  one pair, half duplex: the node switches the direction line (DE/RE) of its transceiver to talk, waits a turnaround, sends the frame as it is, and listens.
//   CAN     a frame is cut in pieces of seven bytes (one byte of each CAN frame says which piece it is), each with an identifier that names the feeder.
//   I2C     the node writes the frame to the 7-bit address of the feeder and reads the answer back; a feeder that is not there does not acknowledge its address.
//
// The three classes here are simulations of those buses, with faults that can be switched on (a frame lost or damaged, a late answer, a bus held by somebody else, two talkers at
// once, a feeder that holds the clock) and a seed, so a run is the same every time. The times are the nominal ones of the bus (bits over the rate) and the numbers of the faults
// are made up: nothing here has been measured on a real wire.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "feeder_config.hpp"
#include "feeder_frame.hpp"

namespace pnp_feeder {

/// A small generator with a seed (xorshift32): the same seed gives the same run.
class Rng {
public:
    explicit Rng(std::uint32_t seed) : s_(seed == 0 ? 0x9E3779B9u : seed) {}
    std::uint32_t next() { s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5; return s_; }
    /// True with the chance of `permille` in a thousand.
    bool chance(int permille) { return static_cast<int>(next() % 1000u) < permille; }
private:
    std::uint32_t s_;
};

/// The faults of a bus that a test switches on. All chances are in a thousand.
struct BusFaults {
    int drop_permille = 0;       // a frame (on CAN, one CAN frame) is lost, each way
    int corrupt_permille = 0;    // one bit of a frame is changed, each way (on CAN the hardware sees it and sends the frame again)
    int busy_permille = 0;       // the bus is held by somebody else: the node gives up this try
    int collision_permille = 0;  // two talk at once (RS-485: both frames are lost; CAN and I2C: the arbitration costs a frame time)
    int stretch_permille = 0;    // I2C: a feeder holds the clock
    int stretch_us = 0;          // for how long it holds it
    int delay_us = 0;            // extra time added to every exchange (a long cable, a slow feeder)
    bool cut = false;            // the whole bus is cut: nobody answers and nobody hears
};

/// What the bus counted since the start.
struct LinkStats {
    unsigned long exchanges = 0;          // requests the node put on the bus
    unsigned long busy = 0;               // tries given up because the bus was held
    unsigned long collisions = 0;         // two talkers at once
    unsigned long corrupted = 0;          // frames with a changed bit
    unsigned long dropped = 0;            // frames lost
    unsigned long nacks = 0;              // an address, or a byte, that nobody acknowledged (I2C and CAN)
    unsigned long stretch_timeouts = 0;   // a feeder held the I2C clock longer than the limit
    unsigned long late = 0;               // an answer that came after the reply timeout
    unsigned long wire_frames = 0;        // frames on the wire: CAN frames, or I2C and RS-485 transfers
    unsigned long reassembly_errors = 0;  // CAN pieces that did not make a whole frame
};

/// The other end of the bus: the feeders.
class FrameServer {
public:
    virtual ~FrameServer() = default;
    /// A feeder with this address is there and awake (it would acknowledge its address or its CAN frame).
    virtual bool present(std::uint8_t addr) const = 0;
    /// Gives a frame to the feeders; what they answer is appended to `replies`.
    virtual void serve(const Frame& request, std::vector<Frame>& replies) = 0;
};

/// One bus, simulated.
class Medium {
public:
    Medium(const Settings& s, std::uint32_t seed) : cfg_(s), rng_(seed) {}
    virtual ~Medium() = default;
    virtual BusKind kind() const = 0;
    /// Carries one request to the feeders and brings back the bytes of what they answered (frames of `eh3d.feeder/1`, empty when nothing good came back).
    /// `elapsed_us` is the time the exchange took on the wire; an answer later than `timeout_ms` is counted as late and not delivered.
    virtual std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& request, FrameServer& far, std::uint32_t timeout_ms, std::uint32_t& elapsed_us) = 0;

    BusFaults faults;
    const LinkStats& stats() const { return stats_; }

protected:
    bool roll(int permille) { return rng_.chance(permille); }  // always draws a number, so that a run does not change when a chance is switched on
    void flip_a_bit(std::vector<std::uint8_t>& bytes) {
        if (bytes.empty()) return;
        const std::uint32_t at = rng_.next() % static_cast<std::uint32_t>(bytes.size());
        const std::uint32_t bit = rng_.next() % 8u;
        bytes[at] = static_cast<std::uint8_t>(bytes[at] ^ (1u << bit));
    }
    /// Reads whole frames out of bytes that arrived and gives them to the feeders; the answers are encoded in the order they come.
    std::vector<std::vector<std::uint8_t>> deliver(const std::vector<std::uint8_t>& bytes, FrameServer& far) {
        std::vector<std::vector<std::uint8_t>> answers;
        Decoder d;
        d.push(bytes);
        Frame f;
        while (d.next(f)) {
            std::vector<Frame> replies;
            far.serve(f, replies);
            for (const Frame& r : replies) answers.push_back(encode(r));
        }
        return answers;
    }
    Settings cfg_;
    Rng rng_;
    LinkStats stats_;
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RS-485

class Rs485Medium : public Medium {
public:
    using Medium::Medium;
    BusKind kind() const override { return BusKind::Rs485; }

    /// How long a number of bytes take on the wire: ten bits each (start, eight data bits, stop).
    std::uint32_t byte_time_us(std::size_t bytes) const {
        return static_cast<std::uint32_t>(static_cast<unsigned long long>(bytes) * 10ULL * 1000000ULL / static_cast<unsigned long long>(cfg_.rs485_baud));
    }

    std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& request, FrameServer& far, std::uint32_t timeout_ms, std::uint32_t& elapsed_us) override {
        elapsed_us = static_cast<std::uint32_t>(faults.delay_us);
        ++stats_.exchanges;
        if (faults.cut) return {};
        const bool busy = roll(faults.busy_permille);
        const bool collide = roll(faults.collision_permille);
        const bool lose = roll(faults.drop_permille);
        const bool damage = roll(faults.corrupt_permille);
        // somebody else is talking: the node waits for the line to be idle for three and a half characters, then gives up this try
        if (busy) { ++stats_.busy; elapsed_us += byte_time_us(4); return {}; }
        // the node switches the direction line to transmit, waits the turnaround, sends, and switches it back
        elapsed_us += static_cast<std::uint32_t>(cfg_.rs485_turnaround_us) + byte_time_us(request.size());
        ++stats_.wire_frames;
        if (collide) { ++stats_.collisions; return {}; }  // two drivers at once: neither frame keeps its check
        if (lose) { ++stats_.dropped; return {}; }
        std::vector<std::uint8_t> sent = request;
        if (damage) { ++stats_.corrupted; flip_a_bit(sent); }
        std::vector<std::uint8_t> back;
        for (const std::vector<std::uint8_t>& answer : deliver(sent, far)) {
            // the feeder waits its own turnaround before it drives the line
            elapsed_us += static_cast<std::uint32_t>(cfg_.rs485_turnaround_us) + byte_time_us(answer.size());
            ++stats_.wire_frames;
            const bool lost = roll(faults.drop_permille);
            const bool bad = roll(faults.corrupt_permille);
            if (lost) { ++stats_.dropped; continue; }
            std::vector<std::uint8_t> bytes = answer;
            if (bad) { ++stats_.corrupted; flip_a_bit(bytes); }
            back.insert(back.end(), bytes.begin(), bytes.end());
        }
        if (elapsed_us > timeout_ms * 1000u) { ++stats_.late; return {}; }
        return back;
    }
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// CAN

/// A CAN frame with an 11-bit identifier and up to eight data bytes.
struct CanFrame {
    std::uint16_t id = 0;
    std::uint8_t dlc = 0;
    std::uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

/// The data bytes of a frame of the chain that one CAN frame carries (the first byte of the eight says which piece it is).
constexpr std::size_t kCanPieceBytes = 7;

/// The most pieces one frame of the chain is cut in.
constexpr std::size_t kCanMaxPieces = (static_cast<std::size_t>(kMaxFrameBytes) + kCanPieceBytes - 1) / kCanPieceBytes;

/// Cuts the bytes of one frame of the chain into CAN frames with the identifier `id`: the first data byte holds the number of the piece (high four bits) and the number of
/// pieces less one (low four bits), the rest are up to seven bytes of the frame.
inline std::vector<CanFrame> can_fragment(std::uint16_t id, const std::vector<std::uint8_t>& frame) {
    std::vector<CanFrame> out;
    const std::size_t pieces = frame.empty() ? 1 : (frame.size() + kCanPieceBytes - 1) / kCanPieceBytes;
    for (std::size_t i = 0; i < pieces; ++i) {
        CanFrame c;
        c.id = id;
        const std::size_t from = i * kCanPieceBytes;
        const std::size_t n = frame.size() - from < kCanPieceBytes ? frame.size() - from : kCanPieceBytes;
        c.data[0] = static_cast<std::uint8_t>((i << 4) | (pieces - 1));
        for (std::size_t k = 0; k < n; ++k) c.data[1 + k] = frame[from + k];
        c.dlc = static_cast<std::uint8_t>(1 + n);
        out.push_back(c);
    }
    return out;
}

/// Puts the pieces back together. A piece out of order, of another identifier or of another count drops what was gathered and counts an error; the check of the frame is
/// done after, by the decoder.
class CanReassembler {
public:
    /// True when `out` holds a whole frame of the chain.
    bool push(const CanFrame& c, std::vector<std::uint8_t>& out) {
        if (c.dlc < 2 || c.dlc > 8) return fail();
        const std::size_t index = c.data[0] >> 4;
        const std::size_t total = static_cast<std::size_t>(c.data[0] & 0x0F) + 1;
        if (total > kCanMaxPieces || index >= total) return fail();
        if (index == 0) {
            if (active_) ++errors_;  // a frame was started and never finished
            buf_.clear();
            active_ = true;
            id_ = c.id;
            total_ = total;
            next_ = 0;
        } else if (!active_ || c.id != id_ || total != total_ || index != next_) {
            return fail();
        }
        buf_.insert(buf_.end(), c.data + 1, c.data + c.dlc);
        ++next_;
        if (next_ < total_) return false;  // not whole yet
        out = buf_;
        reset();
        return true;
    }
    unsigned long errors() const { return errors_; }
    void reset() { buf_.clear(); active_ = false; next_ = 0; total_ = 0; id_ = 0; }

private:
    bool fail() { ++errors_; reset(); return false; }
    std::vector<std::uint8_t> buf_;
    bool active_ = false;
    std::uint16_t id_ = 0;
    std::size_t total_ = 0, next_ = 0;
    unsigned long errors_ = 0;
};

/// The identifiers of the chain on CAN: the node talks to feeder N with `base + N` and to all with `base`; feeder N answers with `base + 64 + N`.
inline std::uint16_t can_id_to(const Settings& s, std::uint8_t addr) { return static_cast<std::uint16_t>(addr == kAddrAll ? s.can_id_base : s.can_id_base + addr); }
inline std::uint16_t can_id_from(const Settings& s, std::uint8_t addr) { return static_cast<std::uint16_t>(s.can_id_base + 64 + addr); }

class CanMedium : public Medium {
public:
    using Medium::Medium;
    BusKind kind() const override { return BusKind::Can; }

    /// The nominal time of one CAN frame: 47 bits of overhead and eight per data byte (bit stuffing, which adds up to a fifth, is not counted).
    std::uint32_t frame_time_us(const CanFrame& c) const {
        return static_cast<std::uint32_t>(static_cast<unsigned long long>(47 + 8 * c.dlc) * 1000000ULL / static_cast<unsigned long long>(cfg_.can_bitrate));
    }

    std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& request, FrameServer& far, std::uint32_t timeout_ms, std::uint32_t& elapsed_us) override {
        elapsed_us = static_cast<std::uint32_t>(faults.delay_us);
        ++stats_.exchanges;
        if (faults.cut || request.size() < 6 || request[0] != kFrameStart) return {};
        const std::uint8_t target = request[2];
        const std::vector<CanFrame> pieces = can_fragment(can_id_to(cfg_, target), request);
        // the bus is busy with traffic of a more important identifier: the try is given up
        if (roll(faults.busy_permille)) { ++stats_.busy; elapsed_us += frame_time_us(pieces[0]); return {}; }
        const bool acked = target == kAddrAll || far.present(target);
        CanReassembler asm_far;
        std::vector<std::uint8_t> whole;
        bool complete = false;
        for (const CanFrame& p : pieces) {
            if (!send_one(p, acked, elapsed_us)) continue;
            std::vector<std::uint8_t> got;
            if (asm_far.push(p, got)) { whole = got; complete = true; }
        }
        stats_.reassembly_errors += asm_far.errors();
        if (!complete) return {};
        std::vector<std::uint8_t> back;
        for (const std::vector<std::uint8_t>& answer : deliver(whole, far)) {
            const std::vector<CanFrame> back_pieces = can_fragment(can_id_from(cfg_, target), answer);
            CanReassembler asm_node;
            std::vector<std::uint8_t> got;
            bool done = false;
            for (const CanFrame& p : back_pieces) {
                if (!send_one(p, true, elapsed_us)) continue;
                std::vector<std::uint8_t> g;
                if (asm_node.push(p, g)) { got = g; done = true; }
            }
            stats_.reassembly_errors += asm_node.errors();
            if (done) back.insert(back.end(), got.begin(), got.end());
        }
        if (elapsed_us > timeout_ms * 1000u) { ++stats_.late; return {}; }
        return back;
    }

private:
    /// One CAN frame on the wire. The hardware checks every frame: one with a changed bit is not delivered, it is sent again (up to three times); one that nobody acknowledges
    /// is sent again too. A frame that the bus loses is lost.
    bool send_one(const CanFrame& p, bool acknowledged, std::uint32_t& elapsed_us) {
        const std::uint32_t one = frame_time_us(p);
        const std::uint32_t error_frame = static_cast<std::uint32_t>(20ULL * 1000000ULL / static_cast<unsigned long long>(cfg_.can_bitrate));
        for (int attempt = 0; attempt < 3; ++attempt) {
            const bool damage = roll(faults.corrupt_permille);
            const bool lose = roll(faults.drop_permille);
            const bool arbitration = roll(faults.collision_permille);
            ++stats_.wire_frames;
            if (arbitration) { ++stats_.collisions; elapsed_us += one; }  // a frame of a higher priority goes first
            elapsed_us += one;
            if (!acknowledged) { ++stats_.nacks; elapsed_us += error_frame; continue; }
            if (damage) { ++stats_.corrupted; elapsed_us += error_frame; continue; }
            if (lose) { ++stats_.dropped; return false; }
            return true;
        }
        return false;
    }
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// I2C

/// The 7-bit address of feeder N, or 0 (the general call) for all of them; 0xFF when the feeder has no address on this bus.
inline std::uint8_t i2c_address_of(const Settings& s, std::uint8_t feeder) {
    if (feeder == kAddrAll) return 0;
    if (feeder < 1 || feeder > s.slots) return 0xFF;
    const int a = s.i2c_base_addr + feeder - 1;
    return a > kI2cLastUsable ? 0xFF : static_cast<std::uint8_t>(a);
}

class I2cMedium : public Medium {
public:
    using Medium::Medium;
    BusKind kind() const override { return BusKind::I2c; }

    /// The time of a number of clocks at the rate of the bus.
    std::uint32_t clocks_us(std::size_t clocks) const {
        return static_cast<std::uint32_t>(static_cast<unsigned long long>(clocks) * 1000000ULL / static_cast<unsigned long long>(cfg_.i2c_clock_hz));
    }

    std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& request, FrameServer& far, std::uint32_t timeout_ms, std::uint32_t& elapsed_us) override {
        elapsed_us = static_cast<std::uint32_t>(faults.delay_us);
        ++stats_.exchanges;
        if (faults.cut || request.size() < 6 || request[0] != kFrameStart) return {};
        const std::uint8_t target = request[2];
        const std::uint8_t address = i2c_address_of(cfg_, target);
        const bool busy = roll(faults.busy_permille);
        const bool arbitration = roll(faults.collision_permille);
        const bool hold = roll(faults.stretch_permille);
        const bool lose = roll(faults.drop_permille);
        const bool damage = roll(faults.corrupt_permille);
        // SDA or SCL is held low by somebody else: the start condition cannot be made
        if (busy) { ++stats_.busy; elapsed_us += clocks_us(10); return {}; }
        // the node writes: start, the address and the write bit, then the frame, nine clocks for each byte
        elapsed_us += clocks_us(1 + 9);
        ++stats_.wire_frames;
        if (address == 0xFF || (target != kAddrAll && !far.present(target))) { ++stats_.nacks; elapsed_us += clocks_us(1); return {}; }  // nobody acknowledges the address
        if (arbitration) { ++stats_.collisions; elapsed_us += clocks_us(request.size() * 9 + 2); }  // another master won: the node waits and writes again
        if (hold) {
            // a feeder busy with its own work holds the clock low; past the limit the node gives up
            const std::uint32_t limit = static_cast<std::uint32_t>(cfg_.i2c_stretch_limit_us);
            const std::uint32_t held = static_cast<std::uint32_t>(faults.stretch_us);
            if (held > limit) { ++stats_.stretch_timeouts; elapsed_us += limit; return {}; }
            elapsed_us += held;
        }
        elapsed_us += clocks_us(request.size() * 9 + 1);
        if (lose) { ++stats_.nacks; ++stats_.dropped; return {}; }  // a byte was not acknowledged: the write is cut
        std::vector<std::uint8_t> sent = request;
        if (damage) { ++stats_.corrupted; flip_a_bit(sent); }
        std::vector<std::uint8_t> back;
        for (const std::vector<std::uint8_t>& answer : deliver(sent, far)) {
            // a repeated start, the address and the read bit, then the bytes the feeder gives
            elapsed_us += clocks_us(1 + 9 + answer.size() * 9 + 1);
            ++stats_.wire_frames;
            const bool lost = roll(faults.drop_permille);
            const bool bad = roll(faults.corrupt_permille);
            if (lost) { ++stats_.dropped; continue; }
            std::vector<std::uint8_t> bytes = answer;
            if (bad) { ++stats_.corrupted; flip_a_bit(bytes); }
            back.insert(back.end(), bytes.begin(), bytes.end());
        }
        if (elapsed_us > timeout_ms * 1000u) { ++stats_.late; return {}; }
        return back;
    }
};

/// The simulated bus the settings name.
inline std::unique_ptr<Medium> make_medium(const Settings& s, std::uint32_t seed) {
    switch (s.bus) {
        case BusKind::Can: return std::make_unique<CanMedium>(s, seed);
        case BusKind::I2c: return std::make_unique<I2cMedium>(s, seed);
        default: return std::make_unique<Rs485Medium>(s, seed);
    }
}

}  // namespace pnp_feeder
