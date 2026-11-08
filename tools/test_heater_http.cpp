#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <ArduinoJson.h>
#include "heater_diagnostics.h"
constexpr uint8_t HEATER_TYPE_AUTOTERM=0, HEATER_TYPE_WEBASTO=4;
static bool is_webasto_heater(uint8_t t){return t==HEATER_TYPE_WEBASTO;}
struct { uint8_t heater_type=HEATER_TYPE_WEBASTO; bool data_is_demo=false; HeaterDiagnostics heater_diagnostics; } state;
struct StateLockGuard {};
static uint32_t clock_ms=100;
static uint32_t millis(){return clock_ms;}
#include "heater_under_test.h"
// Minimal LVGL surface for exercising the actual diagnostic-access function.
struct TestWidget { bool hidden=false; };
static TestWidget *btn_diagnostics=nullptr,*diagnostics_overlay=nullptr;
constexpr int LV_OBJ_FLAG_HIDDEN=1;
static unsigned closed=0;
static void lv_obj_add_flag(TestWidget *p,int){p->hidden=true;}
static void lv_obj_clear_flag(TestWidget *p,int){p->hidden=false;}
static void close_diagnostics(void*){diagnostics_overlay=nullptr;++closed;}
#include "heater_ui_under_test.h"
static void check(bool ok,const char *name){if(!ok){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
static bool available(unsigned i,bool demo=false,bool connected=true,uint8_t type=HEATER_TYPE_WEBASTO){return state.heater_diagnostics.available(i,clock_ms,demo,connected,type);}
int main(int argc,char **argv){
    TestWidget button,overlay;btn_diagnostics=&button;
    for(uint8_t type=0;type<=4;++type){
        state.heater_type=type;diagnostics_overlay=&overlay;closed=0;
        update_diagnostics_access();
        check(button.hidden==(type!=HEATER_TYPE_AUTOTERM),"diagnosis button only available for Autoterm");
        check(closed==(type!=HEATER_TYPE_AUTOTERM),"type change closes unsupported diagnosis overlay");
    }
    state.heater_type=HEATER_TYPE_WEBASTO;btn_diagnostics=nullptr;diagnostics_overlay=nullptr;
    update_diagnostics_access();
    check(argc==2,"VanPi flow fixtures required");
    std::ifstream input(argv[1]); std::string live,missing;std::getline(input,live);std::getline(input,missing);
    check(!live.empty()&&!missing.empty(),"flow fixture input");
    parse_heater_json(live.c_str());
    check(available(0)&&available(3),"actual Webasto flow to /heater telemetry");
    check(std::fabs(state.heater_diagnostics.values[0]-13.2f)<0.001f && state.heater_diagnostics.values[3]==36,"flow scaling applied exactly once");
    parse_heater_json(missing.c_str());
    check(!available(0)&&!available(3),"VanPi wait/zero defaults are unavailable, not measured zero");
    parse_heater_json(R"({"heatvolt":"13,2","heattemp":"0","autoterm1":{"heatvolt":99,"heattemp":144}})");
    check(available(0)&&available(3)&&state.heater_diagnostics.values[3]==0,"decimal strings and genuine temperature zero");
    check(std::fabs(state.heater_diagnostics.values[0]-13.2f)<0.001f,"Webasto never borrows Autoterm fields");
    const char *invalid[]={R"({})",R"({"heatvolt":null,"heattemp":null})",R"({"heatvolt":false,"heattemp":true})",
      R"({"heatvolt":"wait","heattemp":"N/A"})",R"({"heatvolt":"NaN","heattemp":"Infinity"})",
      R"({"heatvolt":{},"heattemp":[]})",R"({"heatvolt":"13V","heattemp":"36C"})",
      R"({"autoterm1":{"heatvolt":13,"heattemp":144}})"};
    for(auto p:invalid){parse_heater_json(live.c_str());++clock_ms;parse_heater_json(p);check(!available(0)&&!available(3),"partial/invalid reply clears old field validity");}
    parse_heater_json(live.c_str());parse_heater_json(R"({"heatvolt":12.5})");
    check(available(0)&&!available(3),"voltage-only reply invalidates temperature independently");
    parse_heater_json(live.c_str());parse_heater_json(R"({"heatvolt":0,"heattemp":36})");
    check(!available(0)&&available(3),"default voltage zero does not suppress real temperature");
    parse_heater_json(live.c_str());const auto timestamp=state.heater_diagnostics.updated;
    ++clock_ms;parse_heater_json("{invalid");
    check(state.heater_diagnostics.updated==timestamp,"bad JSON cannot refresh telemetry");
    clock_ms=timestamp+45001;check(!available(0)&&!available(3),"stale HTTP values hidden");
    clock_ms=timestamp;check(!available(0,false,false),"disconnected values hidden");
    check(!available(0,true),"LIVE cache cannot appear as DEMO");
    state.heater_type=HEATER_TYPE_AUTOTERM;check(!available(0,false,true,HEATER_TYPE_AUTOTERM),"cache cannot cross configured heater types");
    parse_heater_json(R"({"heatvolt":0,"heattemp":36,"autoterm1":{"heatvolt":13,"heatfan":0,"heatglow":1.5,"heattemp":144}})");
    check(state.heater_diagnostics.fields==15 && state.heater_diagnostics.values[3]==144,"Autoterm nested diagnostics preserved including real fan zero");
    check(available(0,false,true,HEATER_TYPE_AUTOTERM)&&!available(0),"Autoterm cache unavailable to Webasto UI");
    parse_heater_json(R"({"heatvolt":13,"heatfan":0,"heatglow":1.5,"heattemp":144})");
    check(state.heater_diagnostics.fields==15,"legacy flat Autoterm diagnostics preserved");
    state.heater_type=HEATER_TYPE_WEBASTO;state.data_is_demo=true;parse_heater_json(live.c_str());
    check(available(0,true,false)&&!available(0,false),"DEMO cache separated from LIVE");
    std::puts("PASS: production /heater parser and diagnosis-button access, real VanPi flow output, missing/zero/partial values, freshness, type/source isolation and Autoterm regression.");
}
