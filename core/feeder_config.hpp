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

/// The bus the chain of feeders is wired with. The same frame `eh3d.feeder/1` goes over all three; the bus is chosen in the settings file.
enum class BusKind { Rs485, Can, I2c };

inline const char* bus_text(BusKind b) { return b == BusKind::Can ? "can" : (b == BusKind::I2c ? "i2c" : "rs485"); }

/// The steps of the tape the node knows how to advance (the pitch of the pockets), in millimetres.
inline bool valid_pitch(long long mm) { return mm == 2 || mm == 4 || mm == 8 || mm == 12 || mm == 16; }

/// The most feeders one chain holds, whatever the bus (an address of the frame is 1 to 32).
constexpr int kMaxSlots = 32;
/// The 7-bit addresses 0x00 to 0x07 and 0x78 to 0x7F are reserved by the I2C standard: a feeder may sit on 0x08 to 0x77 only.
constexpr int kI2cFirstUsable = 0x08;
constexpr int kI2cLastUsable = 0x77;
/// The longest frame of `eh3d.feeder/1`: the start byte, five header bytes, 32 payload bytes and the CRC.
constexpr int kMaxFrameBytes = 1 + 5 + 32 + 2;

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
    // the bus under the chain
    BusKind bus = BusKind::Rs485;            // rs485, can or i2c
    int reply_timeout_ms = 20;               // 5..500: how long the node waits for the answer to a frame (a frame that takes longer is counted as lost)
    int rs485_baud = 115200;                 // 9600..2000000
    int rs485_turnaround_us = 100;           // 10..5000: the time between the end of a frame and the switch of the direction line (DE/RE) of the transceiver
    int can_bitrate = 250000;                // 125000, 250000, 500000 or 1000000
    int can_id_base = 256;                   // 1..1900: the first identifier; a frame to feeder N has id base+N, its answer base+64+N, a frame to all has id base
    int i2c_clock_hz = 100000;               // 100000 (standard) or 400000 (fast)
    int i2c_base_addr = 16;                  // 8..119: the 7-bit address of feeder 1; feeder N sits on base+N-1
    int i2c_stretch_limit_us = 5000;         // 100..100000: a feeder that holds the clock longer than this is a timeout
    // the automatic stop of a bad feeder: this many faults in the last `fault_window` advances disable it until a person re-enables it
    int fault_window = 6;                    // 2..32
    int fault_limit = 3;                     // 1..32 (never above the window)
};

struct Problem {
    std::string key;
    std::string code;  // unknown_key, bad_value, out_of_range
};

/// The most feeders the chain holds on its bus: 32 on RS-485 and on CAN; on I2C the 7-bit addresses from the base up to 0x77 (32 with the default base).
inline int bus_max_slots(const Settings& s) {
    if (s.bus == BusKind::I2c) {
        const int room = kI2cLastUsable - s.i2c_base_addr + 1;
        return room < kMaxSlots ? room : kMaxSlots;
    }
    return kMaxSlots;
}

