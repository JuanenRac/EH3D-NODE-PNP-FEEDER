// EH3D-NODE-PNP-FEEDER - tests/test_frame.cpp: the check of the frames, the reading of frames out of noise, and the settings file.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../core/feeder_config.hpp"
#include "../core/feeder_frame.hpp"
#include "../core/feeder_sim.hpp"
#include "check.hpp"

using namespace pnp_feeder;

TEST(crc_matches_the_published_check_value) {
    const char* s = "123456789";
    CHECK(crc16(reinterpret_cast<const std::uint8_t*>(s), 9) == 0x29B1);
    CHECK(crc16(nullptr, 0) == 0xFFFF);
}

TEST(a_frame_survives_a_round_trip) {
    Frame f;
    f.addr = 7; f.cmd = kCmdAdvance; f.seq = 200; f.payload = {3};
    std::vector<std::uint8_t> bytes = encode(f);
    CHECK(bytes.size() == 6 + 1 + 2);
    CHECK(bytes[0] == kFrameStart);
    Decoder d;
    d.push(bytes);
    Frame g;
    CHECK(d.next(g));
    CHECK(g.addr == 7 && g.cmd == kCmdAdvance && g.seq == 200 && g.payload.size() == 1 && g.payload[0] == 3);
    CHECK(!d.next(g));
}

TEST(a_frame_with_no_payload_and_with_a_full_payload) {
    Frame f;
    f.addr = 1; f.cmd = kCmdPing; f.seq = 1;
    Decoder d;
    d.push(encode(f));
    Frame g;
    CHECK(d.next(g) && g.payload.empty());
    f.payload.assign(kMaxPayload + 10, 0xAB);  // more than fits: the encoder cuts it
    std::vector<std::uint8_t> bytes = encode(f);
    CHECK(bytes.size() == 6 + kMaxPayload + 2);
    d.push(bytes);
    CHECK(d.next(g) && g.payload.size() == kMaxPayload);
}

TEST(noise_between_frames_costs_nothing) {
    Frame a, b;
    a.addr = 2; a.cmd = kCmdStatus; a.seq = 1;
    b.addr = 3; b.cmd = kCmdPing; b.seq = 2;
    std::vector<std::uint8_t> bytes = {0x00, 0x13, 0xFF};
    for (auto x : encode(a)) bytes.push_back(x);
    bytes.push_back(0x55);
    for (auto x : encode(b)) bytes.push_back(x);
    Decoder d;
    d.push(bytes);
    Frame g;
    CHECK(d.next(g) && g.addr == 2 && g.seq == 1);
    CHECK(d.next(g) && g.addr == 3 && g.seq == 2);
    CHECK(!d.next(g));
    CHECK(d.dropped() >= 4);
}

TEST(a_start_byte_inside_the_payload_does_not_confuse_the_reader) {
    Frame f;
    f.addr = 4; f.cmd = kCmdSetCount; f.seq = 9; f.payload = {kFrameStart, kFrameStart};
    Decoder d;
    d.push(encode(f));
    Frame g;
    CHECK(d.next(g) && g.payload.size() == 2 && g.payload[0] == kFrameStart);
}

TEST(a_changed_bit_is_always_seen_and_the_next_frame_is_read) {
    Frame f;
    f.addr = 5; f.cmd = kCmdAdvance; f.seq = 4; f.payload = {1};
    std::vector<std::uint8_t> good = encode(f);
    // every single bit of the frame changed in turn: the frame is never accepted as it was sent with another content
    for (std::size_t i = 1; i < good.size(); ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            std::vector<std::uint8_t> bad = good;
            bad[i] ^= static_cast<std::uint8_t>(1u << bit);
            Decoder d;
            d.push(bad);
            for (int k = 0; k < 5; ++k) d.push(good);  // intact frames behind it (a stream goes on: a longer false length is only a wait)
            Frame g;
            bool got = false;
            while (d.next(g)) {
                // the only frame that may come out is the intact one
                CHECK(g.addr == 5 && g.cmd == kCmdAdvance && g.seq == 4 && g.payload.size() == 1 && g.payload[0] == 1);
                got = true;
            }
            CHECK(got);
        }
    }
}

TEST(a_wrong_version_and_a_wrong_length_are_refused) {
    Frame f;
    f.addr = 1; f.cmd = kCmdPing; f.seq = 1;
    std::vector<std::uint8_t> bytes = encode(f);
    bytes[1] = 2;  // another version
    Decoder d;
    d.push(bytes);
    Frame g;
    CHECK(!d.next(g));
    std::vector<std::uint8_t> big = encode(f);
    big[5] = 200;  // a length no frame has
    Decoder e;
    e.push(big);
    CHECK(!e.next(g));
}

