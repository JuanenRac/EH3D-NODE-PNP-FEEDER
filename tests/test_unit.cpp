// EH3D-NODE-PNP-FEEDER - tests/test_unit.cpp: the feeder on a simulated tape and the node's side of the chain: advances, a tape that ends, a jam, a slack or tight tape, a
// film that is not peeled, a pocket that is not found, too many faults, a bus that loses and damages frames, and a node that is lost. The random numbers have a fixed seed.
#include <cmath>
#include <string>
#include <vector>

#include "../core/feeder_chain.hpp"
#include "../core/feeder_sim.hpp"
#include "check.hpp"

using namespace pnp_feeder;

namespace {

struct Rig {
    Settings s;
    SimChain chain;
    Controller ctrl;
    explicit Rig(Settings in, std::uint32_t seed = 1) : s(in), chain(s, seed), ctrl(chain, s) { ctrl.discover(); }
    SimUnit& u(int n = 1) { return chain.unit(n); }
};

Settings three_slots() {
    Settings s;
    s.slots = 3;
    return s;
}

bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

}  // namespace

TEST(an_advance_moves_one_pitch_and_counts_a_part) {
    Rig r(three_slots());
    CHECK(r.ctrl.slot(1).online && r.ctrl.slot(2).online && r.ctrl.slot(3).online);
    CHECK(r.ctrl.request(1, kCmdSetCount, {0, 50}).ok);
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(o.ok && o.why == "ok");
    CHECK(near(r.u().tape.pos_mm, 4.0));
    CHECK(r.ctrl.slot(1).remaining == 49);
    CHECK(r.ctrl.slot(1).state == State::Idle);
    CHECK(r.u().unit.advances_done() == 1);
}

TEST(each_pitch_moves_the_tape_by_that_pitch) {
    for (int pitch : {2, 4, 8, 12, 16}) {
        Settings s = three_slots();
        s.pitch_mm = pitch;
        Rig r(s);
        CHECK(r.ctrl.advance(1, 1).ok);
        CHECK(near(r.u().tape.pos_mm, pitch));
        CHECK(r.ctrl.advance(1, 3).ok);
        CHECK(near(r.u().tape.pos_mm, pitch * 4.0));
        CHECK(r.ctrl.slot(1).pitch == pitch);
    }
}

TEST(the_pitch_of_one_feeder_can_be_changed_when_it_is_still) {
    Rig r(three_slots());
    CHECK(r.ctrl.request(2, kCmdSetPitch, {8}).ok);
    CHECK(r.ctrl.slot(2).pitch == 8);
    Reply bad = r.ctrl.request(2, kCmdSetPitch, {5});
    CHECK(!bad.ok && bad.error == kErrBadArgument);
    CHECK(r.ctrl.slot(2).pitch == 8);
}

TEST(a_tape_that_ends_is_reported_as_empty_and_refuses_more) {
    Rig r(three_slots());
    r.u().tape.parts = 3;
    CHECK(r.ctrl.advance(1, 1).ok);
    CHECK(r.ctrl.advance(1, 1).ok);
    Outcome o = r.ctrl.advance(1, 1);  // the third advance finds the end
    CHECK(!o.ok && o.why == "empty");
    CHECK(r.ctrl.slot(1).state == State::Empty);
    Outcome again = r.ctrl.advance(1, 1);
    CHECK(!again.ok && again.why == "empty");
}

TEST(a_counted_reel_stops_when_the_count_is_used_up_and_warns_before) {
    Rig r(three_slots());
    r.u().tape.parts = 61;
    CHECK(r.ctrl.request(1, kCmdSetCount, {0, 60}).ok);
    bool warned = false;
    for (int i = 0; i < 60; ++i) {
        CHECK(r.ctrl.advance(1, 1).ok);
        if ((r.ctrl.slot(1).flags & 16) != 0) warned = true;
        if (r.ctrl.slot(1).remaining == 21) CHECK((r.ctrl.slot(1).flags & 16) == 0);
        if (r.ctrl.slot(1).remaining == 20) CHECK((r.ctrl.slot(1).flags & 16) != 0);
    }
    CHECK(warned);
    CHECK(r.ctrl.slot(1).remaining == 0);
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(!o.ok && o.why == "empty");
    CHECK(near(r.u().tape.pos_mm, 240.0));
}

TEST(a_new_spool_clears_the_empty_state) {
    Rig r(three_slots());
    r.u().tape.parts = 1;
    CHECK(!r.ctrl.advance(1, 1).ok);
    CHECK(r.ctrl.slot(1).state == State::Empty);
    r.u().tape.parts = 100;  // the person puts a new spool in
    CHECK(r.ctrl.request(1, kCmdSetCount, {0, 99}).ok);
    CHECK(r.ctrl.slot(1).state == State::Idle);
}