/// The time, in microseconds, that the longest frame and the longest answer take on the wire, with the turnaround or the overhead of the bus. The reply timeout must be longer than this.
inline long long bus_round_trip_us(const Settings& s) {
    switch (s.bus) {
        case BusKind::Can: {
            const long long fragments = (kMaxFrameBytes + 6) / 7;                 // seven bytes of the frame in each CAN frame
            const long long bits = fragments * (47 + 8 * 8);                      // a standard frame with 8 data bytes, without bit stuffing
            return 2 * bits * 1000000LL / s.can_bitrate;
        }
        case BusKind::I2c: {
            const long long clocks = 2 * ((kMaxFrameBytes + 1) * 9 + 2);          // address and bytes at 9 clocks each, start and stop, write then read
            return clocks * 1000000LL / s.i2c_clock_hz;
        }
        default: {
            const long long bits = static_cast<long long>(kMaxFrameBytes) * 10;   // start, eight data bits, stop
            return 2 * (s.rs485_turnaround_us + bits * 1000000LL / s.rs485_baud);
        }
    }
}

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
            else if (key == "bus") { if (v == "rs485") s.bus = BusKind::Rs485; else if (v == "can") s.bus = BusKind::Can; else if (v == "i2c") s.bus = BusKind::I2c; else out.push_back({key, "bad_value"}); }
            else if (key == "reply_timeout_ms") detail::set_int(key, v, 5, 500, s.reply_timeout_ms, out);
            else if (key == "rs485_baud") detail::set_int(key, v, 9600, 2000000, s.rs485_baud, out);
            else if (key == "rs485_turnaround_us") detail::set_int(key, v, 10, 5000, s.rs485_turnaround_us, out);
            else if (key == "can_bitrate") { if (cfg::parse_int(v, 1, 2000000, n) && (n == 125000 || n == 250000 || n == 500000 || n == 1000000)) s.can_bitrate = static_cast<int>(n); else out.push_back({key, "bad_value"}); }
            else if (key == "can_id_base") detail::set_int(key, v, 1, 1900, s.can_id_base, out);
            else if (key == "i2c_clock_hz") { if (cfg::parse_int(v, 1, 1000000, n) && (n == 100000 || n == 400000)) s.i2c_clock_hz = static_cast<int>(n); else out.push_back({key, "bad_value"}); }
            else if (key == "i2c_base_addr") detail::set_int(key, v, kI2cFirstUsable, kI2cLastUsable, s.i2c_base_addr, out);
            else if (key == "i2c_stretch_limit_us") detail::set_int(key, v, 100, 100000, s.i2c_stretch_limit_us, out);
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
    // the chain must fit on its bus (32 addresses; on I2C the addresses above the base) and a frame must fit in the reply timeout
    if (s.slots > bus_max_slots(s)) { out.push_back({"slots", "out_of_range"}); s.slots = defaults.slots; s.i2c_base_addr = defaults.i2c_base_addr; }
    if (bus_round_trip_us(s) > static_cast<long long>(s.reply_timeout_ms) * 1000) {
        out.push_back({"reply_timeout_ms", "out_of_range"});
        s.reply_timeout_ms = defaults.reply_timeout_ms; s.rs485_baud = defaults.rs485_baud; s.rs485_turnaround_us = defaults.rs485_turnaround_us;
        s.can_bitrate = defaults.can_bitrate; s.i2c_clock_hz = defaults.i2c_clock_hz;
    }
    return out;
}

/// The text of the settings as a file (the token is never written).
inline std::string settings_text(const Settings& s) {
    char b[2400];
    std::snprintf(b, sizeof b,
                  "name = %s\nport = %u\nslots = %d\npitch_mm = %d\nactuator = %s\nsteps_per_mm = %g\nservo_deg_per_mm = %g\nmotion_mm_per_s = %g\nretry_mm = %g\nmax_retries = %d\n"
                  "pocket_check = %d\nfilm_check = %d\ntension_min = %d\ntension_max = %d\nlow_pockets = %d\nretract_enabled = %d\nlink_lost_ms = %d\nserver_lost_ms = %d\n"
                  "advance_timeout_ms = %d\nbus_retries = %d\noffline_after = %d\nbus = %s\nreply_timeout_ms = %d\nrs485_baud = %d\nrs485_turnaround_us = %d\ncan_bitrate = %d\ncan_id_base = %d\n"
                  "i2c_clock_hz = %d\ni2c_base_addr = %d\ni2c_stretch_limit_us = %d\nfault_window = %d\nfault_limit = %d\n",
                  s.name.c_str(), static_cast<unsigned>(s.port), s.slots, s.pitch_mm, actuator_text(s.actuator), s.steps_per_mm, s.servo_deg_per_mm, s.motion_mm_per_s, s.retry_mm,
                  s.max_retries, s.pocket_check ? 1 : 0, s.film_check ? 1 : 0, s.tension_min, s.tension_max, s.low_pockets, s.retract_enabled ? 1 : 0, s.link_lost_ms,
                  s.server_lost_ms, s.advance_timeout_ms, s.bus_retries, s.offline_after, bus_text(s.bus), s.reply_timeout_ms, s.rs485_baud, s.rs485_turnaround_us, s.can_bitrate,
                  s.can_id_base, s.i2c_clock_hz, s.i2c_base_addr, s.i2c_stretch_limit_us, s.fault_window, s.fault_limit);
    return b;
}

}  // namespace pnp_feeder
