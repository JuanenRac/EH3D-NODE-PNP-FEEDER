// EH3D-NODE-PNP-FEEDER - core/feeder_node.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// What the node says on the network. It speaks HTTP on one port, with the same words as the other nodes of the suite (the token as `Authorization: Bearer <token>` or `?token=`):
//   GET  /api/status                       the node and all its feeders as one line of JSON (always answered)
//   GET  /api/feeder?slot=N                one feeder (needs the token)
//   POST /api/feeder/advance?slot=N[&count=K]   moves the tape K pitches (1..8, default 1) and answers when the move is over (needs the token)
//   POST /api/feeder/retract?slot=N[&count=K]   the same backwards; refused unless the setting retract_enabled is on
//   POST /api/feeder/identify?slot=N       the feeder blinks its LED and tells its firmware version
//   POST /api/feeder/count?slot=N&n=K      says how many parts are left in the tape (0..65534)
//   POST /api/feeder/pitch?slot=N&mm=P     the pitch of this feeder: 2, 4, 8, 12 or 16
//   POST /api/feeder/ack[?slot=N][&reenable=1]  clears a fault that is gone (with a slot), or the safe state of the node (without)
//   POST /api/feeder/stop                  stops every feeder at once
//   POST /api/discover                     asks every slot again who is there
// The words are built here so that the server, the desktop programs and the tests read the same ones. A node without a token refuses every request that moves or changes
// something. If the server is not heard for `server_lost_ms` the node stops the chain and refuses to move it until a person acknowledges.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "auth.hpp"
#include "feeder_chain.hpp"
#include "feeder_config.hpp"
#include "json.hpp"
#include "version.hpp"

namespace pnp_feeder {

struct Request {
    std::string method;
    std::string path;
    std::string query;
};

struct Response {
    int status = 200;
    std::string body;
};

/// Splits `POST /api/feeder/advance?slot=2 HTTP/1.1` (the first line of a request).
inline bool parse_request_line(const std::string& line, Request& out) {
    std::size_t a = line.find(' ');
    if (a == std::string::npos || a == 0) return false;
    std::size_t b = line.find(' ', a + 1);
    std::string target = line.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
    if (target.empty() || target[0] != '/') return false;
    out.method = line.substr(0, a);
    std::size_t q = target.find('?');
    out.path = q == std::string::npos ? target : target.substr(0, q);
    out.query = q == std::string::npos ? "" : target.substr(q + 1);
    return true;
}

inline std::string query_value(const std::string& query, const std::string& key) {
    std::size_t pos = 0;
    while (pos <= query.size()) {
        std::size_t end = query.find('&', pos);
        if (end == std::string::npos) end = query.size();
        std::string pair = query.substr(pos, end - pos);
        std::size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key) return pair.substr(eq + 1);
        pos = end + 1;
    }
    return "";
}

inline std::string error_body(const char* error, const char* key = nullptr) {
    eh3d::json::Writer w;
    w.begin_object().field("error", error);
    if (key) w.field("key", key);
    w.end_object();
    return w.str();
}

class Node {
public:
    Node(const Settings& settings, Bus& bus) : cfg_(settings), bus_(bus), ctrl_(bus, cfg_) {}

    /// At power-up: look for the feeders and start the watch over the server.
    void start() {
        ctrl_.discover();
        server_seen_ = bus_.now_ms();
        started_ = server_seen_;
        last_beat_ = server_seen_;
    }

    bool safe() const { return safe_; }
    const std::string& safe_reason() const { return safe_reason_; }
    Controller& controller() { return ctrl_; }

    /// Called often by the firmware: if the server has been silent too long, the chain is stopped.
    void tick() {
        const std::uint32_t now = bus_.now_ms();
        if (!safe_ && now - server_seen_ > static_cast<std::uint32_t>(cfg_.server_lost_ms)) {
            ctrl_.broadcast(kCmdStop);
            safe_ = true;
            safe_reason_ = "server_lost";
        }
        // the heartbeat: while the node is fine the feeders hear it often; when it stops (the node is safe, lost or dead) they stop by themselves after link_lost_ms
        if (!safe_ && now - last_beat_ >= static_cast<std::uint32_t>(cfg_.link_lost_ms / 3)) {
            ctrl_.broadcast(kCmdPing);
            last_beat_ = now;
        }
    }