TEST(a_jam_holds_a_fault_and_three_in_a_row_disable_the_feeder) {
    Rig r(three_slots());
    r.u().tape.jammed = true;
    for (int i = 0; i < 2; ++i) {
        Outcome o = r.ctrl.advance(1, 1);
        CHECK(!o.ok && o.why == "fault");
        CHECK(r.ctrl.slot(1).state == State::Fault && r.ctrl.slot(1).fault == Fault::Stall);
        CHECK(std::string(fault_code(r.ctrl.slot(1).fault)) == "FDR-J01");
        Reply busy = r.ctrl.request(1, kCmdAdvance, {1});  // a fault is held: nothing moves until it is acknowledged
        CHECK(!busy.ok && busy.error == kErrFault);
        CHECK(r.ctrl.request(1, kCmdAck).ok);
    }
    Outcome third = r.ctrl.advance(1, 1);
    CHECK(!third.ok);
    CHECK(r.ctrl.slot(1).state == State::Disabled && r.ctrl.slot(1).fault == Fault::TooManyFaults);
    Reply plain = r.ctrl.request(1, kCmdAck);  // a plain acknowledgement does not re-enable it
    CHECK(!plain.ok && plain.error == kErrDisabled);
    r.u().tape.jammed = false;
    CHECK(r.ctrl.request(1, kCmdAck, {1}).ok);
    CHECK(r.ctrl.slot(1).state == State::Idle);
    CHECK(r.ctrl.advance(1, 1).ok);
}

TEST(a_feeder_that_works_between_faults_is_not_disabled) {
    Settings s = three_slots();
    s.fault_window = 4;
    s.fault_limit = 3;
    Rig r(s);
    for (int round = 0; round < 6; ++round) {
        r.u().tape.jammed = true;
        CHECK(!r.ctrl.advance(1, 1).ok);
        CHECK(r.ctrl.request(1, kCmdAck).ok);
        r.u().tape.jammed = false;
        for (int k = 0; k < 4; ++k) CHECK(r.ctrl.advance(1, 1).ok);  // four good advances push the fault out of the window
    }
    CHECK(r.ctrl.slot(1).state == State::Idle);
}

TEST(a_slack_or_tight_tape_is_refused_before_the_move) {
    Rig r(three_slots());
    r.u().tape.tension = 100;
    Reply a = r.ctrl.request(1, kCmdAdvance, {1});
    CHECK(!a.ok && a.error == kErrTension);
    r.u().tape.tension = 900;
    Reply b = r.ctrl.request(1, kCmdAdvance, {1});
    CHECK(!b.ok && b.error == kErrTension);
    CHECK(near(r.u().tape.pos_mm, 0.0));
    r.u().tape.tension = 500;
    CHECK(r.ctrl.advance(1, 1).ok);
}

TEST(a_tape_that_goes_slack_during_the_move_holds_a_fault_until_it_is_tight) {
    Rig r(three_slots());
    CHECK(r.ctrl.request(1, kCmdAdvance, {2}).ok);
    r.chain.wait(60);
    r.u().tape.tension = 50;
    r.chain.wait(40);
    CHECK(r.u().unit.state() == State::Fault && r.u().unit.fault() == Fault::TensionLow);
    CHECK(std::string(fault_code(Fault::TensionLow)) == "FDR-T01");
    double stopped_at = r.u().tape.pos_mm;
    r.chain.wait(500);
    CHECK(near(r.u().tape.pos_mm, stopped_at));  // it does not go on
    Reply still = r.ctrl.request(1, kCmdAck);
    CHECK(!still.ok && still.error == kErrStillActive);
    r.u().tape.tension = 500;
    CHECK(r.ctrl.request(1, kCmdAck).ok);
}

TEST(a_film_that_is_not_peeled_stops_the_feeder_and_is_not_cleared_while_it_is_there) {
    Rig r(three_slots());
    r.u().tape.film_clear = false;
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(!o.ok);
    CHECK(r.ctrl.slot(1).fault == Fault::Film && std::string(fault_code(Fault::Film)) == "FDR-F01");
    Reply still = r.ctrl.request(1, kCmdAck);
    CHECK(!still.ok && still.error == kErrStillActive);
    r.u().tape.film_clear = true;
    CHECK(r.ctrl.request(1, kCmdAck).ok);
    CHECK(r.ctrl.advance(1, 1).ok);
}

TEST(the_film_check_can_be_switched_off) {
    Settings s = three_slots();
    s.film_check = false;
    Rig r(s);
    r.u().tape.film_clear = false;
    CHECK(r.ctrl.advance(1, 1).ok);
}

