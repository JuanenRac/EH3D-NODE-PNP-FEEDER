// EH3D-NODE-PNP-FEEDER - core/feeder_sim.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// A chain of feeders on a computer: a tape that moves, sensors that read it, a bus that can lose or damage frames, and faults that can be switched on (a jam, a tape that
// ends, a slack or tight tape, a film that is not peeled, a pocket that is off, a feeder that does not answer, a bus that is cut). The random numbers come from a seed, so a
// run is the same every time. The numbers of the tape and of the noise are made up for the simulation: nothing here has been compared with a real feeder.
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "feeder_chain.hpp"
#include "feeder_transport.hpp"

namespace pnp_feeder {

struct SimTape {
    double pos_mm = 0;          // how far the tape has gone
    int pitch_mm = 4;           // the pitch of the pockets of this tape
    int parts = 100;            // parts in the tape
    double tolerance_mm = 0.4;  // how far from the middle of a pocket the sensor still sees it
    int tension = 500;
    bool film_clear = true;
    bool jammed = false;        // the tape does not move and the motor stalls
    double slip = 0.0;          // the tape moves this fraction less than the motor (0.02 = 2 %)
};

class SimUnit {
public:
    SimUnit(const UnitConfig& c, std::uint8_t addr) : unit(c, addr) {}
    Unit unit;
    SimTape tape;
    bool silent = false;        // a feeder that does not answer (unplugged)

    Sensors sensors() const {
        Sensors s;
        const double pockets = tape.pos_mm / tape.pitch_mm;
        s.tape_present = std::floor(pockets + 0.5) < tape.parts;
        const double off = tape.pos_mm - std::floor(pockets + 0.5) * tape.pitch_mm;
        s.pocket_found = std::fabs(off) <= tape.tolerance_mm;
        s.film_clear = tape.film_clear;
        s.tension = tape.tension;
        s.stalled = tape.jammed && (unit.state() == State::Advancing || unit.state() == State::Retracting);
        return s;
    }

    void tick(std::uint32_t now_ms, std::uint32_t dt_ms) {
        unit.tick(now_ms, dt_ms, sensors());
        if (!tape.jammed) tape.pos_mm += unit.step_mm() * (1.0 - tape.slip);
    }
};

class SimChain : public Bus, private FrameServer {
private:
    std::unique_ptr<Medium> medium_;   // the simulated bus the settings name; declared first because the faults below refer to it

public:
    SimChain(const Settings& settings, std::uint32_t seed)
        : medium_(make_medium(settings, seed)),
          drop_permille(medium_->faults.drop_permille),
          corrupt_permille(medium_->faults.corrupt_permille),
          cut(medium_->faults.cut) {
        for (int n = 1; n <= settings.slots; ++n) {
            auto u = std::make_unique<SimUnit>(UnitConfig::from(settings), static_cast<std::uint8_t>(n));
            u->tape.pitch_mm = settings.pitch_mm;
            units_.push_back(std::move(u));
        }
    }

    SimUnit& unit(int slot) { return *units_[static_cast<std::size_t>(slot - 1)]; }
    /// The simulated bus: its `faults` (loss, damage, a late answer, a held bus, a collision, a feeder that holds the clock) and its counters.
    Medium& medium() { return *medium_; }

    // the faults of the bus (the same fields as `medium().faults`)
    int& drop_permille;      // the chance (in a thousand) that a frame is lost, each way
    int& corrupt_permille;   // the chance that a byte of a frame is changed, each way
    bool& cut;               // the whole bus is cut: nobody answers and nobody hears

    std::vector<std::uint8_t> exchange(const std::vector<std::uint8_t>& request, std::uint32_t timeout_ms) override {
        wait(2);  // a frame takes some time to go and come back
        std::uint32_t us = 0;
        std::vector<std::uint8_t> back = medium_->transfer(request, *this, timeout_ms, us);
        const std::uint32_t ms = (us + 999u) / 1000u;
        if (ms > 2) wait(ms - 2);  // the rest of the time the frames took on the wire
        return back;
    }

    LinkStats link_stats() const override { return medium_->stats(); }

    void wait(std::uint32_t ms) override {
        while (ms > 0) {
            std::uint32_t dt = ms > 10 ? 10 : ms;
            now_ += dt;
            for (auto& u : units_) u->tick(now_, dt);
            ms -= dt;
        }
    }

    std::uint32_t now_ms() const override { return now_; }

private:
    bool present(std::uint8_t addr) const override { return addr >= 1 && addr <= units_.size() && !units_[static_cast<std::size_t>(addr) - 1]->silent; }
    void serve(const Frame& request, std::vector<Frame>& replies) override {
        for (auto& u : units_) {
            if (u->silent) continue;
            Frame reply;
            if (u->unit.handle(request, now_, reply)) replies.push_back(reply);
        }
    }

    std::vector<std::unique_ptr<SimUnit>> units_;
    std::uint32_t now_ = 1000;
};

}  // namespace pnp_feeder
