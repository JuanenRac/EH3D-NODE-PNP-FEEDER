// EH3D-NODE-PNP-FEEDER - core/feeder_unit.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// What one feeder does with the tape: the logic that the firmware of a feeder runs and that the simulator runs on a computer. It takes frames from the node and the
// readings of the sensors (tape present, pocket found, tension, film peeled, motor stalled) and moves the tape. No hardware is touched here: the motor and the sensors
// are the caller's.
//
// The rules, in the order they are checked when an advance is asked: the feeder must hear the node (else it is in its safe state), must not be disabled or holding a
// fault, must have tape and parts left, and the tension must be in its band. After the move it looks for the pocket (closed loop) and, if it is not there, makes up to
// `max_retries` small moves before it holds a fault. A feeder that holds `fault_limit` faults in its last `fault_window` advances disables itself.
#pragma once

#include <cstdint>
#include <vector>

#include "feeder_config.hpp"
#include "feeder_frame.hpp"

namespace pnp_feeder {

enum class State : std::uint8_t { Idle = 0, Advancing = 1, Retracting = 2, Fault = 3, Empty = 4, Disabled = 5, Safe = 6 };

enum class Fault : std::uint8_t {
    None = 0,
    Stall = 1,         // the motor was stopped by the tape (a jam)
    NoPocket = 2,      // the pocket was not found after the retries
    TensionLow = 3,    // the tape is slack
    TensionHigh = 4,   // the tape is too tight
    Film = 5,          // the cover film is not peeled
    LinkLost = 6,      // the node was not heard
    TooManyFaults = 7  // disabled by the limit of faults
};

inline const char* state_text(State s) {
    switch (s) {
        case State::Idle: return "idle";
        case State::Advancing: return "advancing";
        case State::Retracting: return "retracting";
        case State::Fault: return "fault";
        case State::Empty: return "empty";
        case State::Disabled: return "disabled";
        case State::Safe: return "safe";
    }
    return "unknown";
}

/// The code of a fault as the programs read it (FDR-xxx); a code is never reused for another meaning.
inline const char* fault_code(Fault f) {
    switch (f) {
        case Fault::None: return "none";
        case Fault::Stall: return "FDR-J01";
        case Fault::NoPocket: return "FDR-J02";
        case Fault::TensionLow: return "FDR-T01";
        case Fault::TensionHigh: return "FDR-T02";
        case Fault::Film: return "FDR-F01";
        case Fault::LinkLost: return "FDR-L01";
        case Fault::TooManyFaults: return "FDR-D01";
    }
    return "none";
}

/// Every code a feeder can hold, for the page of the contract and its test.
inline std::vector<const char*> all_fault_codes() { return {"FDR-J01", "FDR-J02", "FDR-T01", "FDR-T02", "FDR-F01", "FDR-L01", "FDR-D01"}; }

struct Sensors {
    bool tape_present = true;
    bool pocket_found = true;
    bool film_clear = true;
    bool stalled = false;
    int tension = 500;  // 0..1023
};

struct UnitConfig {
    int pitch_mm = 4;
    double motion_mm_per_s = 20.0;
    double retry_mm = 0.5;
    int max_retries = 2;
    bool pocket_check = true;
    bool film_check = true;
    int tension_min = 200;
    int tension_max = 800;
    int low_pockets = 20;
    bool retract_enabled = false;
    int link_lost_ms = 3000;
    int fault_window = 6;
    int fault_limit = 3;

    static UnitConfig from(const Settings& s) {
        UnitConfig c;
        c.pitch_mm = s.pitch_mm; c.motion_mm_per_s = s.motion_mm_per_s; c.retry_mm = s.retry_mm; c.max_retries = s.max_retries;
        c.pocket_check = s.pocket_check; c.film_check = s.film_check; c.tension_min = s.tension_min; c.tension_max = s.tension_max;
        c.low_pockets = s.low_pockets; c.retract_enabled = s.retract_enabled; c.link_lost_ms = s.link_lost_ms;
        c.fault_window = s.fault_window; c.fault_limit = s.fault_limit;
        return c;
    }
};

constexpr std::uint16_t kCountUnknown = 0xFFFF;
constexpr std::uint8_t kFirmwareMajor = 0, kFirmwareMinor = 0, kFirmwarePatch = 1;

class Unit {
public:
    Unit(const UnitConfig& config, std::uint8_t addr) : cfg_(config), addr_(addr), pitch_(config.pitch_mm) {}

    State state() const { return state_; }
    Fault fault() const { return fault_; }
    std::uint8_t address() const { return addr_; }
    int pitch_mm() const { return pitch_; }
    std::uint16_t remaining() const { return remaining_; }
    bool low() const { return remaining_ != kCountUnknown && remaining_ <= static_cast<std::uint16_t>(cfg_.low_pockets); }
    unsigned long advances_done() const { return advances_done_; }
    unsigned long identify_until_ms() const { return identify_until_; }

