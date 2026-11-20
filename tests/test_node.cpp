// EH3D-NODE-PNP-FEEDER - tests/test_node.cpp: the words of the node on the network, its token, its watch over the server and the page of the contract, which this test reads
// and compares with what the node answers, so that the page cannot go stale.
#include <fstream>
#include <set>
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

struct Rig {
    Settings s;
    SimChain chain;
    Node node;
    explicit Rig(Settings in, std::uint32_t seed = 3) : s(in), chain(s, seed), node(s, chain) { node.start(); }
    // the node is called often by its firmware: time goes on in steps of 100 ms
    void run_for(std::uint32_t ms) {
        for (std::uint32_t t = 0; t < ms; t += 100) { chain.wait(100); node.tick(); }
    }
    Response get(const std::string& target, bool with_token = true) { return node.handle("GET " + target + " HTTP/1.1", with_token ? "tok" : ""); }
    Response post(const std::string& target, bool with_token = true) { return node.handle("POST " + target + " HTTP/1.1", with_token ? "tok" : ""); }
};

Settings with_token() {
    Settings s;
    s.slots = 3;
    s.token = "tok";
    return s;
}

bool parse(const std::string& text, eh3d::json::Value& v) { return eh3d::json::parse(text, v); }

std::string read_file(const std::string& rel) {
    std::ifstream in(std::string(PNP_FEEDER_ROOT) + "/" + rel, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// The first column of the rows of the table that follows the heading `heading` in the page: the words between backticks.
std::set<std::string> keys_of_section(const std::string& page, const std::string& heading) {
    std::set<std::string> keys;
    std::size_t at = page.find(heading);
    if (at == std::string::npos) return keys;
    std::size_t end = page.find("\n## ", at + heading.size());
    std::istringstream in(page.substr(at, end == std::string::npos ? std::string::npos : end - at));
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("| `", 0) != 0) continue;
        std::size_t close = line.find('`', 3);
        if (close != std::string::npos) keys.insert(line.substr(3, close - 3));
    }
    return keys;
}

std::set<std::string> codes_in(const std::string& page, const std::string& prefix) {
    std::set<std::string> codes;
    for (std::size_t at = page.find(prefix); at != std::string::npos; at = page.find(prefix, at + 1)) {
        if (at + 7 <= page.size()) codes.insert(page.substr(at, 7));
    }
    return codes;
}

}  // namespace

TEST(the_status_is_always_answered_and_is_valid_json) {
    Rig r(with_token());
    Response a = r.get("/api/status", false);
    CHECK(a.status == 200);
    eh3d::json::Value v;
    CHECK(parse(a.body, v) && v.is_object());
    CHECK(v.string_or("node", "") == "pnp-feeder");
    CHECK(v.string_or("version", "") == kVersion);
    CHECK(v.integer_or("slots", 0, 0, 100) == 3 && v.integer_or("online", 0, 0, 100) == 3);
    CHECK(v.string_or("selfcheck", "") == "ok");
    const eh3d::json::Value* feeders = v.get("feeders");
    CHECK(feeders != nullptr && feeders->is_array() && feeders->items.size() == 3);
    CHECK(feeders->items[0].string_or("state", "") == "idle");
}

TEST(a_node_without_a_token_refuses_everything_that_moves_and_says_so) {
    Settings s;
    s.slots = 2;
    Rig r(s);
    Response a = r.post("/api/feeder/advance?slot=1", true);
    CHECK(a.status == 401);
    Response b = r.node.handle("POST /api/feeder/advance?slot=1&token= HTTP/1.1", "");
    CHECK(b.status == 401);
    Response c = r.get("/api/feeder?slot=1");
    CHECK(c.status == 401);
    eh3d::json::Value v;
    CHECK(parse(r.get("/api/status", false).body, v));
    CHECK(v.string_or("selfcheck", "") == "warn" && v.string_or("selfcheck_codes", "").find("FDN-S01") != std::string::npos);
    CHECK(r.chain.unit(1).tape.pos_mm == 0.0);
}

TEST(the_token_goes_in_the_header_or_in_the_query_and_a_wrong_one_is_refused) {
    Rig r(with_token());
    CHECK(r.node.handle("GET /api/feeder?slot=1 HTTP/1.1", "tok").status == 200);
    CHECK(r.node.handle("GET /api/feeder?slot=1&token=tok HTTP/1.1", "").status == 200);
    CHECK(r.node.handle("GET /api/feeder?slot=1 HTTP/1.1", "tokk").status == 401);
    CHECK(r.node.handle("GET /api/feeder?slot=1 HTTP/1.1", "to").status == 401);
    CHECK(r.node.handle("GET /api/feeder?slot=1 HTTP/1.1", "").status == 401);
    CHECK(r.node.handle("GET /api/feeder?slot=1&token=nope HTTP/1.1", "").status == 401);
}