    Response handle(const std::string& request_line, const std::string& bearer) {
        Request r;
        if (!parse_request_line(request_line, r)) return {400, error_body("bad_request")};
        const bool status = r.path == "/api/status";
        if (status && r.method != "GET") return {405, error_body("bad_method")};
        if (!status && !known(r)) return {404, error_body("not_found")};
        const bool authorized = !cfg_.token.empty() && eh3d::auth::same_text(cfg_.token, !bearer.empty() ? bearer : query_value(r.query, "token"));
        if (authorized) server_seen_ = bus_.now_ms();
        if (status) return {200, status_json()};
        if (!authorized) return {401, error_body("unauthorized")};
        if (r.path == "/api/feeder") {
            if (r.method != "GET") return {405, error_body("bad_method")};
            int slot = 0;
            if (!slot_of(r, slot)) return {400, error_body("bad_request", "slot")};
            return {200, feeder_json(slot, true)};
        }
        if (r.method != "POST") return {405, error_body("bad_method")};
        if (r.path == "/api/discover") {
            int found = ctrl_.discover();
            eh3d::json::Writer w;
            w.begin_object().field("ok", true).field("found", found).end_object();
            return {200, w.str()};
        }
        if (r.path == "/api/feeder/stop") {
            ctrl_.broadcast(kCmdStop);
            eh3d::json::Writer w;
            w.begin_object().field("ok", true).end_object();
            return {200, w.str()};
        }
        if (r.path == "/api/feeder/ack" && query_value(r.query, "slot").empty()) {
            if (!safe_) return {200, result_body(true, "not_held", 0)};
            safe_ = false;
            safe_reason_.clear();
            ctrl_.discover();
            return {200, result_body(true, "cleared", 0)};
        }
        int slot = 0;
        if (!slot_of(r, slot)) return {400, error_body("bad_request", "slot")};
        if (r.path == "/api/feeder/advance" || r.path == "/api/feeder/retract") {
            if (safe_) return {409, error_body("safe_state")};
            long long count = 1;
            std::string c = query_value(r.query, "count");
            if (!c.empty() && !cfg::parse_int(c, 1, 8, count)) return {400, error_body("bad_request", "count")};
            Outcome o = ctrl_.advance(slot, static_cast<int>(count), r.path == "/api/feeder/retract");
            return {outcome_status(o), result_body(o.ok, o.why.c_str(), slot)};
        }
        if (r.path == "/api/feeder/identify") return simple(slot, kCmdIdentify, {});
        if (r.path == "/api/feeder/count") {
            long long n = 0;
            if (!cfg::parse_int(query_value(r.query, "n"), 0, 65534, n)) return {400, error_body("bad_request", "n")};
            return simple(slot, kCmdSetCount, {static_cast<std::uint8_t>(n >> 8), static_cast<std::uint8_t>(n & 0xFF)});
        }
        if (r.path == "/api/feeder/pitch") {
            long long mm = 0;
            if (!cfg::parse_int(query_value(r.query, "mm"), 1, 100, mm) || !valid_pitch(mm)) return {400, error_body("bad_request", "mm")};
            return simple(slot, kCmdSetPitch, {static_cast<std::uint8_t>(mm)});
        }
        if (r.path == "/api/feeder/ack") return simple(slot, kCmdAck, {static_cast<std::uint8_t>(query_value(r.query, "reenable") == "1" ? 1 : 0)});
        return {404, error_body("not_found")};
    }

    /// The codes behind the verdict of the node on itself.
    std::vector<std::string> selfcheck_codes() const {
        std::vector<std::string> codes;
        if (cfg_.token.empty()) codes.push_back("FDN-S01");
        if (safe_) codes.push_back("FDN-S02");
        int online = 0, offline = 0, faulty = 0, low = 0;
        for (int n = 1; n <= cfg_.slots; ++n) {
            const SlotInfo& s = ctrl_.slot(n);
            if (s.online) ++online; else ++offline;
            if (s.seen && (s.state == State::Fault || s.state == State::Disabled || s.state == State::Safe)) ++faulty;
            if (s.seen && ((s.flags & 16) || s.state == State::Empty)) ++low;
        }
        if (online == 0) codes.push_back("FDN-B02");
        else if (offline > 0) codes.push_back("FDN-B01");
        if (faulty > 0) codes.push_back("FDN-F01");
        if (low > 0) codes.push_back("FDN-E01");
        return codes;
    }

    /// Every code the node's check can raise, for the page of the contract and its test.
    static std::vector<const char*> all_codes() { return {"FDN-S01", "FDN-S02", "FDN-B01", "FDN-B02", "FDN-F01", "FDN-E01"}; }

    static const char* severity(const std::string& code) {
        if (code == "FDN-S02" || code == "FDN-B02") return "fail";
        if (code == "FDN-E01") return "info";
        return "warn";
    }

