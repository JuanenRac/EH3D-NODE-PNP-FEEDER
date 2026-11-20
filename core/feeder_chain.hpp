// EH3D-NODE-PNP-FEEDER - core/feeder_chain.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The node's side of the chain: it talks to the feeders through a `Bus` (the serial port of the firmware, or the simulator of the tests), sends a frame again when no
// answer or a bad one comes back, finds the feeders that are there, and runs an advance from the order to the end of the move. A slot that fails `offline_after` times in
// a row is marked offline and no advance is sent to it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "feeder_config.hpp"
#include "feeder_frame.hpp"
#include "feeder_unit.hpp"

namespace pnp_feeder {

constexpr std::uint8_t kErrNoAnswer = 100;  // not a code of the bus: nobody answered

class Bus {
public:
    virtual ~Bus() = default;
    /// Sends the bytes and returns what came back within the time (empty when nothing did).
    virtual std::vector<std::uint8_t> exchange(const std::vector<std::uint8_t>& request, std::uint32_t timeout_ms) = 0;
    /// Lets time pass (the firmware sleeps; the simulator runs its feeders).
    virtual void wait(std::uint32_t ms) = 0;
    virtual std::uint32_t now_ms() const = 0;
};

inline const char* error_text(std::uint8_t code) {
    switch (code) {
        case kErrBadCommand: return "bad_command";
        case kErrBadArgument: return "bad_argument";
        case kErrBusy: return "busy";
        case kErrFault: return "fault";
        case kErrEmpty: return "empty";
        case kErrTension: return "tension";
        case kErrDisabled: return "disabled";
        case kErrSafe: return "safe_state";
        case kErrStillActive: return "still_active";
        case kErrRetractOff: return "retract_off";
        case kErrNoAnswer: return "no_answer";
        default: return "error";
    }
}

struct SlotInfo {
    bool online = false;
    bool seen = false;          // a status was read at least once
    int fails = 0;              // exchanges without a good answer in a row
    State state = State::Idle;
    Fault fault = Fault::None;
    std::uint16_t remaining = kCountUnknown;
    std::uint8_t flags = 0;     // 1 tape, 2 pocket, 4 tension ok, 8 film clear, 16 low
    std::uint8_t pitch = 0;
    std::uint8_t fw[3] = {0, 0, 0};
};

struct Reply {
    bool ok = false;
    std::uint8_t error = 0;
    Frame frame;
    int attempts = 0;
};

struct Outcome {
    bool ok = false;
    std::string why = "ok";   // ok, or the text of the refusal, or timeout
    int slot = 0;
};

class Controller {
public:
    Controller(Bus& bus, const Settings& settings) : bus_(bus), cfg_(settings), slots_(static_cast<std::size_t>(settings.slots) + 1) {}

    const SlotInfo& slot(int n) const { return slots_[static_cast<std::size_t>(n)]; }
    int slot_count() const { return cfg_.slots; }
    unsigned long exchanges() const { return exchanges_; }
    unsigned long retries() const { return retries_; }
    unsigned long bus_errors() const { return errors_; }

    /// One command to one feeder, sent again up to `bus_retries` times when nothing good comes back.
    Reply request(std::uint8_t addr, std::uint8_t cmd, const std::vector<std::uint8_t>& payload = {}) {
        Reply r;
        Frame f;
        f.addr = addr; f.cmd = cmd; f.seq = ++seq_; f.payload = payload;
        const std::vector<std::uint8_t> bytes = encode(f);
        for (int attempt = 0; attempt <= cfg_.bus_retries; ++attempt) {
            ++r.attempts;
            ++exchanges_;
            if (attempt > 0) ++retries_;
            Decoder d;
            d.push(bus_.exchange(bytes, 20));
            Frame got;
            bool have = false;
            while (d.next(got)) { if (got.seq == f.seq && got.addr == kAddrNode) { have = true; break; } }
            if (!have) { ++errors_; continue; }
            r.frame = got;
            if (got.cmd == kCmdError) {
                r.error = got.payload.empty() ? kErrBadCommand : got.payload[0];
                note(addr, true);
                // an error answer carries no status: ask for it, so that what the node shows is what the feeder holds
                if (cmd != kCmdStatus && cmd != kCmdPing && addr != kAddrAll) request(addr, kCmdStatus);
                return r;
            }
            r.ok = true;
            note(addr, true);
            update(addr, got);
            return r;
        }
        r.error = kErrNoAnswer;
        note(addr, false);
        return r;
    }

