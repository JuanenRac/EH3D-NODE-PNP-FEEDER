// EH3D-NODE-PNP-FEEDER - tests/test_bus.cpp: the three buses under the chain (RS-485, CAN and I2C), the choice of the bus in the settings, the limits of each bus, the pieces
// of a CAN frame, the addresses of I2C, and the faults that can be put on a bus. The random numbers have a fixed seed.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../core/feeder_node.hpp"
#include "../core/feeder_sim.hpp"
#include "check.hpp"

using namespace pnp_feeder;

#ifndef PNP_FEEDER_ROOT
#error "PNP_FEEDER_ROOT must be the folder of the repository"
#endif

namespace {

const BusKind kBuses[] = {BusKind::Rs485, BusKind::Can, BusKind::I2c};

Settings on(BusKind b, int slots = 3) {
    Settings s;
    s.bus = b;
    s.slots = slots;
    return s;
}

struct Rig {
    Settings s;
    SimChain chain;
    Controller ctrl;
    explicit Rig(Settings in, std::uint32_t seed = 1) : s(in), chain(s, seed), ctrl(chain, s) { ctrl.discover(); }
};

/// The other end of a bus that is not a chain: it answers every frame to a feeder that is there with six bytes, and keeps the frames it was given.
class Far : public FrameServer {
public:
    explicit Far(int slots) : slots_(slots) {}
    bool present(std::uint8_t addr) const override { return !silent && addr >= 1 && addr <= slots_; }
    void serve(const Frame& request, std::vector<Frame>& replies) override {
        heard.push_back(request);
        if (request.addr == kAddrAll || silent) return;
        Frame r;
        r.addr = kAddrNode; r.cmd = static_cast<std::uint8_t>(request.cmd | kReplyFlag); r.seq = request.seq; r.payload.assign(6, 0);
        replies.push_back(r);
    }
    bool silent = false;
    std::vector<Frame> heard;
private:
    int slots_;
};

std::vector<std::uint8_t> ping(std::uint8_t addr, std::uint8_t seq = 1) {
    Frame f;
    f.addr = addr; f.cmd = kCmdPing; f.seq = seq;
    return encode(f);
}

std::string read_file(const std::string& rel) {
    std::ifstream in(std::string(PNP_FEEDER_ROOT) + "/" + rel, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Tally {
    unsigned long exchanges, retries, errors, dropped, corrupted;
    std::uint32_t now;
    bool operator==(const Tally& o) const {
        return exchanges == o.exchanges && retries == o.retries && errors == o.errors && dropped == o.dropped && corrupted == o.corrupted && now == o.now;
    }
};

/// A run of twenty advances on a bus that loses and damages frames.
Tally noisy_run(BusKind b, std::uint32_t seed) {
    Settings s = on(b);
    s.bus_retries = 5;
    Rig r(s, seed);
    r.chain.drop_permille = 100;
    r.chain.corrupt_permille = 100;
    for (int i = 0; i < 20; ++i) r.ctrl.advance(1 + i % 3, 1);
    const LinkStats st = r.chain.link_stats();
    return {r.ctrl.exchanges(), r.ctrl.retries(), r.ctrl.bus_errors(), st.dropped, st.corrupted, r.chain.now_ms()};
}

}  // namespace

// --- the settings -----------------------------------------------------------------------------------------------------------------------------------------------------------

TEST(the_bus_is_chosen_in_the_settings_and_each_name_is_read) {
    CHECK(Settings().bus == BusKind::Rs485);
    for (BusKind b : kBuses) {
        Settings s;
        CHECK(parse_settings(std::string("bus = ") + bus_text(b) + "\n", s).empty());
        CHECK(s.bus == b);
        Settings t;
        CHECK(parse_settings(settings_text(s), t).empty() && t.bus == b);  // read back the same
    }
}

TEST(a_bus_value_nobody_knows_is_refused_and_the_default_stays) {
    for (const char* line : {"bus = rs232", "bus = ", "bus = CAN", "can_bitrate = 300000", "can_bitrate = 0", "i2c_clock_hz = 123", "i2c_clock_hz = 1000000", "can_id_base = 0",
                             "can_id_base = 1901", "i2c_base_addr = 7", "i2c_base_addr = 120", "rs485_baud = 9599", "rs485_baud = 2000001", "rs485_turnaround_us = 9",
                             "rs485_turnaround_us = 5001", "reply_timeout_ms = 4", "reply_timeout_ms = 501", "i2c_stretch_limit_us = 99", "i2c_stretch_limit_us = 100001"}) {
        Settings s;
        std::vector<Problem> p = parse_settings(std::string(line) + "\n", s);
        CHECK(!p.empty());
        CHECK(settings_text(s) == settings_text(Settings()));
    }
    for (const char* line : {"can_bitrate = 125000", "can_bitrate = 1000000", "i2c_clock_hz = 400000", "can_id_base = 1900", "i2c_base_addr = 8", "i2c_base_addr = 119",
                             "rs485_baud = 921600","rs485_baud = 2000000", "rs485_turnaround_us = 5000", "reply_timeout_ms = 500", "i2c_stretch_limit_us = 100000"}) {
        Settings s;
        CHECK(parse_settings(std::string(line) + "\n", s).empty());
    }
}

TEST(the_example_file_gives_the_range_of_every_bus_key) {
    const std::string example = read_file("feeder.cfg.example");
    for (const char* key : {"bus", "reply_timeout_ms", "rs485_baud", "rs485_turnaround_us", "can_bitrate", "can_id_base", "i2c_clock_hz", "i2c_base_addr", "i2c_stretch_limit_us"}) {
        const std::size_t at = example.find(std::string("\n") + key + " = ");
        CHECK(at != std::string::npos);
        const std::size_t end = example.find('\n', at + 1);
        CHECK(end != std::string::npos && example.substr(at, end - at).find('#') != std::string::npos);  // the range is written next to the default
    }
    CHECK(example.find("rs485, can or i2c") != std::string::npos);
}

TEST(a_chain_cannot_be_longer_than_its_bus_holds) {
    for (BusKind b : kBuses) CHECK(bus_max_slots(on(b)) == 32);
    {
        Settings s;
        CHECK(parse_settings("bus = i2c\nslots = 32\n", s).empty() && s.slots == 32);  // 0x10 to 0x2F
    }
    {
        Settings s;
        CHECK(parse_settings("bus = i2c\ni2c_base_addr = 88\nslots = 32\n", s).empty() && s.slots == 32);  // the last is 0x77, the last usable
    }
    {
        Settings s;
        std::vector<Problem> p = parse_settings("bus = i2c\ni2c_base_addr = 89\nslots = 32\n", s);  // the last would be 0x78, which the standard keeps
        CHECK(p.size() == 1 && p[0].key == "slots");
        CHECK(s.slots == Settings().slots && s.i2c_base_addr == Settings().i2c_base_addr);
    }
    {
        Settings s = on(BusKind::I2c, 8);
        s.i2c_base_addr = 119;
        CHECK(bus_max_slots(s) == 1);
        Settings t;
        CHECK(!parse_settings("bus = i2c\ni2c_base_addr = 119\n", t).empty());  // eight slots from 119 do not fit
    }
    {
        Settings s;
        CHECK(!parse_settings("slots = 33\n", s).empty() && s.slots == Settings().slots);  // the frame has 32 addresses, whatever the bus
    }
}

TEST(a_rate_too_slow_for_the_reply_timeout_goes_back_to_the_default) {
    Settings s;
    std::vector<Problem> p = parse_settings("rs485_baud = 9600\n", s);  // forty bytes each way take 83 ms at 9600 baud, more than the 20 ms the node waits
    CHECK(p.size() == 1 && p[0].key == "reply_timeout_ms");
    CHECK(s.rs485_baud == Settings().rs485_baud && s.reply_timeout_ms == Settings().reply_timeout_ms);
    Settings t;
    CHECK(parse_settings("rs485_baud = 9600\nreply_timeout_ms = 100\n", t).empty() && t.rs485_baud == 9600);  // with the time it needs, it is accepted
    Settings c;
    CHECK(parse_settings("bus = can\ncan_bitrate = 125000\n", c).empty());
    Settings i;
    CHECK(parse_settings("bus = i2c\ni2c_clock_hz = 100000\n", i).empty());
    for (BusKind b : kBuses) CHECK(bus_round_trip_us(on(b)) < static_cast<long long>(Settings().reply_timeout_ms) * 1000);  // the defaults always fit
}

// --- the same chain on the three buses ----------------------------------------------------------------------------------------------------------------------------------------

TEST(the_same_advance_works_on_the_three_buses) {
    for (BusKind b : kBuses) {
        Rig r(on(b));
        CHECK(r.chain.medium().kind() == b);
        CHECK(r.ctrl.slot(1).online && r.ctrl.slot(2).online && r.ctrl.slot(3).online);
        CHECK(r.ctrl.request(1, kCmdSetCount, {0, 50}).ok);
        Outcome o = r.ctrl.advance(1, 2);
        CHECK(o.ok && o.why == "ok");
        CHECK(r.chain.unit(1).tape.pos_mm > 7.9 && r.chain.unit(1).tape.pos_mm < 8.1);
        CHECK(r.ctrl.slot(1).remaining == 48);
        CHECK(r.chain.link_stats().wire_frames > 0);
        CHECK(r.ctrl.bus_errors() == 0);
    }
}

TEST(a_chain_of_thirty_two_feeders_is_found_and_driven_on_every_bus) {
    for (BusKind b : kBuses) {
        Rig r(on(b, 32));
        int online = 0;
        for (int n = 1; n <= 32; ++n) online += r.ctrl.slot(n).online ? 1 : 0;
        CHECK(online == 32);
        CHECK(r.ctrl.advance(32, 1).ok);   // the last address of the chain
        CHECK(r.ctrl.advance(1, 1).ok);
        CHECK(r.chain.unit(32).tape.pos_mm > 3.9 && r.chain.unit(32).tape.pos_mm < 4.1);
    }
}

TEST(the_frame_is_the_same_bytes_on_every_bus) {
    std::vector<std::vector<Frame>> heard;
    for (BusKind b : kBuses) {
        Settings s = on(b, 4);
        std::unique_ptr<Medium> m = make_medium(s, 1);
        Far far(4);
        std::uint32_t us = 0;
        std::vector<std::uint8_t> back = m->transfer(ping(3, 9), far, 20, us);
        Decoder d;
        d.push(back);
        Frame f;
        CHECK(d.next(f) && f.cmd == (kCmdPing | kReplyFlag) && f.seq == 9);
        heard.push_back(far.heard);
    }
    CHECK(heard[0].size() == 1 && heard[1].size() == 1 && heard[2].size() == 1);
    for (std::size_t i = 1; i < heard.size(); ++i)
        CHECK(encode(heard[i][0]) == encode(heard[0][0]));  // what the feeders were given is the same frame, byte for byte
}

TEST(a_command_to_all_is_heard_by_all_and_never_answered_on_every_bus) {
    for (BusKind b : kBuses) {
        Rig r(on(b, 5));
        r.ctrl.request(1, kCmdAdvance, {8});
        r.chain.wait(100);
        r.ctrl.broadcast(kCmdStop);
        r.chain.wait(100);
        CHECK(r.chain.unit(1).unit.state() != State::Advancing);
        Settings s = on(b, 5);
        std::unique_ptr<Medium> m = make_medium(s, 1);
        Far far(5);
        std::uint32_t us = 0;
        CHECK(m->transfer(ping(kAddrAll), far, 20, us).empty());
        CHECK(far.heard.size() == 1 && far.heard[0].addr == kAddrAll);
    }
}

TEST(a_cut_bus_is_the_same_thing_on_every_bus_and_comes_back) {
    for (BusKind b : kBuses) {
        Rig r(on(b));
        r.chain.cut = true;
        for (int i = 0; i < r.s.offline_after; ++i) CHECK(r.ctrl.request(1, kCmdStatus).error == kErrNoAnswer);
        CHECK(!r.ctrl.slot(1).online);   // offline_after requests without an answer
        r.chain.cut = false;
        CHECK(r.ctrl.discover() == 3 && r.ctrl.slot(1).online);
    }
}

// --- RS-485 -------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST(rs485_waits_the_turnaround_before_each_frame_and_the_time_of_the_bytes) {
    Settings s = on(BusKind::Rs485, 4);
    Rs485Medium m(s, 1);
    Far far(4);
    std::uint32_t us = 0;
    const std::vector<std::uint8_t> req = ping(2);
    CHECK(!m.transfer(req, far, 20, us).empty());
    const std::uint32_t want = static_cast<std::uint32_t>(s.rs485_turnaround_us) * 2 + m.byte_time_us(req.size()) + m.byte_time_us(6 + 6 + 2);
    CHECK(us == want);
    CHECK(m.byte_time_us(1) == 86);  // ten bits at 115200 baud
    CHECK(m.stats().wire_frames == 2);
}

TEST(a_slower_rs485_rate_and_a_longer_turnaround_take_longer) {
    std::uint32_t fast = 0, slow = 0, long_turn = 0;
    {
        Settings s = on(BusKind::Rs485, 4);
        Rs485Medium m(s, 1);
        Far far(4);
        m.transfer(ping(2), far, 50, fast);
    }
    {
        Settings s = on(BusKind::Rs485, 4);
        s.rs485_baud = 38400; s.reply_timeout_ms = 100;
        Rs485Medium m(s, 1);
        Far far(4);
        m.transfer(ping(2), far, 100, slow);
    }
    {
        Settings s = on(BusKind::Rs485, 4);
        s.rs485_turnaround_us = 1000;
        Rs485Medium m(s, 1);
        Far far(4);
        m.transfer(ping(2), far, 50, long_turn);
    }
    CHECK(slow > fast * 2);
    CHECK(long_turn == fast + 2 * (1000 - 100));
}

TEST(two_talkers_at_once_lose_both_frames_on_rs485) {
    Settings s = on(BusKind::Rs485);
    Rs485Medium m(s, 1);
    Far far(3);
    m.faults.collision_permille = 1000;
    std::uint32_t us = 0;
    CHECK(m.transfer(ping(1), far, 20, us).empty());
    CHECK(m.stats().collisions == 1 && far.heard.empty());
    Rig r(on(BusKind::Rs485));
    r.chain.medium().faults.collision_permille = 1000;
    CHECK(r.ctrl.request(1, kCmdStatus).error == kErrNoAnswer);
    CHECK(r.ctrl.retries() >= static_cast<unsigned long>(r.s.bus_retries));  // each try was repeated
}

// --- CAN ----------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST(can_cuts_a_frame_in_pieces_of_seven_bytes_and_puts_it_back) {
    for (std::size_t payload = 0; payload <= kMaxPayload; ++payload) {
        Frame f;
        f.addr = 5; f.cmd = kCmdSetCount; f.seq = 77;
        f.payload.assign(payload, 0x7E);  // start bytes inside the payload must not confuse anything
        const std::vector<std::uint8_t> bytes = encode(f);
        const std::vector<CanFrame> pieces = can_fragment(261, bytes);
        CHECK(pieces.size() == (bytes.size() + 6) / 7);
        CHECK(pieces.size() <= kCanMaxPieces);
        CanReassembler re;
        std::vector<std::uint8_t> back;
        std::size_t done = 0;
        for (const CanFrame& p : pieces) {
            CHECK(p.dlc >= 2 && p.dlc <= 8 && p.id == 261);   // a CAN frame holds eight data bytes: one says the piece, up to seven are the frame
            if (re.push(p, back)) ++done;
        }
        CHECK(done == 1 && back == bytes && re.errors() == 0);
    }
    static_assert(kCanMaxPieces == 6, "the longest frame, 40 bytes, is six pieces");
}