TEST(an_advance_over_http_moves_the_tape_and_says_how_it_ended) {
    Rig r(with_token());
    Response a = r.post("/api/feeder/advance?slot=2&count=2");
    CHECK(a.status == 200);
    eh3d::json::Value v;
    CHECK(parse(a.body, v) && v.bool_or("ok", false) && v.string_or("why", "") == "ok" && v.integer_or("slot", 0, 0, 99) == 2);
    const eh3d::json::Value* f = v.get("feeder");
    CHECK(f != nullptr && f->string_or("state", "") == "idle");
    CHECK(r.chain.unit(2).tape.pos_mm > 7.99 && r.chain.unit(2).tape.pos_mm < 8.01);
    CHECK(r.chain.unit(1).tape.pos_mm == 0.0);
}

TEST(a_bad_request_is_a_400_with_the_word_that_is_wrong) {
    Rig r(with_token());
    eh3d::json::Value v;
    Response a = r.post("/api/feeder/advance?slot=9");
    CHECK(a.status == 400 && parse(a.body, v) && v.string_or("key", "") == "slot");
    Response b = r.post("/api/feeder/advance?slot=1&count=9");
    CHECK(b.status == 400 && parse(b.body, v) && v.string_or("key", "") == "count");
    Response c = r.post("/api/feeder/advance");
    CHECK(c.status == 400);
    Response d = r.post("/api/feeder/count?slot=1&n=70000");
    CHECK(d.status == 400 && parse(d.body, v) && v.string_or("key", "") == "n");
    Response e = r.post("/api/feeder/pitch?slot=1&mm=5");
    CHECK(e.status == 400 && parse(e.body, v) && v.string_or("key", "") == "mm");
    CHECK(r.node.handle("nonsense", "tok").status == 400);
    CHECK(r.node.handle("GET  HTTP/1.1", "tok").status == 400);
}

TEST(unknown_paths_and_wrong_verbs) {
    Rig r(with_token());
    CHECK(r.get("/api/nothing").status == 404);
    CHECK(r.get("/api/feeder/advance?slot=1").status == 405);
    CHECK(r.post("/api/status").status == 405);
    CHECK(r.post("/api/feeder?slot=1").status == 405);
}

TEST(what_the_feeder_refuses_comes_back_as_a_409_with_the_reason) {
    Rig r(with_token());
    r.chain.unit(1).tape.tension = 50;
    Response a = r.post("/api/feeder/advance?slot=1");
    eh3d::json::Value v;
    CHECK(a.status == 409 && parse(a.body, v) && !v.bool_or("ok", true) && v.string_or("why", "") == "tension");
    Response b = r.post("/api/feeder/retract?slot=2");
    CHECK(b.status == 409 && parse(b.body, v) && v.string_or("why", "") == "retract_off");
}

TEST(a_slot_that_is_not_there_is_a_503) {
    Rig r(with_token());
    r.chain.unit(3).silent = true;
    CHECK(r.post("/api/discover").status == 200);
    Response a = r.post("/api/feeder/advance?slot=3");
    eh3d::json::Value v;
    CHECK(a.status == 503 && parse(a.body, v) && v.string_or("why", "") == "offline");
    CHECK(parse(r.get("/api/status", false).body, v) && v.string_or("selfcheck_codes", "").find("FDN-B01") != std::string::npos);
}

TEST(count_pitch_identify_and_ack_work_on_one_feeder) {
    Rig r(with_token());
    eh3d::json::Value v;
    CHECK(r.post("/api/feeder/count?slot=1&n=120").status == 200);
    CHECK(r.post("/api/feeder/pitch?slot=1&mm=8").status == 200);
    CHECK(r.post("/api/feeder/identify?slot=1").status == 200);
    CHECK(parse(r.get("/api/feeder?slot=1").body, v));
    CHECK(v.integer_or("remaining", -1, -1, 70000) == 120 && v.integer_or("pitch_mm", 0, 0, 99) == 8 && v.string_or("firmware", "") == "0.0.1");
    CHECK(parse(r.get("/api/feeder?slot=2").body, v) && v.get("remaining") != nullptr && v.get("remaining")->type == eh3d::json::Type::kNull);
    CHECK(parse(r.post("/api/feeder/ack?slot=1").body, v) && v.bool_or("ok", false));
}

TEST(a_stop_over_http_stops_the_chain) {
    Settings s = with_token();
    s.pitch_mm = 16;
    Rig r(s);
    r.node.controller().request(1, kCmdAdvance, {8});
    r.chain.wait(300);
    CHECK(r.post("/api/feeder/stop").status == 200);
    CHECK(r.chain.unit(1).unit.state() == State::Idle);
}

TEST(a_server_that_keeps_asking_keeps_the_chain_awake) {
    Rig r(with_token());
    for (int i = 0; i < 12; ++i) {  // 60 s, a poll every 5 s: longer than the watch of the server and of the feeders
        r.run_for(5000);
        CHECK(r.get("/api/status").status == 200);
    }
    CHECK(!r.node.safe());
    CHECK(r.chain.unit(1).unit.state() == State::Idle);
    CHECK(r.post("/api/feeder/advance?slot=1").status == 200);
}