    /// A command to all the feeders: not answered, so it is sent twice (the bus is noisy).
    void broadcast(std::uint8_t cmd, const std::vector<std::uint8_t>& payload = {}) {
        Frame f;
        f.addr = kAddrAll; f.cmd = cmd; f.seq = ++seq_; f.payload = payload;
        const std::vector<std::uint8_t> bytes = encode(f);
        bus_.exchange(bytes, 5);
        bus_.exchange(bytes, 5);
    }

    /// Asks every slot for its status at the start and after a cut of the bus: the ones that answer are online.
    int discover() {
        int found = 0;
        for (int n = 1; n <= cfg_.slots; ++n) {
            SlotInfo& s = slots_[static_cast<std::size_t>(n)];
            s.fails = 0;
            Reply r = request(static_cast<std::uint8_t>(n), kCmdPing);
            if (r.ok) ++found; else s.online = false;  // who is there now, not who was there before
        }
        return found;
    }

    /// An advance from the order to the end of the move.
    Outcome advance(int slot_n, int count, bool backward = false) {
        Outcome o;
        o.slot = slot_n;
        if (slot_n < 1 || slot_n > cfg_.slots) { o.ok = false; o.why = "bad_slot"; return o; }
        if (count < 1 || count > 8) { o.ok = false; o.why = "bad_argument"; return o; }
        SlotInfo& s = slots_[static_cast<std::size_t>(slot_n)];
        if (!s.online) { o.ok = false; o.why = "offline"; return o; }
        Reply r = request(static_cast<std::uint8_t>(slot_n), backward ? kCmdRetract : kCmdAdvance, {static_cast<std::uint8_t>(count)});
        if (!r.ok) { o.ok = false; o.why = error_text(r.error); return o; }
        std::uint32_t waited = 0;
        const std::uint32_t poll = 20;
        while (s.state == State::Advancing || s.state == State::Retracting) {
            if (waited >= static_cast<std::uint32_t>(cfg_.advance_timeout_ms)) {
                request(static_cast<std::uint8_t>(slot_n), kCmdStop);
                o.ok = false; o.why = "timeout";
                return o;
            }
            bus_.wait(poll);
            waited += poll;
            if (!request(static_cast<std::uint8_t>(slot_n), kCmdStatus).ok) { o.ok = false; o.why = "no_answer"; return o; }
        }
        switch (s.state) {
            case State::Idle: o.ok = true; o.why = "ok"; break;
            case State::Fault:
            case State::Disabled:
            case State::Safe: o.ok = false; o.why = "fault"; break;
            case State::Empty: o.ok = false; o.why = "empty"; break;
            default: o.ok = false; o.why = "error"; break;
        }
        return o;
    }

private:
    void note(std::uint8_t addr, bool good) {
        if (addr < 1 || addr > cfg_.slots) return;
        SlotInfo& s = slots_[addr];
        if (good) { s.fails = 0; s.online = true; }
        else if (++s.fails >= cfg_.offline_after) s.online = false;
    }

    void update(std::uint8_t addr, const Frame& f) {
        if (addr < 1 || addr > cfg_.slots) return;
        SlotInfo& s = slots_[addr];
        if ((f.cmd & 0x7F) == kCmdIdentify && f.payload.size() == 3) { for (int i = 0; i < 3; ++i) s.fw[i] = f.payload[static_cast<std::size_t>(i)]; return; }
        if (f.payload.size() != 6) return;
        s.seen = true;
        s.state = static_cast<State>(f.payload[0]);
        s.fault = static_cast<Fault>(f.payload[1]);
        s.remaining = static_cast<std::uint16_t>((f.payload[2] << 8) | f.payload[3]);
        s.flags = f.payload[4];
        s.pitch = f.payload[5];
    }

    Bus& bus_;
    Settings cfg_;
    std::vector<SlotInfo> slots_;
    std::uint8_t seq_ = 0;
    unsigned long exchanges_ = 0, retries_ = 0, errors_ = 0;
};

}  // namespace pnp_feeder