TEST(can_pieces_out_of_order_or_missing_or_of_another_frame_do_not_make_a_frame) {
    Frame f;
    f.addr = 5; f.cmd = kCmdSetCount; f.seq = 1; f.payload.assign(32, 0x11);
    const std::vector<CanFrame> p = can_fragment(261, encode(f));
    std::vector<std::uint8_t> out;
    {
        CanReassembler re;   // a piece missing in the middle
        CHECK(!re.push(p[0], out) && !re.push(p[1], out) && !re.push(p[3], out));
        CHECK(re.errors() == 1);
        CHECK(!re.push(p[4], out) && !re.push(p[5], out) && out.empty());   // what follows has no start: it is dropped
    }
    {
        CanReassembler re;   // the first piece never came
        CHECK(!re.push(p[1], out) && re.errors() == 1);
    }
    {
        CanReassembler re;   // a piece of another identifier in the middle of a frame
        CanFrame other = p[1];
        other.id = 262;
        CHECK(!re.push(p[0], out) && !re.push(other, out) && re.errors() == 1);
    }
    {
        CanReassembler re;   // a new frame starts before the old one is finished: the old one is counted lost and the new one is whole
        CHECK(!re.push(p[0], out) && !re.push(p[1], out));
        bool whole = false;
        for (const CanFrame& c : p) if (re.push(c, out)) whole = true;
        CHECK(whole && re.errors() == 1);
        Decoder d;
        d.push(out);
        Frame g;
        CHECK(d.next(g) && g.seq == 1 && g.payload.size() == 32);
    }
    {
        CanReassembler re;   // a header that no frame has
        CanFrame bad = p[0];
        bad.data[0] = 0x07;   // seven more pieces than a frame can have
        CHECK(!re.push(bad, out) && re.errors() == 1);
        CanFrame empty = p[0];
        empty.dlc = 1;
        CHECK(!re.push(empty, out) && re.errors() == 2);
    }
}

