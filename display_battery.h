#pragma once
#include <stdint.h>
#include "battery_runtime.h"

// Local display battery only. No inferred SOC, current or charger state.
struct DisplayBatteryReading {
    uint16_t raw = 0;
    uint8_t accepted = 0, attempted = 0;
    uint32_t updated = 0, errors = 0;
    bool valid = false, seen = false;
    uint8_t inputs = 0, direction = 0;
    bool input_valid = false, direction_valid = false;
    uint32_t io_updated = 0;
    float minimum = 0, maximum = 0;
    static constexpr float reference = 3.3f, divider = 3.0f;
    static constexpr float adcVoltage(uint16_t value) { return value * reference / 1023.0f; }
    static constexpr float batteryVoltage(uint16_t value) { return adcVoltage(value) * divider; }
    bool fresh(uint32_t now) const { return valid && uint32_t(now - updated) <= 10000; }
    void finish(uint32_t total, uint8_t good, uint8_t tries, uint32_t now) {
        accepted = good; attempted = tries; errors += tries - good; valid = good != 0;
        if (!valid) return;
        raw = uint16_t((total + good / 2) / good); updated = now;
        const float voltage = batteryVoltage(raw);
        if (!seen || voltage < minimum) minimum = voltage;
        if (!seen || voltage > maximum) maximum = voltage;
        seen = true;
    }
};

struct DisplayBatteryTestSnapshot {
    BatteryRuntime runtime;
    bool storage_ok = false, invalid_store = false;
};
void display_battery_init();
bool display_battery_start_test();
bool display_battery_stop_test();
DisplayBatteryTestSnapshot display_battery_test_snapshot();
const char *display_battery_reset_reason(uint32_t reason);
void display_battery_tick();
DisplayBatteryReading display_battery_snapshot();
// Rounded estimate for the status bar, or -1 without a valid current reading.
int display_battery_estimated_soc(uint32_t now);
