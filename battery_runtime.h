#pragma once
#include <array>
#include <stdint.h>

enum class BatteryTestState : uint32_t { None, Running, Stopped, Interrupted };
struct BatteryTestResult {
    uint64_t elapsed_ms = 0;
    uint32_t minimum_mv = 0, restarts = 0, reset_reason = 0;
    bool minimum_valid = false, lower_bound = false, sample_gap = false;
};
struct BatteryRuntime {
    BatteryTestState state = BatteryTestState::None, last_state = BatteryTestState::None;
    BatteryTestResult current, last;
    uint64_t last_tick = 0;
    bool running() const { return state == BatteryTestState::Running; }
    bool start(uint64_t now) {
        if (running()) return false;
        current = {}; state = BatteryTestState::Running; last_tick = now; return true;
    }
    void advance(uint64_t now) {
        if (running() && now >= last_tick) {
            if (now - last_tick > 10000) current.sample_gap = true;
            current.elapsed_ms += now - last_tick;
        }
        last_tick = now;
    }
    void sample(uint32_t mv, bool valid) {
        if (!running()) return;
        if (!valid) { current.sample_gap = true; return; }
        if (!current.minimum_valid || mv < current.minimum_mv) current.minimum_mv = mv;
        current.minimum_valid = true;
    }
    bool stop(uint64_t now) {
        if (!running()) return false;
        advance(now); state = BatteryTestState::Stopped;
        last = current; last_state = state; return true;
    }
    bool recover(uint32_t reason) {
        if (!running()) return false;
        state = BatteryTestState::Interrupted; current.lower_bound = true;
        ++current.restarts; current.reset_reason = reason;
        last = current; last_state = state; return true;
    }
};

// Fixed-width, versioned NVS record; independent of C++ struct padding.
using BatteryRuntimeStorage = std::array<uint32_t, 17>;
inline uint32_t battery_runtime_checksum(const BatteryRuntimeStorage &words) {
    uint32_t value = 2166136261U;
    for (unsigned i = 0; i < 16; ++i) for (unsigned b = 0; b < 4; ++b) {
        value ^= (words[i] >> (b * 8)) & 255U; value *= 16777619U;
    }
    return value;
}
inline BatteryRuntimeStorage battery_runtime_encode(const BatteryRuntime &runtime) {
    BatteryRuntimeStorage w{}; w[0] = 0x42525431; w[1] = 1;
    w[2] = uint32_t(runtime.state); w[9] = uint32_t(runtime.last_state);
    auto put = [&](unsigned flags, unsigned time, unsigned minimum, unsigned restarts, unsigned reset, const BatteryTestResult &r) {
        w[flags] = (r.minimum_valid ? 1U : 0U) | (r.lower_bound ? 2U : 0U) | (r.sample_gap ? 4U : 0U);
        w[time] = uint32_t(r.elapsed_ms); w[time+1] = uint32_t(r.elapsed_ms >> 32);
        w[minimum] = r.minimum_mv; w[restarts] = r.restarts; w[reset] = r.reset_reason;
    };
    put(3,4,6,7,8,runtime.current); put(10,11,13,14,15,runtime.last);
    w[16] = battery_runtime_checksum(w); return w;
}
inline bool battery_runtime_decode(const BatteryRuntimeStorage &w, BatteryRuntime &runtime) {
    if (w[0] != 0x42525431 || w[1] != 1 || w[16] != battery_runtime_checksum(w) ||
        w[2] > 3 || w[9] > 3 || w[9] == uint32_t(BatteryTestState::Running) ||
        (w[3] & ~7U) || (w[10] & ~7U) || w[6] > 9900 || w[13] > 9900) return false;
    BatteryRuntime decoded;
    decoded.state = BatteryTestState(w[2]); decoded.last_state = BatteryTestState(w[9]);
    auto get = [&](unsigned flags, unsigned time, unsigned minimum, unsigned restarts, unsigned reset, BatteryTestResult &r) {
        r.elapsed_ms = uint64_t(w[time]) | (uint64_t(w[time+1]) << 32);
        r.minimum_mv = w[minimum]; r.restarts = w[restarts]; r.reset_reason = w[reset];
        r.minimum_valid = w[flags] & 1; r.lower_bound = w[flags] & 2; r.sample_gap = w[flags] & 4;
    };
    get(3,4,6,7,8,decoded.current); get(10,11,13,14,15,decoded.last);
    runtime = decoded; return true;
}