TEST(can_identifiers_name_the_feeder_and_stay_inside_eleven_bits) {
    Settings s;
    s.can_id_base = 256;
    CHECK(can_id_to(s, 1) == 257 && can_id_to(s, 32) == 288 && can_id_to(s, kAddrAll) == 256);
    CHECK(can_id_from(s, 1) == 321 && can_id_from(s, 32) == 352);
    s.can_id_base = 1900;
    CHECK(can_id_from(s, 32) <= 0x7FF);   // the highest base still gives a standard identifier
    for (int n = 1; n <= 32; ++n) CHECK(can_id_to(s, static_cast<std::uint8_t>(n)) != can_id_from(s, static_cast<std::uint8_t>(n)));
    // the identifier of the node's frames and of the answers never meet
    CHECK(can_id_to(s, 32) < can_id_from(s, 1));
}

TEST(can_time_on_the_wire_is_the_bits_over_the_bitrate) {
    for (int rate : {125000, 250000, 500000, 1000000}) {
        Settings s = on(BusKind::Can, 4);
        s.can_bitrate = rate;
        CanMedium m(s, 1);
        Far far(4);
        std::uint32_t us = 0;
        const std::vector<std::uint8_t> req = ping(2);
        CHECK(!m.transfer(req, far, 20, us).empty());
        std::uint32_t want = 0;
        for (const CanFrame& c : can_fragment(can_id_to(s, 2), req)) want += m.frame_time_us(c);
        Frame answer;
        answer.addr = kAddrNode; answer.cmd = kCmdPing | kReplyFlag; answer.seq = 1; answer.payload.assign(6, 0);
        for (const CanFrame& c : can_fragment(can_id_from(s, 2), encode(answer))) want += m.frame_time_us(c);
        CHECK(us == want);
    }
    Settings s = on(BusKind::Can);
    CanMedium m(s, 1);
    CanFrame c;
    c.dlc = 8;
    CHECK(m.frame_time_us(c) == 444);   // 111 bits at 250000 bit/s
}

