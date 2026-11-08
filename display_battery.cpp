#include "display_battery.h"
#include "WS_CH32_IO.h"
#include "system_state.h"
#include "display_sync.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_timer.h>

namespace {
DisplayBatteryReading reading;
BatteryRuntime runtime;
Preferences storage;
bool initialized = false, enabled = false, storage_open = false, save_ok = true, invalid_store = false, pending_save = false;
uint32_t last_sample = 0, total = 0;
uint8_t good = 0, tries = 0;
uint64_t last_save_attempt = 0;
constexpr uint64_t checkpoint_ms = 300000;

// Provisional 4.2 V 1S LiPo curve, to be tuned with on-device discharge data.
struct SocPoint { float voltage; int percent; };
constexpr SocPoint soc_curve[] = {
    {3.30f, 0}, {3.65f, 10}, {3.75f, 25}, {3.85f, 50},
    {4.00f, 75}, {4.12f, 90}, {4.20f, 100}
};
constexpr float soc_hysteresis = 2.0f;
float filtered_voltage = 0;
int estimated_soc = -1;

float interpolate_soc(float voltage) {
    if (voltage <= soc_curve[0].voltage) return 0;
    for (unsigned i = 1; i < sizeof(soc_curve) / sizeof(soc_curve[0]); ++i) {
        const auto &low = soc_curve[i - 1];
        const auto &high = soc_curve[i];
        if (voltage <= high.voltage) {
            const float fraction = (voltage - low.voltage) / (high.voltage - low.voltage);
            return low.percent + fraction * (high.percent - low.percent);
        }
    }
    return 100;
}

void update_soc_estimate(float voltage, bool valid) {
    // Allow headroom for ADC tolerance. This range cannot detect an absent cell.
    if (!valid || voltage < 2.9f || voltage > 4.35f) {
        estimated_soc = -1;
        return;
    }
    const bool first = estimated_soc < 0;
    filtered_voltage = first ? voltage : filtered_voltage + 0.2f * (voltage - filtered_voltage);
    const float percent = interpolate_soc(filtered_voltage);
    if (first) {
        estimated_soc = int((percent + 5.0f) / 10.0f) * 10;
        return;
    }
    while (estimated_soc < 100 && percent > estimated_soc + 5.0f + soc_hysteresis) {
        estimated_soc += 10;
    }
    while (estimated_soc > 0 && percent < estimated_soc - 5.0f - soc_hysteresis) {
        estimated_soc -= 10;
    }
}
uint64_t runtime_now() { return uint64_t(esp_timer_get_time()) / 1000; }
bool save_runtime() {
    last_save_attempt = runtime_now();
    const auto record = battery_runtime_encode(runtime);
    save_ok = storage_open && storage.putBytes("runtime_v1", record.data(), sizeof(record)) == sizeof(record);
    if (storage_open) display_request_resync(); // Existing RGB recovery after flash access.
    pending_save = !save_ok;
    if (save_ok) invalid_store = false;
    return save_ok;
}
}

void display_battery_init() {
    if (storage_open) storage.end();
    reading = {}; runtime = {}; enabled = false; total = good = tries = 0;
    invalid_store = pending_save = false;
    estimated_soc = -1;
    storage_open = storage.begin("display_bat", false); save_ok = storage_open;
    if (storage_open && storage.getBytesLength("runtime_v1")) {
        BatteryRuntimeStorage record{};
        invalid_store = storage.getBytesLength("runtime_v1") != sizeof(record) ||
                        storage.getBytes("runtime_v1", record.data(), sizeof(record)) != sizeof(record) ||
                        !battery_runtime_decode(record, runtime);
        if (!invalid_store && runtime.recover(uint32_t(esp_reset_reason()))) save_runtime();
    }
    initialized = true; last_save_attempt = runtime_now();
}
DisplayBatteryReading display_battery_snapshot() { return reading; }
int display_battery_estimated_soc(uint32_t now) { return reading.fresh(now) ? estimated_soc : -1; }
DisplayBatteryTestSnapshot display_battery_test_snapshot() {
    return {runtime, storage_open && save_ok, invalid_store};
}
bool display_battery_start_test() {
    if (!initialized) display_battery_init();
    if (runtime.running() || !storage_open) return false;
    const auto previous = runtime;
    runtime.start(runtime_now());
    // A test starts a fresh sample window; do not mix pre-start USB samples into its minimum.
    if (!save_runtime()) { runtime = previous; return false; }
    total = good = tries = 0; return true;
}
bool display_battery_stop_test() {
    if (!runtime.stop(runtime_now())) return false;
    return save_runtime(); // RAM result remains stopped even if persistence fails.
}
const char *display_battery_reset_reason(uint32_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "Einschalten / Power-on";
        case ESP_RST_SW: return "Software-Neustart";
        case ESP_RST_PANIC: return "Programmfehler";
        case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "Watchdog";
        case ESP_RST_BROWNOUT: return "Unterspannung (Ursache unklar)";
        default: return "Anderer / unbekannter Resetgrund";
    }
}
void display_battery_tick() {
    if (!initialized) display_battery_init();
    const uint64_t clock = runtime_now(); runtime.advance(clock);
    if ((runtime.running() || pending_save) && clock - last_save_attempt >= checkpoint_ms) save_runtime();
    bool show_diagnostics, show_soc;
    {
        StateLockGuard lock;
        show_diagnostics = state.home_show_display_battery;
        show_soc = state.home_show_display_battery_status;
    }
    show_diagnostics = show_diagnostics || runtime.running(); // Hiding Home never stops a running test.
    const bool requested = show_diagnostics || show_soc;
    if (!show_soc) estimated_soc = -1;
    if (requested != enabled) {
        enabled = requested; reading = {}; total = good = tries = 0; last_sample = millis() - 250;
    }
    if (!enabled) return;
    const uint32_t now = millis();
    if (uint32_t(now - last_sample) < 250) return;
    last_sample = now;
    uint8_t bytes[2] = {};
    const bool received = WS_CH32_IO::readRegisterBytes(Wire, WS_CH32_IO::REG_ADC,
                              bytes, sizeof(bytes), WS_CH32_IO::I2C_ADDRESS, 1);
    const uint16_t raw = uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
    if (received && raw <= 1023) { total += raw; ++good; }
    if (++tries == 8) {
        if (!reading.fresh(now)) estimated_soc = -1;
        reading.finish(total, good, tries, now);
        const float voltage = DisplayBatteryReading::batteryVoltage(reading.raw);
        if (show_soc) update_soc_estimate(voltage, reading.valid);
        runtime.sample(uint32_t(voltage * 1000.0f + 0.5f), reading.valid);
        if (show_diagnostics) {
            // Read-only V4 diagnosis. EXIO0/STAT is NOT interpreted as supply detection.
            reading.input_valid = WS_CH32_IO::readRegisterBytes(Wire, WS_CH32_IO::REG_INPUT,
                                      &reading.inputs, 1, WS_CH32_IO::I2C_ADDRESS, 1);
            reading.direction_valid = WS_CH32_IO::readRegisterBytes(Wire, WS_CH32_IO::REG_DIRECTION,
                                      &reading.direction, 1, WS_CH32_IO::I2C_ADDRESS, 1);
            reading.io_updated = now;
        }
        total = good = tries = 0;
    }
}