    /// The distance the tape moved in the last tick, in mm (the simulator adds it to the position of the tape; the firmware counts the steps it made).
    double step_mm() const { return step_mm_; }

    /// Answers one frame addressed to this feeder (or to all). Returns false when there is no answer (a command to all is never answered).
    bool handle(const Frame& req, std::uint32_t now_ms, Frame& reply) {
        if (req.addr != addr_ && req.addr != kAddrAll) return false;
        heard_ = true;
        last_heard_ = now_ms;
        const bool to_all = req.addr == kAddrAll;
        reply.addr = kAddrNode;
        reply.seq = req.seq;
        reply.payload.clear();
        bool ok = true;
        std::uint8_t err = 0;
        if (req.cmd != kCmdAdvance && req.cmd != kCmdRetract) have_move_seq_ = false;  // any other order ends the window in which a repeat is told from a new order
        switch (req.cmd) {
            case kCmdPing:
            case kCmdStatus: fill_status(reply.payload); break;
            case kCmdIdentify:
                identify_until_ = now_ms + 3000;
                reply.payload = {kFirmwareMajor, kFirmwareMinor, kFirmwarePatch};
                break;
            case kCmdAdvance:
            case kCmdRetract:
                // the node sends a frame again when the answer was lost: the same sequence number is the same order, and a move is never made twice
                if (have_move_seq_ && move_seq_ == req.seq) { fill_status(reply.payload); break; }
                ok = start_move(req, req.cmd == kCmdRetract, err);
                if (ok) { have_move_seq_ = true; move_seq_ = req.seq; }
                fill_status(reply.payload);
                break;
            case kCmdSetCount:
                if (req.payload.size() != 2) { ok = false; err = kErrBadArgument; break; }
                remaining_ = static_cast<std::uint16_t>((req.payload[0] << 8) | req.payload[1]);
                if (state_ == State::Empty && remaining_ != 0) state_ = State::Idle;  // a new spool: the sensors say again if there is tape
                fill_status(reply.payload);
                break;
            case kCmdStop:
                if (state_ == State::Advancing || state_ == State::Retracting) { state_ = State::Idle; target_mm_ = moved_mm_ = 0; }
                fill_status(reply.payload);
                break;
            case kCmdAck: ok = acknowledge(req.payload.empty() ? 0 : req.payload[0], err); fill_status(reply.payload); break;
            case kCmdSetPitch:
                if (req.payload.size() != 1 || !valid_pitch(req.payload[0])) { ok = false; err = kErrBadArgument; break; }
                if (state_ == State::Advancing || state_ == State::Retracting) { ok = false; err = kErrBusy; break; }
                pitch_ = req.payload[0];
                fill_status(reply.payload);
                break;
            default: ok = false; err = kErrBadCommand; break;
        }
        if (to_all) return false;
        if (ok) {
            reply.cmd = static_cast<std::uint8_t>(req.cmd | kReplyFlag);
        } else {
            reply.cmd = kCmdError;
            reply.payload = {err};
        }
        return true;
    }

    /// Moves time on by `dt_ms` with the readings of the sensors at this moment.
    void tick(std::uint32_t now_ms, std::uint32_t dt_ms, const Sensors& s) {
        step_mm_ = 0;
        sensors_ = s;
        if (heard_ && state_ != State::Safe && now_ms - last_heard_ > static_cast<std::uint32_t>(cfg_.link_lost_ms)) {
            // the node is gone: stop whatever is moving and hold a fault until a person acknowledges it
            target_mm_ = moved_mm_ = 0;
            state_ = State::Safe;
            fault_ = Fault::LinkLost;
            return;
        }
        if (state_ == State::Idle && !s.tape_present) state_ = State::Empty;
        if (state_ != State::Advancing && state_ != State::Retracting) return;
        if (s.stalled) { raise(Fault::Stall); return; }
        if (s.tension < cfg_.tension_min) { raise(Fault::TensionLow); return; }
        if (s.tension > cfg_.tension_max) { raise(Fault::TensionHigh); return; }
        double step = cfg_.motion_mm_per_s * static_cast<double>(dt_ms) / 1000.0;
        double left = target_mm_ - moved_mm_;
        if (step > left) step = left;
        moved_mm_ += step;
        step_mm_ = state_ == State::Retracting ? -step : step;
        if (moved_mm_ + 1e-9 < target_mm_) return;
        finish_move(s);
    }

private:
    void fill_status(std::vector<std::uint8_t>& p) const {
        std::uint8_t flags = 0;
        if (sensors_.tape_present) flags |= 1;
        if (sensors_.pocket_found) flags |= 2;
        if (sensors_.tension >= cfg_.tension_min && sensors_.tension <= cfg_.tension_max) flags |= 4;
        if (sensors_.film_clear) flags |= 8;
        if (low()) flags |= 16;
        p = {static_cast<std::uint8_t>(state_), static_cast<std::uint8_t>(fault_), static_cast<std::uint8_t>(remaining_ >> 8), static_cast<std::uint8_t>(remaining_ & 0xFF), flags,
             static_cast<std::uint8_t>(pitch_)};
    }