TEST(can_hardware_sends_a_damaged_frame_again_and_a_lost_piece_loses_the_frame) {
    Settings s = on(BusKind::Can);
    {
        CanMedium m(s, 1);
        Far far(3);
        m.faults.corrupt_permille = 1000;   // every try is damaged: after three tries the piece is given up
        std::uint32_t us = 0;
        CHECK(m.transfer(ping(1), far, 100, us).empty());
        CHECK(m.stats().corrupted >= 3 && far.heard.empty());
    }
    {
        CanMedium m(s, 7);
        Far far(3);
        m.faults.corrupt_permille = 300;    // most frames are repaired by the repeat of the hardware, with time lost
        std::uint32_t us = 0, clean = 0;
        int good = 0;
        CanMedium ref(s, 7);
        Far far2(3);
        ref.transfer(ping(1), far2, 100, clean);
        for (int i = 0; i < 40; ++i) {
            if (!m.transfer(ping(1), far, 100, us).empty()) ++good;
        }
        CHECK(good > 30);
        CHECK(m.stats().corrupted > 0);
        CHECK(clean > 0);
    }
    {
        CanMedium m(s, 3);
        Far far(3);
        m.faults.drop_permille = 400;       // a piece lost on the way is a frame lost
        std::uint32_t us = 0;
        for (int i = 0; i < 60; ++i) m.transfer(ping(1), far, 100, us);
        CHECK(m.stats().dropped > 0 && m.stats().reassembly_errors > 0);
        CHECK(far.heard.size() < 60);
    }
}

