#include "ui_main.h"
#include "ui_chart_style.h"
#include "display_battery.h"
#include "ui_display_battery.h"

namespace {
lv_obj_t *overlay = nullptr, *voltage, *test, *last, *reset, *adc, *io, *status, *start_button, *stop_button;
lv_obj_t *text_label(lv_obj_t *parent, const char *text, int y, int height, const lv_font_t *font) {
    auto label = lv_label_create(parent); lv_label_set_text(label, text);
    lv_obj_set_pos(label, 0, y); lv_obj_set_size(label, 408, height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, font, 0); lv_obj_set_style_text_color(label, ui_theme_text(), 0);
    return label;
}
const char *state_text(BatteryTestState state) {
    switch (state) {
        case BatteryTestState::Running: return "Test läuft (manuell gestartet)";
        case BatteryTestState::Stopped: return "Test beendet / Teiltest";
        case BatteryTestState::Interrupted: return "Test durch Neustart unterbrochen";
        default: return "Noch kein Test gestartet";
    }
}
void close(lv_event_t *) { ui_display_battery_reset(); }
lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int width, lv_event_cb_t callback, bool active = false) {
    auto obj = lv_btn_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,44);
    ui_chart_button_style(obj,active); lv_obj_add_event_cb(obj,callback,LV_EVENT_CLICKED,nullptr);
    auto caption = lv_label_create(obj); lv_label_set_text(caption,text);
    lv_obj_set_style_text_font(caption,&lv_font_montserrat_14,0); lv_obj_center(caption); return obj;
}
}