    bool start_move(const Frame& req, bool backward, std::uint8_t& err) {
        if (req.payload.size() != 1 || req.payload[0] < 1 || req.payload[0] > 8) { err = kErrBadArgument; return false; }
        if (state_ == State::Safe) { err = kErrSafe; return false; }
        if (state_ == State::Disabled) { err = kErrDisabled; return false; }
        if (state_ == State::Fault) { err = kErrFault; return false; }
        if (state_ == State::Advancing || state_ == State::Retracting) { err = kErrBusy; return false; }
        if (backward && !cfg_.retract_enabled) { err = kErrRetractOff; return false; }
        if (!backward && (state_ == State::Empty || !sensors_.tape_present || remaining_ == 0)) { err = kErrEmpty; return false; }
        if (!backward && remaining_ != kCountUnknown && remaining_ < req.payload[0]) { err = kErrEmpty; return false; }
        if (sensors_.tension < cfg_.tension_min || sensors_.tension > cfg_.tension_max) { err = kErrTension; return false; }
        if (!backward && cfg_.film_check && !sensors_.film_clear) { raise(Fault::Film); err = kErrFault; return false; }
        count_ = req.payload[0];
        target_mm_ = static_cast<double>(pitch_) * count_;
        moved_mm_ = 0;
        retries_ = 0;
        state_ = backward ? State::Retracting : State::Advancing;
        return true;
    }

    void finish_move(const Sensors& s) {
        if (state_ == State::Retracting) { state_ = State::Idle; target_mm_ = moved_mm_ = 0; return; }
        if (!s.tape_present) {  // the tape ran out under the move
            state_ = State::Empty;
            if (remaining_ != kCountUnknown) remaining_ = 0;
            record(true);
            target_mm_ = moved_mm_ = 0;
            return;
        }
        if (cfg_.film_check && !s.film_clear) { raise(Fault::Film); return; }
        if (cfg_.pocket_check && !s.pocket_found) {
            if (retries_ < cfg_.max_retries) {  // a small move more, then look again
                ++retries_;
                target_mm_ += cfg_.retry_mm;
                return;
            }
            raise(Fault::NoPocket);
            return;
        }
        if (remaining_ != kCountUnknown) remaining_ = static_cast<std::uint16_t>(remaining_ >= count_ ? remaining_ - count_ : 0);
        ++advances_done_;
        record(false);
        state_ = State::Idle;
        target_mm_ = moved_mm_ = 0;
    }

    void raise(Fault f) {
        fault_ = f;
        state_ = State::Fault;
        target_mm_ = moved_mm_ = 0;
        record(true);
        int bits = 0;
        std::uint32_t mask = cfg_.fault_window >= 32 ? 0xFFFFFFFFu : ((1u << cfg_.fault_window) - 1u);
        for (std::uint32_t v = history_ & mask; v; v &= v - 1) ++bits;
        if (bits >= cfg_.fault_limit) { state_ = State::Disabled; fault_ = Fault::TooManyFaults; }
    }

    void record(bool bad) { history_ = (history_ << 1) | (bad ? 1u : 0u); }

    bool acknowledge(std::uint8_t reenable, std::uint8_t& err) {
        if (state_ == State::Disabled) {
            if (!reenable) { err = kErrDisabled; return false; }
            history_ = 0;
        } else if (state_ == State::Safe) {
            if (!heard_) { err = kErrSafe; return false; }
        } else if (state_ != State::Fault) {
            return true;  // nothing was held
        }
        // a cause that is still there is not cleared
        if (fault_ == Fault::Film && cfg_.film_check && !sensors_.film_clear) { err = kErrStillActive; return false; }
        if (fault_ == Fault::TensionLow && sensors_.tension < cfg_.tension_min) { err = kErrStillActive; return false; }
        if (fault_ == Fault::TensionHigh && sensors_.tension > cfg_.tension_max) { err = kErrStillActive; return false; }
        if (fault_ == Fault::Stall && sensors_.stalled) { err = kErrStillActive; return false; }
        fault_ = Fault::None;
        state_ = sensors_.tape_present ? State::Idle : State::Empty;
        return true;
    }

    UnitConfig cfg_;
    std::uint8_t addr_;
    int pitch_;
    State state_ = State::Idle;
    Fault fault_ = Fault::None;
    Sensors sensors_;
    std::uint16_t remaining_ = kCountUnknown;
    std::uint8_t count_ = 1;
    double target_mm_ = 0, moved_mm_ = 0, step_mm_ = 0;
    int retries_ = 0;
    std::uint32_t history_ = 0;
    unsigned long advances_done_ = 0;
    bool have_move_seq_ = false;
    std::uint8_t move_seq_ = 0;
    bool heard_ = false;
    std::uint32_t last_heard_ = 0;
    unsigned long identify_until_ = 0;
};

}  // namespace pnp_feeder