TEST(can_arbitration_costs_a_frame_time_and_a_busy_bus_gives_the_try_up) {
    Settings s = on(BusKind::Can);
    CanMedium calm(s, 1), crowd(s, 1);
    Far a(3), b(3);
    std::uint32_t t_calm = 0, t_crowd = 0;
    CHECK(!calm.transfer(ping(1), a, 100, t_calm).empty());
    crowd.faults.collision_permille = 1000;
    CHECK(!crowd.transfer(ping(1), b, 100, t_crowd).empty());   // the frame still gets through: the arbitration only delays it
    CHECK(t_crowd > t_calm && crowd.stats().collisions > 0);
    crowd.faults.busy_permille = 1000;
    std::uint32_t t = 0;
    CHECK(crowd.transfer(ping(1), b, 100, t).empty() && crowd.stats().busy == 1);
}

TEST(can_frames_that_nobody_acknowledges_are_counted_and_the_slot_goes_offline) {
    Rig r(on(BusKind::Can));
    r.chain.unit(2).silent = true;
    CHECK(r.ctrl.request(2, kCmdStatus).error == kErrNoAnswer);
    CHECK(r.chain.link_stats().nacks > 0);
    for (int i = 1; i < r.s.offline_after; ++i) r.ctrl.request(2, kCmdStatus);
    CHECK(!r.ctrl.slot(2).online && r.ctrl.slot(1).online);
}

