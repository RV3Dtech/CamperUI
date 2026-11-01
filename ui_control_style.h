#pragma once
#include "ui_main.h"
constexpr int UI_FAVORITE_HEIGHT=56;
inline void ui_control_surface_style(lv_obj_t *obj) {
    lv_obj_set_style_radius(obj,ui_radius_button(),0);
    lv_obj_set_style_bg_color(obj,ui_theme_card(),0);
    lv_obj_set_style_border_width(obj,ui_border_width(),0);
    lv_obj_set_style_border_color(obj,ui_theme_border(),0);
    lv_obj_set_style_text_color(obj,ui_theme_btn_text(),0);
    lv_obj_set_style_shadow_width(obj,0,0);
}
inline void ui_favorite_button_style(lv_obj_t *button, lv_color_t active_color = ui_theme_primary()) {
    ui_control_surface_style(button);
    lv_obj_set_height(button,UI_FAVORITE_HEIGHT);
    // Include the combined pressed state so LVGL's base theme cannot supply
    // a different accent while a checked button is touched.
    for(lv_style_selector_t selector : {lv_style_selector_t(LV_STATE_CHECKED),
                                      lv_style_selector_t(LV_STATE_CHECKED | LV_STATE_PRESSED)}) {
        lv_obj_set_style_bg_color(button,active_color,selector);
        lv_obj_set_style_border_color(button,active_color,selector);
        lv_obj_set_style_text_color(button,ui_theme_btn_active_text(),selector);
    }
}