void ui_format_battery_runtime(char *text, size_t size, uint64_t elapsed_ms, bool lower_bound) {
    const uint64_t minutes = elapsed_ms / 60000;
    snprintf(text,size,"%s%llu h %02llu min",lower_bound ? "mind. " : "",
             (unsigned long long)(minutes/60),(unsigned long long)(minutes%60));
}
void ui_display_battery_reset() {
    if (overlay) lv_obj_del_async(overlay);
    overlay = nullptr;
    voltage = test = last = reset = adc = io = status = start_button = stop_button = nullptr;
}
void ui_display_battery_open() {
    if (overlay) return;
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 32768) {
        auto box = lv_msgbox_create(nullptr,"Akku-Diagnose","Zu wenig freier Speicher.",nullptr,true);
        if (box) lv_obj_center(box); return;
    }
    overlay = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay,480,480); lv_obj_set_pos(overlay,0,0);
    lv_obj_set_style_bg_color(overlay,ui_theme_bg(),0); lv_obj_set_style_bg_opa(overlay,LV_OPA_COVER,0);
    lv_obj_clear_flag(overlay,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(overlay,LV_OBJ_FLAG_CLICKABLE);
    auto panel = ui_create_card(overlay,456,456); lv_obj_center(panel);
    lv_obj_set_style_pad_all(panel,0,0); lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    button(panel,LV_SYMBOL_LEFT " Zurück",12,12,108,close);
    auto heading = lv_label_create(panel); lv_label_set_text(heading,"Display-Akku");
    lv_obj_set_pos(heading,136,25); lv_obj_set_style_text_font(heading,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(heading,ui_theme_text(),0);
    auto content = lv_obj_create(panel); lv_obj_set_pos(content,12,68); lv_obj_set_size(content,432,306);
    lv_obj_set_style_bg_opa(content,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(content,0,0);
    lv_obj_set_style_pad_all(content,4,0); lv_obj_set_scroll_dir(content,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content,LV_SCROLLBAR_MODE_AUTO);
    voltage = text_label(content,"-- V",0,34,&lv_font_montserrat_28);
    test = text_label(content,"",42,64,&lv_font_montserrat_14);
    last = text_label(content,"",118,80,&lv_font_montserrat_14);
    reset = text_label(content,"",210,48,&lv_font_montserrat_12);
    adc = text_label(content,"",270,60,&lv_font_montserrat_12);
    io = text_label(content,"",342,60,&lv_font_montserrat_12);
    status = text_label(content,"",414,76,&lv_font_montserrat_12);
    auto note = text_label(content,"Versorgung vor Teststart manuell trennen.\nTestende bedeutet Teiltest, nicht vollständige Entladung.\nBei Abschaltung bleibt nur die letzte Sicherung erhalten (typisch bis zu 5 min Verlust).\nSOC, Strom und Versorgung sind unbekannt; EXIO0 wird nur roh angezeigt.",502,104,&lv_font_montserrat_12);
    lv_obj_set_style_text_color(note,ui_theme_muted(),0);
    start_button = button(panel,"Test starten",12,394,204,[](lv_event_t*) {
        display_battery_start_test(); ui_display_battery_update();
    },true);
    stop_button = button(panel,"Test beenden",240,394,204,[](lv_event_t*) {
        display_battery_stop_test(); ui_display_battery_update();
    });
    ui_display_battery_update();
}
void ui_display_battery_update() {
    if (!overlay) return;
    const auto reading = display_battery_snapshot(); const auto snapshot = display_battery_test_snapshot();
    const auto &runtime = snapshot.runtime; const uint32_t now = millis();
    char text[256], duration[48], minimum[32];
    if (reading.fresh(now)) ui_label_set_float(voltage,"%.3f V",DisplayBatteryReading::batteryVoltage(reading.raw));
    else ui_label_set_text_if_changed(voltage,"-- V");
    ui_format_battery_runtime(duration,sizeof(duration),runtime.current.elapsed_ms,runtime.current.lower_bound);
    if (runtime.current.minimum_valid) snprintf(minimum,sizeof(minimum),"%.3f V",runtime.current.minimum_mv/1000.0);
    else snprintf(minimum,sizeof(minimum),"unbekannt");
    snprintf(text,sizeof(text),"%s\nMessdauer: %s\nTest-Minimum: %s%s",state_text(runtime.state),
             runtime.state==BatteryTestState::None ? "--" : duration,minimum,runtime.current.sample_gap ? " / Messlücken" : "");
    ui_label_set_text_if_changed(test,text);
    if (runtime.last_state==BatteryTestState::None) snprintf(text,sizeof(text),"Letzter Test: --");
    else {
        ui_format_battery_runtime(duration,sizeof(duration),runtime.last.elapsed_ms,runtime.last.lower_bound);
        if(runtime.last.minimum_valid) snprintf(minimum,sizeof(minimum),"%.3f V",runtime.last.minimum_mv/1000.0);
        else snprintf(minimum,sizeof(minimum),"unbekannt");
        snprintf(text,sizeof(text),"Letzter Test: %s\n%s\nMinimum: %s%s",duration,state_text(runtime.last_state),minimum,
                 runtime.last.sample_gap ? " / Messlücken" : "");
    }
    ui_label_set_text_if_changed(last,text);
    if (runtime.current.restarts) snprintf(text,sizeof(text),"Neustart bei offenem Test: %lu\n%s (Code %lu)",
        (unsigned long)runtime.current.restarts,display_battery_reset_reason(runtime.current.reset_reason),(unsigned long)runtime.current.reset_reason);
    else if (runtime.last.restarts) snprintf(text,sizeof(text),"Letzter Test: %lu Neustart\n%s",(unsigned long)runtime.last.restarts,display_battery_reset_reason(runtime.last.reset_reason));
    else snprintf(text,sizeof(text),"Neustart bei offenem Test: keiner erfasst");
    ui_label_set_text_if_changed(reset,text);
    if(reading.fresh(now)) snprintf(text,sizeof(text),"ADC: %u / 1023 / Eingang %.3f V\nMin/Max seit Messaktivierung: %.3f / %.3f V\n%u/8 Samples / Fehler: %lu / vor %lu s",reading.raw,
        DisplayBatteryReading::adcVoltage(reading.raw),reading.minimum,reading.maximum,reading.accepted,(unsigned long)reading.errors,(unsigned long)((now-reading.updated)/1000));
    else if(reading.seen) snprintf(text,sizeof(text),"ADC: -- / aktuelle Messung fehlt\nLetztes Min/Max: %.3f / %.3f V\n%u/8 Samples / Fehler: %lu",reading.minimum,reading.maximum,reading.accepted,(unsigned long)reading.errors);
    else snprintf(text,sizeof(text),"ADC: -- / Min/Max: --\n%u/8 Samples / Fehler: %lu",reading.accepted,(unsigned long)reading.errors);
    ui_label_set_text_if_changed(adc,text);
    const bool io_fresh = uint32_t(now-reading.io_updated)<=10000;
    char inputs[24],direction[24];
    if(io_fresh&&reading.input_valid)snprintf(inputs,sizeof(inputs),"%u / IN 0x%02X",reading.inputs&1,reading.inputs);
    else snprintf(inputs,sizeof(inputs),"unbekannt");
    if(io_fresh&&reading.direction_valid)snprintf(direction,sizeof(direction),"0x%02X",reading.direction);
    else snprintf(direction,sizeof(direction),"unbekannt");
    snprintf(text,sizeof(text),"V4 EXIO0/STAT roh: %s / DIR: %s\nLadezustand: nicht bewertet\nVersorgung / USB / Akkubetrieb: unbekannt",inputs,direction);
    ui_label_set_text_if_changed(io,text);
    snprintf(text,sizeof(text),"Lokale echte Boardwerte, auch in Demo.\n%s\n%s",snapshot.storage_ok ? "Sicherung: verfügbar / alle 5 min im Test" : "Persistenzfehler: Daten ggf. nicht gespeichert",
        snapshot.invalid_store ? "Gespeicherte Testdaten ungültig; neuer Test nötig." : "Kein automatischer Start oder Wiederanlauf.");
    ui_label_set_text_if_changed(status,text);
    ui_text_color_if_changed(status,snapshot.storage_ok&&!snapshot.invalid_store ? ui_theme_muted() : lv_color_hex(UI_COLOR_WARNING));
    if(runtime.running()){lv_obj_add_state(start_button,LV_STATE_DISABLED);lv_obj_clear_state(stop_button,LV_STATE_DISABLED);}
    else{lv_obj_clear_state(start_button,LV_STATE_DISABLED);lv_obj_add_state(stop_button,LV_STATE_DISABLED);}
}