// --- I2C ----------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST(i2c_addresses_are_seven_bits_from_the_base_and_never_a_reserved_one) {
    Settings s = on(BusKind::I2c, 32);
    CHECK(i2c_address_of(s, 1) == 0x10 && i2c_address_of(s, 32) == 0x2F);
    CHECK(i2c_address_of(s, kAddrAll) == 0);   // the general call
    CHECK(i2c_address_of(s, 0) == 0xFF && i2c_address_of(s, 33) == 0xFF);
    for (int base = kI2cFirstUsable; base <= kI2cLastUsable; ++base) {
        Settings t = on(BusKind::I2c, 1);
        t.i2c_base_addr = base;
        t.slots = bus_max_slots(t);
        CHECK(t.slots >= 1 && t.slots <= 32);
        for (int n = 1; n <= t.slots; ++n) {
            const std::uint8_t a = i2c_address_of(t, static_cast<std::uint8_t>(n));
            CHECK(a >= kI2cFirstUsable && a <= kI2cLastUsable);
        }
    }
}

TEST(i2c_time_on_the_wire_is_nine_clocks_a_byte) {
    Settings s = on(BusKind::I2c, 4);
    I2cMedium m(s, 1);
    Far far(4);
    std::uint32_t us = 0;
    CHECK(!m.transfer(ping(2), far, 20, us).empty());
    // write: start and address (10 clocks), the 8 bytes of the frame (72) and the stop (1); read: start and address (10), the 14 bytes of the answer (126) and the stop (1)
    CHECK(us == (10 + 72 + 1 + 10 + 126 + 1) * 10);   // 100 kHz: ten microseconds a clock
    Settings f = s;
    f.i2c_clock_hz = 400000;
    I2cMedium fast(f, 1);
    std::uint32_t us_fast = 0;
    CHECK(!fast.transfer(ping(2), far, 20, us_fast).empty());
    CHECK(us_fast * 4 <= us + 4 && us_fast * 4 + 8 >= us);
}

TEST(i2c_a_feeder_that_is_not_there_does_not_acknowledge_its_address) {
    Rig r(on(BusKind::I2c));
    r.chain.unit(3).silent = true;
    CHECK(r.ctrl.request(3, kCmdStatus).error == kErrNoAnswer);
    CHECK(r.chain.link_stats().nacks >= 1);
    Settings s = on(BusKind::I2c, 4);
    I2cMedium m(s, 1);
    Far far(4);
    std::uint32_t us = 0;
    CHECK(m.transfer(ping(4), far, 20, us).size() > 0);
    CHECK(m.transfer(ping(9), far, 20, us).empty());   // no feeder 9: nobody acknowledges
    CHECK(far.heard.size() == 1);                       // and the frame never reached anybody
    CHECK(m.stats().nacks == 1 && us < 200);           // a NACK ends the transfer after the address: it costs a few clocks, not a time-out
}