TEST(a_pocket_a_little_off_is_found_with_one_small_move) {
    Rig r(three_slots());
    r.u().tape.pos_mm = -0.6;  // the tape sits 0.6 mm behind where it should
    CHECK(r.ctrl.advance(1, 1).ok);
    CHECK(near(r.u().tape.pos_mm, 3.9, 1e-6));
}

TEST(a_pocket_far_off_is_not_found_and_the_feeder_holds_a_fault) {
    Rig r(three_slots());
    r.u().tape.pos_mm = -2.0;
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(!o.ok && o.why == "fault");
    CHECK(r.ctrl.slot(1).fault == Fault::NoPocket && std::string(fault_code(Fault::NoPocket)) == "FDR-J02");
    CHECK(near(r.u().tape.pos_mm, 3.0));  // the advance and the two small moves, then it gave up
}

TEST(a_pitch_set_wrong_for_the_tape_is_caught_by_the_pocket_check) {
    Settings s = three_slots();
    s.pitch_mm = 2;
    Rig r(s);
    r.u().tape.pitch_mm = 4;  // the tape in the feeder has pockets every 4 mm
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(!o.ok && r.ctrl.slot(1).fault == Fault::NoPocket);
}

TEST(without_the_pocket_check_a_misplaced_tape_goes_unnoticed) {
    Settings s = three_slots();
    s.pocket_check = false;
    Rig r(s);
    r.u().tape.pos_mm = -2.0;
    CHECK(r.ctrl.advance(1, 1).ok);
}

TEST(a_backward_move_is_refused_unless_it_is_switched_on) {
    Rig r(three_slots());
    CHECK(r.ctrl.advance(1, 2).ok);
    Reply a = r.ctrl.request(1, kCmdRetract, {1});
    CHECK(!a.ok && a.error == kErrRetractOff);
    Settings s = three_slots();
    s.retract_enabled = true;
    Rig q(s);
    CHECK(q.ctrl.advance(1, 2).ok);
    CHECK(q.ctrl.advance(1, 2, true).ok);
    CHECK(near(q.u().tape.pos_mm, 0.0));
}

TEST(a_feeder_that_loses_the_node_stops_the_tape_and_holds_the_safe_state) {
    Settings s = three_slots();
    s.pitch_mm = 16;
    Rig r(s);
    CHECK(r.ctrl.request(1, kCmdAdvance, {8}).ok);  // 128 mm: more than the time of the cut
    r.chain.wait(500);
    r.chain.cut = true;
    r.chain.wait(s.link_lost_ms + 500);
    CHECK(r.u().unit.state() == State::Safe && r.u().unit.fault() == Fault::LinkLost);
    CHECK(std::string(fault_code(Fault::LinkLost)) == "FDR-L01");
    double stopped = r.u().tape.pos_mm;
    CHECK(stopped > 0.0 && stopped < 128.0);
    r.chain.wait(5000);
    CHECK(near(r.u().tape.pos_mm, stopped));
    r.chain.cut = false;
    Reply refused = r.ctrl.request(1, kCmdAdvance, {1});
    CHECK(!refused.ok && refused.error == kErrSafe);
    CHECK(r.ctrl.request(1, kCmdAck).ok);
    CHECK(r.ctrl.slot(1).state == State::Idle);
}

TEST(a_stop_to_all_stops_every_feeder) {
    Settings s = three_slots();
    s.pitch_mm = 16;
    Rig r(s);
    CHECK(r.ctrl.request(1, kCmdAdvance, {8}).ok);
    CHECK(r.ctrl.request(2, kCmdAdvance, {8}).ok);
    r.chain.wait(300);
    r.ctrl.broadcast(kCmdStop);
    CHECK(r.u(1).unit.state() == State::Idle && r.u(2).unit.state() == State::Idle);
    double p1 = r.u(1).tape.pos_mm;
    r.chain.wait(500);
    CHECK(near(r.u(1).tape.pos_mm, p1));
}

TEST(an_advance_that_does_not_finish_in_time_is_stopped) {
    Settings s = three_slots();
    s.pitch_mm = 16;
    s.advance_timeout_ms = 500;
    Rig r(s);
    Outcome o = r.ctrl.advance(1, 8);
    CHECK(!o.ok && o.why == "timeout");
    CHECK(r.u().unit.state() == State::Idle);
}

