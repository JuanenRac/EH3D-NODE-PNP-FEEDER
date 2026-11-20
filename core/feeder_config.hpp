// EH3D-NODE-PNP-FEEDER - core/feeder_config.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Every tuned value of the node lives in one settings file (feeder.cfg), never in the code. A value out of range or a key nobody knows is reported and the default
// stays, so a typing mistake never turns a safety limit off. The defaults are the conservative ones.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "cfg_text.hpp"

namespace pnp_feeder {

enum class Actuator { Stepper, Servo };

/// The steps of the tape the node knows how to advance (the pitch of the pockets), in millimetres.
inline bool valid_pitch(long long mm) { return mm == 2 || mm == 4 || mm == 8 || mm == 12 || mm == 16; }

struct Settings {
    // the link
    std::string name = "pnp-feeder";
    std::string token;                       // write-only: never shown, never logged; empty: the node refuses every command that changes something
    std::uint16_t port = 80;                 // 1..65535
    // the chain of feeders
    int slots = 8;                           // 1..32 feeders on the chain
    int pitch_mm = 4;                        // 2, 4, 8, 12 or 16: how far one advance moves the tape
    Actuator actuator = Actuator::Stepper;   // stepper or servo
    double steps_per_mm = 80.0;              // 1..2000 (stepper)
    double servo_deg_per_mm = 9.0;           // 0.1..90 (servo)
    double motion_mm_per_s = 20.0;           // 1..200
    double retry_mm = 0.5;                   // 0.1..2: the small move made when the pocket is not found after an advance
    int max_retries = 2;                     // 0..5
    bool pocket_check = true;                // look for the pocket after every advance (closed loop)
    bool film_check = true;                  // refuse to go on while the cover film is not peeled
    int tension_min = 200;                   // 0..1023 sensor units: below this the tape is slack
    int tension_max = 800;                   // 0..1023: above this the tape is too tight
    int low_pockets = 20;                    // 0..1000: warn when this few parts are left (when the count is known)
    bool retract_enabled = false;            // a backward move is refused unless this is on
    // the watchdogs and the repeat of the bus
    int link_lost_ms = 3000;                 // 200..60000: a feeder that hears nothing from the node for this long stops and holds a fault
    int server_lost_ms = 15000;              // 1000..600000: the node that hears nothing from the server for this long stops the chain
    int advance_timeout_ms = 5000;           // 500..60000: an advance that is not finished by then is stopped
    int bus_retries = 2;                     // 0..5: a frame without an answer or with a bad check is sent again this many times
    int offline_after = 3;                   // 1..20: failed exchanges in a row after which a slot is offline
    // the automatic stop of a bad feeder: this many faults in the last `fault_window` advances disable it until a person re-enables it
    int fault_window = 6;                    // 2..32
    int fault_limit = 3;                     // 1..32 (never above the window)
};

struct Problem {
    std::string key;
    std::string code;  // unknown_key, bad_value, out_of_range
};

inline const char* actuator_text(Actuator a) { return a == Actuator::Servo ? "servo" : "stepper"; }

namespace detail {

inline void set_int(const std::string& key, const std::string& v, long long lo, long long hi, int& target, std::vector<Problem>& out) {
    long long n = 0;
    if (!cfg::parse_int(v, lo, hi, n)) { out.push_back({key, "out_of_range"}); return; }
    target = static_cast<int>(n);
}

inline void set_double(const std::string& key, const std::string& v, double lo, double hi, double& target, std::vector<Problem>& out) {
    double d = 0;
    if (!cfg::parse_double(v, lo, hi, d)) { out.push_back({key, "out_of_range"}); return; }
    target = d;
}

inline void set_bool(const std::string& key, const std::string& v, bool& target, std::vector<Problem>& out) {
    bool b = false;
    if (!cfg::parse_bool(v, b)) { out.push_back({key, "bad_value"}); return; }
    target = b;
}

inline bool name_ok(const std::string& n) {
    if (n.empty() || n.size() > 32) return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

}  // namespace detail

/// Reads the text of a settings file into `s` (which holds the defaults or the values read before). Returns what was refused.
inline std::vector<Problem> parse_settings(const std::string& text, Settings& s) {
    std::vector<Problem> out;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string key, v;
        if (cfg::split_line(text.substr(pos, end - pos), key, v)) {
            long long n = 0;
            if (key == "name") { if (detail::name_ok(v)) s.name = v; else out.push_back({key, "bad_value"}); }
            else if (key == "token") { if (v.size() <= 64) s.token = v; else out.push_back({key, "out_of_range"}); }
            else if (key == "port") { if (cfg::parse_int(v, 1, 65535, n)) s.port = static_cast<std::uint16_t>(n); else out.push_back({key, "out_of_range"}); }
            else if (key == "slots") detail::set_int(key, v, 1, 32, s.slots, out);
            else if (key == "pitch_mm") { if (cfg::parse_int(v, 1, 100, n) && valid_pitch(n)) s.pitch_mm = static_cast<int>(n); else out.push_back({key, "bad_value"}); }
            else if (key == "actuator") { if (v == "stepper") s.actuator = Actuator::Stepper; else if (v == "servo") s.actuator = Actuator::Servo; else out.push_back({key, "bad_value"}); }
            else if (key == "steps_per_mm") detail::set_double(key, v, 1, 2000, s.steps_per_mm, out);
            else if (key == "servo_deg_per_mm") detail::set_double(key, v, 0.1, 90, s.servo_deg_per_mm, out);
            else if (key == "motion_mm_per_s") detail::set_double(key, v, 1, 200, s.motion_mm_per_s, out);
            else if (key == "retry_mm") detail::set_double(key, v, 0.1, 2, s.retry_mm, out);
            else if (key == "max_retries") detail::set_int(key, v, 0, 5, s.max_retries, out);
            else if (key == "pocket_check") detail::set_bool(key, v, s.pocket_check, out);
            else if (key == "film_check") detail::set_bool(key, v, s.film_check, out);
            else if (key == "tension_min") detail::set_int(key, v, 0, 1023, s.tension_min, out);
            else if (key == "tension_max") detail::set_int(key, v, 0, 1023, s.tension_max, out);
            else if (key == "low_pockets") detail::set_int(key, v, 0, 1000, s.low_pockets, out);
            else if (key == "retract_enabled") detail::set_bool(key, v, s.retract_enabled, out);
            else if (key == "link_lost_ms") detail::set_int(key, v, 200, 60000, s.link_lost_ms, out);
            else if (key == "server_lost_ms") detail::set_int(key, v, 1000, 600000, s.server_lost_ms, out);
            else if (key == "advance_timeout_ms") detail::set_int(key, v, 500, 60000, s.advance_timeout_ms, out);
            else if (key == "bus_retries") detail::set_int(key, v, 0, 5, s.bus_retries, out);
            else if (key == "offline_after") detail::set_int(key, v, 1, 20, s.offline_after, out);
            else if (key == "fault_window") detail::set_int(key, v, 2, 32, s.fault_window, out);
            else if (key == "fault_limit") detail::set_int(key, v, 1, 32, s.fault_limit, out);
            else out.push_back({key, "unknown_key"});
        }
        pos = end + 1;
    }
    // the settings must also agree with each other; a pair that does not is put back to the defaults, the safe way
    Settings defaults;
    if (s.tension_min >= s.tension_max) { out.push_back({"tension_min", "out_of_range"}); s.tension_min = defaults.tension_min; s.tension_max = defaults.tension_max; }
    if (s.fault_limit > s.fault_window) { out.push_back({"fault_limit", "out_of_range"}); s.fault_limit = defaults.fault_limit; s.fault_window = defaults.fault_window; }
    return out;
}

/// The text of the settings as a file (the token is never written).
inline std::string settings_text(const Settings& s) {
    char b[1400];
    std::snprintf(b, sizeof b,
                  "name = %s\nport = %u\nslots = %d\npitch_mm = %d\nactuator = %s\nsteps_per_mm = %g\nservo_deg_per_mm = %g\nmotion_mm_per_s = %g\nretry_mm = %g\nmax_retries = %d\n"
                  "pocket_check = %d\nfilm_check = %d\ntension_min = %d\ntension_max = %d\nlow_pockets = %d\nretract_enabled = %d\nlink_lost_ms = %d\nserver_lost_ms = %d\n"
                  "advance_timeout_ms = %d\nbus_retries = %d\noffline_after = %d\nfault_window = %d\nfault_limit = %d\n",
                  s.name.c_str(), static_cast<unsigned>(s.port), s.slots, s.pitch_mm, actuator_text(s.actuator), s.steps_per_mm, s.servo_deg_per_mm, s.motion_mm_per_s, s.retry_mm,
                  s.max_retries, s.pocket_check ? 1 : 0, s.film_check ? 1 : 0, s.tension_min, s.tension_max, s.low_pockets, s.retract_enabled ? 1 : 0, s.link_lost_ms,
                  s.server_lost_ms, s.advance_timeout_ms, s.bus_retries, s.offline_after, s.fault_window, s.fault_limit);
    return b;
}

}  // namespace pnp_feeder