TEST(i2c_a_clock_held_within_the_limit_costs_time_and_beyond_it_is_a_timeout) {
    Settings s = on(BusKind::I2c, 4);
    I2cMedium m(s, 1);
    Far far(4);
    std::uint32_t base = 0, held = 0, too_long = 0;
    m.transfer(ping(1), far, 50, base);
    m.faults.stretch_permille = 1000;
    m.faults.stretch_us = 3000;   // below the limit of 5000
    CHECK(!m.transfer(ping(1), far, 50, held).empty());
    CHECK(held == base + 3000 && m.stats().stretch_timeouts == 0);
    m.faults.stretch_us = 6000;   // above it
    CHECK(m.transfer(ping(1), far, 50, too_long).empty());
    CHECK(m.stats().stretch_timeouts == 1 && too_long < base + 6000);
    s.i2c_stretch_limit_us = 8000;   // the limit is a setting
    I2cMedium wide(s, 1);
    wide.faults.stretch_permille = 1000;
    wide.faults.stretch_us = 6000;
    std::uint32_t us = 0;
    CHECK(!wide.transfer(ping(1), far, 50, us).empty() && wide.stats().stretch_timeouts == 0);
}

TEST(i2c_the_general_call_reaches_every_feeder_and_is_not_answered) {
    Rig r(on(BusKind::I2c, 4));
    for (int n = 1; n <= 4; ++n) r.ctrl.request(static_cast<std::uint8_t>(n), kCmdAdvance, {8});
    r.chain.wait(100);
    r.ctrl.broadcast(kCmdStop);
    r.chain.wait(50);
    for (int n = 1; n <= 4; ++n) CHECK(r.chain.unit(n).unit.state() != State::Advancing);
    Settings s = on(BusKind::I2c, 4);
    I2cMedium m(s, 1);
    Far far(4);
    std::uint32_t us = 0;
    CHECK(m.transfer(ping(kAddrAll), far, 20, us).empty());
    CHECK(far.heard.size() == 1 && m.stats().nacks == 0);
}

TEST(i2c_a_second_master_costs_the_frame_time_again) {
    Settings s = on(BusKind::I2c);
    I2cMedium calm(s, 1), crowd(s, 1);
    Far a(3), b(3);
    std::uint32_t t_calm = 0, t_crowd = 0;
    calm.transfer(ping(1), a, 50, t_calm);
    crowd.faults.collision_permille = 1000;
    CHECK(!crowd.transfer(ping(1), b, 50, t_crowd).empty());
    CHECK(t_crowd > t_calm && crowd.stats().collisions == 1);
}

// --- faults of any bus --------------------------------------------------------------------------------------------------------------------------------------------------------

TEST(lost_and_damaged_frames_are_sent_again_on_every_bus) {
    for (BusKind b : kBuses) {
        Settings s = on(b);
        s.bus_retries = 5;
        Rig r(s, 5);
        r.chain.drop_permille = 100;
        r.chain.corrupt_permille = 100;
        int good = 0;
        for (int i = 0; i < 20; ++i) if (r.ctrl.advance(1 + i % 3, 1).ok) ++good;
        CHECK(good >= 18);
        CHECK(r.ctrl.retries() > 0);
        CHECK(r.chain.link_stats().dropped + r.chain.link_stats().corrupted > 0);
    }
}

TEST(a_bus_held_by_somebody_else_costs_a_try_and_is_counted_on_every_bus) {
    for (BusKind b : kBuses) {
        Settings s = on(b);
        s.bus_retries = 5;
        Rig r(s, 2);
        r.chain.medium().faults.busy_permille = 200;
        int good = 0;
        for (int i = 0; i < 10; ++i) if (r.ctrl.advance(1, 1).ok) ++good;
        CHECK(good >= 8);
        CHECK(r.chain.link_stats().busy > 0 && r.ctrl.retries() > 0);
        r.chain.medium().faults.busy_permille = 1000;   // held for good: nothing gets through
        CHECK(r.ctrl.request(1, kCmdStatus).error == kErrNoAnswer);
    }
}

TEST(two_talkers_at_once_only_delay_can_and_i2c_and_cost_rs485_its_frames) {
    for (BusKind b : kBuses) {
        Settings s = on(b);
        s.bus_retries = 0;
        Rig r(s, 1);
        r.chain.medium().faults.collision_permille = 1000;
        const bool got = r.ctrl.request(1, kCmdStatus).ok;
        CHECK(got == (b != BusKind::Rs485));
        CHECK(r.chain.link_stats().collisions > 0);
    }
}