TEST(a_lost_or_damaged_frame_is_sent_again_and_the_run_is_the_same_every_time) {
    auto run = [](std::uint32_t seed, int& good, unsigned long& retries, unsigned long& errors, double& pos) {
        Settings s;
        s.slots = 1;
        s.bus_retries = 4;
        Rig r(s, seed);
        r.u().tape.parts = 1000;
        r.chain.drop_permille = 100;
        r.chain.corrupt_permille = 100;
        good = 0;
        for (int i = 0; i < 60; ++i) if (r.ctrl.advance(1, 1).ok) ++good;
        retries = r.ctrl.retries();
        errors = r.ctrl.bus_errors();
        pos = r.u().tape.pos_mm;
    };
    int g1 = 0, g2 = 0, g3 = 0;
    unsigned long r1 = 0, r2 = 0, r3 = 0, e1 = 0, e2 = 0, e3 = 0;
    double p1 = 0, p2 = 0, p3 = 0;
    run(42, g1, r1, e1, p1);
    run(42, g2, r2, e2, p2);
    run(43, g3, r3, e3, p3);
    CHECK(g1 == g2 && r1 == r2 && e1 == e2 && near(p1, p2));  // the same seed, the same run
    CHECK(r1 > 0 && e1 > 0);                                   // the noise did happen
    CHECK(g1 >= 50);                                           // and the repeats carried the work through
    CHECK(r1 != r3 || e1 != e3 || g1 != g3);                   // another seed, another run
    CHECK(p1 <= 60 * 4.0 + 1e-6);                              // a repeated frame never moved the tape twice: it moved at most what was asked
}

TEST(a_bus_that_is_cut_makes_the_slots_offline_and_they_come_back) {
    Rig r(three_slots());
    r.chain.cut = true;
    for (int i = 0; i < 3; ++i) CHECK(r.ctrl.request(1, kCmdPing).error == kErrNoAnswer);
    CHECK(!r.ctrl.slot(1).online);
    Outcome o = r.ctrl.advance(1, 1);
    CHECK(!o.ok && o.why == "offline");
    r.chain.cut = false;
    CHECK(r.ctrl.discover() == 3);
    CHECK(r.ctrl.slot(1).online);
}

TEST(a_feeder_that_is_unplugged_is_not_listed_and_the_others_work) {
    Settings s = three_slots();
    Rig q(s);
    q.u(2).silent = true;
    CHECK(q.ctrl.discover() == 2);
    CHECK(q.ctrl.slot(1).online && !q.ctrl.slot(2).online && q.ctrl.slot(3).online);
    CHECK(q.ctrl.advance(3, 1).ok);
    Outcome o = q.ctrl.advance(2, 1);
    CHECK(!o.ok && o.why == "offline");
}

TEST(identify_gives_the_firmware_version) {
    Rig r(three_slots());
    CHECK(r.ctrl.request(1, kCmdIdentify).ok);
    CHECK(r.ctrl.slot(1).fw[0] == kFirmwareMajor && r.ctrl.slot(1).fw[1] == kFirmwareMinor && r.ctrl.slot(1).fw[2] == kFirmwarePatch);
    CHECK(r.u().unit.identify_until_ms() > 0);
}

TEST(a_command_nobody_knows_and_a_bad_argument_are_answered_with_an_error) {
    Rig r(three_slots());
    Reply a = r.ctrl.request(1, 0x55);
    CHECK(!a.ok && a.error == kErrBadCommand);
    Reply b = r.ctrl.request(1, kCmdAdvance, {9});
    CHECK(!b.ok && b.error == kErrBadArgument);
    Reply c = r.ctrl.request(1, kCmdAdvance, {});
    CHECK(!c.ok && c.error == kErrBadArgument);
    Outcome d = r.ctrl.advance(7, 1);
    CHECK(!d.ok && d.why == "bad_slot");
    Outcome e = r.ctrl.advance(1, 0);
    CHECK(!e.ok && e.why == "bad_argument");
}

TEST(the_simulation_of_a_whole_reel_ends_in_the_same_place_every_time) {
    auto run = [](std::uint32_t seed) {
        Settings s;
        s.slots = 2;
        Rig r(s, seed);
        r.u(1).tape.parts = 40;
        r.u(1).tape.slip = 0.01;
        r.chain.drop_permille = 50;
        int ok = 0, empty = 0;
        for (int i = 0; i < 60; ++i) {
            Outcome o = r.ctrl.advance(1, 1);
            if (o.ok) ++ok; else if (o.why == "empty") ++empty;
        }
        return std::to_string(ok) + "/" + std::to_string(empty) + "/" + std::to_string(r.ctrl.retries());
    };
    CHECK(run(9) == run(9));
    std::string s = run(9);
    CHECK(s.rfind("39/", 0) == 0 || s.rfind("38/", 0) == 0 || s.rfind("37/", 0) == 0);  // the tape holds 40 pockets: about 39 advances go through
}

int main() { return check::run_all("unit"); }