TEST(a_server_that_goes_silent_puts_the_node_and_the_feeders_in_the_safe_state) {
    Rig r(with_token());
    r.run_for(static_cast<std::uint32_t>(r.s.server_lost_ms + r.s.link_lost_ms) + 1500);
    CHECK(r.node.safe() && r.node.safe_reason() == "server_lost");
    CHECK(r.chain.unit(1).unit.state() == State::Safe && r.chain.unit(1).unit.fault() == Fault::LinkLost);
    eh3d::json::Value v;
    CHECK(parse(r.get("/api/status", false).body, v));
    CHECK(v.bool_or("safe", false) && v.string_or("safe_reason", "") == "server_lost" && v.string_or("selfcheck", "") == "fail");
    Response a = r.post("/api/feeder/advance?slot=1");
    CHECK(a.status == 409 && parse(a.body, v) && v.string_or("error", "") == "safe_state");
    CHECK(r.post("/api/feeder/count?slot=1&n=5").status == 409);
    // a person acknowledges the node, then each feeder; only then does the tape move again
    CHECK(parse(r.post("/api/feeder/ack").body, v) && v.string_or("why", "") == "cleared");
    CHECK(!r.node.safe());
    CHECK(r.post("/api/feeder/advance?slot=1").status == 409);  // the feeder itself still holds its fault
    for (int n = 1; n <= 3; ++n) CHECK(r.post("/api/feeder/ack?slot=" + std::to_string(n)).status == 200);
    CHECK(r.post("/api/feeder/advance?slot=1").status == 200);
    CHECK(parse(r.post("/api/feeder/ack").body, v) && v.string_or("why", "") == "not_held");
}

TEST(a_stopped_heartbeat_is_what_the_feeders_see_as_a_lost_node) {
    Rig r(with_token());
    r.run_for(1000);
    CHECK(r.chain.unit(1).unit.state() == State::Idle);
    r.chain.cut = true;  // the cable is cut but the node and the server are fine
    r.run_for(static_cast<std::uint32_t>(r.s.link_lost_ms) + 500);
    CHECK(r.chain.unit(1).unit.state() == State::Safe);
}

TEST(the_status_words_are_the_words_of_the_page_of_the_contract) {
    std::string page = read_file("docs/CONTRACT.md");
    CHECK(!page.empty());
    Rig r(with_token());
    eh3d::json::Value v;
    CHECK(parse(r.get("/api/status", false).body, v));
    std::set<std::string> in_status(v.names.begin(), v.names.end());
    CHECK(in_status == keys_of_section(page, "## `GET /api/status`"));
    const eh3d::json::Value* feeders = v.get("feeders");
    CHECK(feeders != nullptr && !feeders->items.empty());
    std::set<std::string> in_feeder(feeders->items[0].names.begin(), feeders->items[0].names.end());
    eh3d::json::Value one;
    CHECK(parse(r.get("/api/feeder?slot=1").body, one));
    in_feeder.insert(one.names.begin(), one.names.end());  // the one feeder adds the firmware
    CHECK(in_feeder == keys_of_section(page, "## `GET /api/feeder?slot=N`"));
}

TEST(every_code_is_in_the_page_and_every_code_of_the_page_exists) {
    std::string page = read_file("docs/CONTRACT.md");
    std::set<std::string> node_codes, fault_codes;
    for (const char* c : Node::all_codes()) node_codes.insert(c);
    for (const char* c : all_fault_codes()) fault_codes.insert(c);
    CHECK(node_codes == codes_in(page, "FDN-"));
    CHECK(fault_codes == codes_in(page, "FDR-"));
    // every fault the unit can hold has one of the codes
    for (int f = 1; f <= 7; ++f) CHECK(fault_codes.count(fault_code(static_cast<Fault>(f))) == 1);
    for (const char* c : Node::all_codes()) CHECK(std::string(Node::severity(c)).size() > 0);
}

TEST(every_route_of_the_node_is_in_the_page) {
    std::string page = read_file("docs/CONTRACT.md");
    for (const char* route : {"/api/status", "/api/feeder?slot=N", "/api/feeder/advance", "/api/feeder/retract", "/api/feeder/identify", "/api/feeder/count", "/api/feeder/pitch",
                              "/api/feeder/ack", "/api/feeder/stop", "/api/discover"})
        CHECK(page.find(route) != std::string::npos);
    for (const char* cmd : {"PING", "IDENTIFY", "ADVANCE", "RETRACT", "STATUS", "SET_COUNT", "STOP", "ACK", "SET_PITCH"}) CHECK(page.find(cmd) != std::string::npos);
}

TEST(the_version_of_the_firmware_is_the_one_of_the_manifest) {
    eh3d::json::Value m;
    CHECK(parse(read_file("eh3d.project.json"), m));
    CHECK(m.string_or("version", "?") == std::string(kVersion));
}

int main() { return check::run_all("node"); }