TEST(an_answer_slower_than_the_reply_timeout_is_late_and_not_delivered_on_every_bus) {
    for (BusKind b : kBuses) {
        Settings s = on(b);
        s.bus_retries = 1;
        Rig r(s, 1);
        r.chain.medium().faults.delay_us = 30000;   // the cable or the feeder is slower than the 20 ms the node waits
        Reply reply = r.ctrl.request(1, kCmdStatus);
        CHECK(!reply.ok && reply.error == kErrNoAnswer);
        CHECK(r.chain.link_stats().late >= 2);
        r.chain.medium().faults.delay_us = 0;
        CHECK(r.ctrl.request(1, kCmdStatus).ok);
        s.reply_timeout_ms = 60;                    // a longer wait takes the same answer
        Rig slow(s, 1);
        slow.chain.medium().faults.delay_us = 30000;
        CHECK(slow.ctrl.request(1, kCmdStatus).ok);
    }
}

TEST(the_same_seed_gives_the_same_run_on_every_bus_and_another_seed_another_run) {
    for (BusKind b : kBuses) {
        CHECK(noisy_run(b, 11) == noisy_run(b, 11));
        bool differs = false;
        const Tally first = noisy_run(b, 1);
        for (std::uint32_t seed = 2; seed <= 6; ++seed) if (!(noisy_run(b, seed) == first)) differs = true;
        CHECK(differs);
    }
}

TEST(a_slower_bus_makes_the_same_advance_take_longer_but_move_the_same_tape) {
    std::uint32_t t_rs = 0, t_can = 0, t_i2c = 0;
    double pos[3] = {0, 0, 0};
    int k = 0;
    for (BusKind b : kBuses) {
        Settings s = on(b);
        Rig r(s);
        const std::uint32_t before = r.chain.now_ms();
        r.ctrl.advance(1, 3);
        pos[k++] = r.chain.unit(1).tape.pos_mm;
        const std::uint32_t took = r.chain.now_ms() - before;
        if (b == BusKind::Rs485) t_rs = took; else if (b == BusKind::Can) t_can = took; else t_i2c = took;
    }
    CHECK(pos[0] == pos[1] && pos[1] == pos[2]);
    CHECK(t_rs > 0 && t_can > 0 && t_i2c > 0);
}

// --- the node and its page ----------------------------------------------------------------------------------------------------------------------------------------------------

TEST(the_node_says_the_bus_and_counts_what_was_busy_or_collided) {
    for (BusKind b : kBuses) {
        Settings s = on(b);
        s.token = "tok";
        SimChain chain(s, 1);
        Node node(s, chain);
        node.start();
        chain.medium().faults.busy_permille = 1000;
        node.controller().request(1, kCmdStatus);
        chain.medium().faults.busy_permille = 0;
        chain.medium().faults.collision_permille = 1000;
        node.controller().request(1, kCmdStatus);
        Response r = node.handle("GET /api/status HTTP/1.1", "");
        eh3d::json::Value v;
        CHECK(eh3d::json::parse(r.body, v));
        CHECK(v.string_or("bus", "?") == bus_text(b));
        CHECK(v.number_or("bus_busy", -1) >= 1);
        CHECK(v.number_or("bus_collisions", -1) >= 1);
    }
}

TEST(the_page_of_the_contract_names_the_three_buses_and_every_key_of_them) {
    const std::string page = read_file("docs/CONTRACT.md");
    CHECK(page.find("## The bus under the chain") != std::string::npos);
    for (const char* word : {"RS-485", "CAN", "I2C", "`rs485_baud`", "`rs485_turnaround_us`", "`can_bitrate`", "`can_id_base`", "`i2c_clock_hz`", "`i2c_base_addr`", "`i2c_stretch_limit_us`",
                             "`reply_timeout_ms`", "DE/RE", "general call", "pieces of 7 bytes"})
        CHECK(page.find(word) != std::string::npos);
    const std::string hardware = read_file("docs/HARDWARE.md");
    for (const char* test : {"F-11", "F-12", "F-13", "F-14", "F-15", "F-16", "F-17"}) CHECK(hardware.find(test) != std::string::npos);
}

int main() { return check::run_all("bus"); }
