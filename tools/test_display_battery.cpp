#include "display_battery.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Mock persistence and read-only I2C; production driver is included below.
static std::vector<uint8_t> nvs;
static unsigned writes=0;
static bool write_ok=true,begin_ok=true;
class Preferences {
public:
    bool begin(const char *name,bool readonly){assert(std::strcmp(name,"display_bat")==0&&!readonly);return begin_ok;}
    void end(){}
    size_t getBytesLength(const char*){return nvs.size();}
    size_t getBytes(const char*,void *out,size_t length){if(nvs.size()!=length)return 0;std::memcpy(out,nvs.data(),length);return length;}
    size_t putBytes(const char*,const void *data,size_t length){++writes;if(!write_ok)return 0;auto p=static_cast<const uint8_t*>(data);nvs.assign(p,p+length);return length;}
};
struct {bool home_show_display_battery=false,home_show_display_battery_status=false,debug_mode=false;} state;
struct StateLockGuard {};
static uint64_t test_time=100;
static uint32_t millis(){return uint32_t(test_time);}
static int64_t esp_timer_get_time(){return int64_t(test_time*1000);}
enum {ESP_RST_POWERON=1,ESP_RST_SW=3,ESP_RST_PANIC=4,ESP_RST_INT_WDT=5,ESP_RST_TASK_WDT=6,ESP_RST_WDT=7,ESP_RST_BROWNOUT=9};
static uint32_t reboot_reason=ESP_RST_PANIC;
static uint32_t esp_reset_reason(){return reboot_reason;}
static unsigned resyncs=0;
static void display_request_resync(){++resyncs;}
static int Wire=0;
namespace WS_CH32_IO {
constexpr uint8_t REG_ADC=6,REG_INPUT=4,REG_DIRECTION=2,I2C_ADDRESS=0x24;
static uint16_t next_raw=414;
static bool ok=true,io_ok=true;
static unsigned reads=0,io_reads=0;
static bool readRegisterBytes(int&,uint8_t reg,uint8_t *data,size_t length,uint8_t address,uint8_t retries){
    assert(address==I2C_ADDRESS&&retries==1);
    if(reg==REG_ADC){assert(length==2);++reads;data[0]=next_raw&255;data[1]=next_raw>>8;return ok;}
    assert((reg==REG_INPUT||reg==REG_DIRECTION)&&length==1);++io_reads;data[0]=reg==REG_INPUT?1:255;return io_ok;
}
}
#include "display_battery_under_test.h"
static void window(){for(int i=0;i<8;++i){display_battery_tick();test_time+=250;}}
int main(){
    // Real ADC zero is different from a failed read; the clock wraps safely.
    DisplayBatteryReading r;r.finish(8*414,8,8,100);
    assert(std::fabs(r.batteryVoltage(r.raw)-4.00645f)<0.001f&&r.fresh(100));
    r.finish(7*400,7,8,200);assert(r.errors==1&&r.accepted==7&&r.minimum<r.maximum);
    r.finish(0,0,8,300);assert(!r.valid&&r.updated==200&&r.errors==9);
    r.finish(0,8,8,400);assert(r.fresh(400)&&r.minimum==0);
    assert(r.fresh(10400)&&!r.fresh(10401));
    r.finish(8*1023,8,8,0xfffffff0U);assert(r.fresh(10));

    BatteryRuntime model;assert(model.start(0));assert(!model.start(100));
    model.advance(5000);model.sample(3900,true);model.sample(3800,true);model.sample(0,false);
    assert(model.stop(6000)&&model.current.elapsed_ms==6000&&model.last.minimum_mv==3800&&model.last.sample_gap);
    assert(model.last_state==BatteryTestState::Stopped&&!model.last.lower_bound);
    assert(!model.stop(9000));
    assert(model.start(10000));model.advance((uint64_t(1)<<32)+10000);model.sample(3700,true);
    auto encoded=battery_runtime_encode(model);BatteryRuntime decoded;
    assert(battery_runtime_decode(encoded,decoded)&&decoded.current.elapsed_ms==(uint64_t(1)<<32));
    assert(decoded.recover(ESP_RST_PANIC)&&!decoded.running()&&decoded.current.lower_bound&&decoded.current.restarts==1);
    assert(decoded.last_state==BatteryTestState::Interrupted&&!decoded.recover(ESP_RST_POWERON));
    decoded.advance(99999999999ULL);assert(decoded.current.elapsed_ms==(uint64_t(1)<<32));
    for(size_t i=0;i<encoded.size();++i){auto damaged=encoded;damaged[i]^=1;assert(!battery_runtime_decode(damaged,decoded));}

    display_battery_init();display_battery_tick();assert(writes==0&&WS_CH32_IO::reads==0);
    state.home_show_display_battery=true;window();auto reading=display_battery_snapshot();
    assert(reading.valid&&reading.raw==414&&reading.accepted==8&&WS_CH32_IO::reads==8);
    assert(reading.input_valid&&reading.inputs==1&&reading.direction==255&&WS_CH32_IO::io_reads==2);
    const auto reads=WS_CH32_IO::reads;test_time-=249;display_battery_tick();assert(WS_CH32_IO::reads==reads);test_time+=249;
    WS_CH32_IO::next_raw=65535;window();assert(!display_battery_snapshot().valid&&display_battery_snapshot().errors==8);
    WS_CH32_IO::ok=false;window();assert(display_battery_snapshot().errors==16);
    state.home_show_display_battery=false;display_battery_tick();const auto stopped=WS_CH32_IO::reads;
    test_time+=10000;display_battery_tick();assert(WS_CH32_IO::reads==stopped&&!display_battery_snapshot().seen&&writes==0);
    // The status-only path uses the shared ADC and skips the diagnostic I2C registers.
    state.home_show_display_battery_status = true;
    WS_CH32_IO::ok = true;
    WS_CH32_IO::next_raw = 434;
    const auto before_status = WS_CH32_IO::reads;
    const auto before_status_io = WS_CH32_IO::io_reads;
    window();
    assert(display_battery_estimated_soc(millis()) == 100);
    assert(WS_CH32_IO::reads == before_status + 8 && WS_CH32_IO::io_reads == before_status_io);
    WS_CH32_IO::next_raw = 398;
    window();
    assert(display_battery_estimated_soc(millis()) >= 90); // One low window is smoothed.
    assert(display_battery_estimated_soc(millis() + 10001) == -1);
    WS_CH32_IO::next_raw = 0;
    window();
    assert(display_battery_estimated_soc(millis()) == -1 && display_battery_snapshot().valid);
    WS_CH32_IO::next_raw = 400;
    window();
    assert(display_battery_estimated_soc(millis()) == 50); // First valid sample after the gap.
    state.home_show_display_battery_status = false;
    display_battery_tick();
    assert(display_battery_estimated_soc(millis()) == -1);
    WS_CH32_IO::ok=true;WS_CH32_IO::next_raw=400;state.debug_mode=true;state.home_show_display_battery=true;window();
    assert(display_battery_snapshot().raw==400); // Real ADC even in Demo.

    assert(display_battery_start_test()&&writes==1);const uint64_t began=test_time;
    assert(!display_battery_start_test()&&writes==1);
    state.home_show_display_battery=false;window(); // Sampling and test survive hidden Home.
    assert(display_battery_test_snapshot().runtime.running()&&display_battery_snapshot().valid);
    auto test=display_battery_test_snapshot().runtime;
    assert(test.current.minimum_valid&&test.current.minimum_mv>3800&&test.current.minimum_mv<3900);
    WS_CH32_IO::ok=false;window();assert(display_battery_test_snapshot().runtime.current.sample_gap);
    WS_CH32_IO::ok=true;
    test_time=began+299999;display_battery_tick();assert(writes==1);
    test_time=began+300000;display_battery_tick();assert(writes==2);
    test_time=began+301000;display_battery_tick();assert(writes==2);
    // Simulated power loss: only the persisted checkpoint survives, never downtime.
    test_time=0;display_battery_init();test=display_battery_test_snapshot().runtime;
    assert(!test.running()&&test.state==BatteryTestState::Interrupted&&test.current.lower_bound);
    assert(test.current.elapsed_ms==300000&&test.current.restarts==1&&writes==3);
    test_time=900000;display_battery_tick();assert(display_battery_test_snapshot().runtime.current.elapsed_ms==300000);
    display_battery_init();assert(writes==3&&display_battery_test_snapshot().runtime.current.restarts==1);
    assert(display_battery_start_test()&&writes==4);
    test_time+=61000;display_battery_tick();assert(display_battery_stop_test()&&writes==5);
    test=display_battery_test_snapshot().runtime;
    assert(test.last_state==BatteryTestState::Stopped&&test.last.elapsed_ms==61000&&!test.last.lower_bound);
    display_battery_init();assert(display_battery_test_snapshot().runtime.last.elapsed_ms==61000&&writes==5);
    // Failed persistence never pretends a test start was safely recorded.
    write_ok=false;assert(!display_battery_start_test()&&!display_battery_test_snapshot().runtime.running());
    assert(!display_battery_test_snapshot().storage_ok);write_ok=true;
    assert(display_battery_start_test());write_ok=false;test_time+=1000;
    assert(!display_battery_stop_test()&&!display_battery_test_snapshot().runtime.running());
    const auto failed_writes=writes;test_time+=299999;display_battery_tick();assert(writes==failed_writes);
    write_ok=true;test_time+=1;display_battery_tick();assert(writes==failed_writes+1);
    assert(display_battery_test_snapshot().storage_ok);
    nvs[0]^=1;display_battery_init();assert(display_battery_test_snapshot().invalid_store);
    assert(display_battery_test_snapshot().runtime.state==BatteryTestState::None);
    assert(display_battery_start_test()&&!display_battery_test_snapshot().invalid_store);
    assert(display_battery_stop_test());
    begin_ok=false;display_battery_init();assert(!display_battery_start_test()&&!display_battery_test_snapshot().storage_ok);
    std::puts("PASS: production battery polling, 64-bit test timer, minimum, missing data, CRC/versioned persistence, checkpoint rate, reboot recovery, save failures and hidden-Home/Demo behavior.");
}