TEST(random_bytes_never_give_a_frame_and_never_crash) {
    Rng rng(12345);
    Decoder d;
    int frames = 0;
    for (int i = 0; i < 20000; ++i) {
        std::uint8_t b = static_cast<std::uint8_t>(rng.next());
        d.push(&b, 1);
        Frame g;
        while (d.next(g)) ++frames;
    }
    CHECK(frames == 0);  // with this seed; a chance frame needs a valid check over random bytes
}

TEST(defaults_are_the_conservative_ones) {
    Settings s;
    CHECK(s.token.empty());
    CHECK(!s.retract_enabled);
    CHECK(s.pocket_check && s.film_check);
    CHECK(s.pitch_mm == 4 && s.slots == 8);
    CHECK(s.fault_limit <= s.fault_window);
    CHECK(s.tension_min < s.tension_max);
}

TEST(the_pitches_are_two_four_eight_twelve_and_sixteen) {
    for (long long mm : {2, 4, 8, 12, 16}) CHECK(valid_pitch(mm));
    for (long long mm : {0, 1, 3, 5, 6, 10, 24, -4}) CHECK(!valid_pitch(mm));
    Settings s;
    std::vector<Problem> p = parse_settings("pitch_mm = 12\n", s);
    CHECK(p.empty() && s.pitch_mm == 12);
    p = parse_settings("pitch_mm = 5\n", s);
    CHECK(p.size() == 1 && p[0].key == "pitch_mm" && p[0].code == "bad_value" && s.pitch_mm == 12);  // the value before stays
}

TEST(a_value_out_of_range_or_a_key_nobody_knows_is_reported_and_the_default_stays) {
    Settings s;
    std::vector<Problem> p = parse_settings("# a comment\nslots = 99\nmotion_mm_per_s = fast\nspeed = 3\nport = 0\nretract_enabled = maybe\nname = Bad Name\n\nmax_retries = 9\n", s);
    CHECK(p.size() == 7);
    CHECK(s.slots == 8 && s.motion_mm_per_s == 20.0 && s.port == 80 && !s.retract_enabled && s.name == "pnp-feeder" && s.max_retries == 2);
    bool unknown = false;
    for (auto& x : p) if (x.key == "speed" && x.code == "unknown_key") unknown = true;
    CHECK(unknown);
}

TEST(settings_that_disagree_go_back_to_the_defaults) {
    Settings s;
    std::vector<Problem> p = parse_settings("tension_min = 900\ntension_max = 100\n", s);
    CHECK(!p.empty());
    CHECK(s.tension_min == 200 && s.tension_max == 800);
    Settings t;
    p = parse_settings("fault_window = 3\nfault_limit = 5\n", t);
    CHECK(!p.empty() && t.fault_limit == 3 && t.fault_window == 6);
}

TEST(the_settings_text_is_read_back_the_same_and_never_holds_the_token) {
    Settings s;
    s.token = "secret-token";
    s.slots = 12; s.pitch_mm = 8; s.actuator = Actuator::Servo; s.retract_enabled = true; s.motion_mm_per_s = 12.5; s.fault_window = 10; s.fault_limit = 4;
    std::string text = settings_text(s);
    CHECK(text.find("secret-token") == std::string::npos);
    CHECK(text.find("token") == std::string::npos);
    Settings back;
    CHECK(parse_settings(text, back).empty());
    CHECK(back.slots == 12 && back.pitch_mm == 8 && back.actuator == Actuator::Servo && back.retract_enabled && back.motion_mm_per_s == 12.5 && back.fault_window == 10 && back.fault_limit == 4);
    CHECK(settings_text(back) == text);
}

TEST(the_token_is_read_but_limited) {
    Settings s;
    CHECK(parse_settings("token = abc123\n", s).empty() && s.token == "abc123");
    std::string long_token(65, 'x');
    CHECK(!parse_settings("token = " + long_token + "\n", s).empty() && s.token == "abc123");
}

TEST(the_example_file_is_the_defaults_names_every_key_and_has_no_problem) {
    std::ifstream in(std::string(PNP_FEEDER_ROOT) + "/feeder.cfg.example", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string example = ss.str();
    CHECK(!example.empty());
    Settings s;
    CHECK(parse_settings(example, s).empty());
    CHECK(settings_text(s) == settings_text(Settings()));
    // every key that the node writes is in the example, and the token is there as a comment
    std::istringstream keys(settings_text(Settings()));
    std::string line;
    while (std::getline(keys, line)) CHECK(example.find("\n" + line.substr(0, line.find(' ')) + " = ") != std::string::npos);
    CHECK(example.find("# token = ") != std::string::npos);
}

TEST(a_comment_after_a_value_is_cut_off_and_a_hash_in_a_token_is_not_possible) {
    Settings s;
    CHECK(parse_settings("slots = 5   # a comment\n", s).empty() && s.slots == 5);
    Settings t;
    CHECK(parse_settings("token = abc#def\n", t).empty() && t.token == "abc");
}

int main() { return check::run_all("frame"); }