    std::string status_json() const {
        std::vector<std::string> codes = selfcheck_codes();
        const char* verdict = "ok";
        std::string joined;
        for (const std::string& c : codes) {
            if (!joined.empty()) joined += ",";
            joined += c;
            std::string sev = severity(c);
            if (sev == "fail") verdict = "fail";
            else if (sev == "warn" && std::string(verdict) != "fail") verdict = "warn";
        }
        int online = 0;
        for (int n = 1; n <= cfg_.slots; ++n) if (ctrl_.slot(n).online) ++online;
        eh3d::json::Writer w;
        w.begin_object();
        w.field("node", cfg_.name).field("version", kVersion).field("slots", cfg_.slots).field("online", online);
        w.field("safe", safe_).field("safe_reason", safe_ ? safe_reason_ : std::string("none"));
        w.field("bus_exchanges", static_cast<long long>(ctrl_.exchanges())).field("bus_retries", static_cast<long long>(ctrl_.retries()));
        w.field("bus_errors", static_cast<long long>(ctrl_.bus_errors()));
        w.field("uptime_s", static_cast<long long>((bus_.now_ms() - started_) / 1000u));
        w.field("selfcheck", verdict).field("selfcheck_codes", joined);
        w.key("feeders").begin_array();
        for (int n = 1; n <= cfg_.slots; ++n) w.raw(feeder_json(n, false));
        w.end_array();
        w.end_object();
        return w.str();
    }

    std::string feeder_json(int n, bool with_version) const {
        const SlotInfo& s = ctrl_.slot(n);
        eh3d::json::Writer w;
        w.begin_object().field("slot", n).field("online", s.online);
        w.field("state", s.seen ? state_text(s.state) : "unknown").field("fault", s.seen ? fault_code(s.fault) : "none");
        w.key("remaining");
        if (s.seen && s.remaining != kCountUnknown) w.integer(s.remaining); else w.null();
        w.field("low", s.seen && (s.flags & 16) != 0).field("pitch_mm", static_cast<int>(s.pitch));
        w.field("tape", (s.flags & 1) != 0).field("pocket", (s.flags & 2) != 0).field("tension_ok", (s.flags & 4) != 0).field("film_clear", (s.flags & 8) != 0);
        if (with_version) {
            char fw[24];
            std::snprintf(fw, sizeof fw, "%d.%d.%d", s.fw[0], s.fw[1], s.fw[2]);
            w.field("firmware", fw);
        }
        w.end_object();
        return w.str();
    }

private:
    static bool known(const Request& r) {
        static const char* paths[] = {"/api/feeder", "/api/feeder/advance", "/api/feeder/retract", "/api/feeder/identify", "/api/feeder/count", "/api/feeder/pitch",
                                      "/api/feeder/ack", "/api/feeder/stop", "/api/discover"};
        for (const char* p : paths) if (r.path == p) return true;
        return false;
    }

    bool slot_of(const Request& r, int& slot) const {
        long long n = 0;
        if (!cfg::parse_int(query_value(r.query, "slot"), 1, cfg_.slots, n)) return false;
        slot = static_cast<int>(n);
        return true;
    }

    static int outcome_status(const Outcome& o) {
        if (o.ok) return 200;
        if (o.why == "offline" || o.why == "no_answer") return 503;
        if (o.why == "bad_slot" || o.why == "bad_argument") return 400;
        return 409;
    }

    std::string result_body(bool ok, const char* why, int slot) const {
        eh3d::json::Writer w;
        w.begin_object().field("ok", ok).field("why", why);
        if (slot > 0) w.field("slot", slot).key("feeder").raw(feeder_json(slot, false));
        w.end_object();
        return w.str();
    }

    Response simple(int slot, std::uint8_t cmd, const std::vector<std::uint8_t>& payload) {
        if (safe_) return {409, error_body("safe_state")};
        Reply r = ctrl_.request(static_cast<std::uint8_t>(slot), cmd, payload);
        Outcome o;
        o.ok = r.ok;
        o.why = r.ok ? "ok" : error_text(r.error);
        return {outcome_status(o), result_body(o.ok, o.why.c_str(), slot)};
    }

    Settings cfg_;
    Bus& bus_;
    Controller ctrl_;
    bool safe_ = false;
    std::string safe_reason_;
    std::uint32_t server_seen_ = 0;
    std::uint32_t started_ = 0;
    std::uint32_t last_beat_ = 0;
};

}  // namespace pnp_feeder
